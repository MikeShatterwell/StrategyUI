// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <UObject/Interface.h>

#include <Interfaces/IStrategyEntryBase.h>

#include "IRadialItemEntry.generated.h"

enum class EStrategyEntryState : uint8;

/**
 * All floats are in [0..1] ready for use in a dynamic material.
 *
 * Included example is used to render a wedge for the item entry in a radial layout.
 * Feel free to extend this struct with additional data for your own use cases.
 */
USTRUCT(BlueprintType)
struct STRATEGYUIEXAMPLES_API FRadialItemMaterialData
{
	GENERATED_BODY()

public:
	// The center of the spiral in this entry’s local UV space (0..1)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float UVCenterX = -1.f;

	// The center of the spiral in this entry’s local UV space (0..1)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float UVCenterY = -1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float WedgeWidth   = 0.f; // 0 = 0°, 0.25 = 90°, etc.

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float AngleOffset = 0.f; // 0 = 0°, 0.25 = 90°, etc.
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SpiralMinRadius = 0.f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SpiralMaxRadius = 0.f;

	// How close this item is to the pointer's depth in [0..1] (0 = far, 0.5 = at pointer, 1 = near)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float DistanceFactor = 0.f;

	FString ToString() const
	{
		return FString::Printf(
			TEXT("UVCenterX: %.2f, UVCenterY: %.2f, WedgeWidth: %.2f, AngleOffset: %.2f, SpiralMinRadius: %.2f, SpiralMaxRadius: %.2f, DistanceFactor: %.2f"),
			UVCenterX, UVCenterY, WedgeWidth, AngleOffset, SpiralMinRadius, SpiralMaxRadius, DistanceFactor);
	}
};

UINTERFACE(BlueprintType)
class STRATEGYUIEXAMPLES_API URadialItemEntry : public UStrategyEntryBase
{
	GENERATED_BODY()
};

class IRadialItemEntry : public IStrategyEntryBase
{
	GENERATED_IINTERFACE_BODY()
public:
	/** Called when the widget is assigned new material data */
	UFUNCTION(BlueprintImplementableEvent, Category="IRadialItemEntry")
	void BP_SetRadialItemMaterialData(const FRadialItemMaterialData& InMaterialData);
};
