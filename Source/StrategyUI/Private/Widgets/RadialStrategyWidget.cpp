#include "Widgets/RadialStrategyWidget.h"

#include <Components/CanvasPanelSlot.h>
#include <Blueprint/WidgetTree.h>
#include <Editor/WidgetCompilerLog.h>
#include <Fonts/SlateFontInfo.h>
#include <Styling/CoreStyle.h>

#include "DebugRadialItem.h"
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
}

void URadialStrategyWidget::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#pragma region Public API
void URadialStrategyWidget::SetItems(const TArray<UObject*>& InItems)
{
	Items = InItems;

	ResetInput();
	GetLayoutStrategyChecked().InitializeStrategy(this);
}

void URadialStrategyWidget::HandleInput(const FVector2D& Delta, const float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	if (Delta.IsZero())
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

	// Scale by a sensitivity factor (optional) and DeltaTime for smooth motion
	const float RotationDeltaDegrees = TangentialDelta * RotationSensitivity * DeltaTime;

	// Apply the calculated rotation delta
	ApplyManualRotation(RotationDeltaDegrees);
}

void URadialStrategyWidget::StepIndex(const int32 Delta)
{
	StepIndexAnimated(Delta);
}

void URadialStrategyWidget::StepIndexAnimated(const int32 Delta, const float Duration)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (Items.Num() <= 0)
	{
		return;
	}

	if (!LayoutStrategy)
	{
		return;
	}

	float FinalDuration = (Duration <= 0.f) ? World->GetDeltaSeconds() : Duration;
	
	const int32 CurrentIndex = DataFocusedIndex;
	int32 TargetDataIndex = (CurrentIndex + Delta) % Items.Num();
	if (TargetDataIndex < 0)
	{
		// Stepping back from 0 to the last index in Spiral mode
		TargetDataIndex = Items.Num() - 1;
	}

	// The following logic improves the scrolling UX by allowing the user to interrupt the gap crossing
	// animation with a second consecutive input in the same direction.

	// Are we “currently crossing” the gap from an INDEX_NONE state?
	const bool bCurrentlyCrossingGap = (CurrentIndex == INDEX_NONE);

	// Check if crossing from 0 -> N-1 or from N-1 -> 0
	const bool bForward = (Delta > 0);
	const bool bCrossedGap = 
		   (bForward && TargetDataIndex < CurrentIndex)    // e.g. going forward from last index back to 0
		|| (!bForward && TargetDataIndex > CurrentIndex)   // e.g. going backward from 0 to last index
		|| bCurrentlyCrossingGap;                          // or from the initial gap state

	static bool bLastCrossDirection = false;
	static bool bCrossedGapLastTime = false;

	if (bCrossedGap)
	{
		// Check if we just *previously* crossed the gap in the same direction
		const bool bSameDirectionAsBefore = (bForward == bLastCrossDirection);

		if (bCrossedGapLastTime && bSameDirectionAsBefore)
		{
			// This is the second consecutive cross in the same direction.
			// Skip or drastically reduce the gap penalty. For example:
			UE_LOG(LogStrategyUI, Verbose, TEXT("Skipping gap on second consecutive cross"));

			// We could skip entirely:
			//     /* do nothing special */ 
			// Or we could partially reduce:
			FinalDuration *= 0.25f; // e.g. only 25% of normal
		}
		else
		{
			// Original logic: scale by gap segments
			FinalDuration *= GetLayoutStrategyChecked<URadialLayoutStrategy>().GetGapSegments();
		}

		// Record that we definitely crossed a gap this time
		bCrossedGapLastTime  = true;
		bLastCrossDirection  = bForward;
	}
	else
	{
		// Did not cross a gap this time
		bCrossedGapLastTime = false;
	}

	if (bCurrentlyCrossingGap)
	{
		// Your existing logic that subtracts out “last gap” time if we’re
		// currently crossing from an initial state, etc.
		static float LastGapDuration = 0.f;
		LastGapDuration = FinalDuration; // keep track if you want more nuance
		FinalDuration -= LastGapDuration;
		LastGapDuration = 0.f;
	}

	ScrollToItemAnimated(TargetDataIndex, FinalDuration);
}


void URadialStrategyWidget::ResetInput()
{
	// @TODO: Consider a better cleanup and init flow in case new data comes in at runtime and we need to reset the layout

	RuntimeScrollingAnimState.bIsAnimating = false;
	SetCurrentAngle(0.0f);
	SetFocusedIndex(INDEX_NONE);
	
	GlobalFocusIndex = 0;
}

void URadialStrategyWidget::UpdateLayout()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	// @TODO: Remove the bound widget canvas panel and add one to the widget tree in code
	if (!CanvasPanel)
	{
		return;
	}

	if (!WidgetTree)
	{
		return;
	}

	if (!LayoutStrategy)
	{
		return;
	}

	ArrangeItems();
}

void URadialStrategyWidget::SelectFocusedItem()
{
	if (DataFocusedIndex != INDEX_NONE)
	{
		OnItemSelected.Broadcast(DataFocusedIndex, Items[DataFocusedIndex]);
	}
}


void URadialStrategyWidget::ScrollToDataIndex(const int32 DataIndex)
{
	if (Items.IsValidIndex(DataIndex))
	{
		const float TargetAngle = DataIndex * GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing();
		SetCurrentAngle(TargetAngle);
		SetFocusedIndex(DataIndex);
	}
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

void URadialStrategyWidget::ScrollToAngle(const float Angle)
{
	SetCurrentAngle(Angle);

	const int32 NewIndex = GetLayoutStrategyChecked<URadialLayoutStrategy>().FindFocusedGlobalIndexByAngle();
	SetFocusedIndex(NewIndex);
}

void URadialStrategyWidget::ScrollToCenterOfFocusedWedge()
{
	if (DataFocusedIndex == INDEX_NONE || Items.Num() == 0)
	{
		return;
	}

	ScrollToCenterOfFocusedWedgeAnimated(0.f);
}

void URadialStrategyWidget::ScrollToCenterOfFocusedWedgeAnimated(const float Duration)
{
	if (DataFocusedIndex == INDEX_NONE || Items.Num() == 0)
	{
		return;
	}

	const float TargetAngle = DataFocusedIndex * GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing();
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
	UpdateLayout();


	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	TimerManager.SetTimerForNextTick([this]()
	{
		// Defer updating material data to the next tick
		// This fixes a tricky bug where, when a pooled widget is reused, it may have stale material data for a frame
		// which appears as a flicker
		SyncMaterialData();
	});
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
	
	DrawItemDebugInfo(AllottedGeometry, OutDrawElements, LayerId);
	
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
	TSet<int32> DesiredIndices = GetLayoutStrategyChecked<URadialLayoutStrategy>().ComputeDesiredIndices();
	
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
		if (DataIndex == INDEX_NONE) // e.g. if Items.Num() == 0
		{
			continue;
		}

		// Is the data index valid in the array?
		const bool bValidData = Items.IsValidIndex(DataIndex);

		// Focus check
		const bool bIsFocused = (DataIndex == DataFocusedIndex);
			
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

void URadialStrategyWidget::SetFocusedIndex(const int32 InFocusedIndex)
{
	DataFocusedIndex = InFocusedIndex;
	if (DataFocusedIndex != INDEX_NONE && Items.IsValidIndex(DataFocusedIndex))
	{
		OnItemFocused.Broadcast(DataFocusedIndex, Items[DataFocusedIndex]);
	}
	else
	{
		OnItemFocused.Broadcast(INDEX_NONE, nullptr);
	}
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

void URadialStrategyWidget::ArrangeItems()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (Items.Num() == 0)
	{
		// Create debug data
		TArray<UObject*> DebugItems;
		for (int32 i = 0; i < DebugItemCount ; ++i)
		{
			UDebugRadialItem* DebugItem = NewObject<UDebugRadialItem>(this);
			DebugItem->DebugLabel = FString::Printf(TEXT("Item %d"), i);
			DebugItem->Id = i;
			DebugItems.Add(DebugItem);
		}

		// Use the debug data
		SetItems(DebugItems);
		SetFocusedIndex(INDEX_NONE);
		return;
	}
	
	const int32 NewGlobalFocusIndex = GetLayoutStrategyChecked<URadialLayoutStrategy>().FindFocusedGlobalIndexByAngle();
	const int32 NewDataFocusIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(NewGlobalFocusIndex);

	if (NewGlobalFocusIndex != GlobalFocusIndex)
	{
		GlobalFocusIndex = NewGlobalFocusIndex;
	}
	
	if (NewDataFocusIndex != DataFocusedIndex)
	{
		SetFocusedIndex(NewDataFocusIndex);
	}

	UpdateVisibleWidgets();
}

void URadialStrategyWidget::SyncMaterialData() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	for (UUserWidget* Widget : EntryWidgetPool.GetActiveWidgets())
	{
		const int32* GlobalIndexKey = IndexToWidgetMap.FindKey(Widget);
		if (GlobalIndexKey == nullptr || !Widget)
		{
			UE_LOG(LogStrategyUI, Error, TEXT("%hs: Invalid widget or index"), __FUNCTION__);
			continue;
		}

		const int32 GlobalIndex = *GlobalIndexKey;
		const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(GlobalIndex);
		const bool bIsFocused = DataIndex == DataFocusedIndex;
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
}

void URadialStrategyWidget::UpdateVisibleWidgets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	// Gather all the indices we want to keep.
	TSet<int32> DesiredIndices = GetLayoutStrategyChecked().ComputeDesiredIndices();

	// 1) Release any old widgets that are no longer needed (scrolled out of view)
	ReleaseUndesiredWidgets(DesiredIndices);
	
	// 2) Create or update each desired widget
	for (const int32 GlobalIndex : DesiredIndices)
	{
		UUserWidget* Widget = AcquireEntryWidget(GlobalIndex);

		// Let the widget update itself
		UpdateEntryWidget(Widget, GlobalIndex);

		// Set common slot properties
		const FVector2D& EntrySize = GetLayoutStrategyChecked().ComputeEntryWidgetSize(GlobalIndex);
		const FVector2D& LocalPos = GetLayoutStrategyChecked().GetItemPosition(GlobalIndex);
		
		// Make sure it's actually on the Canvas and prepare to update the slot properties
		if (Widget->GetParent() != CanvasPanel)
		{
			CanvasPanel->AddChildToCanvas(Widget);
		}
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
		ensure(CanvasSlot);
		
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetSize(EntrySize);
		CanvasSlot->SetZOrder(0);
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(Center); // Entries are positioned at the center of the radius...
		// ... And use render transform to move the widget into positions
		Widget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Widget->SetRenderTranslation(LocalPos);
		
		const bool bShouldBeVisible = GetLayoutStrategyChecked().ShouldBeVisible(GlobalIndex);

		// Grab old & new states
		const FGameplayTag& OldState = IndexToStateMap.FindRef(GlobalIndex);
		const FGameplayTag& NewState = bShouldBeVisible ?	StrategyUIGameplayTags::StrategyUI_EntryState_Active : StrategyUIGameplayTags::StrategyUI_EntryState_Deactivated;

		// Check for transitions
		if (NewState != OldState)
		{
			IndexToStateMap[GlobalIndex] = NewState;

			// Tell the entry widget it changed states
			if (Widget->Implements<UStrategyEntryBase>())
			{
				IStrategyEntryBase::Execute_BP_OnStrategyEntryStateChanged(Widget, OldState, NewState);
			}
		}
	}
}

UUserWidget* URadialStrategyWidget::AcquireEntryWidget(const int32 Index)
{
	return Super::AcquireEntryWidget(Index);
}

void URadialStrategyWidget::UpdateEntryWidget(UUserWidget* Widget, const int32 InGlobalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	UE_LOG(LogStrategyUI, Verbose, TEXT("\nStarting UpdateEntryWidget for index %d,"), InGlobalIndex);
	if (!Widget)
	{
		return;
	}

	const int32 DataIndex = GetLayoutStrategyChecked().GlobalIndexToDataIndex(InGlobalIndex);
	const bool bIsFocused = DataIndex == DataFocusedIndex;

	// DataIndex can be INDEX_NONE if we're in a gap with no data,
	// so to keep a consistent angle calculation for the widget, we'll use the previous valid index incremented
	// This way, the widget can still do its own thing with the angle, even if it's not a valid data index (e.g. empty wedge)
	static int32 LastValidDataIndex;
	if (DataIndex != INDEX_NONE)
	{
		LastValidDataIndex = InGlobalIndex;
	}
	else if	(LastValidDataIndex != INDEX_NONE)
	{
		LastValidDataIndex++;
	}
	
	if (Widget->Implements<URadialItemEntry>())
	{
		const int32 AngleIndex = (InGlobalIndex == INDEX_NONE) ? LastValidDataIndex : InGlobalIndex;

		FRadialItemSlotData ItemData;
		ItemData.Angle = AngleIndex * GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing();
		ItemData.DataIndex = DataIndex;
		
		IRadialItemEntry::Execute_BP_SetRadialItemSlotData(Widget, ItemData);

		// @TODO: Only call this if the focused index has changed, skip if the focus is the same
		IRadialItemEntry::Execute_BP_OnItemFocusChanged(Widget, bIsFocused);
		
	}
}

void URadialStrategyWidget::ConstructMaterialData(const UUserWidget* EntryWidget, const int32 InGlobalIndex, const bool bIsFocused, FRadialItemMaterialData& OutMaterialData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	const float AngularSpacing = GetLayoutStrategyChecked<URadialLayoutStrategy>().GetAngularSpacing();

	// 1) Figure out angles (in normalized 0..1). E.g.:
	const float RawStartDeg = InGlobalIndex * AngularSpacing - (AngularSpacing * 0.5); // e.g.  0..360..720 if multiple turns
	
	// A helper to clamp angle into [0..360)
	auto ClampAngle0To360 = [](float A) {
		A = FMath::Fmod(A, 360.f);
		if (A < 0.f) A += 360.f;
		return A;
	};
	const float StartDeg = ClampAngle0To360(RawStartDeg);

	constexpr float Gap = 1.f; // small gap between wedges, @TODO: Make this a parameter
	constexpr float HalfGap = Gap * 0.5f;

	const float GappedStartDeg = StartDeg + HalfGap;
	const float WedgeWidthDeg = AngularSpacing - Gap;

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
#pragma endregion Protected Helper Functions
