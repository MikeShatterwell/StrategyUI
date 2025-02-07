#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>

#include "IStrategyDataProvider.generated.h"

/**
 * Delegate to broadcast that the provider's data has changed 
 * (items added/removed/modified).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDataProviderUpdated);

/*
 * Wrapper for the dynamic multicast delegate to allow it to be exposed to Blueprints.
 *
 * A data provider can return an instance of this delegate wrapper to allow potentially several strategy widgets to bind to the delegate.
 */
UCLASS(Blueprintable)
class UOnDataProviderUpdatedDelegateWrapper : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "StrategyUI|Delegates")
	FOnDataProviderUpdated OnDataProviderUpdatedDelegate;
};

/**
 * Interface for an object that provides an array of data items for UBaseStrategyWidget.
 * Could be a manager subsystem, etc.
 *
 * If assigned to a widget, the widget will automatically fetch data items from the provider
 */
UINTERFACE(BlueprintType, Blueprintable)
class STRATEGYUI_API UStrategyDataProvider : public UInterface
{
	GENERATED_BODY()
};

class IStrategyDataProvider : public IInterface
{
	GENERATED_BODY()

public:
	/**
	 * Return the list of data items (UObjects) that the widget should display.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyUI|StrategyDataProvider")
	TArray<UObject*> GetDataItems() const;
	
	/** 
	 * Return whether the provider is “ready” 
	 * (in case it’s loading from disk or waiting for a streaming level). 
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyUI|StrategyDataProvider")
	bool IsProviderReady() const;

	/**
	 * Initialize the data provider.
	 * This is called when the provider is first assigned to a widget.
	 * Override this to initialize any internal data, (UOnDataProviderUpdatedDelegateWrapper, etc).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyUI|StrategyDataProvider")
	void InitializeDataProvider();

	/**
	 * Return a delegate used to notify that data changed.
	 * The widget can bind to this to call Refresh.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyUI|StrategyDataProvider")
	UOnDataProviderUpdatedDelegateWrapper* GetOnDataProviderUpdated();
};
