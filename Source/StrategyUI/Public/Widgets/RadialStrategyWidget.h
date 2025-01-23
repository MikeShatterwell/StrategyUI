#pragma once

#include <CoreMinimal.h>

#include <Blueprint/UserWidgetPool.h>
#include <Blueprint/UserWidget.h>
#include <Components/CanvasPanel.h>

#include "BaseStrategyWidget.h"
#include "RadialStrategyWidget.generated.h"

struct FRadialItemMaterialData;
class URadialLayoutStrategy;
class UBaseStrategyWidget;

// Delegate for when a radial item gains focus.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRadialItemFocusedDelegate, int32, Index, UObject*, Item);

// Delegate for when the radial pointer's rotation angle updates.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRadialPointerRotationUpdatedDelegate, float, Angle);

// Delegate for when a radial item is clicked or selected.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRadialItemClickedDelegate, int32, Index, UObject*, Item);


// Holds all data needed for animating a scroll from one angle to another.
USTRUCT(BlueprintType)
struct FRadialScrollAnimationData
{
	GENERATED_BODY()

public:
	// Whether the widget is currently animating a scroll
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialStrategyWidget|RuntimeAnimationData")
	bool bIsAnimating = false;

	// How long the current animation will take (in seconds, if bIsAnimating is true).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialStrategyWidget|RuntimeAnimationData")
	float Duration = 0.f;

	// How long the current animation has been running (in seconds, if bIsAnimating is true).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialStrategyWidget|RuntimeAnimationData")
	float ElapsedTime = 0.f;
	
	// The "base" angle at animation start (already unwound to [0..360] in wheel mode, or just stored as-is in spiral mode).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialStrategyWidget|RuntimeAnimationData")
	float StartAngle = 0.f;

	// The target angle (StartAngle + DeltaAngle) in degrees
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialStrategyWidget|RuntimeAnimationData")
	float EndAngle = 0.f;

	// The difference in degrees from StartAngle to EndAngle
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RadialStrategyWidget|RuntimeAnimationData")
	float DeltaAngle = 0.f;
};


/**
 * A container widget that arranges items in a radial layout.
 * Supports a rotating pointer that enables "scrolling" through items.
 */
UCLASS(Abstract, ClassGroup="StrategyUI")
class STRATEGYUI_API URadialStrategyWidget : public UBaseStrategyWidget
{
	GENERATED_BODY()

public:
	explicit URadialStrategyWidget(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif


#pragma region Public API
	// ---------------------------------------------------------------------------------------------
	// BaseStrategyWidget API Overrides
	// ---------------------------------------------------------------------------------------------
	virtual void SetItems(const TArray<UObject*>& InItems) override;
	virtual void UpdateEntryWidget(int32 InGlobalIndex) override;
	virtual void PositionWidget(int32 GlobalIndex) override;
	
	// ---------------------------------------------------------------------------------------------
	// RadialSpiralWidget API
	// ---------------------------------------------------------------------------------------------
	/**
	 * Handles radial input from mouse/joystick/etc.
	 * @param Delta     2D directional input.
	 * @param DeltaTime Frame time, for smooth rotation if desired.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	void HandleInput(const FVector2D& Delta, float DeltaTime);

	/** Resets the pointer angle, visible window, etc. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	void ResetInput();
	void UpdateFocusIndex();

	/** Selects the currently-focused item (if any). */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	void SelectFocusedItem();
	
	/** Adds a rotation delta equal to a single item to the current pointer angle. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	void StepIndex(int32 Delta);

	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget|Animation")
	void StepIndexAnimated(int32 Delta, const float Duration = 0.0f);
	
	UFUNCTION(BlueprintCallable, Category = "StrategyUI|RadialStrategyWidget")
	void ScrollToDataIndex(int32 DataIndex);

	/**
	 * Animate the pointer angle until it aligns with a particular ItemIndex's wedge center.
	 * @param DataIndex  The index in the Items array to scroll to.
	 * @param Duration   How long (in seconds) to take for the animation. Use 0 for an instant jump.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget|Animation")
	void ScrollToItemAnimated(int32 DataIndex, float Duration);

	UFUNCTION(BlueprintCallable, Category = "StrategyUI|RadialStrategyWidget")
	void ScrollToAngle(float Angle);

	/** Re-center the pointer on the currently focused item. */
	UFUNCTION(BlueprintCallable, Category = "StrategyUI|RadialStrategyWidget")
	void ScrollToCenterOfFocusedWedge();

	/**
	 * Re-center the pointer on the currently focused item, over some duration.
	 * @param Duration   Duration (in seconds). 0 = instant.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget|Animation")
	void ScrollToCenterOfFocusedWedgeAnimated(float Duration);
#pragma endregion Public API

protected:
#pragma region UWidget Overrides
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;
#pragma endregion UWidget Overrides


#pragma region Debug Drawing
	/** Draws debug information for data items, positioned radially as if they were entry widgets */
	void DrawItemDebugInfo(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
#pragma endregion Debug Drawing


#pragma region Events
	/** Broadcasts when an item is focused (hovered) by the pointer. */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|RadialStrategyWidget|Event")
	FRadialItemFocusedDelegate OnItemFocused;

	/** Broadcasts the current pointer angle when it changes (in degrees [-180..180]). */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|RadialStrategyWidget|Event")
	FRadialPointerRotationUpdatedDelegate OnPointerRotationUpdated;

	/** Broadcasts when the focused item is selected via SelectFocusedItem(). */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|RadialStrategyWidget|Event")
	FRadialItemClickedDelegate OnItemSelected;
#pragma endregion Events


#pragma region Protected Properties
	/** Sensitivity for input rotation (higher = faster scrolling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|RadialStrategyWidget")
	float RotationSensitivity = 100.f;
	
	/**
	 * Current pointer angle in degrees; can exceed 360 or be negative.
	 * Used to figure out which item is "closest" in angle, or how many turns we've scrolled.
	 */
	UPROPERTY(BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|Pointer", meta=(AllowPrivateAccess="true"))
	float CurrentPointerAngle = 0.f;
	void SetCurrentAngle(const float InNewAngle);
	
	/** Currently-focused item index (in the entire data Items array). INDEX_NONE (-1) if invalid. */
	UPROPERTY(BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|Pointer", meta=(AllowPrivateAccess="true"))
	int32 DataFocusedIndex = INDEX_NONE;
	void SetFocusedIndex(const int32 InFocusedIndex);
	
	/** Used when in spiral mode to store the item index without being constrained by data indexes. Can be negative or exceed the number of Items */
	UPROPERTY(BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|Pointer", meta=(AllowPrivateAccess="true"))
	int32 GlobalFocusIndex = INDEX_NONE;

	/** Stores all scroll animation parameters. */
	UPROPERTY(BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|Animation", meta=(AllowPrivateAccess="true"))
	FRadialScrollAnimationData RuntimeScrollingAnimState;
#pragma endregion Protected Properties

	
#pragma region Protected Helper Functions
	/***********************************************************************************************
	 * Rotation Handling
	 ***********************************************************************************************/
	/** Called by HandleInput to apply a direct rotation to CurrentAngle. */
	void ApplyManualRotation(float DeltaDegrees);
	
	/** Adds degrees to CurrentAngle, triggers layout update. */
	void AddRotation_Internal(float DeltaDegrees);

	/** Begin an animation from CurrentAngle to InTargetAngle over Duration. */
	void BeginAngleAnimation(float InTargetAngle, float Duration);
	

	/***********************************************************************************************
	 * Item/Widget Handling
	 ***********************************************************************************************/
	/** Ensure that only the items in [VisibleStartIndex..] are displayed, reusing or releasing widgets as needed. */
	//void UpdateVisibleWidgets();

	/** Computes data for the entry widget to pass to a dynamic material.
	 *
	 * This data includes values normalized in [0..1] for straightforward material math.
	 *
	 * These can be used to render a wedge for the item entry in a radial layout.
	 */
	void ConstructMaterialData(const UUserWidget* EntryWidget, int32 InGlobalIndex, const bool bIsFocused, FRadialItemMaterialData& OutMaterialData) const;

	void SyncMaterialData() const;
#pragma endregion Protected Helper Functions

	
private:
#pragma region Private Data Members
	/** Cached geometry size for consistent layout math. */
	mutable FVector2D CachedSize = FVector2D::ZeroVector;
	mutable FVector2D Center = FVector2D::ZeroVector;
#pragma endregion Private Data Members
};
