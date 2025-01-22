#pragma once

#include <CoreMinimal.h>
#include <UObject/Object.h>

#include "DebugRadialItem.generated.h"

/**
 * A simple UObject to store debug data that can be passed to the RadialSpiralPanel.
 * You can create and populate instances of this class in code or in Blueprint,
 * then pass them into URadialSpiralPanel::SetRadialItems().
 */
UCLASS(Blueprintable)
class STRATEGYUI_API UDebugRadialItem : public UObject
{
	GENERATED_BODY()

public:
	/** Arbitrary label for debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DebugRadialItem")
	FString DebugLabel;

	/** The item's unique ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DebugRadialItem")
	int32 Id = INDEX_NONE;
	
	/** If true, this item is valid and has data. */
	bool IsValid() const { return Id != INDEX_NONE; }
};

