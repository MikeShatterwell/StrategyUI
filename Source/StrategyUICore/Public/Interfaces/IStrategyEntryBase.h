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
	UFUNCTION(BlueprintImplementableEvent, Category="StrategyUI|IStrategyEntryBase")
	void BP_OnStrategyEntryStateTagsChanged(const FGameplayTagContainer& OldState, const FGameplayTagContainer& NewState);

	/** Called when the widget is assigned new data */
	UFUNCTION(BlueprintImplementableEvent, Category="StrategyUI|IStrategyEntryBase")
	void BP_OnStrategyEntryItemAssigned(const UObject* InItem);

	UFUNCTION(BlueprintImplementableEvent, Category="StrategyUI|IStrategyEntryBase")
	void BP_OnItemSelectionChanged(bool bIsSelected);

	UFUNCTION(BlueprintImplementableEvent, Category="StrategyUI|IStrategyEntryBase")
	void BP_OnItemFocusChanged(bool bIsFocused);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category="StrategyUI|IStrategyEntryBase")
	void BP_NotifyEntryClicked();
};