#pragma once

#include <CoreMinimal.h>

#include "BaseLayoutStrategy.h"
#include "Widgets/RadialStrategyWidget.h" // @TODO Refactor out
#include "RadialLayoutStrategy.generated.h"


UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, meta=(DisableNativeTick), ClassGroup="RadialLayout")
class STRATEGYUI_API URadialLayoutStrategy : public UBaseLayoutStrategy
{
	GENERATED_BODY()

public:
#pragma region Editable Properties
	//----------------------------------------------------------------------------------------------
	// Editable Properties
	//----------------------------------------------------------------------------------------------
	/** Base radius for the radial layouts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RadialStrategy|Layout", meta=(ClampMin="0.0"))
	float BaseRadius = 400.f;

	/** Number of radial segments for layout */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RadialStrategy|Layout", meta=(ClampMin="1"))
	int32 RadialSegmentCount = 8;
	
	/**
	 * Maximum number of visible entries at once (centered around the focused item).
	 * If Items.Num() exceeds this value, we only display a subset "visible window" that scrolls as the pointer rotates through the items.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RadialStrategy|Entry", meta=(ClampMin="1"))
	int32 MaxVisibleEntries = 8;

	/**
	 * Number of entries to keep deactivated (hidden) outside the bounds of the visible window.
	 * This is useful for potentially adding animations to entries as they scroll in/out of view,
	 * or for preloading data for entries that may soon be visible.
	 *
	 * Set this as low as is necessary as it can lead to performance issues with many hidden widgets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RadialStrategy|Entry", meta=(ClampMin="0"), meta=(ClampMax="10"))
	int32 NumDeactivatedEntries = 2;
#pragma endregion Editable Properties

#pragma region Public API
	//----------------------------------------------------------------------------------------------
	// Public API
	//----------------------------------------------------------------------------------------------
	/**
	 * Allows the layout strategy to initialize any internal data
	 * based on the owning widget or item list, etc.
	 */
	virtual void InitializeStrategy(UBaseStrategyWidget* OwnerWidget) override;

	/**
	 * Validates the strategy properties and settings.
	 * This is called during widget compilation to ensure the strategy is set up correctly.
	 */
	virtual void ValidateStrategy(TArray<FText>& OutErrors) const override;

	/**
	 * Optionally ensure the working pointer angle is within a valid range for the layout strategy.
	 * For a wheel, this is typically [0..360]
	 * For an infinite spiral, this can be unbounded.
	 */
	virtual float SanitizeAngle(const float InAngle) const;

	/**
	 * Sets the number of items in the owner's data list.
	 */
	void SetNumItems(const int32 InNumItems);

	/**
	 * Sets the latest pointer angle in degrees.
	 * This angle is used to determine which item is "focused" or "closest" to the pointer.
	 */
	void SetPointerAngle(const float InAngle);

	/**
	 * Gets the latest pointer angle in degrees.
	 * This angle is used to determine which item is "focused" or "closest" to the pointer.
	 */
	virtual float GetPointerAngle() const { return LatestPointerAngle; }

	/**
	 * Computes the number of "gap segments" to add after the last item.
	 * This is useful for maintaining consistent spacing if we have fewer items than segments.
	 */
	virtual int32 UpdateGapSegments(const int32 TotalItems) PURE_VIRTUAL(URadialLayoutStrategy::ComputeGapSegments, return 0;);

	/**
	 * Gets the number of "gap segments" to add after the last item.
	 * This is useful for maintaining consistent spacing if we have fewer items than segments.
	 */
	int32 GetGapSegments() const { return GapPaddingSegments; }

	/**
	 * Angle difference between each item in the layout. Assumes equidistant segments.
	 */
	virtual float GetAngularSpacing() const { return AngularSpacing; }

	/**
	* Gets the global index representing the head of the "visible window" (for item num > MaxVisibleEntries).
	*/
	int32 GetVisibleStartIndex() const { return VisibleStartIndex; }

	/**
	* Gets The global index representing the tail of the "visible window" (for item num > MaxVisibleEntries).
	*/
	int32 GetVisibleEndIndex() const { return VisibleEndIndex; }
	

	/**
	 * Returns the set of desired global indices to display in the current visible window.
	 * Best to call UpdateVisibleWindow before this to ensure the window is up-to-date.
	 */
	virtual TSet<int32> ComputeDesiredIndices() override;

	/** 
	 * Calculates how many degrees lie between consecutive data items and caches it as AngularSpacing
	 * (e.g. 360 / SegmentCount in a simple wheel). Assumes equidistant segments.
	 */
	virtual void UpdateAngularSpacing();
	
	/**
	 * Finds the global index of the item that is "in focus" (closest to CurrentAngle).
	 */
	virtual int32 FindFocusedGlobalIndexByAngle() const PURE_VIRTUAL(URadialLayoutStrategy::FindFocusIndexByAngle, return INDEX_NONE;);

	/**
	 * Computes the angle for the entry at the given data index closest to the current angle.
	 * This is useful for calculating the position of the item in the layout.
	 */
	virtual float ComputeShortestUnboundAngleForDataIndex(const int32 DataIndex) const PURE_VIRTUAL(URadialLayoutStrategy::ComputeShortestUnboundAngleForDataIndex, return 0.f;);
	
	virtual int32 GlobalIndexToDataIndex(const int32 GlobalIndex) const override;

	/**
	 * Calculates the distance factor for the given global index.
	 * This is used to determine how close an item is to the pointer's depth in [0..1] (0 = far, 0.5 = at pointer, 1 = near).
	 */
	virtual float CalculateDistanceFactorForGlobalIndex(const int32 GlobalIndex) const PURE_VIRTUAL(URadialLayoutStrategy::CalculateDistanceFactorForGlobalIndex, return 0.f;);

	/**
	 * This is used to determine the layout's radial wedge angle for the item.
	 */
	virtual float CalculateItemAngleDegreesForGlobalIndex(const int32 GlobalIndex) const;

	/**
	 * This is used to determine the layout wedge radius for the item (how far from the center it should be positioned radially).
	 */
	virtual float CalculateRadiusForGlobalIndex(const int32 GlobalIndex) const;
	
	virtual bool ShouldBeVisible(const int32 GlobalIndex) const override;

	/**
	 * Returns the minimum radius for the layout.
	 * This is useful for determining the innermost radius of the layout in order to construct the wedge material data.
	 * Returns BaseRadius for a simple wheel layout, but can be overridden for more complex layouts (see spiral layout strategy).
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="RadialStrategy|Layout")
	virtual float GetMinRadius() const;

	/**
	 * Returns the maximum radius for the layout.
	 * This is useful for determining the outermost radius of the layout in order to construct the wedge material data.
	 * Returns BaseRadius for a simple wheel layout, but can be overridden for more complex layouts (see spiral layout strategy).
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="RadialStrategy|Layout")
	virtual float GetMaxRadius() const;
#pragma endregion Public API

	/**
	 * Draws BaseRadius circle, radial segments, and the pointer angle.
	 */
	virtual void DrawDebugVisuals(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FVector2D& Center) const override;

protected:
#pragma region Runtime Properties
	//----------------------------------------------------------------------------------------------
	// Runtime Properties
	//----------------------------------------------------------------------------------------------
	/** Number of data item objects provided to the layout */
	UPROPERTY(BlueprintReadOnly, Category = "RadialStrategy|Data")
	int32 NumItems = 0;

	/** Additional gap segments appended after the last item so we can maintain consistent spacing if we have fewer items than segments. */
	UPROPERTY(BlueprintReadOnly, Category = "RadialStrategy|Layout")
	int32 GapPaddingSegments = 0;

	/**
	* The global index representing the head of the "visible window" (for item num > MaxVisibleEntries).
	*/
	UPROPERTY(BlueprintReadOnly, Category="RadialStrategy|Layout")
	int32 VisibleStartIndex = INDEX_NONE;

	/**
	 * The global index representing the tail of the "visible window" (for item num > MaxVisibleEntries).
	 */
	UPROPERTY(BlueprintReadOnly, Category="RadialStrategy|Layout")
	int32 VisibleEndIndex = INDEX_NONE;

	/** Angle difference between each item in the layout. Assumes equidistant segments. */
	UPROPERTY(BlueprintReadOnly, Category = "RadialStrategy|Layout")
	float AngularSpacing = 0.f;

	/**
	 * The current angle of the pointer in degrees.
	 *
	 * This is the angle that determines which item is "focused" and how the layout is rotated.
	 */
	UPROPERTY(BlueprintReadOnly, Category="RadialStrategy|Input")
	float LatestPointerAngle = 0.f;
#pragma endregion Runtime Properties
};