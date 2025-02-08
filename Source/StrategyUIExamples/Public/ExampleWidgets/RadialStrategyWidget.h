#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidgetPool.h>

#include <Widgets/BaseStrategyWidget.h>

#include "RadialStrategyWidget.generated.h"

struct FRadialItemMaterialData;
class URadialLayoutStrategy;
class UBaseStrategyWidget;

// Delegate for when the radial pointer's rotation angle updates. Angle is passed in degrees [-180, 180].
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRadialPointerRotationUpdatedDelegate, float, Angle);

// Holds all data needed for animating a scroll from one angle to another.
USTRUCT(BlueprintType)
struct FRadialScrollAnimationData
{
	GENERATED_BODY()

public:
	// Whether the widget is currently animating a scroll
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|RuntimeAnimationData")
	bool bIsAnimating = false;

	// How long the current animation will take (in seconds, if bIsAnimating is true).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|RuntimeAnimationData")
	float Duration = 0.f;

	// How long the current animation has been running (in seconds, if bIsAnimating is true).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|RuntimeAnimationData")
	float ElapsedTime = 0.f;
	
	// The "base" angle at animation start (already unwound to [0..360] in wheel mode, or just stored as-is in spiral mode).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|RuntimeAnimationData")
	float StartAngle = 0.f;

	// The target angle (StartAngle + DeltaAngle) in degrees
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|RuntimeAnimationData")
	float EndAngle = 0.f;

	// The difference in degrees from StartAngle to EndAngle
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|RuntimeAnimationData")
	float DeltaAngle = 0.f;
};


/**
 * A container widget that arranges items in a radial layout.
 * Supports a rotating pointer that enables "scrolling" through items.
 */
UCLASS(Abstract, ClassGroup="StrategyUI")
class STRATEGYUIEXAMPLES_API URadialStrategyWidget : public UBaseStrategyWidget
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

	// ---------------------------------------------------------------------------------------------
	// UBaseStrategyWidget API Overrides
	// ---------------------------------------------------------------------------------------------
#pragma region UBaseStrategyWidget API Overrides
	virtual void Reset() override;

	virtual void UpdateEntryWidget(int32 InGlobalIndex) override;
	virtual void UpdateWidgets() override;

	virtual void SetItems_Internal_Implementation(const TArray<UObject*>& InItems) override;
#pragma endregion
	
	// ---------------------------------------------------------------------------------------------
	// URadialSpiralWidget API Base
	// ---------------------------------------------------------------------------------------------
#pragma region URadialSpiralWidget API
	/**
	 * Handles radial input from joystick/etc.
	 * @param Delta     2D directional input.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	virtual void HandleStickInput(const FVector2D& Delta);

	/**
	 * Handles radial input from mouse.
	 * @param InMouseScreenPos 2D mouse position.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	virtual void HandleMouseInput(const FVector2D& InMouseScreenPos);

	/** Resets the pointer angle, visible window, etc. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	virtual void ResetInput();

	/** Updates the stored focus index based on the LayoutStrategy's focused global index. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	virtual void UpdateFocusIndex();
	
	/** Adds a rotation delta equal to a single item to the current pointer angle. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	virtual void StepIndex(int32 Delta);

	/**
	 * Adds a rotation delta equal to a single item to the current pointer angle over an animation.
	 * @param Delta     The index delta.
	 * @param Duration  Animation duration (in seconds). 0 for an instant jump.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget|Animation")
	virtual void StepIndexAnimated(int32 Delta, const float Duration = 0.0f);

	/** Scroll instantly to the given data index. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	virtual void ScrollToItem(int32 DataIndex);

	/**
	 * Animate scrolling to a given data index.
	 * @param DataIndex  The index in the Items array to scroll to.
	 * @param Duration   Animation duration (in seconds).
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget|Animation")
	virtual void ScrollToItemAnimated(int32 DataIndex, float Duration);

	/** Re-center the pointer on the currently focused item. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget")
	virtual void ScrollToCenterOfFocusedWedge();

	/**
	 * Re-center the pointer on the currently focused item, over some duration.
	 * @param Duration   Duration (in seconds). 0 = instant.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|RadialStrategyWidget|Animation")
	virtual void ScrollToCenterOfFocusedWedgeAnimated(float Duration);
#pragma endregion

protected:
	//----------------------------------------------------------------------------------------------
	// UUserWidget & UWidget Overrides
	//----------------------------------------------------------------------------------------------
#pragma region UUserWidget & UWidget Overrides
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
#pragma endregion

	//----------------------------------------------------------------------------------------------
	// URadialStrategyWidget - Debug Drawing
	//----------------------------------------------------------------------------------------------
#pragma region URadialStrategyWidget - Debug Drawing
#if !UE_BUILD_SHIPPING
	/** Draws debug information for data items, positioned radially as if they were entry widgets */
	virtual void DrawItemDebugInfo(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
#endif
#pragma endregion

	//----------------------------------------------------------------------------------------------
	// URadialStrategyWidget - Events
	//----------------------------------------------------------------------------------------------
#pragma region URadialStrategyWidget - Events
	/** Broadcasts the current pointer angle when it changes (in degrees [-180..180]). */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|RadialStrategyWidget|Event")
	FRadialPointerRotationUpdatedDelegate OnPointerRotationUpdated;
#pragma endregion

	//----------------------------------------------------------------------------------------------
	// URadialStrategyWidget Properties - Editable
	//----------------------------------------------------------------------------------------------
#pragma region URadialStrategyWidget Properties - Editable
	/** Sensitivity for input rotation (higher = faster scrolling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|RadialStrategyWidget")
	float RotationSensitivity = 100.f;

	/** Gap (in degrees) between wedge slices used in the calculation of the radial wedge material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|RadialStrategyWidget")
	float DynamicWedgeGapSize = 1.f;
#pragma endregion

	//----------------------------------------------------------------------------------------------
	// URadialStrategyWidget Properties - Runtime
	//----------------------------------------------------------------------------------------------
#pragma region URadialStrategyWidget Properties - Runtime
	/**
	 * Current pointer angle in degrees; can exceed 360 or be negative.
	 * Used to figure out which item is "closest" in angle, or how many turns we've scrolled.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|Pointer", meta=(AllowPrivateAccess="true"))
	float CurrentPointerAngle = 0.f;

	/** Stores runtime scrolling animation parameters. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|RadialStrategyWidget|Animation", meta=(AllowPrivateAccess="true"))
	FRadialScrollAnimationData RuntimeScrollingAnimState;

	/** Flag set when at least one entry widget has valid geometry. */
	mutable bool bAreChildrenReady = false;
#pragma endregion

	//----------------------------------------------------------------------------------------------
	// URadialStrategyWidget Functions - Rotation Handling
	//----------------------------------------------------------------------------------------------
#pragma region URadialStrategyWidget Functions - Rotation Handling
	/** Updates the current pointer angle, triggers OnPointerRotationUpdated event delegate. */
	virtual void SetCurrentAngle(const float InNewAngle);

	/** Called by HandleInput to apply a direct rotation to CurrentAngle. */
	virtual void ApplyManualRotation(float DeltaDegrees);
	
	/** Adds degrees to CurrentAngle, triggers layout update. */
	virtual void AddRotation_Internal(float DeltaDegrees);

	/** Begin an animation from CurrentAngle to InTargetAngle over Duration. */
	virtual void BeginAngleAnimation(float InTargetAngle, float Duration);

	/** Called by StepIndexAnimated to determine the final duration based on the number of gap items crossed. */
	virtual float ScaleDurationByGapItems(const float FinalDuration) const;
#pragma endregion

	//----------------------------------------------------------------------------------------------
	// URadialStrategyWidget Functions - Radial Material
	//----------------------------------------------------------------------------------------------
#pragma region URadialStrategyWidget Functions - Radial Material
	/**
	 * Computes data for the entry widget to pass to a dynamic material.
	 * This data includes values normalized in [0..1] for straightforward material math.
	 */
	virtual void ConstructMaterialData(const UUserWidget* EntryWidget, int32 InGlobalIndex, FRadialItemMaterialData& OutMaterialData) const;

	/** Updates the material data on an entry widget. */
	virtual void SyncMaterialData(const int32 InGlobalIndex);
#pragma endregion
};
