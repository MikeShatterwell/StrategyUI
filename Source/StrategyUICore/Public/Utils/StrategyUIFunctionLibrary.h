// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Kismet/BlueprintFunctionLibrary.h>
#include <GameplayTagContainer.h>
#include <Slate/SObjectWidget.h>

#include "StrategyUIFunctionLibrary.generated.h"

UCLASS()
class STRATEGYUI_API UStrategyUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fetch the widget class for a given GameplayTag from the project settings.
	 * 
	 * @param InTag          The GameplayTag to look up.
	 * @param bLogWarnings   Whether to log warnings if the tag is missing or invalid.
	 * @return The mapped widget class, or nullptr if none is found.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|Settings")
	static TSubclassOf<UUserWidget> GetWidgetClassForTag(
		const FGameplayTag& InTag,
		bool bLogWarnings = true
	);

	/**
	 * Same as GetWidgetClassForTag, but also returns success/failure as a boolean
	 * and outputs the found class in an Out parameter.
	 *
	 * @param InTag          The GameplayTag to look up.
	 * @param OutClass       The returned widget class if found.
	 * @param bLogWarnings   Whether to log warnings if the tag is missing or invalid.
	 * @return True if a class was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|Settings")
	static bool TryGetWidgetClassForTag(
		const FGameplayTag& InTag,
		TSubclassOf<UUserWidget>& OutClass,
		bool bLogWarnings = true
	);

	/**
	 * Similar to GetWidgetClassForTag but returns a user-specified fallback class
	 * if the mapping is not found.
	 * 
	 * @param InTag          The GameplayTag to look up.
	 * @param FallbackClass  Class returned if no mapping exists.
	 * @param bLogWarnings   Whether to log warnings if the tag is missing or invalid.
	 * @return The mapped widget class if found; otherwise FallbackClass.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|Settings")
	static TSubclassOf<UUserWidget> GetWidgetClassForTagWithFallback(
		const FGameplayTag& InTag,
		const TSubclassOf<UUserWidget> FallbackClass,
		bool bLogWarnings = true
	);

	UFUNCTION(BlueprintCallable, Category="StrategyUI|Settings")
	static TSoftClassPtr<UUserWidget> GetWidgetSoftClassForTag(
		const FGameplayTag& InTag,
		bool bLogWarnings = true
	);

	UFUNCTION(BlueprintCallable, Category="StrategyUI|Settings")
	static bool TryGetWidgetSoftClassForTag(
		const FGameplayTag& InTag,
		TSoftClassPtr<UUserWidget>& OutClass,
		bool bLogWarnings = true
	);

	UFUNCTION(BlueprintCallable, Category="StrategyUI|Settings")
	static TSoftClassPtr<UUserWidget> GetWidgetSoftClassForTagWithFallback(
		const FGameplayTag& InTag,
		const TSoftClassPtr<UUserWidget> FallbackClass,
		bool bLogWarnings = true
	);

	static FString GetFriendlySlateWidgetName(const TSharedPtr<SWidget>& InWidget)
	{
		if (!InWidget.IsValid())
		{
			return TEXT("Invalid");
		}

		if (const TSharedPtr<SObjectWidget> ObjWidget = StaticCastSharedPtr<SObjectWidget>(InWidget))
		{
			if (const UUserWidget* WrappedObject = ObjWidget->GetWidgetObject())
			{
				return WrappedObject->GetName();
			}
		}

		return InWidget->GetTypeAsString();
	}

	static FString GetFriendlySlateWidgetName(const TWeakPtr<SWidget>& InWidget)
	{
		if (!InWidget.IsValid())
		{
			return TEXT("Invalid");
		}

		if (const TSharedPtr<SObjectWidget> ObjWidget = StaticCastSharedPtr<SObjectWidget>(InWidget.Pin()))
		{
			if (const UUserWidget* WrappedObject = ObjWidget->GetWidgetObject())
			{
				return WrappedObject->GetName();
			}
		}

		return InWidget.Pin()->GetTypeAsString();
	}
};
