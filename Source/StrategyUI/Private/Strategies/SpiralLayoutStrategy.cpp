#include "Strategies/SpiralLayoutStrategy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SpiralLayoutStrategy)

int32 USpiralLayoutStrategy::UpdateGapSegments(const int32 TotalItems)
{
	const int32 Remainder = TotalItems % RadialSegmentCount;

	if (Remainder == 0)
	{
		// If the number of items is an exact multiple of RadialSegments, no gap is needed.
		GapPaddingSegments = 0;
		return GapPaddingSegments;
	}
		
	// Otherwise, figure out how many additional “segments” we need so that
	// after a full 360 degrees we’re back aligned with item #0 again.
	// For example, if Items.Num()=10 and RadialSegments=8, remainder=2, needed=6
	// This ensures that (10 + 6) is a multiple of 8, so the tail aligns with the head plus gap.
	const int32 Needed = RadialSegmentCount - Remainder; // how many extra “slots” until next multiple

	GapPaddingSegments = FMath::Clamp(Needed, 0, RadialSegmentCount);
	return GapPaddingSegments;
}

TSet<int32> USpiralLayoutStrategy::ComputeDesiredGlobalIndices()
{
	VisibleGlobalIndices.Empty(MaxVisibleEntries);

	// We'll pick a window half on each side of the focused item.
	const int32 HalfWindow = MaxVisibleEntries / 2;
	VisibleStartIndex = FindFocusedGlobalIndexByAngle() - HalfWindow;

	// The last index to show (inclusive):
	VisibleEndIndex = VisibleStartIndex + MaxVisibleEntries - 1;
	
	const int32 ExtendedStart = VisibleStartIndex - NumDeactivatedEntries;
	const int32 ExtendedEnd   = VisibleEndIndex + NumDeactivatedEntries;

	for (int32 i = ExtendedStart; i <= ExtendedEnd; ++i)
	{
		VisibleGlobalIndices.Add(i);
	}
	return VisibleGlobalIndices;
}

FVector2D USpiralLayoutStrategy::ComputeEntryWidgetSize(const int32 GlobalIndex)
{
	return FVector2D((BaseRadius + SpiralOutwardOffset) * 2.f);
	/*const float DistanceFactor = CalculateDistanceFactorForGlobalIndex(GlobalIndex);

	// We'll linearly shrink or grow from 1.0x to 0.6x:
	const float Scale = FMath::Lerp(0.6f, 1.0f, DistanceFactor);

	const float InnerAbs = FMath::Abs(SpiralInwardOffset);
	const float OuterAbs = FMath::Abs(SpiralOutwardOffset);
	const float MaxExtent = FMath::Max(InnerAbs, OuterAbs);

	// This might still clip the wedge if Scale < 1.0, so weigh that carefully:
	const float Dimension = (BaseRadius + MaxExtent) * 2.f * Scale;

	return FVector2D(Dimension, Dimension);*/
}

FVector2D USpiralLayoutStrategy::GetItemPosition(const int32 GlobalIndex) const
{
	const float ItemAngleDeg = CalculateItemAngleDegreesForGlobalIndex(GlobalIndex);
	
	const float ItemAngleRad = FMath::DegreesToRadians(ItemAngleDeg);

	const float FinalRadius = CalculateRadiusForGlobalIndex(GlobalIndex);

	const float PosX = FinalRadius * FMath::Cos(ItemAngleRad);
	const float PosY = FinalRadius * FMath::Sin(ItemAngleRad);
	
	return FVector2D(PosX, PosY);
}

int32 USpiralLayoutStrategy::FindFocusedGlobalIndexByAngle() const
{
	const float EffectiveAngularSpacing = GetAngularSpacing();
	if (FMath::IsNearlyZero(EffectiveAngularSpacing))
	{
		return 0;
	}
	// Spiral can have unbounded angles. We'll offset by half a wedge:
	const float OffsetAngle = GetPointerAngle() + (EffectiveAngularSpacing * 0.5f);
	return FMath::FloorToInt(OffsetAngle / EffectiveAngularSpacing);
}

float USpiralLayoutStrategy::ComputeShortestUnboundAngleForDataIndex(const int32 DataIndex) const
{
	if (NumItems <= 0)
	{
		return GetPointerAngle();
	}
	
	const float EffectiveAngularSpacing = GetAngularSpacing();

	const float ItemBaseAngle = DataIndex * EffectiveAngularSpacing;
	const float CycleStep = (NumItems + GapPaddingSegments) * EffectiveAngularSpacing;

	// Avoid dividing by zero
	if (FMath::IsNearlyZero(CycleStep))
	{
		return ItemBaseAngle;
	}
	const float LatestEffectivePointerAngle = GetPointerAngle();

	// offset = how far "past" the base angle we are
	const float Offset = (LatestEffectivePointerAngle - ItemBaseAngle);

	// fractional cycle index
	const float nFloat  = Offset / CycleStep;
	const int32 nFloor  = FMath::FloorToInt(nFloat);
	const int32 nCeil   = FMath::CeilToInt(nFloat);

	// Calculate two possible angles that are closest to the pointer
	const float FloorAngle = ItemBaseAngle + (nFloor * CycleStep);
	const float CeilAngle  = ItemBaseAngle + (nCeil  * CycleStep);

	// Measure the absolute difference between the two angles
	const float DistFloor = FMath::Abs(FloorAngle - LatestEffectivePointerAngle);
	const float DistCeil  = FMath::Abs(CeilAngle  - LatestEffectivePointerAngle);

	// Return the smaller of the two angles (shortest unbound angle to the pointer)
	return (DistFloor <= DistCeil) ? FloorAngle : CeilAngle;
}

int32 USpiralLayoutStrategy::GlobalIndexToDataIndex(const int32 GlobalIndex) const
{
	if (NumItems <= 0)
	{
		return INDEX_NONE;
	}

	// Each "cycle" of the spiral includes all items and the gap segments
	const int32 VirtualCycle = NumItems + GapPaddingSegments;

	// Proper mod that never goes negative:
	const int32 WrappedIndex = ((GlobalIndex % VirtualCycle) + VirtualCycle) % VirtualCycle;

	// If inside the real portion, return it; otherwise it’s in the gap
	return (WrappedIndex < NumItems) ? WrappedIndex : INDEX_NONE;
}

float USpiralLayoutStrategy::CalculateItemAngleDegreesForGlobalIndex(const int32 GlobalIndex) const
{
	return GlobalIndex * GetAngularSpacing();
}

float USpiralLayoutStrategy::CalculateDistanceFactorForGlobalIndex(const int32 GlobalIndex) const
{
	// Item angle is fixed, but radius depends on partial turn difference
	const float ItemAngleDeg = CalculateItemAngleDegreesForGlobalIndex(GlobalIndex);

	const float PointerTurns = GetPointerAngle() / 360.f;
	const float ItemTurns = ItemAngleDeg / 360.f;
	const float TurnDiff = PointerTurns - ItemTurns;

	const float ClampedDiff = FMath::Clamp(TurnDiff, -DistanceFactorTurnThreshold, DistanceFactorTurnThreshold);

	return FMath::GetMappedRangeValueClamped(
		FVector2D(-DistanceFactorTurnThreshold, DistanceFactorTurnThreshold),
		FVector2D(0.f, +1.f),
		ClampedDiff
	);
}

float USpiralLayoutStrategy::CalculateRadiusForGlobalIndex(const int32 GlobalIndex) const
{
	const float DistanceFactor = CalculateDistanceFactorForGlobalIndex(GlobalIndex);

	const float Offset = FMath::Lerp(SpiralInwardOffset, SpiralOutwardOffset, DistanceFactor);
	return BaseRadius + Offset;
}

float USpiralLayoutStrategy::GetMinRadius() const
{
	return BaseRadius + SpiralInwardOffset;
}

float USpiralLayoutStrategy::GetMaxRadius() const
{
	return BaseRadius + SpiralOutwardOffset;
}

void USpiralLayoutStrategy::DrawDebugVisuals(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
                                             const FVector2D& Center) const
{
	Super::DrawDebugVisuals(AllottedGeometry, OutDrawElements, LayerId, Center);

	// Draw a circle at SpiralOutwardOffset
	{
		TArray<FVector2D> CirclePoints;
		constexpr int32 NumSegments = 32;
		CirclePoints.Reserve(NumSegments + 1);

		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float Angle = 2.f * PI * static_cast<float>(i) / static_cast<float>(NumSegments);
			FVector2D Pos = Center + FVector2D(
			 GetMaxRadius() * FMath::Cos(Angle),
			 GetMaxRadius() * FMath::Sin(Angle)
			);
			CirclePoints.Add(Pos);
		}

		FSlateDrawElement::MakeLines(
		 OutDrawElements,
		 LayerId,
		 AllottedGeometry.ToPaintGeometry(),
		 CirclePoints,
		 ESlateDrawEffect::None,
		 FLinearColor(/*Magenta*/0.75f, 0.25f, 0.75f, 1.f),
		 true,
		 1.f
		);
	}

	// Draw a circle at SpiralInwardOffset
	{
		TArray<FVector2D> CirclePoints;
		constexpr int32 NumSegments = 32;
		CirclePoints.Reserve(NumSegments + 1);

		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float Angle = 2.f * PI * static_cast<float>(i) / static_cast<float>(NumSegments);
			FVector2D Pos = Center + FVector2D(
			 GetMinRadius() * FMath::Cos(Angle),
			 GetMinRadius() * FMath::Sin(Angle)
			);
			CirclePoints.Add(Pos);
		}

		FSlateDrawElement::MakeLines(
		 OutDrawElements,
		 LayerId,
		 AllottedGeometry.ToPaintGeometry(),
		 CirclePoints,
		 ESlateDrawEffect::None,
		 FLinearColor(/*Magenta*/0.75f, 0.25f, 0.75f, 1.f),
		 true,
		 1.f
		);
	}

	// 3) Draw the entire spiral in gray
	{
		constexpr int32 RangeAroundFocus = 50.f;

		// Find whichever global index is "focused" by the pointer right now:
		const int32 CenterGlobalIndex = FindFocusedGlobalIndexByAngle();

		// Build a polyline from [CenterIndex - Range .. CenterIndex + Range].
		TArray<FVector2D> SpiralPoints;
		SpiralPoints.Reserve(RangeAroundFocus * 2 + 1);

		for (int32 i = CenterGlobalIndex - RangeAroundFocus; i <= CenterGlobalIndex + RangeAroundFocus; ++i)
		{
			// Get the *local position* of item i (0,0 is the center).
			FVector2D LocalPos = GetItemPosition(i);

			// Shift by the actual widget center to make it screen-space
			SpiralPoints.Add(Center + LocalPos);
		}

		// Finally, issue the draw call. We get a single continuous line.
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			SpiralPoints,
			ESlateDrawEffect::None,
			FLinearColor(/*Gray*/.3f, .3f, .3f, .5f),
			true,
			2.f
		);
	}

	// 4) Draw the portion of the spiral that represents the visible range in yellow
	{
		// How big a chunk of the spiral do we draw on each side of the pointer?
		const int32 RangeAroundFocus = MaxVisibleEntries * 0.5f;

		// Find whichever global index is "focused" by the pointer right now:
		const int32 CenterGlobalIndex = FindFocusedGlobalIndexByAngle();

		// Build a polyline from [CenterIndex - Range .. CenterIndex + Range].
		TArray<FVector2D> SpiralPoints;
		SpiralPoints.Reserve(RangeAroundFocus * 2 + 1);

		for (int32 i = CenterGlobalIndex - RangeAroundFocus; i <= CenterGlobalIndex + RangeAroundFocus; ++i)
		{
			// Get the *local position* of item i (0,0 is the center).
			FVector2D LocalPos = GetItemPosition(i);

			// Shift by the actual widget center to make it screen-space
			SpiralPoints.Add(Center + LocalPos);
		}

		// Finally, issue the draw call. We get a single continuous line.
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			SpiralPoints,
			ESlateDrawEffect::None,
			FLinearColor::Yellow,
			true,
			2.f
		);
	}
}
