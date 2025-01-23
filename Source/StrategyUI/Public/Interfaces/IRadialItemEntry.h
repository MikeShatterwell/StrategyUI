// IRadialItemEntry.h
#pragma once

#include "IStrategyEntryBase.h"
#include "IStrategyInteractiveEntry.h"
#include "UObject/Interface.h"
#include "IRadialItemEntry.generated.h"

enum class EStrategyEntryState : uint8;

USTRUCT(BlueprintType)
struct FRadialItemSlotData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialItemSlotData")
	float Angle = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialItemSlotData")
	int32 DataIndex = INDEX_NONE;
};

/**
 * All floats are in [0..1] ready for use in a dynamic material.
 *
 * Included example is used to render a wedge for the item entry in a radial layout.
 * Feel free to extend this struct with additional data for your own use cases.
 */
USTRUCT(BlueprintType)
struct FRadialItemMaterialData
{
	GENERATED_BODY()

public:
	// The center of the spiral in this entry’s local UV space (0..1)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float UVCenterX = -1.f;

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

	// Whether this wedge is focused/hovered
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsFocused = false;
};

UINTERFACE(BlueprintType)
class STRATEGYUI_API URadialItemEntry : public UStrategyInteractiveEntry
{
	GENERATED_BODY()
};

class IRadialItemEntry : public IStrategyInteractiveEntry
{
	GENERATED_IINTERFACE_BODY()
public:
	/** Called when the widget is assigned new material data */
	UFUNCTION(BlueprintImplementableEvent, Category="IRadialItemEntry")
	void BP_SetRadialItemMaterialData(const FRadialItemMaterialData& InMaterialData);
	
	UFUNCTION(BlueprintImplementableEvent, Category="IRadialItemEntry")
	void BP_SetRadialItemSlotData(const FRadialItemSlotData& InSlotData);
};
