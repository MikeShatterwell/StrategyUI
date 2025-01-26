#include "Strategies/WheelLayoutStrategy.h"

#include "Utils/LogStrategyUI.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WheelLayoutStrategy)

void UWheelLayoutStrategy::InitializeStrategy(UBaseStrategyWidget* OwnerWidget)
{
	Super::InitializeStrategy(OwnerWidget);

	if (NumItems > RadialSegmentCount)
	{
		UE_LOG(LogStrategyUI, Warning, TEXT("%hs: NumItems is greater than RadialSegmentCount. Increasing the segment count to fit the number of items."), __FUNCTION__);
		RadialSegmentCount = NumItems;
	}

	MaxVisibleEntries = FMath::Min(MaxVisibleEntries, RadialSegmentCount);
	
	UpdateGapSegments(NumItems);
	UpdateAngularSpacing();
}

float UWheelLayoutStrategy::SanitizeAngle(const float InAngle) const
{
	float WorkingAngle = InAngle;
	WorkingAngle = FMath::Fmod(WorkingAngle, 360.f);
		
	// If negative, wrap around
	if (WorkingAngle < 0.f)
	{
		WorkingAngle += 360.f;
	}
	return WorkingAngle;
}

int32 UWheelLayoutStrategy::UpdateGapSegments(const int32 TotalItems)
{
	GapPaddingSegments = FMath::Max(0, RadialSegmentCount - TotalItems);
	return GapPaddingSegments;
}

TSet<int32> UWheelLayoutStrategy::ComputeDesiredIndices()
{
	// Wheel layout has no concept of a "visible window" since all items are always visible
	VisibleStartIndex = 0;
	VisibleEndIndex = RadialSegmentCount;
	
	TSet<int32> Indices;
	Indices.Reserve(RadialSegmentCount);

	const int32 EndIndex = RadialSegmentCount;
	for (int32 i = VisibleStartIndex; i < EndIndex; ++i)
	{
		Indices.Add(i);
	}
	return Indices;
}

FVector2D UWheelLayoutStrategy::ComputeEntryWidgetSize(const int32 GlobalIndex)
{
	return FVector2D(BaseRadius * 2.f);
}

FVector2D UWheelLayoutStrategy::GetItemPosition(const int32 GlobalIndex) const
{
	const int32 ClampedIndex = FMath::Clamp(GlobalIndex, 0, RadialSegmentCount - 1);

	const float Degrees = static_cast<float>(ClampedIndex) * AngularSpacing;
	const float Radians = FMath::DegreesToRadians(Degrees);

	// Simply place the item around the fixed radius
	const float X = BaseRadius * FMath::Cos(Radians);
	const float Y = BaseRadius * FMath::Sin(Radians);

	return FVector2D(X, Y);
}

int32 UWheelLayoutStrategy::FindFocusedGlobalIndexByAngle() const
{
	// Clamp to [0..360] since we don't need to worry about multiple turn cycles in a wheel
	float CleanAngle = FMath::Fmod(LatestPointerAngle, 360.f);
	if (CleanAngle < 0.f)
	{
		CleanAngle += 360.f;
	}

	// Add half wedge to center on segments
	CleanAngle += (AngularSpacing * 0.5f);

	// Wrap again into [0..360]
	CleanAngle = FMath::Fmod(CleanAngle, 360.f);

	// Find which wedge we're in
	return FMath::FloorToInt(CleanAngle / AngularSpacing);
}

float UWheelLayoutStrategy::ComputeShortestUnboundAngleForDataIndex(const int32 DataIndex) const
{
	const float ItemBaseAngle = DataIndex * AngularSpacing;
	return ItemBaseAngle;
}

float UWheelLayoutStrategy::CalculateDistanceFactorForGlobalIndex(const int32 GlobalIndex) const
{
	constexpr float DistanceFactor = 0.5f;
	return DistanceFactor;
}
