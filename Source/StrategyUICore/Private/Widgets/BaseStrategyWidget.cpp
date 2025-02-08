// Copyright Mike Desrosiers 2025, All Rights Reserved.

#include "Widgets/BaseStrategyWidget.h"

#include <Components/CanvasPanel.h>
#include <Components/CanvasPanelSlot.h>
#include <Blueprint/WidgetTree.h>
#include <Editor/WidgetCompilerLog.h>
#include <Modules/ModuleManager.h>

#include "Interfaces/IStrategyDataProvider.h"
#include "Interfaces/IStrategyEntryBase.h"
#include "Interfaces/IStrategyEntryWidgetProvider.h"
#include "Strategies/BaseLayoutStrategy.h"
#include "Utils/LogStrategyUI.h"
#include "Utils/StrategyUIFunctionLibrary.h"
#include "Utils/StrategyUIGameplayTags.h"
#include "Utils/ReflectedObjectsDebugCategory.h"
#include "Widgets/SStrategyCanvasPanel.h"

#define WITH_MVVM FModuleManager::Get().IsModuleLoaded("ModelViewViewModel")
#define IS_DATA_PROVIDER_READY_AND_VALID(DataProvider) \
	IsValid(DataProvider) && DataProvider->Implements<UStrategyDataProvider>() && IStrategyDataProvider::Execute_IsProviderReady(DataProvider)

#if WITH_EDITOR

void UBaseStrategyWidget::ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	if (!LayoutStrategy)
	{
		CompileLog.Error(FText::FromString(TEXT("Please assign a LayoutStrategy in the details panel!")));
	}
	else
	{
		TArray<FText> Errors;
		LayoutStrategy->ValidateStrategy(Errors);
		for (const FText& Text : Errors)
		{
			CompileLog.Error(Text);
		}
	}

	if (!DefaultEntryWidgetClass)
	{
		CompileLog.Error(FText::FromString(TEXT("Please assign an EntryWidgetClass in the details panel!")));
	}
	else
	{
		if (!DefaultEntryWidgetClass->ImplementsInterface(UStrategyEntryBase::StaticClass()))
		{
			CompileLog.Error(FText::FromString(TEXT("EntryWidgetClass must implement IStrategyEntryBase interface!")));
		}
	}
	
	// Warn if MVVM is loaded but also using DataProvider
	if (DataProvider && DefaultDataProviderClass && WITH_MVVM)
	{
		CompileLog.Warning(FText::FromString(TEXT(
			"You are using the MVVM plugin but have set a DataProvider in BaseStrategyWidget. "
			"Consider removing DataProvider and bind SetItems to a view model. Do not use both simultaneously."
		)));
	}
}

void UBaseStrategyWidget::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (LayoutStrategy)
	{
		GetLayoutStrategyChecked().InitializeStrategy(this);

		TArray<FText> Errors;
		GetLayoutStrategyChecked().ValidateStrategy(Errors);
		for (const FText& Text : Errors)
		{
			UE_LOG(LogStrategyUI, Error, TEXT("%s"), *Text.ToString());
		}

		Reset();
		TryCreateDefaultDataProvider();
		RefreshFromProvider();
		UpdateWidgets();

#if WITH_GAMEPLAY_DEBUGGER
		UpdateReflectedObjectsDebugCategory();
#endif
	}
}

#endif // WITH_EDITOR

#pragma region UBaseStrategyWidget - API Base
void UBaseStrategyWidget::SetLayoutStrategy(UBaseLayoutStrategy* NewStrategy)
{
	UE_CLOG(
		!LayoutStrategy,
		LogStrategyUI,
		Error,
		TEXT("%hs called with nullptr layout strategy -- a valid strategy is required."),
		__FUNCTION__
	);

	if (LayoutStrategy == NewStrategy)
	{
		// No change
		return;
	}

	LayoutStrategy = NewStrategy;
	if (LayoutStrategy)
	{
		LayoutStrategy->InitializeStrategy(this);

		TArray<FText> ErrorText;
		LayoutStrategy->ValidateStrategy(ErrorText);
		for (const FText& Text : ErrorText)
		{
			UE_LOG(LogStrategyUI, Error, TEXT("%s"), *Text.ToString());
		}

		UpdateWidgets();
	}
}

void UBaseStrategyWidget::SetItems(const TArray<UObject*>& InItems)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	UE_LOG(LogStrategyUI, Verbose, TEXT("%s: Setting %d items"), *GetName(), InItems.Num());

	if (!ensureAlwaysMsgf(
		LayoutStrategy, 
		TEXT("No LayoutStrategy assigned to %s!"), *GetName()))
	{
		return;
	}

	SetItems_Internal(InItems);
}

void UBaseStrategyWidget::SetDataProvider(UObject* NewProvider)
{
	// Unbind from any existing provider
	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		UE_LOG(LogStrategyUI, Verbose, TEXT("%hs: Unbinding from existing data provider"), __FUNCTION__);
		IStrategyDataProvider::Execute_GetOnDataProviderUpdated(DataProvider)->OnDataProviderUpdatedDelegate.RemoveDynamic(
			this,
			&UBaseStrategyWidget::OnDataProviderUpdated
		);
	}

	DataProvider = NewProvider;

	// Initialize the new provider
	if (DataProvider)
	{
		UE_LOG(LogStrategyUI, Verbose, TEXT("%hs: Initializing new data provider"), __FUNCTION__);
		IStrategyDataProvider::Execute_InitializeDataProvider(DataProvider);
	}

	// If valid/ready, bind to it and set items
	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		UE_LOG(LogStrategyUI, Verbose, TEXT("%hs: Binding to new data provider"), __FUNCTION__);
		IStrategyDataProvider::Execute_GetOnDataProviderUpdated(DataProvider)->OnDataProviderUpdatedDelegate.AddDynamic(
			this,
			&UBaseStrategyWidget::OnDataProviderUpdated
		);

		SetItems(IStrategyDataProvider::Execute_GetDataItems(DataProvider));
	}
}

void UBaseStrategyWidget::Reset()
{
	UE_LOG(LogStrategyUI, Verbose, TEXT("%s - %hs: Begin widget reset"), *GetName(), __FUNCTION__);

	// Unbind from data provider
	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		FOnDataProviderUpdated OnDataProviderUpdated =
			IStrategyDataProvider::Execute_GetOnDataProviderUpdated(DataProvider)->OnDataProviderUpdatedDelegate;

		if (OnDataProviderUpdated.IsAlreadyBound(this, &UBaseStrategyWidget::OnDataProviderUpdated))
		{
			UE_LOG(LogStrategyUI, Verbose, TEXT("%s - %hs: Unbinding from data provider"), *GetName(), __FUNCTION__);
			OnDataProviderUpdated.RemoveDynamic(this, &UBaseStrategyWidget::OnDataProviderUpdated);
		}

		DataProvider = nullptr;
	}

	SelectedDataIndices.Empty();
	FocusedGlobalIndex = 0;
	FocusedDataIndex   = INDEX_NONE;
	IndexToWidgetMap.Empty();
	IndexToTagStateMap.Empty();
	IndexToPositionMap.Empty();
	Items.Empty();


#if WITH_GAMEPLAY_DEBUGGER
	if (FReflectedObjectsDebugCategory::ActiveInstance.IsValid())
	{
		FReflectedObjectsDebugCategory::ActiveInstance->ClearTargets();
	}
#endif

	UE_LOG(LogStrategyUI, Verbose, TEXT("%s - %hs: End widget reset"), *GetName(), __FUNCTION__);
}

void UBaseStrategyWidget::UpdateFocusedIndex(const int32 InNewGlobalFocusIndex)
{
	if (FocusedGlobalIndex == InNewGlobalFocusIndex)
	{
		return; // No change
	}

	const FGameplayTag& FocusedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Focused;

	//----------------------------------------------------------
	// Unfocus old index
	//----------------------------------------------------------
	const int32 OldDataIndex = FocusedDataIndex;
	if (OldDataIndex != INDEX_NONE)
	{
		for (auto& Pair : IndexToWidgetMap)
		{
			const int32 MappedGlobalIndex = Pair.Key;
			UUserWidget* Widget = Pair.Value.Get();
			if (!Widget) { continue; }

			const int32 MappedDataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(MappedGlobalIndex);
			if (MappedDataIndex == OldDataIndex)
			{
				UpdateEntryInteractionTagState(MappedGlobalIndex, FocusedState, /*bEnable=*/ false);
			}
		}
	}

	//----------------------------------------------------------
	// Update new focus, broadcast
	//----------------------------------------------------------
	FocusedGlobalIndex = InNewGlobalFocusIndex;
	FocusedDataIndex   = GetLayoutStrategyChecked().GlobalIndexToDataIndex(InNewGlobalFocusIndex);

	if (FocusedDataIndex != INDEX_NONE && Items.IsValidIndex(FocusedDataIndex))
	{
		OnItemFocused.Broadcast(FocusedDataIndex, Items[FocusedDataIndex]);
	}
	else
	{
		OnItemFocused.Broadcast(INDEX_NONE, nullptr);
	}

	//----------------------------------------------------------
	// Focus all entries for the new data index
	//----------------------------------------------------------
	const int32 NewDataIndex = FocusedDataIndex;
	if (NewDataIndex != INDEX_NONE)
	{
		for (auto& Pair : IndexToWidgetMap)
		{
			const int32 MappedGlobalIndex = Pair.Key;
			UUserWidget* Widget = Pair.Value.Get();
			if (!Widget) { continue; }

			const int32 MappedDataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(MappedGlobalIndex);
			if (MappedDataIndex == NewDataIndex)
			{
				UpdateEntryInteractionTagState(MappedGlobalIndex, FocusedState, /*bEnable=*/ true);
			}
		}
	}
}

void UBaseStrategyWidget::SetSelectedGlobalIndex(const int32 InGlobalIndex, const bool bShouldBeSelected)
{
	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(InGlobalIndex);
	if (DataIndex == INDEX_NONE)
	{
		return; // out-of-range (e.g., "gap" index)
	}

	const bool bAlreadySelected = SelectedDataIndices.Contains(DataIndex);
	const FGameplayTag& SelectedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Selected;

	// Update the interaction tag for all widgets of this data index
	for (auto& Pair : IndexToWidgetMap)
	{
		const int32 MappedGlobalIndex = Pair.Key;
		const UUserWidget* Widget     = Pair.Value.Get();
		if (!Widget) { continue; }

		const int32 MappedDataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(MappedGlobalIndex);
		if (MappedDataIndex == DataIndex)
		{
			UpdateEntryInteractionTagState(MappedGlobalIndex, SelectedState, bShouldBeSelected);
		}
	}

	// Add or remove from the selected set
	if (bShouldBeSelected && !bAlreadySelected)
	{
		SelectedDataIndices.Add(DataIndex);
		UObject* Item = Items.IsValidIndex(DataIndex) ? Items[DataIndex] : nullptr;
		OnItemSelected.Broadcast(DataIndex, Item);
	}
	else if (!bShouldBeSelected && bAlreadySelected)
	{
		SelectedDataIndices.Remove(DataIndex);
	}
}

void UBaseStrategyWidget::ToggleFocusedIndexSelection()
{
	const bool bNewSelected = !SelectedDataIndices.Contains(FocusedDataIndex);
	SetSelectedGlobalIndex(FocusedGlobalIndex, bNewSelected);
}
#pragma endregion

#pragma region UUserWidget & UWidget Overrides
void UBaseStrategyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LayoutStrategy)
	{
		const int32 MaxVisibleEntries = GetLayoutStrategyChecked().MaxVisibleEntries;
		IndexToTagStateMap.Reserve(MaxVisibleEntries);
		IndexToWidgetMap.Reserve(MaxVisibleEntries);
	}

	TryCreateDefaultDataProvider();

#if WITH_GAMEPLAY_DEBUGGER
	UpdateReflectedObjectsDebugCategory();
#endif

	UpdateWidgets();
}

void UBaseStrategyWidget::NativeDestruct()
{
	Reset();
	Super::NativeDestruct();
}

TSharedRef<SWidget> UBaseStrategyWidget::RebuildWidget()
{
	if (IsDesignTime())
	{
		return SNew(SSpacer);
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%s: No valid world found!"), *GetName());
		return SNew(SSpacer);
	}
	APlayerController* PC = GetOwningLocalPlayer()->GetPlayerController(World);
	if (!PC)
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%s: No valid player controller found!"), *GetName());
		return SNew(SSpacer);
	}
	
	StrategyCanvasPanel = SNew(SStrategyCanvasPanel);
	StrategyCanvasPanel->InitializePools(World, PC);

	return StrategyCanvasPanel.ToSharedRef();
}

void UBaseStrategyWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	StrategyCanvasPanel.Reset();
}

int32 UBaseStrategyWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	int32 MaxLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled
	);
	
	if (CachedSize != AllottedGeometry.GetLocalSize())
	{
		CachedSize = AllottedGeometry.GetLocalSize();
		Center = CachedSize * 0.5f;
	}

	// Optional debug drawing
	if (bPaintDebugInfo && LayoutStrategy)
	{
		LayoutStrategy->DrawDebugVisuals(AllottedGeometry, OutDrawElements, MaxLayer, Center);
		MaxLayer++;
	}

	return MaxLayer;
}
#pragma endregion

#pragma region UBaseStrategyWidget - Entry Widgets Pool & Handling
UUserWidget* UBaseStrategyWidget::AcquireEntryWidget(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!ensureAlwaysMsgf(StrategyCanvasPanel.IsValid(), TEXT("No StrategyCanvasPanel found!")))
	{
		return nullptr;
	}

	// (1) If a widget already exists for this index, reuse it
	if (const TWeakObjectPtr<UUserWidget>* ExistingPtr = IndexToWidgetMap.Find(GlobalIndex))
	{
		if (ExistingPtr->IsValid())
		{
			UE_LOG(
				LogStrategyUI, 
				Verbose, 
				TEXT("Reusing widget %s for global index %d"),
				*ExistingPtr->Get()->GetName(),
				GlobalIndex
			);
			return ExistingPtr->Get();
		}
	}

	// (2) Determine the data item for this index
	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
	const UObject* DataItem = Items.IsValidIndex(DataIndex) ? Items[DataIndex] : nullptr;

	// (3) Decide which widget class to use (could come from data item, or fallback)
	TSubclassOf<UUserWidget> DesiredClass = nullptr;

	if (DataItem && DataItem->Implements<UStrategyEntryWidgetProvider>())
	{
		// A) Attempt to get direct class
		DesiredClass = IStrategyEntryWidgetProvider::Execute_GetEntryWidgetClass(DataItem);

		// B) If still none, attempt to get from a “widget tag”
		if (!DesiredClass)
		{
			const FGameplayTag ItemTag = IStrategyEntryWidgetProvider::Execute_GetEntryWidgetTag(DataItem);
			if (ItemTag.IsValid() && ItemTag != FGameplayTag::EmptyTag)
			{
				if (const TSubclassOf<UUserWidget> FoundClass = UStrategyUIFunctionLibrary::GetWidgetClassForTag(ItemTag))
				{
					DesiredClass = *FoundClass;
				}
			}
		}
	}

	// C) If none found, fallback to default
	if (!DesiredClass)
	{
		if (!ensure(DefaultEntryWidgetClass))
		{
			UE_LOG(
				LogStrategyUI,
				Error,
				TEXT("%hs: No DefaultEntryWidgetClass set! (DataItem=%s at Index %d)"),
				__FUNCTION__,
				DataItem ? *DataItem->GetName() : TEXT("Null"),
				DataIndex
			);
			return nullptr;
		}
		DesiredClass = DefaultEntryWidgetClass;
	}

	// (4) Spawn from the pool
	UUserWidget* NewWidget = StrategyCanvasPanel->AcquireEntryWidget(DesiredClass, GlobalIndex);
	check(NewWidget);

	UE_LOG(LogStrategyUI, Verbose, TEXT("Creating new widget %s for global index %d"), *NewWidget->GetName(), GlobalIndex);
	IndexToWidgetMap.Add(GlobalIndex, NewWidget);

	// If no state container yet, initialize it with "Pooled"
	if (!IndexToTagStateMap.Contains(GlobalIndex))
	{
		FGameplayTagContainer InitialStateContainer;
		InitialStateContainer.AddTag(StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Pooled);
		IndexToTagStateMap.Add(GlobalIndex, InitialStateContainer);

		if (NewWidget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(
				NewWidget, FGameplayTagContainer(), InitialStateContainer
			);
		}
	}

	// If this data index is already in our "selected" set, update the widget
	if (SelectedDataIndices.Contains(DataIndex))
	{
		const FGameplayTag& SelectedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Selected;
		UpdateEntryInteractionTagState(GlobalIndex, SelectedState, /*bEnable=*/ true);
	}

	// Assign data to the widget
	if (Items.IsValidIndex(DataIndex) && NewWidget->Implements<UStrategyEntryBase>())
	{
		IStrategyEntryBase::Execute_BP_OnStrategyEntryItemAssigned(NewWidget, Items[DataIndex]);
	}

	return NewWidget;
}

void UBaseStrategyWidget::ReleaseEntryWidget(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!ensureAlwaysMsgf(StrategyCanvasPanel.IsValid(), TEXT("No StrategyCanvasPanel found!")))
	{
		return;
	}

	if (const TWeakObjectPtr<UUserWidget>* Ptr = IndexToWidgetMap.Find(GlobalIndex))
	{
		if (Ptr->IsValid())
		{
			if (UUserWidget* Widget = Ptr->Get())
			{
				// Transition it back to "Pooled" in IndexToTagStateMap
				if (IndexToTagStateMap.Contains(GlobalIndex))
				{
					FGameplayTagContainer OldState = IndexToTagStateMap[GlobalIndex];
					FGameplayTagContainer PooledState;
					PooledState.AddTag(StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Pooled);

					if (Widget->Implements<UStrategyEntryBase>())
					{
						IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(
							Widget, OldState, PooledState
						);
					}
				}
			}
		}

		StrategyCanvasPanel->ReleaseEntryWidget(GlobalIndex);
		IndexToWidgetMap.Remove(GlobalIndex);
		IndexToTagStateMap.Remove(GlobalIndex);
		IndexToPositionMap.Remove(GlobalIndex);
	}
}

void UBaseStrategyWidget::ReleaseUndesiredWidgets(const TSet<int32>& DesiredIndices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TArray<int32> CurrentIndices;
	IndexToWidgetMap.GenerateKeyArray(CurrentIndices);
	for (int32 OldIndex : CurrentIndices)
	{
		if (!DesiredIndices.Contains(OldIndex))
		{
			ReleaseEntryWidget(OldIndex);
		}
	}
}

void UBaseStrategyWidget::UpdateEntryWidget(const int32 InGlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	UE_LOG(LogStrategyUI, Verbose, TEXT("\nStarting UpdateEntryWidget for index %d"), InGlobalIndex);
	UUserWidget* Widget = AcquireEntryWidget(InGlobalIndex);
	if (Widget && Widget->Implements<UStrategyEntryBase>())
	{
		const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(InGlobalIndex);
		const UObject* Item = Items.IsValidIndex(DataIndex) ? Items[DataIndex] : nullptr;
		// (Optional) re‐assign if needed
		IStrategyEntryBase::Execute_BP_OnStrategyEntryItemAssigned(Widget, Item);
	}
}

void UBaseStrategyWidget::NotifyStrategyEntryStateChange(
	int32 GlobalIndex,
	UUserWidget* Widget,
	const FGameplayTagContainer& OldState,
	const FGameplayTagContainer& NewState
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (NewState != OldState)
	{
		IndexToTagStateMap[GlobalIndex] = NewState;

		if (Widget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(Widget, OldState, NewState);
		}
	}
}

void UBaseStrategyWidget::TryHandlePooledEntryStateTransition(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	const bool bShouldBeVisible = GetLayoutStrategyChecked().ShouldBeVisible(GlobalIndex);
	const FGameplayTag DesiredTag = bShouldBeVisible
		? StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Active
		: StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Deactivated;

	if (const FGameplayTagContainer* ExistingTagsPtr = IndexToTagStateMap.Find(GlobalIndex))
	{
		if (ExistingTagsPtr->HasTag(DesiredTag))
		{
			return; // Already in correct state
		}
	}

	UpdateEntryLifecycleTagState(GlobalIndex, DesiredTag);
}

void UBaseStrategyWidget::UpdateEntryLifecycleTagState(const int32 GlobalIndex, const FGameplayTag& NewStateTag)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	// Validate that NewStateTag is child of EntryLifecycle.*
	const FGameplayTag& EntryLifecycleParent = StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Parent;
	if (!NewStateTag.MatchesTag(EntryLifecycleParent))
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("Invalid EntryLifecycle tag: %s"), *NewStateTag.ToString());
		return;
	}

	FGameplayTagContainer& TagContainer = IndexToTagStateMap.FindOrAdd(GlobalIndex);
	if (TagContainer.HasTag(NewStateTag))
	{
		return; // No change
	}

	FGameplayTagContainer OldTags = TagContainer;

	// Remove other EntryLifecycle tags
	TArray<FGameplayTag> TagsToRemove;
	for (const FGameplayTag& ExistingTag : TagContainer)
	{
		if (ExistingTag.MatchesTag(EntryLifecycleParent))
		{
			TagsToRemove.Add(ExistingTag);
		}
	}
	for (const FGameplayTag& TagToRemove : TagsToRemove)
	{
		TagContainer.RemoveTag(TagToRemove);
	}

	// Add the new one
	TagContainer.AddTag(NewStateTag);

	if (TagContainer == OldTags)
	{
		return; // No net change
	}

	// Notify the widget
	if (UUserWidget* Widget = AcquireEntryWidget(GlobalIndex))
	{
		if (Widget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(Widget, OldTags, TagContainer);
		}
	}
}

void UBaseStrategyWidget::UpdateEntryInteractionTagState(const int32 GlobalIndex, const FGameplayTag& InteractionTag, bool bEnable)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	const FGameplayTag& EntryInteractionParent = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Parent;
	if (!InteractionTag.MatchesTag(EntryInteractionParent))
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("Invalid EntryInteraction tag: %s"), *InteractionTag.ToString());
		return;
	}

	FGameplayTagContainer& TagContainer = IndexToTagStateMap.FindOrAdd(GlobalIndex);
	FGameplayTagContainer OldTags = TagContainer;

	if (bEnable)
	{
		TagContainer.AddTag(InteractionTag);
	}
	else
	{
		TagContainer.RemoveTag(InteractionTag);
	}

	if (UUserWidget* Widget = AcquireEntryWidget(GlobalIndex))
	{
		if (Widget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(Widget, OldTags, TagContainer);

			// Optional: call focus/selection change callbacks
			const FGameplayTag& FocusedTag  = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Focused;
			const FGameplayTag& SelectedTag = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Selected;

			if (InteractionTag == FocusedTag)
			{
				IStrategyEntryBase::Execute_BP_OnItemFocusChanged(Widget, bEnable);
			}
			else if (InteractionTag == SelectedTag)
			{
				IStrategyEntryBase::Execute_BP_OnItemSelectionChanged(Widget, bEnable);
			}
		}
	}
}

void UBaseStrategyWidget::UpdateWidgets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (GetItemCount() == 0)
	{
		UE_LOG(LogStrategyUI, Verbose, TEXT("%hs called with no items to display!"), __FUNCTION__);
		return;
	}

	if (!StrategyCanvasPanel.IsValid())
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%hs: No StrategyCanvasPanel found!"), __FUNCTION__);
		return;
	}

	// Gather desired indices from the layout
	TSet<int32> DesiredIndices = GetLayoutStrategyChecked().ComputeDesiredGlobalIndices();

	FString DesiredIndicesStr;
	for (const int32 Index : DesiredIndices)
	{
		DesiredIndicesStr += FString::Printf(TEXT("%d, "), Index);
	}
	UE_LOG(LogStrategyUI, Verbose, TEXT("Desired global indices: %s"), *DesiredIndicesStr);

	// 1) Release old widgets not in DesiredIndices
	ReleaseUndesiredWidgets(DesiredIndices);

	// We'll fill these structures so we can do a single call to UpdateItemData
	TArray<int32> FinalIndices;
	FinalIndices.Reserve(DesiredIndices.Num());

	TMap<int32, bool> IndexToVisibilityMap;
	TMap<int32, float> IndexToDepthMap;

	// 5) For each desired GlobalIndex
	for (int32 GlobalIndex : DesiredIndices)
	{
		// (a) Possibly transition from "Pooled" to "Active" or "Deactivated"
		TryHandlePooledEntryStateTransition(GlobalIndex);

		// (b) Determine final position via layout
		PositionWidget(GlobalIndex); 

		// (c) Acquire / update the widget
		UpdateEntryWidget(GlobalIndex);

		// (d) Decide if it's visible and possibly compute a depth or z‐order
		const bool bIsVisible = GetLayoutStrategyChecked().ShouldBeVisible(GlobalIndex);
		const float DepthValue = 0.f; // or some distance from camera, etc.

		// (e) Fill out the arrays/maps for a single pass in the Slate panel
		FinalIndices.Add(GlobalIndex);
		IndexToVisibilityMap.Add(GlobalIndex, bIsVisible);
		IndexToDepthMap.Add(GlobalIndex, DepthValue);
	}

	// 6) Finally, pass all data to the panel in one call
	StrategyCanvasPanel->UpdateChildrenData(
		FinalIndices,
		IndexToPositionMap,      // TMap<int32, FVector2D>
		IndexToVisibilityMap,    // TMap<int32, bool>
		IndexToDepthMap          // TMap<int32, float>
	);
}

void UBaseStrategyWidget::PositionWidget(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	const FVector2D& ItemLocalPos = GetLayoutStrategyChecked().GetItemPosition(GlobalIndex) + Center;
	IndexToPositionMap.Add(GlobalIndex, ItemLocalPos);
}
#pragma endregion EntryWidgetsPoolAndHandling

#pragma region UBaseStrategyWidget - Internal Implementations
void UBaseStrategyWidget::SetItems_Internal_Implementation(const TArray<UObject*>& InItems)
{
	Items = InItems;
	if (GetItemCount() <= 0)
	{
		UE_LOG(LogStrategyUI, Log, TEXT("%hs called with no items to display!"), __FUNCTION__);
		return;
	}

	UE_LOG(
		LogStrategyUI,
		Verbose,
		TEXT("Initializing strategy %s as host for %d items"),
		*GetName(),
		GetItemCount()
	);

	GetLayoutStrategyChecked().InitializeStrategy(this);
	UpdateWidgets();
}
#pragma endregion

#pragma region UBaseStrategyWidget - Data Provider
void UBaseStrategyWidget::OnDataProviderUpdated()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	UE_LOG(LogStrategyUI, Verbose, TEXT("%hs: Data provider updated"), __FUNCTION__);

	RefreshFromProvider();
}

void UBaseStrategyWidget::RefreshFromProvider()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		TArray<UObject*> ProvidedItems = IStrategyDataProvider::Execute_GetDataItems(DataProvider);
		UE_LOG(LogStrategyUI, Verbose, TEXT("%hs: Received %d items from provider"), __FUNCTION__, ProvidedItems.Num());

		SetItems(ProvidedItems);
	}
	else
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("%hs: Data provider is not ready or valid!"), __FUNCTION__);
	}
}

void UBaseStrategyWidget::TryCreateDefaultDataProvider()
{
	if (DefaultDataProviderClass && !DataProvider)
	{
		UE_LOG(LogStrategyUI, Log, TEXT("%hs: Creating default data provider"), __FUNCTION__);
		SetDataProvider(NewObject<UObject>(this, DefaultDataProviderClass));
	}
	else
	{
		UE_LOG(
			LogStrategyUI,
			Verbose,
			TEXT("%hs: No DefaultDataProviderClass set or DataProvider already exists"),
			__FUNCTION__
		);
	}
}
#pragma endregion

#pragma region UBaseStrategyWidget - Debug
#if WITH_GAMEPLAY_DEBUGGER

void UBaseStrategyWidget::UpdateReflectedObjectsDebugCategory()
{
	if (FReflectedObjectsDebugCategory::ActiveInstance.IsValid())
	{
		FReflectedObjectsDebugCategory::ActiveInstance->ClearTargets();
		FReflectedObjectsDebugCategory::ActiveInstance->AddTargetObject(this);

		if (LayoutStrategy)
		{
			FReflectedObjectsDebugCategory::ActiveInstance->AddTargetObject(LayoutStrategy);
		}
		if (DataProvider)
		{
			FReflectedObjectsDebugCategory::ActiveInstance->AddTargetObject(DataProvider);
		}

		TArray<FString> Filters;
		Filters.Add(TEXT("StrategyUI|*"));
		FReflectedObjectsDebugCategory::ActiveInstance->SetCategoryFilters(Filters);
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
#pragma endregion