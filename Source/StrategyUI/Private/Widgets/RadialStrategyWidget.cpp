#include "Widgets/RadialStrategyWidget.h"

#include <Components/CanvasPanelSlot.h>
#include <Blueprint/WidgetTree.h>
#include <Editor/WidgetCompilerLog.h>
#include <Fonts/SlateFontInfo.h>
#include <Styling/CoreStyle.h>

#include "Interfaces/IRadialItemEntry.h"
#include "Interfaces/IStrategyEntryBase.h"
#include "Strategies/BaseLayoutStrategy.h"
#include "Utils/LogStrategyUI.h"
#include "Strategies/RadialLayoutStrategy.h"
#include "Utils/StrategyUIGameplayTags.h"

URadialStrategyWidget::URadialStrategyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void URadialStrategyWidget::ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	if (!EntryWidgetClass)
	{
		CompileLog.Error(FText::FromString(TEXT("Please assign an EntryWidgetClass in the details panel!")));
	}
	else
	{
		const UClass* RadialItemEntryInterface = URadialItemEntry::StaticClass();
		if (!EntryWidgetClass->ImplementsInterface(RadialItemEntryInterface))
		{
			CompileLog.Error(FText::FromString(TEXT("EntryWidgetClass must implement IRadialItemEntry interface!")));
		}
	}
	
	const URadialLayoutStrategy* RadialLayout = Cast<URadialLayoutStrategy>(LayoutStrategy);
	if (!RadialLayout)
	{
		CompileLog.Error(FText::FromString(TEXT("Please assign a URadialLayoutStrategy in the details panel!")));
	}
}

void URadialStrategyWidget::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#pragma region Public API
void URadialStrategyWidget::SetItems(const TArray<UObject*>& InItems)
{
	Super::SetItems(InItems);

	ResetInput();
}

void URadialStrategyWidget::HandleStickInput(const FVector2D& Delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return; // No world, skip
	}
	
	if (Delta.IsNearlyZero())
	{
		return; // No movement, skip
	}

	if (RuntimeScrollingAnimState.bIsAnimating)
	{
		RuntimeScrollingAnimState.bIsAnimating = false; // Override any ongoing animations
	}

	// Normalize the input delta to avoid device sensitivity issues
	const FVector2D NormalizedDelta = Delta.GetSafeNormal();

	// Compute the pointer's current position in 2D space
	const float PointerAngleRad = FMath::DegreesToRadians(CurrentPointerAngle);
	const FVector2D PointerPosition(
		FMath::Cos(PointerAngleRad),
		FMath::Sin(PointerAngleRad)
	);

	// Project the input onto the tangent of the pointer's circular path
	// Tangential movement determines rotation direction and speed
	const float TangentialDelta = FVector2D::CrossProduct(PointerPosition, NormalizedDelta);

	// Scale by a sensitivity factor and DeltaTime for smooth motion
	const float RotationDeltaDegrees = TangentialDelta * RotationSensitivity * World->GetDeltaSeconds();

	// Apply the calculated rotation delta
	ApplyManualRotation(RotationDeltaDegrees);
}

void URadialStrategyWidget::HandleMouseInput(const FVector2D& InMouseScreenPos)
{
	// 1) Convert screen to local coords
	const FVector2D& LocalPos = GetCachedGeometry().AbsoluteToLocal(InMouseScreenPos);
	const FVector2D& ToMouse  = LocalPos - Center;
	if (ToMouse.IsNearlyZero())
	{
		return; // too close to center
	}
	
	auto WrapAngleToPlusMinus180 = [](const float Angle) -> float
	{
		// Wrap raw angle to [–180..180].
		float Wrapped = FMath::Fmod(Angle, 360.f);
		if (Wrapped > 180.f)
		{
			Wrapped -= 360.f;
		}
		else if (Wrapped < -180.f)
		{
			Wrapped += 360.f;
		}
		return Wrapped;
	};

	// 2) Cancel any animation in progress
	if (RuntimeScrollingAnimState.bIsAnimating)
	{
		RuntimeScrollingAnimState.bIsAnimating = false;
	}

	// 3) Compute the new “atan2” angle in [–180..180]
	const float NewAtan2Angle = FMath::RadiansToDegrees(FMath::Atan2(ToMouse.Y, ToMouse.X));

	// 4) Derive "last frame's angle" in [–180..180] by wrapping our unbounded pointer angle
	const float CurrentAngleWrapped = WrapAngleToPlusMinus180(CurrentPointerAngle);

	// 5) Compute the actual delta in [–180..180]
	const float DeltaAngle = FMath::FindDeltaAngleDegrees(CurrentAngleWrapped, NewAtan2Angle);

	// 6) Accumulate into our unbounded pointer angle
	const float WorkingAngle = CurrentPointerAngle + DeltaAngle;
	SetCurrentAngle(WorkingAngle);
}

void URadialStrategyWidget::StepIndex(const int32 Delta)
{
	StepIndexAnimated(Delta);
}

float URadialStrategyWidget::ScaleDurationByGapItems(const float InitialDuration) const
{
	const int32 NumGapItems = GetLayoutStrategyChecked<URadialLayoutStrategy>().GetGapSegments();
	const float FinalDuration = InitialDuration * (1.f + NumGapItems);
	return FinalDuration;
}

void URadialStrategyWidget::StepIndexAnimated(const int32 Delta, const float Duration)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (GetItemCount() <= 0)
	{
		return;
	}

	float FinalDuration = (Duration <= 0.f) ? World->GetDeltaSeconds() * 2.f : Duration;
	bool bCrossedGap = false;
	
	const int32 CurrentIndex = FocusedDataIndex;
	int32 TargetDataIndex = (CurrentIndex + Delta) % GetItemCount();
	if (TargetDataIndex < 0)
	{
		// Stepping back from 0 to the last index
		TargetDataIndex = GetItemCount() - 1;
		bCrossedGap = true;
	}
	else if (CurrentIndex == GetItemCount() - 1 && TargetDataIndex >= 0)
	{
		bCrossedGap = true;
	}

	if (bCrossedGap)
	{
		FinalDuration = ScaleDurationByGapItems(World->GetDeltaSeconds());
	}

	ScrollToItemAnimated(TargetDataIndex, FinalDuration);
}

void URadialStrategyWidget::ScrollToItem(const int32 DataIndex)
{
	SetCurrentAngle(DataIndex * GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing());
}


void URadialStrategyWidget::ResetInput()
{
	// @TODO: Consider a better cleanup and init flow in case new data comes in at runtime and we need to reset the layout

	RuntimeScrollingAnimState.bIsAnimating = false;
	SetCurrentAngle(0.0f);
	UpdateFocusedGlobalIndex(INDEX_NONE);
}

void URadialStrategyWidget::UpdateFocusIndex()
{
	const int32 NewGlobalFocusIndex = GetLayoutStrategyChecked<URadialLayoutStrategy>().FindFocusedGlobalIndexByAngle();
	UpdateFocusedGlobalIndex(NewGlobalFocusIndex);
}

void URadialStrategyWidget::ScrollToItemAnimated(const int32 DataIndex, const float Duration)
{
	if (!Items.IsValidIndex(DataIndex))
	{
		return;
	}

	const float TargetAngle = GetLayoutStrategyChecked<URadialLayoutStrategy>().ComputeShortestUnboundAngleForDataIndex(DataIndex);
	BeginAngleAnimation(TargetAngle, Duration);
}

void URadialStrategyWidget::ScrollToCenterOfFocusedWedge()
{
	if (FocusedDataIndex == INDEX_NONE || GetItemCount() == 0)
	{
		return;
	}

	ScrollToCenterOfFocusedWedgeAnimated(0.f);
}

void URadialStrategyWidget::ScrollToCenterOfFocusedWedgeAnimated(const float Duration)
{
	if (FocusedDataIndex == INDEX_NONE || GetItemCount() == 0)
	{
		return;
	}

	const float TargetAngle = FocusedDataIndex * GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing();
	BeginAngleAnimation(TargetAngle, Duration);
}

#pragma endregion Public API


#pragma region UWidget Overrides
void URadialStrategyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	Super::NativeTick(MyGeometry, InDeltaTime);

	// If we have an animation in progress, advance it
	if (RuntimeScrollingAnimState.bIsAnimating)
	{
		RuntimeScrollingAnimState.ElapsedTime += InDeltaTime;
		float Alpha = (RuntimeScrollingAnimState.Duration > 0.f)
			? (RuntimeScrollingAnimState.ElapsedTime / RuntimeScrollingAnimState.Duration)
			: 1.f;
		
		if (Alpha >= 1.f)
		{
			// Finished
			Alpha = 1.f;
			RuntimeScrollingAnimState.bIsAnimating = false;
		}

		// We can swap to e.g. InterpEaseInOut or InterpSinInOut easily.
		const float AnimAngle = FMath::Lerp(RuntimeScrollingAnimState.StartAngle, RuntimeScrollingAnimState.EndAngle, Alpha);
		SetCurrentAngle(AnimAngle);
	}

	GetLayoutStrategyChecked<URadialLayoutStrategy>().SetPointerAngle(CurrentPointerAngle);
	
	UpdateFocusIndex();
	UpdateVisibleWidgets();
}

int32 URadialStrategyWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	const int32 MaxLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled
	);

	if (CachedSize != AllottedGeometry.GetLocalSize())
	{
		CachedSize = AllottedGeometry.GetLocalSize();
		Center     = CachedSize * 0.5f;
	}

	if (!bEnableDebugDraw || !LayoutStrategy)
	{
		return MaxLayer;
	}

#if !UE_BUILD_SHIPPING
	DrawItemDebugInfo(AllottedGeometry, OutDrawElements, LayerId);
#endif
	
	return MaxLayer;
}
#pragma endregion UWidget Overrides



#pragma region Debug Drawing
#if !UE_BUILD_SHIPPING
void URadialStrategyWidget::DrawItemDebugInfo(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, const int32 LayerId) const
{
	// How many data items we want to see around the visible window
	const int32 NumDeactivatedEntries = GetLayoutStrategyChecked<URadialLayoutStrategy>().NumDeactivatedEntries;

	// @TODO: Clean this up
	TSet<int32> DesiredIndices = GetLayoutStrategyChecked<URadialLayoutStrategy>().ComputeDesiredGlobalIndices();
	
	const int32 VisibleStartIndex = GetLayoutStrategyChecked<URadialLayoutStrategy>().GetVisibleStartIndex();
	const int32 VisibleEndIndex = GetLayoutStrategyChecked<URadialLayoutStrategy>().GetVisibleEndIndex();
	const int32 DebugStart = VisibleStartIndex - NumDeactivatedEntries;
	const int32 DebugEnd   = VisibleEndIndex + NumDeactivatedEntries;

	for (int32 GlobalIndex = DebugStart; GlobalIndex <= DebugEnd; ++GlobalIndex)
	{
		// Check if this global index is actually in the "visible" window
		const bool bIsVisibleGlobal = (GlobalIndex >= VisibleStartIndex && GlobalIndex <= VisibleEndIndex);
			
		// Convert global index -> data index
		const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);

		// Is the data index valid in the array?
		const bool bValidData = Items.IsValidIndex(DataIndex);

		// Focus check
		const bool bIsFocused = (DataIndex == FocusedDataIndex && DataIndex != INDEX_NONE);
			
		// Draw debug item text
		{
			// Compute angles and positions
			const float ItemAngleDeg = GetLayoutStrategyChecked<URadialLayoutStrategy>().CalculateItemAngleDegreesForGlobalIndex(GlobalIndex);
			const float OffsetAngle  = ItemAngleDeg - CurrentPointerAngle;
			const float UnwoundAngle = FMath::UnwindDegrees(OffsetAngle);
			
			const FVector2D LocalPos = GetLayoutStrategyChecked().GetItemPosition(GlobalIndex);
			const float Radius = GetLayoutStrategyChecked<URadialLayoutStrategy>().CalculateRadiusForGlobalIndex(GlobalIndex);

			FString DebugString = FString::Printf(
				TEXT("\nG=%d | D=%d\nAng=%.1f\nOff=%.1f\nRadius=%.1f"),
				GlobalIndex, 
				DataIndex, 
				ItemAngleDeg, 
				UnwoundAngle,
				Radius
			);

			if (bIsFocused)
			{
				DebugString += TEXT("\n[Focused]");
			}
			if (!bIsVisibleGlobal)
			{
				DebugString += TEXT("\n[Hidden]");
			}
			if (!bValidData)
			{
				DebugString += TEXT("\n[Invalid Item]");
			}
			if (DataIndex == INDEX_NONE)
			{
				DebugString += TEXT("\n[No Data - In Gap]");
			}

			const FVector2D ScreenPos = Center + LocalPos;
			FSlateLayoutTransform LayoutXform(1.f, ScreenPos);

			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(LayoutXform),
				FText::FromString(DebugString),
				FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10),
				ESlateDrawEffect::None,
				FLinearColor::White
			);
		}
	}
}
#endif
#pragma endregion Debug Drawing


#pragma region Protected Helper Functions
void URadialStrategyWidget::AddRotation_Internal(const float DeltaDegrees)
{
	if (FMath::IsNearlyZero(DeltaDegrees))
	{
		return;
	}

	SetCurrentAngle(CurrentPointerAngle += DeltaDegrees);
}

void URadialStrategyWidget::SetCurrentAngle(const float InNewAngle)
{
	CurrentPointerAngle = InNewAngle;

	GetLayoutStrategyChecked<URadialLayoutStrategy>().SetPointerAngle(CurrentPointerAngle);

	// Broadcast the new angle in a [-180, 180] degree range for convenience in BP
	OnPointerRotationUpdated.Broadcast(FMath::UnwindDegrees(CurrentPointerAngle));
}

void URadialStrategyWidget::ApplyManualRotation(const float DeltaDegrees)
{
	if (!FMath::IsNearlyZero(DeltaDegrees))
	{
		SetCurrentAngle(CurrentPointerAngle + DeltaDegrees);
	}
}

void URadialStrategyWidget::BeginAngleAnimation(const float InTargetAngle, const float Duration)
{
	if (Duration <= 0.f)
	{
		// Instant
		RuntimeScrollingAnimState.bIsAnimating = false;
		SetCurrentAngle(InTargetAngle);
	}
	else
	{
		RuntimeScrollingAnimState.bIsAnimating = true;
		RuntimeScrollingAnimState.Duration     = Duration;
		RuntimeScrollingAnimState.ElapsedTime  = 0.f;
		RuntimeScrollingAnimState.StartAngle   = CurrentPointerAngle;
		RuntimeScrollingAnimState.EndAngle     = InTargetAngle;
		RuntimeScrollingAnimState.DeltaAngle   = (InTargetAngle - CurrentPointerAngle);
	}
}

void URadialStrategyWidget::SyncMaterialData(const int32 InGlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	UUserWidget* Widget = AcquireEntryWidget(InGlobalIndex);
	
	const int32* GlobalIndexKey = IndexToWidgetMap.FindKey(Widget);
	if (GlobalIndexKey == nullptr || !Widget)
	{
		UE_LOG(LogStrategyUI, Error, TEXT("%hs: Invalid widget or index"), __FUNCTION__);
		return;
	}

	const int32 GlobalIndex = *GlobalIndexKey;
	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
	const bool bIsFocused = DataIndex == FocusedDataIndex;
	const FGameplayTag& ItemState = IndexToStateMap.FindChecked(GlobalIndex);

	if (Widget->Implements<URadialItemEntry>())
	{
		const FGameplayTag& ActiveState = StrategyUIGameplayTags::StrategyUI_EntryState_Active;
		if (ItemState == ActiveState)
		{
			FRadialItemMaterialData MaterialData;
			ConstructMaterialData(Widget, GlobalIndex, bIsFocused, MaterialData);
			IRadialItemEntry::Execute_BP_SetRadialItemMaterialData(Widget, MaterialData);
		}
	}
}

void URadialStrategyWidget::PositionWidget(const int32 GlobalIndex)
{
	Super::PositionWidget(GlobalIndex);
	
	UUserWidget* Widget = AcquireEntryWidget(GlobalIndex);
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	check(CanvasSlot);

	const FVector2D& LocalPos = GetLayoutStrategyChecked().GetItemPosition(GlobalIndex);

	//CanvasSlot->SetPosition(Center + LocalPos); // Radial entries are positioned at the center of the radius...
	// ... Or use render transform to move the widget into positions:
	
	CanvasSlot->SetPosition(Center);
	Widget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Widget->SetRenderTranslation(LocalPos);
}

void URadialStrategyWidget::ConstructMaterialData(const UUserWidget* EntryWidget, const int32 InGlobalIndex, const bool bIsFocused, FRadialItemMaterialData& OutMaterialData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	const float ItemAngleDeg = GetLayoutStrategyChecked<URadialLayoutStrategy>()
		.CalculateItemAngleDegreesForGlobalIndex(InGlobalIndex);
	
	// 1) Figure out angles (in normalized 0..1). E.g.:
	// Subtract half a wedge from that final angle
	const float RawStartDeg = ItemAngleDeg - (GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing() * 0.5f);
	
	// A helper to clamp angle into [0..360)
	auto ClampAngle0To360 = [](float A) {
		A = FMath::Fmod(A, 360.f);
		if (A < 0.f) A += 360.f;
		return A;
	};
	const float StartDeg = ClampAngle0To360(RawStartDeg);

	const float Gap = DynamicWedgeGapSize; // small gap between wedges
	const float HalfGap = Gap * 0.5f;

	const float GappedStartDeg = StartDeg + HalfGap;
	const float WedgeWidthDeg = GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing() - Gap;

	// 2) We pass "AngleOffset" and "WedgeWidth" to the material
	const float AngleOffsetN = GappedStartDeg / 360.f;
	const float WedgeWidthN  = WedgeWidthDeg / 360.f;

	// 3) Spiral center -> local UV 
	const FVector2D SpiralCenterAbs     = GetCachedGeometry().LocalToAbsolute(Center);
	const FGeometry EntryGeo              = EntryWidget->GetCachedGeometry();
	const FVector2D SpiralCenterLocal   = EntryGeo.AbsoluteToLocal(SpiralCenterAbs);
	const FVector2D EntrySize           = EntryGeo.GetLocalSize();

	float UVCenterX = 0.5f;
	float UVCenterY = 0.5f;
	if (EntrySize.X > KINDA_SMALL_NUMBER && EntrySize.Y > KINDA_SMALL_NUMBER)
	{
		UVCenterX = SpiralCenterLocal.X / EntrySize.X;
		UVCenterY = SpiralCenterLocal.Y / EntrySize.Y;
	}

	// 4) DistanceFactor used for the radius calculation
	const float DistanceFactor = GetLayoutStrategyChecked<URadialLayoutStrategy>().CalculateDistanceFactorForGlobalIndex(InGlobalIndex);
	
	// 5) Calculate the inner/outer radius
	const float MinRadius = GetLayoutStrategyChecked<URadialLayoutStrategy>().GetMinRadius(); // in px
	const float MaxRadius = GetLayoutStrategyChecked<URadialLayoutStrategy>().GetMaxRadius(); // in px

	float HalfEntryDim = FMath::Min(EntrySize.X, EntrySize.Y);// * 0.5f;

	if (HalfEntryDim < KINDA_SMALL_NUMBER)
	{
		HalfEntryDim = 1.f;
	}
	
	const float SpiralMinRadiusN = MinRadius / HalfEntryDim;
	const float SpiralMaxRadiusN = MaxRadius / HalfEntryDim;
	
	// 6) Fill the struct with the final data
	FRadialItemMaterialData MatData;
	MatData.UVCenterX          = UVCenterX;
	MatData.UVCenterY          = UVCenterY;
	MatData.WedgeWidth         = WedgeWidthN;
	MatData.AngleOffset        = AngleOffsetN;
	MatData.SpiralMinRadius    = SpiralMinRadiusN;
	MatData.SpiralMaxRadius    = SpiralMaxRadiusN;
	MatData.DistanceFactor     = DistanceFactor;
	MatData.bIsFocused         = bIsFocused;

	OutMaterialData = MatData;
}

void URadialStrategyWidget::UpdateEntryWidget(const int32 InGlobalIndex)
{
	Super::UpdateEntryWidget(InGlobalIndex);
	SyncMaterialData(InGlobalIndex);
}
#pragma endregion Protected Helper Functions
