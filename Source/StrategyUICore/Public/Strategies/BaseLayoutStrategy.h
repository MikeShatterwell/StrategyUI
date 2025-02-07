#pragma once

#include <CoreMinimal.h>

#include "Interfaces/ILayoutStrategyHost.h"
#include "BaseLayoutStrategy.generated.h"

#define MAX_ENTRY_COUNT 64

class UWidget;

/**
 * Base class for an object focused on layout strategies for the StrategyUI plugin.
 *
 * Use one of the existing extension classes (radial wheel/spiral) or extend in code/BP to create custom layouts.
 * 
 * Conceptually, these strategy objects don't own the data or widgets, but rather
 * provide the logic for how to calculate positions, sizes, etc. If you find yourself
 * trying to store item data or widget references here, consider moving that to the owning widget.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, meta=(DisableNativeTick), ClassGroup="StrategyUI")
class STRATEGYUI_API UBaseLayoutStrategy : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override
	{
		UObject::PostEditChangeProperty(PropertyChangedEvent);

		MaxVisibleEntries = FMath::Clamp(MaxVisibleEntries, 1, MAX_ENTRY_COUNT);
	}
#endif

	//----------------------------------------------------------------------------------------------
	// Editable Properties
	//----------------------------------------------------------------------------------------------
	/**
	 * Maximum number of visible entries at once.
	 * If Items.Num() exceeds this value, we only display a subset "visible window" that is determined by ComputeDesiredIndices.
	 * 
	 * This value is clamped to MAX_ENTRY_COUNT (default 64) to prevent performance issues with an extreme number of widgets.
	 * If you find yourself needing anywhere near this many entry widgets, reconsider your design.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseLayoutStrategy|Entry", meta=(ClampMin="1"))
	int32 MaxVisibleEntries = 8;

	/**
	 * The set of global indices represented in the layout.
	 */
	UPROPERTY(BlueprintReadOnly, Category="StrategyUI|BaseLayoutStrategy")
	TSet<int32> DesiredGlobalIndices;

	//--------------------------------------------------------------------------
	// BaseLayoutStrategy API - virtual base functions
	//--------------------------------------------------------------------------
	/**
	 * Called by the widget to initialize anything needed upon first assignment.
	 */
	virtual void InitializeStrategy(TScriptInterface<ILayoutStrategyHost> Host)
	{
		MaxVisibleEntries = FMath::Clamp(MaxVisibleEntries, 1, MAX_ENTRY_COUNT);
	};

	/**
	 * Validates the strategy properties and settings.
	 * This is called during widget compilation to ensure the strategy is set up correctly.
	 */
	virtual void ValidateStrategy(TArray<FText>& OutErrors) const {};

	/** Get the position for a given item index. */
	virtual FVector2D GetItemPosition(int32 GlobalIndex) const { return FVector2D::ZeroVector; }

	/**
	 * Get the currently focused global index.
	 * "Focused" can mean different things depending on the layout strategy.
	 * For example, in a radial layout, it might be the item closest to the radial pointer.
	 * In a world marker layout, it might be an item near the crosshair location. //@TODO: Implement this
	 */
	virtual int32 FindFocusedGlobalIndex() const { return 0; }

	/**
	 * Returns the set of desired global indices to display.
	 * Best to call UpdateVisibleWindow before this to ensure the window is up-to-date.
	 */
	virtual TSet<int32> ComputeDesiredGlobalIndices() { return TSet<int32>(); }

	/**
	 * Converts a global index into the actual item index.
	 * This is mostly useful for resolving data in an infinite list (see spiral layout strategy for example).
	 */
	virtual int32 GlobalIndexToDataIndex(const int32 GlobalIndex) const { return INDEX_NONE; };
	
	/**
	 * Checks if the given global index is within the "visible window" of items.
	 * This is useful for determining if an item should be displayed or hidden.
	 */
	virtual bool ShouldBeVisible(const int32 GlobalIndex) const { return true; };
	
	/**
	 * Draws debug visuals for the layout strategy.
	 * This is useful for visualizing the layout in the editor or during development.
	 *
	 * Called automatically by FLayoutStrategyDebugPaintUtil::DrawLayoutStrategyDebugVisuals if using that utility.
	 */
	virtual void DrawDebugVisuals(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FVector2D& Center) const {};
};
