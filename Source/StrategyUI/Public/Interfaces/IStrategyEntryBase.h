#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>

#include "IStrategyEntryBase.generated.h"
// @TODO: Implement me and refactor out some of the IRadialItemEntry stuff

UINTERFACE(BlueprintType)
class STRATEGYUI_API UStrategyEntryBase : public UInterface
{
	GENERATED_BODY()
};

class IStrategyEntryBase
{
	GENERATED_BODY()
public:
	/** Called when this entry widget transitions from Hidden to Visible. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Entry")
	void OnEntryStateChanged(const EStrategyEntryState OldState, const EStrategyEntryState NewState);
};