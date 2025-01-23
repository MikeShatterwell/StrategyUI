#include "Widgets/BaseStrategyWidget.h"
#include "Strategies/BaseLayoutStrategy.h"
#include <Components/CanvasPanel.h>
#include <Components/CanvasPanelSlot.h>
#include <Blueprint/WidgetTree.h>
#include <Editor/WidgetCompilerLog.h>

#include "Interfaces/IStrategyEntryBase.h"
#include "Utils/LogStrategyUI.h"
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
}

void UBaseStrategyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!ensure(WidgetTree))
	{
		return;
	}

	// If there's no root widget or BindWidget, create a new CanvasPanel for it
	if (!WidgetTree->RootWidget && !CanvasPanel)
	{
		CanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = CanvasPanel;
	}
}

void UBaseStrategyWidget::NativeDestruct()
{
	// Clear pool
	EntryWidgetPool.ReleaseAll();
	IndexToWidgetMap.Empty();
	Items.Empty();

	if (CanvasPanel)
	{
		CanvasPanel->ClearChildren();
	}

	Super::NativeDestruct();
}

int32 UBaseStrategyWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bEnableDebugDraw && LayoutStrategy)
	{
		GetLayoutStrategyChecked().DrawDebugVisuals(AllottedGeometry, OutDrawElements, LayerId, AllottedGeometry.GetAbsolutePosition());
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
	UpdateLayout();
}

void UBaseStrategyWidget::SetItems(const TArray<UObject*>& InItems)
{
	Items = InItems;
	UpdateLayout();
}

void UBaseStrategyWidget::UpdateLayout()
{
	if (!CanvasPanel)
	{
		return;
	}

	// 1) Use the strategy to compute layout info (positions, visible indices, etc.)

	TArray<int32> IndicesToShow;
	GetLayoutStrategyChecked().ComputeLayout(Items.Num(), IndicesToShow);

	// 2) Release any widgets not in IndicesToShow
	TArray<int32> CurrentKeys;
	IndexToWidgetMap.GetKeys(CurrentKeys);
	for (int32 OldIndex : CurrentKeys)
	{
		if (!IndicesToShow.Contains(OldIndex))
		{
			ReleaseEntryWidget(OldIndex);
		}
	}

	// 3) Acquire or update widgets for each index
	for (const int32 Index : IndicesToShow)
	{
		UUserWidget* EntryWidget = AcquireEntryWidget(Index);
		UpdateEntryWidget(EntryWidget, Index);

		// Set position, z‐order, etc. from the strategy’s computed transform
		// Example:
		const FVector2D Position = GetLayoutStrategyChecked().GetItemPosition(Index);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EntryWidget->Slot))
		{
			// Center pivot usage, etc. is your choice
			CanvasSlot->SetPosition(Position);
			// Optionally keep the widget centered and change the render transform if cursor hit testing is not needed
		}
	}
}

UUserWidget* UBaseStrategyWidget::AcquireEntryWidget(const int32 Index)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	// If we already have a widget for this index, return it
	if (const TWeakObjectPtr<UUserWidget>* ExistingPtr = IndexToWidgetMap.Find(Index))
	{
		if (ExistingPtr->IsValid())
		{
			UE_LOG(LogStrategyUI, Verbose, TEXT("Reusing widget %s for index %d"), *ExistingPtr->Get()->GetName(), Index);
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

	IndexToWidgetMap.Add(Index, NewWidget);

	// If the index isn’t in IndexStateMap yet, give it an initial state:
	if (!IndexToStateMap.Contains(Index))
	{
		const FGameplayTag& InitialEntryState = StrategyUIGameplayTags::StrategyUI_EntryState_Pooled;
		IndexToStateMap.Add(Index, InitialEntryState);

		if (NewWidget->Implements<UStrategyEntryBase>())
		{
			IStrategyEntryBase::Execute_BP_OnStrategyEntryStateChanged(NewWidget, FGameplayTag::EmptyTag, InitialEntryState);
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

void UBaseStrategyWidget::UpdateEntryWidget(UUserWidget* EntryWidget, const int32 Index)
{
	if (EntryWidget->Implements<UStrategyEntryBase>())
	{
		IStrategyEntryBase::Execute_BP_OnStrategyEntryItemAssigned(EntryWidget, Items[Index]);
	}
}
