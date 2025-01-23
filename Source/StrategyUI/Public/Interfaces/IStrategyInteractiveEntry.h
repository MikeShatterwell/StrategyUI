#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>

#include "IStrategyEntryBase.h"
#include "IStrategyInteractiveEntry.generated.h"

UINTERFACE(BlueprintType)
class STRATEGYUI_API UStrategyInteractiveEntry : public UStrategyEntryBase
{
	GENERATED_BODY()
};

class IStrategyInteractiveEntry : public IStrategyEntryBase
{
	GENERATED_IINTERFACE_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, Category="IStrategyInteractiveEntry")
	void BP_OnItemSelectionChanged(bool bIsSelected);

	UFUNCTION(BlueprintImplementableEvent, Category="IStrategyInteractiveEntry")
	void BP_OnItemFocusChanged(bool bIsFocused);
};