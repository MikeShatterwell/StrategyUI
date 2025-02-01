#include "Widgets/BaseStrategyWidget.h"
#include "Strategies/BaseLayoutStrategy.h"
#include <Components/CanvasPanel.h>
#include <Components/CanvasPanelSlot.h>
#include <Blueprint/WidgetTree.h>
#include <Editor/WidgetCompilerLog.h>

#include "StrategyDebugItem.h"
#include "Interfaces/IStrategyDataProvider.h"
#include "Interfaces/IStrategyEntryBase.h"
#include "Utils/LogStrategyUI.h"
#include "Utils/PropertyDebugPaintUtil.h"
#include "Utils/StrategyUIGameplayTags.h"

#define WITH_MVVM FModuleManager::Get().IsModuleLoaded("ModelViewViewModel")
#define IS_DATA_PROVIDER_READY_AND_VALID(DataProvider) IsValid(DataProvider) && DataProvider->Implements<UStrategyDataProvider>() && IStrategyDataProvider::Execute_IsProviderReady(DataProvider)

UBaseStrategyWidget::UBaseStrategyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	EntryWidgetPool(*this) // Initialize the pool with this as the owning widget
{
}

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

	if (!EntryWidgetClass)
	{
		CompileLog.Error(FText::FromString(TEXT("Please assign an EntryWidgetClass in the details panel!")));
	}
	else
	{
		if (!EntryWidgetClass->ImplementsInterface(UStrategyEntryBase::StaticClass()))
		{
			CompileLog.Error(FText::FromString(TEXT("EntryWidgetClass must implement IStrategyEntryBase interface!")));
		}
	}
	
	// Check if MVVM plugin is loaded
	if (DataProvider && DefaultDataProviderClass && WITH_MVVM)
	{
		CompileLog.Warning(FText::FromString(TEXT("You are using the MVVM plugin but have set a DataProvider in BaseStrategyWidget. Consider removing DataProvider and bind SetItems to a view model. Do not use both systems simultaneously.")));
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
		ConstructDataProviderObject();
		RefreshFromProvider();
		UpdateVisibleWidgets();
	}
}
#endif

void UBaseStrategyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LayoutStrategy)
	{
		const int32 MaxVisibleEntries = GetLayoutStrategyChecked().MaxVisibleEntries;
		
		IndexToTagStateMap.Reserve(MaxVisibleEntries);
		IndexToWidgetMap.Reserve(MaxVisibleEntries);
	}

	ConstructDataProviderObject();
}

void UBaseStrategyWidget::SetItems(const TArray<UObject*>& InItems)
{
	SetItems_Internal(InItems);
}

void UBaseStrategyWidget::Reset()
{
	// Unbind from any existing provider
	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		FOnDataProviderUpdated OnDataProviderUpdated = IStrategyDataProvider::Execute_GetOnDataProviderUpdated(DataProvider)->OnDataProviderUpdatedDelegate;
		if (OnDataProviderUpdated.IsAlreadyBound(this, &UBaseStrategyWidget::OnDataProviderUpdated))
		{
			OnDataProviderUpdated.RemoveDynamic(this, &UBaseStrategyWidget::OnDataProviderUpdated);
		}
	}
	
	EntryWidgetPool.ResetPool();
	IndexToWidgetMap.Empty();
	Items.Empty();

	if (CanvasPanel)
	{
		CanvasPanel->ClearChildren();
	}
}

void UBaseStrategyWidget::UpdateFocusedIndex(const int32 InNewGlobalFocusIndex)
{
	if (FocusedGlobalIndex == InNewGlobalFocusIndex)
	{
		return; // No change
	}

	const FGameplayTag& FocusedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Focused;
	
	//----------------------------------------------------------
	// 1) Unfocus *all* entries for the old focus data index
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
	// 2) Update the new focus index & broadcast
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
	// 3) Focus *all* entries for the new focus data index
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
		// Selected a gap between items
		return;
	}
	const bool bAlreadySelected = SelectedDataIndices.Contains(DataIndex);
	
	const FGameplayTag& SelectedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Selected;

	for (auto& Pair : IndexToWidgetMap)
	{
		const int32 MappedGlobalIndex = Pair.Key;
		const UUserWidget* Widget = Pair.Value.Get();
		if (!Widget)
		{
			continue;
		}

		const int32 MappedDataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(MappedGlobalIndex);
		if (MappedDataIndex == DataIndex)
		{
			// Apply or remove the “selected” tag
			UpdateEntryInteractionTagState(MappedGlobalIndex, SelectedState, bShouldBeSelected);
		}
	}
	
	if (bShouldBeSelected && !bAlreadySelected)
	{
		SelectedDataIndices.Add(DataIndex);
		UpdateEntryInteractionTagState(InGlobalIndex, SelectedState, /*bEnable=*/ true);

		// Broadcast new data selection
		UObject* Item = Items.IsValidIndex(DataIndex) ? Items[DataIndex] : nullptr;
		OnItemSelected.Broadcast(DataIndex, Item);
	}
	else if (!bShouldBeSelected && bAlreadySelected)
	{
		SelectedDataIndices.Remove(DataIndex);
		UpdateEntryInteractionTagState(InGlobalIndex, SelectedState, /*bEnable=*/ false);
	}
}

void UBaseStrategyWidget::ToggleFocusedIndex()
{
	const bool bNewSelected = !SelectedDataIndices.Contains(FocusedDataIndex);
	SetSelectedGlobalIndex(FocusedGlobalIndex, bNewSelected);
}

void UBaseStrategyWidget::NativeDestruct()
{
	Reset();

	Super::NativeDestruct();
}

TSharedRef<SWidget> UBaseStrategyWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		return Super::RebuildWidget();
	}

	// If there's no root widget or BindWidget, create a new CanvasPanel for it
	if (!WidgetTree->RootWidget && !CanvasPanel)
	{
		CanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = CanvasPanel;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
	
	return Super::RebuildWidget();
}

int32 UBaseStrategyWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
									   FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bPaintDebugInfo && LayoutStrategy)
	{
		const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
		FLayoutStrategyDebugPaintUtil::DrawLayoutStrategyDebugVisuals(OutDrawElements, AllottedGeometry, LayerId, LayoutStrategy, Center);
		MaxLayer++;
	}
	return MaxLayer;
}

void UBaseStrategyWidget::SetLayoutStrategy(UBaseLayoutStrategy* NewStrategy)
{
	UE_CLOG(!LayoutStrategy, LogStrategyUI, Error, TEXT("%hs called with nullptr layout strategy -- this risks an imminent crash! There must always be a valid layout strategy. "), __FUNCTION__);

	if (LayoutStrategy == NewStrategy)
	{
		return; // No change
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

		UpdateVisibleWidgets();
	}
}

void UBaseStrategyWidget::SetItems_Internal_Implementation(const TArray<UObject*>& InItems)
{
	Items = InItems;

	if (GetItemCount() <= 0)
	{
		UE_LOG(LogStrategyUI, Log, TEXT("%hs called with no items to display!"), __FUNCTION__);
		return;
	}

	GetLayoutStrategyChecked().InitializeStrategy(this);
	UpdateVisibleWidgets();
}

UUserWidget* UBaseStrategyWidget::AcquireEntryWidget(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	// If we already have a widget for this index, return it
	if (const TWeakObjectPtr<UUserWidget>* ExistingPtr = IndexToWidgetMap.Find(GlobalIndex))
	{
		if (ExistingPtr->IsValid())
		{
			UE_LOG(LogStrategyUI, Verbose, TEXT("Reusing widget %s for global index %d"), *ExistingPtr->Get()->GetName(), GlobalIndex);
			return ExistingPtr->Get();
		}
		// else fall through and create a new one
	}

	// Need a new widget from the pool
	if (!ensure(EntryWidgetClass))
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%hs: No EntryWidgetClass set!"), __FUNCTION__);
		return nullptr;
	}

	UUserWidget* NewWidget = EntryWidgetPool.GetOrCreateInstance(EntryWidgetClass);
	check(NewWidget);

	UE_LOG(LogStrategyUI, Verbose, TEXT("Creating new widget %s for global index %d"), *NewWidget->GetName(), GlobalIndex);
	IndexToWidgetMap.Add(GlobalIndex, NewWidget);
	
	// If the index isn’t in IndexStateMap yet, give it an initial state:
	if (!IndexToTagStateMap.Contains(GlobalIndex))
	{
		FGameplayTagContainer InitialStateContainer;
		InitialStateContainer.AddTag(StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Pooled);
		IndexToTagStateMap.Add(GlobalIndex, InitialStateContainer);

		if (NewWidget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(NewWidget, FGameplayTagContainer(), InitialStateContainer);
		}
	}

	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
	if (SelectedDataIndices.Contains(DataIndex))
	{
		const FGameplayTag& SelectedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Selected;
		constexpr bool bIsSelected = true;
		UpdateEntryInteractionTagState(GlobalIndex, SelectedState, bIsSelected);
	}

	// Assign the data to the widget
	if (Items.IsValidIndex(DataIndex))
	{
		const UObject* Item = Items[DataIndex];
		if (NewWidget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryItemAssigned(NewWidget, Item);
		}
	}

	return NewWidget;
}

void UBaseStrategyWidget::ReleaseEntryWidget(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (const TWeakObjectPtr<UUserWidget>* Ptr = IndexToWidgetMap.Find(GlobalIndex))
	{
		if (Ptr->IsValid())
		{
			if (UUserWidget* Widget = Ptr->Get())
			{
				// Return to pool
				EntryWidgetPool.Release(Widget);

				UE_LOG(LogStrategyUI, Verbose, TEXT("Released widget for global index %d"), GlobalIndex);

				// Clear old tags
				if (IndexToTagStateMap.Contains(GlobalIndex))
				{
					FGameplayTagContainer OldState = IndexToTagStateMap[GlobalIndex];
					FGameplayTagContainer PooledState;
					PooledState.AddTag(StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Pooled);

					// Tell the entry widget it’s becoming “pooled”
					if (Widget->Implements<UStrategyEntryBase>())
					{
						IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(
							Widget, OldState, PooledState
						);
					}
				}
			}
		}

		// Remove from the maps as we no longer track this index
		IndexToWidgetMap.Remove(GlobalIndex);
		IndexToTagStateMap.Remove(GlobalIndex);
	}
}

void UBaseStrategyWidget::ReleaseUndesiredWidgets(const TSet<int32>& DesiredIndices)
{
	TArray<int32> CurrentIndices;
	IndexToWidgetMap.GenerateKeyArray(CurrentIndices);
	for (const int32 OldIndex : CurrentIndices)
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

	UE_LOG(LogStrategyUI, Verbose, TEXT("\nStarting UpdateEntryWidget for index %d,"), InGlobalIndex);
	UUserWidget* Widget = AcquireEntryWidget(InGlobalIndex);
	
	if (Widget->Implements<UStrategyEntryBase>())
	{
		const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(InGlobalIndex);
		const UObject* Item = Items.IsValidIndex(DataIndex) ? Items[DataIndex] : nullptr;
		
		// @TODO: Only call this if the item index has changed for this entry
		IStrategyEntryBase::Execute_BP_OnStrategyEntryItemAssigned(Widget, Item);
	}
}

void UBaseStrategyWidget::NotifyStrategyEntryStateChange(const int32 GlobalIndex, UUserWidget* Widget, const FGameplayTagContainer& OldState, const FGameplayTagContainer& NewState)
{
	// Check for transitions and update the state if there was a change
	if (NewState != OldState)
	{
		IndexToTagStateMap[GlobalIndex] = NewState;

		// Tell the entry widget it changed states
		if (Widget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(Widget, OldState, NewState);
		}
	}
}

void UBaseStrategyWidget::TryHandlePooledEntryStateTransition(const int32 GlobalIndex)
{
	const bool bShouldBeVisible = GetLayoutStrategyChecked().ShouldBeVisible(GlobalIndex);
	
	FGameplayTag NewState;
	if (bShouldBeVisible)
	{
		NewState = StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Active;
	}
	else
	{
		NewState = StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Deactivated;
	}

	UpdateEntryLifecycleTagState(GlobalIndex, NewState);
}

void UBaseStrategyWidget::UpdateEntryLifecycleTagState(const int32 GlobalIndex, const FGameplayTag& NewStateTag)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	// Validate that the new tag is a child of "StrategyUI.EntryState"
	const FGameplayTag& EntryLifecycleParent = StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Parent;
	if (!NewStateTag.MatchesTag(EntryLifecycleParent))
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("Invalid EntryState tag: %s"), *NewStateTag.ToString());
		return;
	}

	// Get the container for this index
	FGameplayTagContainer& TagContainer = IndexToTagStateMap.FindOrAdd(GlobalIndex);
	FGameplayTagContainer OldTags = TagContainer;

	// Remove all existing EntryLifecycle tags since they're mutually exclusive
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

	// Add the new status tag
	TagContainer.AddTag(NewStateTag);

	// Notify the widget of the change
	if (UUserWidget* Widget = AcquireEntryWidget(GlobalIndex))
	{
		if (Widget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(Widget, OldTags, TagContainer);
		}
	}
}

void UBaseStrategyWidget::UpdateEntryInteractionTagState(const int32 GlobalIndex, const FGameplayTag& InteractionTag, const bool bEnable)
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

	// Notify the widget
	if (UUserWidget* Widget = AcquireEntryWidget(GlobalIndex))
	{
		if (Widget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(Widget, OldTags, TagContainer);

			const FGameplayTag& FocusedTag = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Focused;
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


void UBaseStrategyWidget::UpdateVisibleWidgets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	if (GetItemCount() == 0)
	{
		Reset(); // We have no data, reset to default state
		return;
	}

	if (!WidgetTree || IsDesignTime())
	{
		return;
	}

	// Gather all the indices we want to keep.
	TSet<int32> DesiredIndices = GetLayoutStrategyChecked().ComputeDesiredGlobalIndices();

	// Log the desired indices
	FString DesiredIndicesStr;
	for (const int32 Index : DesiredIndices)
	{
		DesiredIndicesStr += FString::Printf(TEXT("%d, "), Index);
	}
	UE_LOG(LogStrategyUI, Verbose, TEXT("Desired indices: %s"), *DesiredIndicesStr);

	// 1) Release any old widgets that are no longer needed (scrolled out of view)
	ReleaseUndesiredWidgets(DesiredIndices);
	
	// 2) Create or update each desired widget
	for (const int32 GlobalIndex : DesiredIndices)
	{
		TryHandlePooledEntryStateTransition(GlobalIndex);
		PositionWidget(GlobalIndex);
		UpdateEntryWidget(GlobalIndex);
	}
}

void UBaseStrategyWidget::PositionWidget(const int32 GlobalIndex)
{
	UUserWidget* Widget = AcquireEntryWidget(GlobalIndex);

	// Set common slot properties
	const FVector2D& EntrySize = GetLayoutStrategyChecked().ComputeEntryWidgetSize(GlobalIndex);
	const FVector2D& LocalPos = GetLayoutStrategyChecked().GetItemPosition(GlobalIndex);

	// Make sure it's actually on the Canvas and prepare to update the slot properties
	if (Widget->GetParent() != CanvasPanel)
	{
		CanvasPanel->AddChildToCanvas(Widget);
	}
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	check(CanvasSlot);

	CanvasSlot->SetAutoSize(false);
	CanvasSlot->SetSize(EntrySize);
	CanvasSlot->SetZOrder(0);
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetPosition(LocalPos);
}

void UBaseStrategyWidget::SetDataProvider(UObject* NewProvider)
{
	// Unbind from any existing provider
	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		IStrategyDataProvider::Execute_GetOnDataProviderUpdated(DataProvider)->OnDataProviderUpdatedDelegate.RemoveDynamic(this, &UBaseStrategyWidget::OnDataProviderUpdated);
	}

	DataProvider = NewProvider;

	if (DataProvider)
	{
		// Initialize the new provider if we have one
		IStrategyDataProvider::Execute_InitializeDataProvider(DataProvider);
	}

	// Listen for updates from the new provider
	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		IStrategyDataProvider::Execute_GetOnDataProviderUpdated(DataProvider)->OnDataProviderUpdatedDelegate.AddDynamic(this, &UBaseStrategyWidget::OnDataProviderUpdated);
	}
}

void UBaseStrategyWidget::OnDataProviderUpdated()
{
	// Called when the provider signals data changed
	RefreshFromProvider();
}

void UBaseStrategyWidget::RefreshFromProvider()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	if (IS_DATA_PROVIDER_READY_AND_VALID(DataProvider))
	{
		// Grab array of items from the provider
		const TArray<UObject*> ProvidedItems = IStrategyDataProvider::Execute_GetDataItems(DataProvider);
	
		// Feed them into the existing system
		SetItems(ProvidedItems);
	}
}

void UBaseStrategyWidget::ConstructDataProviderObject()
{
	if (DefaultDataProviderClass && !DataProvider)
	{
		SetDataProvider(NewObject<UObject>(this, DefaultDataProviderClass));
	}
}