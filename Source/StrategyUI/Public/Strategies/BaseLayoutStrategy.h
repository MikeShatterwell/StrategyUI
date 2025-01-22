#pragma once

#include <CoreMinimal.h>

#include "BaseLayoutStrategy.generated.h"

class UBaseStrategyWidget;
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
	/**
	 * Called by the widget to initialize anything needed upon first assignment.
	 */
	virtual void InitializeStrategy(UBaseStrategyWidget* OwnerWidget) {};

	/**
	 * Validates the strategy properties and settings.
	 * This is called during widget compilation to ensure the strategy is set up correctly.
	 */
	virtual void ValidateStrategy(TArray<FText>& OutErrors) const {};
	
	/**
	 * Computes which indices to show and possibly where to place them.
	 */
	virtual void ComputeLayout(int32 NumItems, TArray<int32>& OutIndicesToShow) {}

	/** Get the position for a given item index. */
	virtual FVector2D GetItemPosition(int32 GlobalIndex) const { return FVector2D::ZeroVector; }

	/**
	 * Returns the set of desired global indices to display.
	 * Best to call UpdateVisibleWindow before this to ensure the window is up-to-date.
	 */
	virtual TSet<int32> ComputeDesiredIndices() { return TSet<int32>(); }

	/**
	 * Computes the size of each entry widget in the layout.
	 */
	virtual FVector2D ComputeEntryWidgetSize(const int32 GlobalIndex) { return FVector2D::ZeroVector; };

	/**
	 * Converts a global index into the actual item index.
	 * This is mostly useful for handling virtualized entries in an infinite list (see spiral layout strategy for example).
	 * Defaults to simply returning the given index as is sufficient for most basic layout strategies.
	 */
	virtual int32 GlobalIndexToDataIndex(const int32 GlobalIndex) const { return GlobalIndex; };
	
	/**
	 * Checks if the given global index is within the "visible window" of items.
	 * This is useful for determining if an item should be displayed or hidden.
	 */
	virtual bool ShouldBeVisible(const int32 GlobalIndex) const { return true; };
	
	/**
	 * Draws debug visuals for the layout strategy.
	 * This is useful for visualizing the layout in the editor or during development.
	 */
	virtual void DrawDebugVisuals(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FVector2D& Center) const {};
};
