// Copyright Mike Desrosiers 2025, All Rights Reserved.

#include "Utils/StrategyUIFunctionLibrary.h"

#include <Blueprint/UserWidget.h>
#include <Engine/Engine.h>
#include <Misc/Paths.h>
#include <UObject/SoftObjectPtr.h>
#include <Logging/LogMacros.h>

#include "Settings/StrategyUIProjectSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogStrategyUISettings, Log, All);

TSubclassOf<UUserWidget> UStrategyUIFunctionLibrary::GetWidgetClassForTag(
	const FGameplayTag& InTag,
	const bool bLogWarnings
)
{
	// Quick validity check
	if (!InTag.IsValid())
	{
		if (bLogWarnings)
		{
			UE_LOG(LogStrategyUISettings, Warning,
				TEXT("%hs called with an invalid tag."), __FUNCTION__);
		}
		return nullptr;
	}

	// Fetch the project settings
	const UStrategyUIProjectSettings* Settings = UStrategyUIProjectSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogStrategyUISettings, Error,
			TEXT("StrategyUIProjectSettings not found. Cannot look up Tag->WidgetClass."));
		return nullptr;
	}

	// Look for a class in TagToWidgetClassMap
	if (Settings->TagToWidgetClassMap.Contains(InTag))
	{
		if (const TSubclassOf<UUserWidget> FoundClass = *Settings->TagToWidgetClassMap.Find(InTag))
		{
			return FoundClass;
		}
	}

	if (bLogWarnings && Settings->bWarnOnMissingClass)
	{
		UE_LOG(LogStrategyUISettings, Warning,
			TEXT("No widget class found for tag [%s]."),
			*InTag.ToString());
	}
	return nullptr;
	
}

bool UStrategyUIFunctionLibrary::TryGetWidgetClassForTag(
	const FGameplayTag& InTag,
	TSubclassOf<UUserWidget>& OutClass,
	const bool bLogWarnings
)
{
	OutClass = GetWidgetClassForTag(InTag, bLogWarnings);
	return (OutClass != nullptr);
}

TSubclassOf<UUserWidget> UStrategyUIFunctionLibrary::GetWidgetClassForTagWithFallback(
	const FGameplayTag& InTag,
	const TSubclassOf<UUserWidget> FallbackClass,
	const bool bLogWarnings
)
{
	// First try to get the mapped class
	const TSubclassOf<UUserWidget> FoundClass = GetWidgetClassForTag(InTag, bLogWarnings);
	// If not found, use fallback
	return FoundClass ? FoundClass : FallbackClass;
}

TSoftClassPtr<UUserWidget> UStrategyUIFunctionLibrary::GetWidgetSoftClassForTag(const FGameplayTag& InTag, const bool bLogWarnings)
{
	// Quick validity check
	if (!InTag.IsValid())
	{
		if (bLogWarnings)
		{
			UE_LOG(LogStrategyUISettings, Warning,
				TEXT("%hs called with an invalid tag."), __FUNCTION__);
		}
		return nullptr;
	}

	// Fetch the project settings
	const UStrategyUIProjectSettings* Settings = UStrategyUIProjectSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogStrategyUISettings, Error,
			TEXT("StrategyUIProjectSettings not found. Cannot look up Tag->WidgetClass."));
		return nullptr;
	}

	// Look for a class in TagToWidgetClassMap
	if (Settings->TagToWidgetSoftClassMap.Contains(InTag))
	{
		if (const TSoftClassPtr<UUserWidget> FoundSoftClass = *Settings->TagToWidgetSoftClassMap.Find(InTag))
		{
			return FoundSoftClass;
		}
	}

	if (bLogWarnings && Settings->bWarnOnMissingClass)
	{
		UE_LOG(LogStrategyUISettings, Warning,
			TEXT("No widget class found for tag [%s]."),
			*InTag.ToString());
	}
	return nullptr;
}

bool UStrategyUIFunctionLibrary::TryGetWidgetSoftClassForTag(const FGameplayTag& InTag,
	TSoftClassPtr<UUserWidget>& OutClass, const bool bLogWarnings)
{
	OutClass = GetWidgetSoftClassForTag(InTag, bLogWarnings);
	return (OutClass != nullptr);
}

TSoftClassPtr<UUserWidget> UStrategyUIFunctionLibrary::GetWidgetSoftClassForTagWithFallback(const FGameplayTag& InTag,
	const TSoftClassPtr<UUserWidget> FallbackClass, const bool bLogWarnings)
{
	// First try to get the mapped class
	const TSoftClassPtr<UUserWidget> FoundClass = GetWidgetSoftClassForTag(InTag, bLogWarnings);
	// If not found, use fallback
	return FoundClass ? FoundClass : FallbackClass;
}
