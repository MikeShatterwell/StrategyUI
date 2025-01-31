#pragma once

#include <CoreMinimal.h>

#include "RadialLayoutStrategy.h"
#include "WheelLayoutStrategy.generated.h"

/**
 * Wheel layout: all items arranged equally around a fixed 360° circle.
 */
UCLASS(Blueprintable, EditInlineNew)
class STRATEGYUI_API UWheelLayoutStrategy : public URadialLayoutStrategy
{
	GENERATED_BODY()

public:
	//--------------------------------------------------------------------------
	// BaseLayoutStrategy overrides
	//--------------------------------------------------------------------------
	virtual void InitializeStrategy(UBaseStrategyWidget* OwnerWidget) override;
	virtual FVector2D GetItemPosition(const int32 GlobalIndex) const override;
	virtual int32 FindFocusedGlobalIndex() const override;
	virtual TSet<int32> ComputeDesiredGlobalIndices() override;
	virtual FVector2D ComputeEntryWidgetSize(const int32 GlobalIndex) override;

	//--------------------------------------------------------------------------
	// RadialLayoutStrategy overrides
	//--------------------------------------------------------------------------	
	virtual float SanitizeAngle(const float InAngle) const override;
	virtual int32 UpdateGapSegments(const int32 TotalItems) override;
	virtual float ComputeShortestUnboundAngleForDataIndex(const int32 DataIndex) const override;
	virtual float CalculateDistanceFactorForGlobalIndex(const int32 GlobalIndex) const override;
};
