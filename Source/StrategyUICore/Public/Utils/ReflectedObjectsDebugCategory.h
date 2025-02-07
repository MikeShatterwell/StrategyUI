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
	static FLinearColor GetPropertyColor(const FProperty* Prop)
	{
		if (Prop->IsA<FFloatProperty>()) return FLinearColor(0.f, 1.f, 0.f, 1.f);   // green
		if (Prop->IsA<FIntProperty>() ||
			Prop->IsA<FInt64Property>() ||
			Prop->IsA<FInt16Property>()) return FLinearColor(0.4f, 0.5f, 1.f, 1.f); // blue
		if (Prop->IsA<FBoolProperty>())  return FLinearColor(1.f, 0.2f, 0.2f, 1.f); // red
		if (Prop->IsA<FStrProperty>() ||
			Prop->IsA<FNameProperty>())  return FLinearColor(1.f, 0.7f, 1.f, 1.f);  // rose
		return FLinearColor(0.8f, 0.8f, 0.8f, 1.f); // fallback (light gray)
	};

	static FString ToHexMarkup(const FLinearColor& InColor)
	{
		// Convert [0..1] floats to [0..255] and clamp
		const int32 R = FMath::Clamp(FMath::RoundToInt(InColor.R * 255.f), 0, 255);
		const int32 G = FMath::Clamp(FMath::RoundToInt(InColor.G * 255.f), 0, 255);
		const int32 B = FMath::Clamp(FMath::RoundToInt(InColor.B * 255.f), 0, 255);

		// Format like "{#RRGGBB}"
		return FString::Printf(TEXT("{#%02X%02X%02X}"), R, G, B);
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
