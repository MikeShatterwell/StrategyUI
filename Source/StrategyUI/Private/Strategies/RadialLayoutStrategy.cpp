#include "Strategies/RadialLayoutStrategy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RadialLayoutStrategy)

void URadialLayoutStrategy::InitializeStrategy(UBaseStrategyWidget* OwnerWidget)
{
	if (!ensureMsgf(OwnerWidget, TEXT("OwnerWidget is null in %hs!"), __FUNCTION__))
	{
		return;
	}
	SetNumItems(OwnerWidget->GetItemCount());

	UpdateGapSegments(NumItems);
	UpdateAngularSpacing();
}

void URadialLayoutStrategy::ValidateStrategy(TArray<FText>& OutErrors) const
{
	if (RadialSegmentCount <= 0)
	{
		OutErrors.Add(FText::FromString(TEXT("RadialSegmentCount must be greater than 0!")));
	}

	if (MaxVisibleEntries <= 0)
	{
		OutErrors.Add(FText::FromString(TEXT("MaxVisibleEntries must be greater than 0!")));
	}
}

float URadialLayoutStrategy::SanitizeAngle(const float InAngle) const
{
	return InAngle;
}

void URadialLayoutStrategy::SetNumItems(const int32 InNumItems)
{
	NumItems = FMath::Max(0, InNumItems);
}

void URadialLayoutStrategy::SetPointerAngle(const float InAngle)
{
	LatestPointerAngle = SanitizeAngle(InAngle);
}

TSet<int32> URadialLayoutStrategy::ComputeDesiredIndices()
{
	TSet<int32> Indices;
	Indices.Reserve(RadialSegmentCount);

	// All segment indices are desired in a basic radial wheel layout
	for (int32 i = 0; i < RadialSegmentCount; ++i)
	{
		Indices.Add(i);
	}
	return Indices;
}

void URadialLayoutStrategy::UpdateAngularSpacing()
{
	AngularSpacing = (RadialSegmentCount > 0) ? (360.f / RadialSegmentCount) : 0.f;
}

int32 URadialLayoutStrategy::GlobalIndexToDataIndex(const int32 GlobalIndex) const
{
	return (GlobalIndex >= 0 && GlobalIndex < NumItems) ? GlobalIndex : INDEX_NONE;
}

float URadialLayoutStrategy::CalculateItemAngleDegreesForGlobalIndex(const int32 GlobalIndex) const
{
	return static_cast<float>(GlobalIndex) * AngularSpacing;
}

float URadialLayoutStrategy::CalculateRadiusForGlobalIndex(const int32 GlobalIndex) const
{
	return BaseRadius;
}

bool URadialLayoutStrategy::ShouldBeVisible(const int32 GlobalIndex) const
{
	return (GlobalIndex >= VisibleStartIndex && GlobalIndex <= VisibleEndIndex);
}

float URadialLayoutStrategy::GetMinRadius() const
{
	return BaseRadius;
}

float URadialLayoutStrategy::GetMaxRadius() const
{
	return BaseRadius;
}

void URadialLayoutStrategy::DrawDebugVisuals(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FVector2D& Center) const
{
	// Draw a reference circle at BaseRadius
	{
		TArray<FVector2D> CirclePoints;
		constexpr int32 NumSegments = 32;
		CirclePoints.Reserve(NumSegments + 1);

		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float Angle = 2.f * PI * static_cast<float>(i) / static_cast<float>(NumSegments);
			FVector2D Pos = Center + FVector2D(
				BaseRadius * FMath::Cos(Angle),
				BaseRadius * FMath::Sin(Angle)
			);
			CirclePoints.Add(Pos);
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			CirclePoints,
			ESlateDrawEffect::None,
			FLinearColor::Green,
			true,
			1.f
		);
	}

	// Draw pointer line in red
	{
		const float PointerAngleRad = FMath::DegreesToRadians(LatestPointerAngle);
		const FVector2D PointerEnd = Center + FVector2D(
			BaseRadius * FMath::Cos(PointerAngleRad),
			BaseRadius * FMath::Sin(PointerAngleRad)
		);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			{ Center, PointerEnd },
			ESlateDrawEffect::None,
			FLinearColor::Red,
			true,
			2.f
		);
	}

	// Draw radial segment wedges
	{
		const float SegmentAngle = 360.f / RadialSegmentCount;
		const float AngleOffset = SegmentAngle * 0.5f;

		for (int32 i = 0; i < RadialSegmentCount; ++i)
		{
			const float StartAngleRad = FMath::DegreesToRadians((i * SegmentAngle) - AngleOffset);

			FVector2D StartPoint = Center + FVector2D(
				BaseRadius * FMath::Cos(StartAngleRad),
				BaseRadius * FMath::Sin(StartAngleRad)
			);

			const TArray<FVector2f> WedgePoints = {
				FVector2f(Center),
				FVector2f(StartPoint),
			};

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				WedgePoints,
				ESlateDrawEffect::None,
				FLinearColor(0.5f, 0.5f, 0.5f, 0.5f),
				/*bAntialias=*/true,
				/*Thickness=*/1.0f
			);
		}
	}
}
