#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>

#include "IStrategyDataProvider.generated.h"

/**
 * Delegate to broadcast that the provider's data has changed 
 * (items added/removed/modified).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDataProviderUpdated);

/**
 * Interface for an object that provides an array of data items for UBaseStrategyWidget.
 * Could be a manager subsystem, etc.
 *
 * If assigned to a widget, the widget will automatically fetch data items from the provider
 */
UINTERFACE(BlueprintType)
class STRATEGYUI_API UStrategyDataProvider : public UInterface
{
	GENERATED_BODY()
};

class IStrategyDataProvider
{
	GENERATED_IINTERFACE_BODY()

public:
	/**
	 * Return the list of data items (UObjects) that the widget should display.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyDataProvider")
	TArray<UObject*> GetDataItems() const;

	/**
	 * Return a delegate used to notify that data changed.
	 * The widget can bind to this to call Refresh.
	 */
	virtual FOnDataProviderUpdated& GetOnDataProviderUpdated();
};
