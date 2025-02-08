// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include <Strategies/RadialLayoutStrategy.h>

#include "SpiralLayoutStrategy.generated.h"

/**
 * Spiral layout: can handle more items than SegmentCount.
 * Possibly includes a "gap" after the last item.
 */
UCLASS(ClassGroup="StrategyUI|RadialLayout|Spiral")
class STRATEGYUIEXAMPLES_API USpiralLayoutStrategy : public URadialLayoutStrategy
{
	GENERATED_BODY()

public:
	//----------------------------------------------------------------------------------------------
	// Editable Properties
	//----------------------------------------------------------------------------------------------
	/**
	 * Entries are offset outward based on distance from the focused item.
	 * This setting controls the maximum offset (in screen units) at the farthest points (distance factor of 1).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpiralStrategy|Layout")
	float SpiralOutwardOffset = 400.f;

	/**
	 * Entries are offset inward based on distance from the focused item.
	 * This setting controls the maximum offset (in screen units) at the farthest points (distance factor of 0).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpiralStrategy|Layout")
	float SpiralInwardOffset = -400.f;

	/**
	 * Number of turns away from the pointer to consider an item at the far extent of the DistanceFactor range.
	 * For example, if set to 2, an item 2 turns away from the pointer will be at the minimum/maximum distance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpiralStrategy|Layout")
	float DistanceFactorTurnThreshold = 2.f;

	//--------------------------------------------------------------------------
	// BaseLayoutStrategy overrides
	//--------------------------------------------------------------------------
	virtual FVector2D GetItemPosition(const int32 GlobalIndex) const override;
	virtual int32 FindFocusedGlobalIndex() const override;
	virtual TSet<int32> ComputeDesiredGlobalIndices() override;
	virtual int32 GlobalIndexToDataIndex(const int32 GlobalIndex) const override;

	/**
	 * Draws debug visuals for the spiral layout.
	 * - Inherits circles from the parent radial strategy
	 * - Adds additional circles for SpiralOutwardOffset/SpiralInwardOffset
	 * - Draws a grey and yellow line sampling the spiral path itself
	 */
	virtual void DrawDebugVisuals(const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Center) const override;

	//--------------------------------------------------------------------------
	// RadialLayoutStrategy overrides
	//--------------------------------------------------------------------------
	virtual int32 UpdateGapSegments(const int32 TotalItems) override;
	virtual float ComputeShortestUnboundAngleForDataIndex(const int32 DataIndex) const override;
	
	virtual float CalculateItemAngleDegreesForGlobalIndex(int32 GlobalIndex) const override;
	virtual float CalculateDistanceFactorForGlobalIndex(const int32 GlobalIndex) const override;
	virtual float CalculateRadiusForGlobalIndex(const int32 GlobalIndex) const override;
	
	virtual float GetMinRadius() const override { return BaseRadius + SpiralInwardOffset; }
	virtual float GetMaxRadius() const override { return BaseRadius + SpiralOutwardOffset; }
};
