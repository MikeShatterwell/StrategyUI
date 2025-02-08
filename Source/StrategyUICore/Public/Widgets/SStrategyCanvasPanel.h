#pragma once

#include "CoreMinimal.h"
#include "Widgets/SPanel.h"
#include "Blueprint/UserWidgetPool.h"
#include "Layout/Children.h"
#include "UObject/GCObject.h"

class UUserWidget;

/**
 * A slot describing how each marker or item is displayed.
 */
class FMarkerSlotData : public TSlotBase<FMarkerSlotData>
{
public:
	FMarkerSlotData()
		: TSlotBase<FMarkerSlotData>()
		, ScreenPosition(FVector2D::ZeroVector)
		, bIsVisible(false)
		, Depth(0.f)
	{}

	/** The pooled UUserWidget for this item. */
	TWeakObjectPtr<UUserWidget> PooledWidget;

	/** Final 2D position on screen (or local). */
	FVector2D ScreenPosition;

	/** Whether this slot is visible and should be arranged. */
	bool bIsVisible;

	/** Depth or Z-order for sorting. */
	float Depth;
};

/**
 * A custom Slate panel to display multiple classes of user widgets,
 * each class having its own FUserWidgetPool. Replaces UCanvasPanel usage.
 */
class STRATEGYUI_API SStrategyCanvasPanel 
	: public SPanel
	, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SStrategyCanvasPanel) {}
	SLATE_END_ARGS()

	SStrategyCanvasPanel();
	virtual ~SStrategyCanvasPanel() override;

	void Construct(const FArguments& InArgs);

	//-------------------------------------------------------------
	// Public API for the Strategy Widget
	//-------------------------------------------------------------

	/**
	 * Initialize the panel's pools by setting a world (and optionally a PlayerController).
	 * This is typically called in your widget’s RebuildWidget().
	 */
	void InitializePools(UWorld* InWorld, APlayerController* InPC = nullptr);

	/**
	 * Acquire (or reuse) a UUserWidget from the pool for a given item index & widget class.
	 * If the pool for WidgetClass doesn't exist yet, we create it on demand.
	 */
	UUserWidget* AcquireEntryWidget(const TSubclassOf<UUserWidget>& WidgetClass, int32 GlobalIndex);

	/**
	 * Release the UUserWidget for a given item index, returning it to the appropriate pool.
	 */
	void ReleaseEntryWidget(int32 GlobalIndex);

	/**
	 * Called each frame/refresh to update the 2D positions, visibility, and optional depth for each item.
	 */
	void UpdateChildrenData(
		const TArray<int32>& GlobalIndices,
		const TMap<int32, FVector2D>& IndexToPosition,
		const TMap<int32, bool>& IndexToVisibility,
		const TMap<int32, float>& IndexToDepth
	);

	//-------------------------------------------------------------
	// SPanel / SWidget interface
	//-------------------------------------------------------------
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }
	virtual FChildren* GetChildren() override { return &CombinedChildren; }

	//-------------------------------------------------------------
	// FGCObject interface
	//-------------------------------------------------------------
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SStrategyCanvasPanel"); }

protected:
	/** Our TPanelChildren containing slot data for each item index. */
	TPanelChildren<FMarkerSlotData> MarkerSlots;

	/** If we had sub-widgets like arrow icons, we could unify them in CombinedChildren. */
	FCombinedChildren CombinedChildren;

	/** Maps GlobalIndex → SlotIndex so we know which slot belongs to a given item. */
	TMap<int32, int32> GlobalIndexToSlot;

	/**
	 * A map of widget class -> FUserWidgetPool, allowing multiple classes in one panel:
	 *  - "ObjectiveMarkerWidget"
	 *  - "PlayerIndicatorWidget"
	 *  - etc.
	 */
	TMap<TSubclassOf<UUserWidget>, FUserWidgetPool> PoolsByClass;

	// We store references to the world and PC for initialization if we create new pools.
	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<APlayerController> CachedPC;
};
