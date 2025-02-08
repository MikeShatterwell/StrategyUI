// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <UObject/Object.h>

#include "StrategyDebugItem.generated.h"

UCLASS(Blueprintable)
class STRATEGYUI_API UStrategyDebugItem : public UObject
{
	GENERATED_BODY()

public:
	/** Arbitrary label for debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|DebugRadialItem")
	FString DebugLabel;

	/** The item's unique ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|DebugRadialItem")
	int32 Id = INDEX_NONE;
	
	/** If true, this item is valid and has data. */
	bool IsValid() const { return Id != INDEX_NONE; }
};

