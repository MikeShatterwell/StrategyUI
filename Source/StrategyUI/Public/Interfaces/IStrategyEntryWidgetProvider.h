#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>
#include <GameplayTagContainer.h>

#include "IStrategyEntryWidgetProvider.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class STRATEGYUI_API UStrategyEntryWidgetProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for providing widget class or tag information for representing a data item.
 *
 * Classes implementing this interface can define how to dynamically determine the appropriate
 * widget to be used for specific data items.
 *
 * For example, a data item representing an objective marker could return a different entry class
 * or tag depending on the type of objective.
 *
 * This interface is not needed if only one entry widget class is used for all data items (in this
 * case, the default widget class can be set directly in the BaseStrategyWidget's properties).
 */
class IStrategyEntryWidgetProvider : public IInterface
{
	GENERATED_BODY()

public:
	/**
	 * Return the class of the widget that should be created to represent this data item.
	 * 
	 * This is the highest priority method for determining the widget class.
	 * If this method returns nullptr, the widget tag will be used to look up a class in the StrategyUI project settings.
	 * If no class is found there, the BaseStrategyWidget's default entry widget class will be used.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyWidgetClassProvider")
	TSubclassOf<UUserWidget> GetEntryWidgetClass() const;

	/**
	 * Return the tag of the widget that should be created to represent this data item.
	 * 
	 * This is used to look up a class in the StrategyUI project settings if GetWidgetClass returns nullptr.
	 * If no class is found there, the BaseStrategyWidget's default entry widget class will be used.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="StrategyWidgetClassProvider")
	FGameplayTag GetEntryWidgetTag() const;
};