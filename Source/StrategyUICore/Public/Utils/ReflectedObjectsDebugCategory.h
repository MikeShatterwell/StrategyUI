// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#if WITH_GAMEPLAY_DEBUGGER
#include <GameplayDebuggerCategory.h>
#include <GameplayDebuggerAddonBase.h>

/**
 * Local-only debug category for StrategyUI which reflects properties of a UObject.
 */
class FReflectedObjectsDebugCategory : public FGameplayDebuggerCategory
{
public:
	FReflectedObjectsDebugCategory();
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance()
	{
		TSharedRef<FReflectedObjectsDebugCategory> NewCategory = MakeShareable(new FReflectedObjectsDebugCategory());
		ActiveInstance = NewCategory;
		return NewCategory;
	}

	// Navigation handlers
	void NextPage();
	void PrevPage();

	/** Assigns a local target UObject to reflect upon. */
	void AddTargetObject(UObject* InObject);
	
	void ClearTargets();
	
	// Simple color helper
	static FString GetPropertyColor(const FProperty* Prop)
	{
		if (Prop->IsA<FFloatProperty>()) return TEXT("{green}");
		if (Prop->IsA<FIntProperty>() ||
			Prop->IsA<FInt64Property>() ||
			Prop->IsA<FInt16Property>()) return TEXT("{cyan}");
		if (Prop->IsA<FBoolProperty>())  return TEXT("{red}");
		if (Prop->IsA<FStrProperty>() ||
			Prop->IsA<FNameProperty>())  return TEXT("{magenta}");
		if (Prop->IsA<FTextProperty>())  return TEXT("{orange}");
		if (Prop->IsA<FObjectProperty>()) return TEXT("{yellow}");
		return TEXT("{gray}");
	};

	/**
	 * Set one or more category filters. E.g. {"StrategyUI|*", "Gameplay"} 
	 * to only show properties in categories that match those wildcards.
	 */
	void SetCategoryFilters(const TArray<FString>& InFilters);

	// Static accessor to the currently active instance.
	static TSharedPtr<FReflectedObjectsDebugCategory> ActiveInstance;

private:
	/** The objects we want to inspect. */
	TArray<TWeakObjectPtr<UObject>> TargetObjects;

	/** Optional category filters for reflection. Empty = show all.
	 */
	TArray<FString> PropertyCategoryFilters;
	
	/** Cached debug text lines for pagination */
	TArray<FString> CachedLines;

	/** Current page index for large reflected data sets. */
	int32 CurrentPage = 0;

	/** How many lines to show per page. */
	int32 LinesPerPage = 0;

	/** How many pages to show. */
	int32 TotalPages = 0;

	/** Affects line height, tweak this to improve pagination fit. */
	float CharHeight = 30.f;

	/** Helper method: reflect a single UObject, printing lines via AddTextLine(). */
	void ReflectObjectProperties(UObject* Obj);

	/** Reflect a TMap property (with TMap key/value) in detail. */
	void ReflectTMapProperty(const FMapProperty* MapProp, UObject* Obj, const FString& DisplayName);
};
#endif // WITH_GAMEPLAY_DEBUGGER
