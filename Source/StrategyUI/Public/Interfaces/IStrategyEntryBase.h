#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>
#include <GameplayTagContainer.h>

#include "IStrategyEntryBase.generated.h"

UINTERFACE(BlueprintType)
class STRATEGYUI_API UStrategyEntryBase : public UInterface
{
	GENERATED_BODY()
};

class IStrategyEntryBase : public IInterface
{
	GENERATED_IINTERFACE_BODY()
public:
	/** Called when this entry widget transitions states. */
	UFUNCTION(BlueprintImplementableEvent, Category="IStrategyEntryBase")
	void BP_OnStrategyEntryStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState);

	/** Called when the widget is assigned new data */
	UFUNCTION(BlueprintImplementableEvent, Category="IStrategyEntryBase")
	void BP_OnStrategyEntryItemAssigned(const UObject* InItem);
};