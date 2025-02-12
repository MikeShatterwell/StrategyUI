#include "Widgets/SStrategyCanvasPanel.h"

#include "Utils/LogStrategyUI.h"
#include "Widgets/SNullWidget.h"

SStrategyCanvasPanel::SStrategyCanvasPanel()
	: SPanel(), Children(this), CombinedChildren(this)
{
	// Initialize our combined children with our panel's children.
	CombinedChildren.AddChildren(Children);
}

SStrategyCanvasPanel::~SStrategyCanvasPanel()
{
}

void SStrategyCanvasPanel::Construct(const FArguments& InArgs)
{
	// No child widgets are created at construction time.
}

void SStrategyCanvasPanel::UpdateChildrenData(const TMap<int32, FStrategyCanvasSlotData_Minimal>& InSlotData)
{
	// --- Step 1: Identify slots to remove and gather index updates ---
	TArray<int32> SlotsToRemove;
	TArray<TTuple<int32, int32>> GlobalIndices; // Stores {GlobalIndex, SlotIndex} pairs -- SlotIndex refers to the TPanelChildren index
	
	// First pass: Identify removals and build current index mapping
	for (const auto& Pair : GlobalIndexToSlot)
	{
		if (!InSlotData.Contains(Pair.Key))
		{
			SlotsToRemove.Add(Pair.Value);
		}
		else
		{
			GlobalIndices.Add(MakeTuple(Pair.Key, Pair.Value));
		}
	}

	// Sort slots to remove in descending order to maintain validity
	SlotsToRemove.Sort([](const int32& A, const int32& B) { return A > B; });

	// --- Step 2: Remove slots from highest index to lowest ---
	for (const int32 SlotIndex : SlotsToRemove)
	{
		if (Children.IsValidIndex(SlotIndex))
		{
			Children.RemoveAt(SlotIndex);
			UE_LOG(LogStrategyUI, Verbose, TEXT("Removed child at slot index %d"), SlotIndex);
		}
	}

	// --- Step 3: Rebuild GlobalIndexToSlot mapping ---
	GlobalIndexToSlot.Empty();
	
	// Sort remaining indices by slot index to maintain order
	GlobalIndices.Sort([](const TTuple<int32, int32>& A, const TTuple<int32, int32>& B) {
		return A.Get<1>() < B.Get<1>();
	});

	// Rebuild mapping with updated indices
	int32 NewSlotIndex = 0;
	for (const auto& Tuple : GlobalIndices)
	{
		const int32 GlobalIndex = Tuple.Get<0>();
		GlobalIndexToSlot.Add(GlobalIndex, NewSlotIndex++);
	}

	// --- Step 4: Update or add new slots ---
	for (const auto& Pair : InSlotData)
	{
		const int32 GlobalIndex = Pair.Key;
		const FStrategyCanvasSlotData_Minimal& NewData = Pair.Value;

		if (int32* ExistingSlotIndex = GlobalIndexToSlot.Find(GlobalIndex))
		{
			// Update existing slot
			if (Children.IsValidIndex(*ExistingSlotIndex))
			{
				FStrategyCanvasChildSlot_Internal& Slot = Children[*ExistingSlotIndex];
				Slot.Position = NewData.Position;
				Slot.Depth = NewData.Depth;
				Slot.Widget = NewData.Widget;
				
				UE_LOG(LogStrategyUI, VeryVerbose, TEXT("Updated existing slot %d for global index %d"), 
					*ExistingSlotIndex, GlobalIndex);
			}
		}
		else
		{
			// Add new slot
			const int32 NewIndex = Children.AddSlot(MakeUnique<FStrategyCanvasChildSlot_Internal>());
			FStrategyCanvasChildSlot_Internal& Slot = Children[NewIndex];
			Slot.Position = NewData.Position;
			Slot.Depth = NewData.Depth;
			Slot.Widget = NewData.Widget;
			Slot[NewData.Widget.ToSharedRef()];
			
			GlobalIndexToSlot.Add(GlobalIndex, NewIndex);
			UE_LOG(LogStrategyUI, VeryVerbose, TEXT("Added new slot %d for global index %d"), 
				NewIndex, GlobalIndex);
		}
	}

	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}


void SStrategyCanvasPanel::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	TSet<TWeakPtr<SWidget>> ArrangedWidgets;
	
	struct FSortSlot
	{
		int32 SlotIndex;
		float Depth;
	};

	TArray<FSortSlot> Sorted;
	Sorted.Empty(Children.Num());

	for (int32 i = 0; i < Children.Num(); i++)
	{
		const FStrategyCanvasChildSlot_Internal& Slot = Children[i];
		if (Slot.Widget.IsValid())
		{
			Sorted.Add({ i, Slot.Depth });
		}
	}

	UE_LOG(LogStrategyUI, VeryVerbose, TEXT("%hs: Sorting %d slots by depth"), __FUNCTION__, Sorted.Num());
	Sorted.Sort([](const FSortSlot& A, const FSortSlot& B)
	{
		return A.Depth < B.Depth;
	});

	// Add each valid slot to ArrangedChildren
	for (const FSortSlot& Entry : Sorted)
	{
		const FStrategyCanvasChildSlot_Internal& Slot = Children[Entry.SlotIndex];
		const TWeakPtr<SWidget> ChildWidget = Slot.Widget;

		if (!ChildWidget.IsValid() || ArrangedWidgets.Contains(ChildWidget))
		{
			UE_LOG(LogStrategyUI, Warning, TEXT("%hs: Slot %d has an invalid widget!"), __FUNCTION__, Entry.SlotIndex);
			continue;
		}

		const TSharedRef<SWidget> ChildWidgetRef = ChildWidget.Pin().ToSharedRef();

		ChildWidgetRef->SlatePrepass();

		const FVector2D EntryDesiredSize = ChildWidgetRef->GetDesiredSize();
		const FVector2D PanelCenter = AllottedGeometry.GetLocalSize() * 0.5f;
		const FVector2D FinalPos = PanelCenter + Slot.Position - (EntryDesiredSize * 0.5f);
		UE_LOG(LogStrategyUI, Verbose, TEXT("%hs: Arranging widget %s at LocalPos %s (center: %s -- FinalPos: %s) with size %s"),__FUNCTION__, *UStrategyUIFunctionLibrary::GetFriendlySlateWidgetName(ChildWidget), *Slot.Position.ToString(), *PanelCenter.ToString(), *FinalPos.ToString(), *EntryDesiredSize.ToString());

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(
				ChildWidgetRef,
				FinalPos,
				EntryDesiredSize
			)
		);

		// Don't re-arrange this widget
		ArrangedWidgets.Add(ChildWidget);
	}
}

FVector2D SStrategyCanvasPanel::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// Let the container determine our size.
	return FVector2D::ZeroVector;
}

FChildren* SStrategyCanvasPanel::GetChildren()
{
	return &Children;
}

int32 SStrategyCanvasPanel::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	FArrangedChildren Arranged(EVisibility::Visible);
	this->ArrangeChildren(AllottedGeometry, Arranged);

	int32 MaxLayerId = LayerId;
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);

	// Paint each arranged child.
	for (int32 i = 0; i < Arranged.Num(); i++)
	{
		const FArrangedWidget& ChildArrangement = Arranged[i];
		const int32 ChildLayerId = ChildArrangement.Widget->Paint(
			Args.WithNewParent(this),
			ChildArrangement.Geometry,
			MyCullingRect,
			OutDrawElements,
			MaxLayerId,
			InWidgetStyle,
			bIsEnabled);
		MaxLayerId = FMath::Max(MaxLayerId, ChildLayerId);

		if (bDebugPaint)
		{
			// Draw a debug border around the child widget.
			FSlateDrawElement::MakeBox(
			OutDrawElements,
			MaxLayerId + 1, // Draw above child widget
			ChildArrangement.Geometry.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("Debug.Border"),
			ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 0.f, 0.5f)
			);
		}

	}

	return MaxLayerId;
}