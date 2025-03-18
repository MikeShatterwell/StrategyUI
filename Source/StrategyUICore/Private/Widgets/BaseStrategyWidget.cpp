// Copyright Mike Desrosiers 2025, All Rights Reserved.

#include "Widgets/BaseStrategyWidget.h"

#include <Blueprint/WidgetTree.h>
#include <Editor/WidgetCompilerLog.h>
#include <Modules/ModuleManager.h>

#include <AsyncWidgetLoaderSubsystem.h>

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
		SynchronizeProperties();
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
	
	// Cancel any pending widget loads
	if (AsyncWidgetLoader)
	{
		for (const auto& Pair : GlobalIndexToRequestId)
		{
			if (Pair.Value != INDEX_NONE)
			{
				AsyncWidgetLoader->CancelRequest(Pair.Value);
			}
		}

		AsyncWidgetLoader->ResetWidgetPools();
	}
	GlobalIndexToRequestId.Empty();

	SelectedDataIndices.Empty();
	FocusedGlobalIndex = 0;
	FocusedDataIndex   = INDEX_NONE;
	
	GlobalIndexToSlotData.Empty();
	
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
		for (TTuple<int32, FStrategyEntrySlotData>& SlotData : GlobalIndexToSlotData)
		{
			const int32 MappedGlobalIndex = SlotData.Key;
			const UUserWidget* Widget = SlotData.Value.Widget.Get();
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
		for (TTuple<int32, FStrategyEntrySlotData>& SlotData : GlobalIndexToSlotData)
		{
			const int32 MappedGlobalIndex = SlotData.Key;
			UUserWidget* Widget = SlotData.Value.Widget.Get();
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
	for (TTuple<int32, FStrategyEntrySlotData>& SlotData : GlobalIndexToSlotData)
	{
		const int32 MappedGlobalIndex = SlotData.Key;
		const UUserWidget* Widget     = SlotData.Value.Widget.Get();
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
		const int32 InitialCapacity = MaxVisibleEntries + GetLayoutStrategyChecked().NumDeactivatedEntries;
		GlobalIndexToSlotData.Reserve(InitialCapacity);
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		AsyncWidgetLoader = GameInstance->GetSubsystem<UAsyncWidgetLoaderSubsystem>();

		if (AsyncWidgetLoader)
		{
			AsyncWidgetLoader->SetWidgetCreationContext(GetWorld(), GetOwningPlayer());
		}
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
	StrategyCanvasPanel = SNew(SStrategyCanvasPanel);
	return StrategyCanvasPanel.ToSharedRef();
}

void UBaseStrategyWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	StrategyCanvasPanel.Reset();
}

void UBaseStrategyWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (StrategyCanvasPanel)
	{
		StrategyCanvasPanel->SetDebugPaint(bPaintEntryWidgetBorders);
	}
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

void UBaseStrategyWidget::OnAsyncWidgetRequested_Implementation(const int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	// Print to screen and log
	UE_LOG(LogStrategyUI, Verbose, TEXT("%s: Requesting widget %s (RequestId: %d"), *GetName(), *WidgetClass.ToString(), RequestId);
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, 
		FString::Printf(TEXT("%s: Requesting widget %s (RequestId: %d)"), *GetName(), *WidgetClass.ToString(), RequestId)
	);
}

void UBaseStrategyWidget::OnAsyncWidgetLoaded_Implementation(const int32 RequestId, UUserWidget* LoadedWidget)
{
	if (!LoadedWidget)
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%hs: Received null widget (Request Id %d)"), __FUNCTION__, RequestId);
		return;
	}

	// Print to screen and log
	UE_LOG(LogStrategyUI, Verbose, TEXT("%s: Loaded widget %s"), *GetName(), *LoadedWidget->GetName());
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
		FString::Printf(TEXT("%s: Loaded widget %s"), *GetName(), *LoadedWidget->GetName())
	);

	// Find which global index this request was for
	int32 GlobalIndex = INT32_MAX;
	for (const auto& Pair : GlobalIndexToRequestId)
	{
		if (Pair.Value == RequestId)
		{
			GlobalIndex = Pair.Key;
			break;
		}
	}
	
	if (GlobalIndex == INT32_MAX)
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("Received loaded widget for unknown request %d"), RequestId);
		return;
	}

	ReplacePlaceholderWithActualWidget(GlobalIndex, LoadedWidget);
}

void UBaseStrategyWidget::OnAsyncWidgetLoadFailed_Implementation(int32 RequestId,
	const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	// Print to screen and log
	UE_LOG(LogStrategyUI, Error, TEXT("%s: Failed to load widget %s"), *GetName(), *WidgetClass.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
		FString::Printf(TEXT("%s: Failed to load widget %s"), *GetName(), *WidgetClass.ToString())
	);

	// Find which global index this request was for
	int32 GlobalIndex = INT_MAX;
	for (const auto& Pair : GlobalIndexToRequestId)
	{
		if (Pair.Value == RequestId)
		{
			GlobalIndex = Pair.Key;
			break;
		}
	}

	if (GlobalIndex == INT_MAX)
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("Received failure for unknown request %d"), RequestId);
		return;
	}

	GlobalIndexToRequestId.Remove(GlobalIndex);
}

void UBaseStrategyWidget::OnAsyncWidgetLoadCancelled_Implementation(int32 RequestId,
	const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	// Print to screen and log
	UE_LOG(LogStrategyUI, Error, TEXT("%s: Load cancelled for widget %s"), *GetName(), *WidgetClass.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
		FString::Printf(TEXT("%s: Load cancelled for widget %s"), *GetName(), *WidgetClass.ToString())
	);

	// Find which global index this request was for
	int32 GlobalIndex = INT_MAX;
	for (const auto& Pair : GlobalIndexToRequestId)
	{
		if (Pair.Value == RequestId)
		{
			GlobalIndex = Pair.Key;
			break;
		}
	}

	if (GlobalIndex == INT_MAX)
	{
		return;
	}

	GlobalIndexToRequestId.Remove(GlobalIndex);
}

void UBaseStrategyWidget::ReplacePlaceholderWithActualWidget(const int32 GlobalIndex, UUserWidget* ActualWidget)
{
	FStrategyEntrySlotData* SlotData = GlobalIndexToSlotData.Find(GlobalIndex);
	if (!SlotData)
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("%s: Slot data not found for global index %d -- likely was removed while waiting for async load"), *GetName(), GlobalIndex);
		return;
	}

	if (!SlotData->bIsPlaceholder)
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("%s: Slot data for global index %d is not a placeholder"), *GetName(), GlobalIndex);
		return;
	}
	
	// Store old placeholder for proper cleanup
	UUserWidget* OldPlaceholder = nullptr;
	if (SlotData->Widget.IsValid())
	{
		OldPlaceholder = SlotData->Widget.Get();
	}
	
	// Update the slot data with the actual widget
	FGameplayTagContainer OldTags = SlotData->TagState;
	SlotData->Widget = ActualWidget;
	SlotData->CachedSlateWidget = ActualWidget->TakeWidget();
	SlotData->bIsPlaceholder = false;
	
	// Setup widget state and data
	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
	const UObject* Item = Items.IsValidIndex(DataIndex) ? Items[DataIndex] : nullptr;
	
	// Assign data to the widget
	if (ActualWidget->Implements<UStrategyEntryBase>() && Item)
	{
		IStrategyEntryBase::Execute_BP_OnStrategyEntryItemAssigned(ActualWidget, Item);
	}
	
	// Notify the actual entry widget of its state
	if (ActualWidget->Implements<UStrategyEntryBase>())
	{
		IStrategyEntryBase::Execute_BP_OnStrategyEntryStateTagsChanged(ActualWidget, FGameplayTagContainer(), SlotData->TagState);
	}
	
	// Apply selected state if needed
	if (SelectedDataIndices.Contains(DataIndex))
	{
		const FGameplayTag& SelectedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Selected;
		UpdateEntryInteractionTagState(GlobalIndex, SelectedState, /*bEnable=*/ true);
	}
	
	// Apply focused state if needed
	if (DataIndex == FocusedDataIndex)
	{
		const FGameplayTag& FocusedState = StrategyUIGameplayTags::StrategyUI::EntryInteraction::Focused;
		UpdateEntryInteractionTagState(GlobalIndex, FocusedState, /*bEnable=*/ true);
	}
	
	// Clean up the old placeholder
	if (OldPlaceholder)
	{
		OldPlaceholder = nullptr;
	}

	// Remove the request ID from tracking
	GlobalIndexToRequestId.Remove(GlobalIndex);
	
	// Rebuild the slate layout with the new widget
	RebuildSlateForIndices(
		GetLayoutStrategyChecked().ComputeDesiredGlobalIndices(),
		/*bForceUpdateWidget=*/false
	);
}

TSoftClassPtr<UUserWidget> UBaseStrategyWidget::ResolveEntryWidgetClass(const int32 GlobalIndex)
{
	const UObject* DataItem = nullptr;
	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
	if (Items.IsValidIndex(DataIndex))
	{
		DataItem = Items[DataIndex];
	}

	TSoftClassPtr<UUserWidget> DesiredClass = nullptr;
	
	if (DataItem && DataItem->Implements<UStrategyEntryWidgetProvider>())
	{
		// A) Attempt to get direct hard class
		DesiredClass = IStrategyEntryWidgetProvider::Execute_GetEntryWidgetClass(DataItem);

		// B) If that fails, attempt to get a soft class
		if (!DesiredClass)
		{
			DesiredClass = IStrategyEntryWidgetProvider::Execute_GetEntryWidgetSoftClass(DataItem);
		}

		// C) If still none, attempt to get hard class from a widget tag
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

		// D) If still none, attempt to get soft class from the widget tag
		if (!DesiredClass)
		{
			const FGameplayTag ItemTag = IStrategyEntryWidgetProvider::Execute_GetEntryWidgetTag(DataItem);
			if (ItemTag.IsValid() && ItemTag != FGameplayTag::EmptyTag)
			{
				if (const TSoftClassPtr<UUserWidget> FoundClass = UStrategyUIFunctionLibrary::GetWidgetSoftClassForTag(ItemTag))
				{
					DesiredClass = FoundClass;
				}
			}
		}
	}

	// E) If none found, fallback to default
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

	if (!ensureAlwaysMsgf(DesiredClass.IsValid(), TEXT("DesiredClass must be valid!")))
	{
		return nullptr;
	}

	return DesiredClass;
}

UUserWidget* UBaseStrategyWidget::AcquireEntryWidget(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!ensureAlwaysMsgf(StrategyCanvasPanel.IsValid(), TEXT("No StrategyCanvasPanel found!")))
	{
		return nullptr;
	}

	if (!ensureAlwaysMsgf(AsyncWidgetLoader, TEXT("No AsyncWidgetLoader found!")))
	{
		return nullptr;
	}

	// (1) If a widget already exists for this index, reuse it
	if (const FStrategyEntrySlotData* ExistingData = GlobalIndexToSlotData.Find(GlobalIndex))
	{
		if (ExistingData->IsValid())
		{
			UE_LOG(
				LogStrategyUI, 
				Verbose, 
				TEXT("%hs: Reusing widget %s for global index %d"),
				__FUNCTION__,
				*ExistingData->Widget->GetName(),
				GlobalIndex
			);
			return ExistingData->Widget.Get();
		}
		
		// Check if we have a pending request
		if (GlobalIndexToRequestId.Contains(GlobalIndex))
		{
			// We have a pending request - return the placeholder if it exists
			return ExistingData->Widget.IsValid() ? ExistingData->Widget.Get() : nullptr;
		}
	}

	// (2) Determine the data item for this index
	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
	UObject* DataItem = Items.IsValidIndex(DataIndex) ? Items[DataIndex] : nullptr;

	// (3) Decide which widget class to use (could come from data item, or fallback)
	const TSoftClassPtr<UUserWidget> DesiredClass = ResolveEntryWidgetClass(GlobalIndex);

	// Prepare slot data, even before we have the actual widget
	FStrategyEntrySlotData& SlotData = GlobalIndexToSlotData.FindOrAdd(GlobalIndex);
	SlotData.TagState.AddTag(StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Pooled);
	SlotData.LastAssignedItem = DataItem;
	
	int32 RequestId = INDEX_NONE;
	if (UUserWidget* Widget = AsyncWidgetLoader->RequestWidget_Async(
		DesiredClass,
		this,
		RequestId,
		FOnAsyncWidgetLoadedDynamic(), // No callback since we're implementing the IAsyncWidgetRequestHandler interface and will handle it in OnAsyncWidgetLoaded_Implementation
		1.0f  // Default priority
	); Widget)
	{
		SlotData.Widget = Widget;
		SlotData.CachedSlateWidget = SlotData.Widget->TakeWidget();
		return SlotData.Widget.Get(); // We have a valid widget already
	}

	if (RequestId == INDEX_NONE)
	{
		UE_LOG(LogStrategyUI, Error, TEXT("Failed to request widget for global index %d"), GlobalIndex);
		return nullptr;
	}
	GlobalIndexToRequestId.Add(GlobalIndex, RequestId);

	// (4) If we don't have a proper widget yet, create a placeholder

	// Create a placeholder widget if enabled
	if (bShowLoadingPlaceholders)
	{
		TSubclassOf<UUserWidget> PlaceholderClass = DefaultLoadingPlaceholderClass;
		if (!PlaceholderClass)
		{
			PlaceholderClass = UDefaultPlaceholderWidget::StaticClass(); // Fallback to default placeholder
		}

		if (UUserWidget* PlaceholderWidget = AsyncWidgetLoader->GetOrCreatePooledWidget(PlaceholderClass))
		{
			SlotData.Widget = PlaceholderWidget;
			SlotData.CachedSlateWidget = PlaceholderWidget->TakeWidget();
			SlotData.bIsPlaceholder = true; 

			UpdateEntryLifecycleTagState(GlobalIndex, StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Loading);
		}
	}

	return SlotData.Widget.Get(); // Return the placeholder or nullptr
}

void UBaseStrategyWidget::ReleaseEntryWidget(const int32 GlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!ensureAlwaysMsgf(StrategyCanvasPanel.IsValid(), TEXT("No StrategyCanvasPanel found!")))
	{
		return;
	}

	// Check for pending requests
	if (const int32* RequestId = GlobalIndexToRequestId.Find(GlobalIndex))
	{
		if (*RequestId != INDEX_NONE && AsyncWidgetLoader)
		{
			// Cancel the pending request
			AsyncWidgetLoader->CancelRequest(*RequestId);
			GlobalIndexToRequestId.Remove(GlobalIndex);
		}
	}

	if (FStrategyEntrySlotData* SlotData = GlobalIndexToSlotData.Find(GlobalIndex))
	{
		if (SlotData->IsValid())
		{
			if (UUserWidget* Widget = SlotData->Widget.Get())
			{
				if (SlotData->bIsPlaceholder)
				{
					// Just remove the placeholder, don't return it to pool
					SlotData->Widget.Reset();
					SlotData->CachedSlateWidget.Reset();
				}
				else if (AsyncWidgetLoader)
				{
					AsyncWidgetLoader->ReleaseWidgetToPool(Widget);
				}
				
				// Transition it back to "Pooled" in IndexToTagStateMap
				if (GlobalIndexToSlotData.Contains(GlobalIndex))
				{
					const FGameplayTagContainer OldState = SlotData->TagState;
					FGameplayTagContainer PooledState;
					PooledState.AddTag(StrategyUIGameplayTags::StrategyUI::EntryLifecycle::Pooled);

					NotifyStrategyEntryStateChange(GlobalIndex, Widget, OldState, PooledState);
				}
			}
		}

		UE_LOG(LogStrategyUI, Verbose, TEXT("%hs: Removing slot data for global index %d"), __FUNCTION__, GlobalIndex);
		SlotData->Reset();
		GlobalIndexToSlotData.Remove(GlobalIndex);
	}
}

void UBaseStrategyWidget::ReleaseUndesiredWidgets(const TSet<int32>& DesiredIndices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TArray<int32> CurrentIndices;
	GlobalIndexToSlotData.GenerateKeyArray(CurrentIndices);
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
	const int32 GlobalIndex,
	UUserWidget* Widget,
	const FGameplayTagContainer& OldState,
	const FGameplayTagContainer& NewState
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (NewState != OldState)
	{
		FStrategyEntrySlotData& SlotData = GlobalIndexToSlotData.FindChecked(GlobalIndex);
		SlotData.TagState = NewState;

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

	const FStrategyEntrySlotData* SlotData = GlobalIndexToSlotData.Find(GlobalIndex);
	if (!SlotData || !SlotData->IsValid())
	{
		AcquireEntryWidget(GlobalIndex);
		SlotData = GlobalIndexToSlotData.Find(GlobalIndex);
	}

	if (!SlotData)
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("Failed to acquire slot data for index %d"), GlobalIndex);
		return;
	}

	if (SlotData->TagState.HasTag(DesiredTag))
	{
		return; // Already in correct state
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

	FGameplayTagContainer& TagContainer = GlobalIndexToSlotData.FindOrAdd(GlobalIndex).TagState;
	if (TagContainer.HasTag(NewStateTag))
	{
		return; // No change
	}

	const FGameplayTagContainer OldTags = TagContainer;

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
		NotifyStrategyEntryStateChange(GlobalIndex, Widget, OldTags, TagContainer);
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

	FGameplayTagContainer& TagContainer = GlobalIndexToSlotData.FindOrAdd(GlobalIndex).TagState;
	const FGameplayTagContainer OldTags = TagContainer;

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
		NotifyStrategyEntryStateChange(GlobalIndex, Widget, OldTags, TagContainer);

		if (Widget->Implements<UStrategyEntryBase>())
		{
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
		UE_LOG(LogStrategyUI, Warning, TEXT("%hs called with no items to display!"), __FUNCTION__);
		return;
	}

	if (!StrategyCanvasPanel.IsValid())
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%hs: No StrategyCanvasPanel found!"), __FUNCTION__);
		return;
	}

	if (!LayoutStrategy)
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%hs: No LayoutStrategy found!"), __FUNCTION__);
		return;
	}

	// Gather desired indices from the layout
	TSet<int32> NewDesiredIndices = GetLayoutStrategyChecked().ComputeDesiredGlobalIndices();
	
	// (1) Release old widgets not in the new set
	ReleaseUndesiredWidgets(NewDesiredIndices);

	// (2) Possibly handle transitions out of "pooled" (like TryHandlePooledEntryStateTransition)
	for (const int32 Idx : NewDesiredIndices)
	{
		TryHandlePooledEntryStateTransition(Idx);
		UpdateEntryWidget(Idx);
	}

	// (3) Rebuild the panel, forcing each widget to re-acquire or update
	RebuildSlateForIndices(NewDesiredIndices, /*bForceUpdateWidget=*/true);
}

bool UBaseStrategyWidget::HasNewDesiredIndices(const TSet<int32>& NewIndices) const
{
	const bool bAreSetsIdentical = (LastDesiredIndices.Num() == NewIndices.Num()
	   && LastDesiredIndices.Includes(NewIndices)
	   && NewIndices.Includes(LastDesiredIndices)
	   && LastDesiredIndices.Num() > 0
	   && NewIndices.Num() > 0);
	return !bAreSetsIdentical;
}

void UBaseStrategyWidget::RebuildSlateForIndices(const TSet<int32>& InIndices, const bool bForceUpdateWidget)
{
	if (!StrategyCanvasPanel.IsValid())
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%hs: No StrategyCanvasPanel found!"), __FUNCTION__);
		return;
	}

	// Create a temporary map to hold minimal slot data for the Slate panel.
	// (This is our “minimal” data that our improved SStrategyCanvasPanel expects.)
	TMap<int32, FStrategyCanvasSlotData_Minimal> MinimalSlotDataMap;
	MinimalSlotDataMap.Reserve(InIndices.Num());

	// Iterate once over all global indices that should be shown.
	for (int32 GlobalIndex : InIndices)
	{
		// Optionally force an update/re-acquisition of the entry widget.
		if (bForceUpdateWidget)
		{
			UpdateEntryWidget(GlobalIndex);
		}

		// Compute the final position for this entry
		const FVector2D ItemLocalPos = GetLayoutStrategyChecked().GetItemPosition(GlobalIndex);
		// For now, we use a fixed depth. @TODO: Implement depth handling in the layout strategy.
		constexpr float DepthValue = 0.f;

		// Find the slot data for this global index.
		FStrategyEntrySlotData* SlotData = GlobalIndexToSlotData.Find(GlobalIndex);
		if (!SlotData)
		{
			// If none exists, try to acquire the widget and then retrieve the slot data.
			AcquireEntryWidget(GlobalIndex);
			SlotData = GlobalIndexToSlotData.Find(GlobalIndex);
			if (!SlotData)
			{
				UE_LOG(LogStrategyUI, Warning, TEXT("Could not create slot for index %d"), GlobalIndex);
				continue;
			}
		}

		// Update the slot data with the computed position and depth.
		UE_LOG(
			LogStrategyUI,
			VeryVerbose,
			TEXT("%hs: Updating slot data for global index %d at position %s"),
			__FUNCTION__,
			GlobalIndex,
			*ItemLocalPos.ToString()
		);
		SlotData->Position = ItemLocalPos;
		SlotData->Depth = DepthValue;

		// Now convert our full slot data into minimal data required by the Slate panel.
		if (SlotData->IsValid())
		{
			const TSharedPtr<SWidget> UnderlyingSlateWidget = SlotData->CachedSlateWidget;
			if (!UnderlyingSlateWidget.IsValid())
			{
				UE_LOG(LogStrategyUI, Warning, TEXT("No cached Slate widget for global index %d"), GlobalIndex);
				continue;
			}

			FStrategyCanvasSlotData_Minimal MinimalData;
			MinimalData.Position = SlotData->Position;
			MinimalData.Depth = SlotData->Depth;
			MinimalData.Widget = UnderlyingSlateWidget.ToSharedRef();
			UE_LOG(
				LogStrategyUI,
				VeryVerbose,
				TEXT("%hs: Adding minimal data for global index %d at position %s"),
				__FUNCTION__,
				GlobalIndex,
				*ItemLocalPos.ToString()
			);
			MinimalSlotDataMap.Add(GlobalIndex, MinimalData);
		}
	}

	// Do one single update call to the Slate panel, passing in the minimal data map.
	StrategyCanvasPanel->UpdateChildrenData(MinimalSlotDataMap);
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
		TEXT("%hs: Initializing %s as strategy host for %d items"),
		__FUNCTION__,
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
		const TArray<UObject*> ProvidedItems = IStrategyDataProvider::Execute_GetDataItems(DataProvider);
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