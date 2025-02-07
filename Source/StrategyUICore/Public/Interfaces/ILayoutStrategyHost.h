#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>

#include "ILayoutStrategyHost.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class STRATEGYUI_API ULayoutStrategyHost : public UInterface
{
	GENERATED_BODY()
};

class ILayoutStrategyHost : public IInterface
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyUI|LayoutStrategyHost")
	int32 GetNumItems() const;
};