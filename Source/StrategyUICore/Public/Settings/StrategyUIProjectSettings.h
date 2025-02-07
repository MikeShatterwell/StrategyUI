#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>
#include <GameplayTagContainer.h>

#include "StrategyUIProjectSettings.generated.h"

/**
 * Configure in Project Settings > Plugins > Strategy UI Settings.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Strategy UI Settings"))
class STRATEGYUI_API UStrategyUIProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UStrategyUIProjectSettings* Get()
	{
		return GetDefault<UStrategyUIProjectSettings>();
	}

	/** If true, we log a warning any time we fail to find a widget class for a given tag. */
	UPROPERTY(EditAnywhere, Config, Category="StrategyUI|Logging")
	bool bWarnOnMissingClass = true;

	/**
	 * Maps a specific "key" tag to a widget class (e.g. "StrategyUI.WorldMarker.Friendly" -> W_FriendlyMarkerEntry)
	 * that the base widget will instantiate if no direct class is provided by the item.
	 */
	UPROPERTY(EditAnywhere, Config, Category="StrategyUI|WidgetMapping")
	TMap<FGameplayTag, TSubclassOf<UUserWidget>> TagToWidgetClassMap;

	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Plugins"));
	}

	virtual FName GetSectionName() const override
	{
		return FName(TEXT("Strategy UI"));
	}
};
