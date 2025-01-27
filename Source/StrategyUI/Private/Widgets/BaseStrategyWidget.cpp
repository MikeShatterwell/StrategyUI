#include "Widgets/BaseStrategyWidget.h"
#include "Strategies/BaseLayoutStrategy.h"
#include <Components/CanvasPanel.h>
#include <Components/CanvasPanelSlot.h>
#include <Blueprint/WidgetTree.h>
#include <Editor/WidgetCompilerLog.h>

#include "DebugRadialItem.h"
#include "Interfaces/IStrategyEntryBase.h"
#include "Utils/LogStrategyUI.h"
#include "Utils/PropertyDebugPaintUtil.h"
#include "Utils/StrategyUIGameplayTags.h"

UBaseStrategyWidget::UBaseStrategyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	EntryWidgetPool(*this) // Initialize the pool with this as the owning widget
{
}

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
		const UClass* ItemEntryInterface = UStrategyEntryBase::StaticClass();
		if (!EntryWidgetClass->ImplementsInterface(ItemEntryInterface))
		{
			CompileLog.Error(FText::FromString(TEXT("EntryWidgetClass must implement IStrategyEntryBase interface!")));
		}
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
	}

	Reset();
	UpdateVisibleWidgets();
}

void UBaseStrategyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const int32 MaxVisibleEntries = GetLayoutStrategyChecked().MaxVisibleEntries;
	IndexToStateMap.Reserve(MaxVisibleEntries);
	IndexToWidgetMap.Reserve(MaxVisibleEntries);
}

void UBaseStrategyWidget::Reset()
{
	EntryWidgetPool.ResetPool();
	IndexToWidgetMap.Empty();
	Items.Empty();

	if (CanvasPanel)
	{
		CanvasPanel->ClearChildren();
	}
}

void UBaseStrategyWidget::UpdateFocusedGlobalIndex(const int32 InNewGlobalFocusIndex)
{
	if (FocusedGlobalIndex == InNewGlobalFocusIndex)
	{
		return; // No change
	}

	// Unfocus old
	if (FocusedGlobalIndex != INDEX_NONE)
	{
		if (UUserWidget* OldEntry = AcquireEntryWidget(FocusedGlobalIndex))
		{
			if (OldEntry->Implements<UStrategyEntryBase>())
			{
				IStrategyEntryBase::Execute_BP_OnItemFocusChanged(OldEntry, /*bIsFocused=*/ false);
			}
		}
	}

	// Update
	FocusedGlobalIndex = InNewGlobalFocusIndex;
	FocusedDataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(InNewGlobalFocusIndex);

	// Broadcast focus change
	if (FocusedDataIndex != INDEX_NONE && Items.IsValidIndex(FocusedDataIndex))
	{
		OnItemFocused.Broadcast(FocusedDataIndex, Items[FocusedDataIndex]);
	}
	else
	{
		OnItemFocused.Broadcast(INDEX_NONE, nullptr);
	}

	// Focus new
	if (FocusedGlobalIndex != INDEX_NONE)
	{
		if (UUserWidget* OldEntry = AcquireEntryWidget(FocusedGlobalIndex))
		{
			if (OldEntry->Implements<UStrategyEntryBase>())
			{
				IStrategyEntryBase::Execute_BP_OnItemFocusChanged(OldEntry, /*bIsFocused=*/ true);
			}
		}
	}
}

void UBaseStrategyWidget::SetSelectedDataIndex(const int32 InDataIndex, const bool bShouldBeSelected)
{
	const bool bAlreadySelected = SelectedDataIndices.Contains(InDataIndex);
	
	if (bShouldBeSelected && !bAlreadySelected)
	{
		SelectedDataIndices.Add(InDataIndex);

		// Broadcast new selection
		UObject* Item = Items.IsValidIndex(InDataIndex) ? Items[InDataIndex] : nullptr;
		OnItemSelected.Broadcast(InDataIndex, Item);
		
		if (UUserWidget* Entry = AcquireEntryWidget(InDataIndex))
		{
			if (Entry->Implements<UStrategyEntryBase>())
			{
				IStrategyEntryBase::Execute_BP_OnItemSelectionChanged(Entry, /*bIsSelected=*/ true);
			}
		}
	}
	else if (!bShouldBeSelected && bAlreadySelected)
	{
		SelectedDataIndices.Remove(InDataIndex);
		if (UUserWidget* Entry = AcquireEntryWidget(InDataIndex))
		{
			if (Entry->Implements<UStrategyEntryBase>())
			{
				IStrategyEntryBase::Execute_BP_OnItemSelectionChanged(Entry, /*bIsSelected=*/ false);
			}
		}
	}
}

void UBaseStrategyWidget::ToggleFocusedIndex()
{
	const bool bNewSelected = !SelectedDataIndices.Contains(FocusedDataIndex);
	SetSelectedDataIndex(FocusedDataIndex, bNewSelected);
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
	if (bEnableDebugDraw && LayoutStrategy)
	{
		const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
		FLayoutStrategyDebugPaintUtil::DrawLayoutStrategyDebugVisuals(OutDrawElements, AllottedGeometry, LayerId, LayoutStrategy, Center);
		MaxLayer++;
	}
	return MaxLayer;
}

void UBaseStrategyWidget::SetLayoutStrategy(UBaseLayoutStrategy* NewStrategy)
{
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
	}
	UpdateVisibleWidgets();
}

void UBaseStrategyWidget::SetItems(const TArray<UObject*>& InItems)
{
	Items = InItems;

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
	if (!IndexToStateMap.Contains(GlobalIndex))
	{
		const FGameplayTag& InitialEntryState = StrategyUIGameplayTags::StrategyUI_EntryState_Pooled;
		IndexToStateMap.Add(GlobalIndex, InitialEntryState);

		if (NewWidget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateChanged(NewWidget, FGameplayTag(), InitialEntryState);
		}
	}

	// Assign the data to the widget
	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
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

void UBaseStrategyWidget::ReleaseEntryWidget(const int32 Index)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (const TWeakObjectPtr<UUserWidget>* Ptr = IndexToWidgetMap.Find(Index))
	{
		if (Ptr->IsValid())
		{
			if (UUserWidget* Widget = Ptr->Get())
			{
				// Return to pool
				EntryWidgetPool.Release(Widget);

				UE_LOG(LogStrategyUI, Verbose, TEXT("Released widget for index %d"), Index);
				
				if (Widget->Implements<UStrategyEntryBase>() && IndexToStateMap.Contains(Index))
				{
					const FGameplayTag& OldState = IndexToStateMap[Index];
					const FGameplayTag& NewState = StrategyUIGameplayTags::StrategyUI_EntryState_Pooled;
					// Tell the widget it's being pooled
					IStrategyEntryBase::Execute_BP_OnStrategyEntryStateChanged(Widget, OldState, NewState);
				}
			}
		}

		// Remove from the maps as we no longer track this index
		IndexToWidgetMap.Remove(Index);
		IndexToStateMap.Remove(Index);
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
		const bool bIsFocused = DataIndex == FocusedDataIndex;
		
		// @TODO: Only call this if the item index has changed for this entry
		IStrategyEntryBase::Execute_BP_OnStrategyEntryItemAssigned(Widget, Item);
		IStrategyEntryBase::Execute_BP_OnItemFocusChanged(Widget, bIsFocused);
	}
}

void UBaseStrategyWidget::NotifyStrategyEntryStateChange(const int32 GlobalIndex, UUserWidget* Widget, const FGameplayTag& OldState, const FGameplayTag& NewState)
{
	// Check for transitions and update the state if there was a change
	if (NewState != OldState)
	{
		IndexToStateMap[GlobalIndex] = NewState;

		// Tell the entry widget it changed states
		if (Widget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateChanged(Widget, OldState, NewState);
		}
	}
}

void UBaseStrategyWidget::TryHandlePooledEntryStateTransition(const int32 GlobalIndex)
{
	UUserWidget* Widget = AcquireEntryWidget(GlobalIndex);
	const bool bShouldBeVisible = GetLayoutStrategyChecked().ShouldBeVisible(GlobalIndex);

	// Grab old & new states
	const FGameplayTag& OldState = IndexToStateMap.FindRef(GlobalIndex);
	ensureMsgf(OldState.IsValid(), TEXT("Invalid state for index %d, make sure we always have a valid state tag!"), GlobalIndex);
	
	const FGameplayTag& NewState = bShouldBeVisible ?	StrategyUIGameplayTags::StrategyUI_EntryState_Active : StrategyUIGameplayTags::StrategyUI_EntryState_Deactivated;

	NotifyStrategyEntryStateChange(GlobalIndex, Widget, OldState, NewState);
}

void UBaseStrategyWidget::UpdateVisibleWidgets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	if (GetItemCount() == 0)
	{
#if WITH_EDITOR
		if (DebugItemCount <= 0)
		{
			return;
		}

		if (IsDesignTime())
		{
			return;
		}
		
		// Create debug data for easy layout testing in the editor
		TArray<UObject*> DebugItems;
		for (int32 i = 0; i < DebugItemCount ; ++i)
		{
			UDebugRadialItem* DebugItem = NewObject<UDebugRadialItem>(this);
			DebugItem->DebugLabel = FString::Printf(TEXT("Item %d"), i);
			DebugItem->Id = i;
			DebugItems.Add(DebugItem);
		}

		// Use the newly created debug data and update again
		SetItems(DebugItems);
#endif
		return;
	}

	if (!WidgetTree)
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
