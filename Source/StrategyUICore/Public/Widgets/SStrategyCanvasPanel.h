#pragma once

#include <CoreMinimal.h>
#include <Widgets/SPanel.h>
#include <Layout/Children.h>
#include <UObject/GCObject.h>

#include "Utils/StrategyUIFunctionLibrary.h"

/**
 * Minimal layout data for a single entry. This structure contains only what SStrategyCanvasPanel needs:
 * a screen position, a depth value for z‐ordering, and the underlying Slate widget to arrange.
 */
struct FStrategyCanvasSlotData_Minimal
{
	/** Screen–space position (usually computed from the layout strategy) */
	FVector2D Position = FVector2D::ZeroVector;
	/** A depth (or Z–order) value to sort the children */
	float Depth = 0.f;
	/** The underlying Slate widget for this entry.*/
	TSharedPtr<SWidget> Widget;
	
	FString ToString() const
	{
		return FString::Printf(TEXT("Position: %s, Depth: %f, Widget: %s"),
			*Position.ToString(), Depth, *UStrategyUIFunctionLibrary::GetFriendlySlateWidgetName(Widget));
	}
};

/**
 * SStrategyCanvasPanel is a pure Slate container that arranges children at explicit positions with a given depth.
 * It is designed to be as lean as possible. All UUserWidget (or other UObject/Unreal) logic should be handled in the implementing UWidget.
 */
class SStrategyCanvasPanel
	: public SPanel
	, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SStrategyCanvasPanel) {}
	SLATE_END_ARGS()

	SStrategyCanvasPanel();
	virtual ~SStrategyCanvasPanel() override;

	/** Constructs this panel. No direct children are set up initially. */
	void Construct(const FArguments& InArgs);

	/**
	 * Update the children data with minimal layout data.
	 * The parameter InSlotData maps a global index (as defined by UBaseStrategyWidget)
	 * to a minimal data struct containing only the position, depth, and Slate widget to show.
	 */
	void UpdateChildrenData(const TMap<int32, FStrategyCanvasSlotData_Minimal>& InSlotData);

	void SetDebugPaint(bool bEnable) { bDebugPaint = bEnable; }

	// SPanel overrides
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FChildren* GetChildren() override;

	// SWidget override for painting
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SStrategyCanvasPanel"); }

private:
	/**
	 * Internal slot type that holds the minimal data for one child.
	 * This slot is a lightweight container that derives from TSlotBase.
	 */
	struct FStrategyCanvasChildSlot_Internal : public TSlotBase<FStrategyCanvasChildSlot_Internal>
	{
		FVector2D Position = FVector2D::ZeroVector;
		float Depth = 0.f;
		TWeakPtr<SWidget> Widget;

		FString ToString() const
		{
			return FString::Printf(TEXT("Position: %s, Depth: %f, Widget: %s"),
				*Position.ToString(), Depth, *UStrategyUIFunctionLibrary::GetFriendlySlateWidgetName(Widget));
		}
	};

	/** The array of children slots */
	TPanelChildren<FStrategyCanvasChildSlot_Internal> Children;

	/** A helper that combines all children */
	FCombinedChildren CombinedChildren;

	/**
	 * Mapping from the “global index” (provided by the host widget) to an index in our Children array.
	 * This lets us update or remove individual children efficiently.
	 */
	TMap<int32, int32> GlobalIndexToSlot;

	bool bDebugPaint = false;
};
