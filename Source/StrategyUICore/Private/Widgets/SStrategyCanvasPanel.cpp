#include "Widgets/SStrategyCanvasPanel.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

SStrategyCanvasPanel::SStrategyCanvasPanel()
	: SPanel()
	, MarkerSlots(this)
	, CombinedChildren(this)
{
	CombinedChildren.AddChildren(MarkerSlots);
}

SStrategyCanvasPanel::~SStrategyCanvasPanel()
{
}

void SStrategyCanvasPanel::Construct(const FArguments& InArgs)
{
	// No direct children
}

void SStrategyCanvasPanel::InitializePools(UWorld* InWorld, APlayerController* InPC /*= nullptr*/)
{
	CachedWorld = InWorld;
	CachedPC = InPC;

	for (auto& Pair : PoolsByClass)
	{
		FUserWidgetPool& Pool = Pair.Value;

		if (InWorld)
		{
			Pool.SetWorld(InWorld);
		}
		if (InPC)
		{
			Pool.SetDefaultPlayerController(InPC);
		}
	}
}

UUserWidget* SStrategyCanvasPanel::AcquireEntryWidget(const TSubclassOf<UUserWidget>& WidgetClass, const int32 GlobalIndex)
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	// 1) Find or create the pool for this specific class
	FUserWidgetPool& Pool = PoolsByClass.FindOrAdd(WidgetClass);

	// 2) If the pool is not yet initialized, do so using CachedWorld or PC
	if (!Pool.IsInitialized())
	{
		if (CachedWorld.IsValid())
		{
			Pool.SetWorld(CachedWorld.Get());
		}
		if (CachedPC.IsValid())
		{
			Pool.SetDefaultPlayerController(CachedPC.Get());
		}
	}

	// 3) Acquire or create a UUserWidget from that pool
	UUserWidget* WidgetInstance = Pool.GetOrCreateInstance(WidgetClass);
	if (!WidgetInstance)
	{
		return nullptr;
	}

	// 4) Make or reuse a slot for this item
	const int32* ExistingSlot = GlobalIndexToSlot.Find(GlobalIndex);
	int32 SlotIndex = INDEX_NONE;
	if (!ExistingSlot)
	{
		SlotIndex = MarkerSlots.AddSlot(MakeUnique<FMarkerSlotData>());
		GlobalIndexToSlot.Add(GlobalIndex, SlotIndex);
	}
	else
	{
		SlotIndex = *ExistingSlot;
	}

	FMarkerSlotData& Slot = MarkerSlots[SlotIndex];
	Slot.PooledWidget = WidgetInstance;
	Slot.bIsVisible   = true;
	Slot.Depth        = 0.f;

	return WidgetInstance;
}

void SStrategyCanvasPanel::ReleaseEntryWidget(int32 GlobalIndex)
{
	if (int32* FoundSlot = GlobalIndexToSlot.Find(GlobalIndex))
	{
		FMarkerSlotData& SlotData = MarkerSlots[*FoundSlot];
		if (UUserWidget* W = SlotData.PooledWidget.Get())
		{
			TSubclassOf<UUserWidget> Cls = W->GetClass();
			if (FUserWidgetPool* PoolPtr = PoolsByClass.Find(Cls))
			{
				PoolPtr->Release(W);
			}
		}
		SlotData.PooledWidget = nullptr;
		SlotData.bIsVisible   = false;
	}
}

void SStrategyCanvasPanel::UpdateChildrenData(
	const TArray<int32>& GlobalIndices,
	const TMap<int32, FVector2D>& IndexToPosition,
	const TMap<int32, bool>& IndexToVisibility,
	const TMap<int32, float>& IndexToDepth
)
{
	for (const int32 GlobalIndex : GlobalIndices)
	{
		if (const int32* FoundSlotIdx = GlobalIndexToSlot.Find(GlobalIndex))
		{
			FMarkerSlotData& Slot = MarkerSlots[*FoundSlotIdx];

			if (const FVector2D* Pos = IndexToPosition.Find(GlobalIndex))
			{
				Slot.ScreenPosition = *Pos;
			}
			if (const bool* bVisible = IndexToVisibility.Find(GlobalIndex))
			{
				Slot.bIsVisible = *bVisible;
			}
			if (const float* Depth = IndexToDepth.Find(GlobalIndex))
			{
				Slot.Depth = *Depth;
			}
		}
	}

	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

// SPanel override: arrange children in one pass
void SStrategyCanvasPanel::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	// Optional: Sort by Depth ascending
	struct FSortSlot
	{
		int32 SlotIndex;
		float Depth;
	};

	TArray<FSortSlot> Sorted;
	Sorted.Reserve(MarkerSlots.Num());

	for (int32 i = 0; i < MarkerSlots.Num(); i++)
	{
		const FMarkerSlotData& Slot = MarkerSlots[i];
		if (Slot.PooledWidget.IsValid() && Slot.bIsVisible)
		{
			Sorted.Add({ i, Slot.Depth });
		}
	}

	Sorted.Sort([](const FSortSlot& A, const FSortSlot& B)
	{
		return A.Depth < B.Depth;
	});

	// Add each valid slot to ArrangedChildren
	for (const FSortSlot& Entry : Sorted)
	{
		const FMarkerSlotData& Slot = MarkerSlots[Entry.SlotIndex];
		UUserWidget* WidgetObj = Slot.PooledWidget.Get();
		if (!WidgetObj) { continue; }
		
		WidgetObj->ForceLayoutPrepass();

		const FVector2D EntryDesiredSize = WidgetObj->GetDesiredSize();
		const FVector2D CenteredPos = Slot.ScreenPosition - (EntryDesiredSize * 0.5f);

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(
				WidgetObj->TakeWidget(),
				CenteredPos,
				EntryDesiredSize
			)
		);
	}
}

// SPanel override: paint children
int32 SStrategyCanvasPanel::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	FArrangedChildren Arranged(EVisibility::Visible);
	this->ArrangeChildren(AllottedGeometry, Arranged);

	int32 MaxLayer = LayerId;
	const bool bShouldBeEnabled = ShouldBeEnabled(bParentEnabled);

	for (int32 i = 0; i < Arranged.Num(); i++)
	{
		const FArrangedWidget& ChildSlot = Arranged[i];
		const int32 ChildLayer = ChildSlot.Widget->Paint(
			Args.WithNewParent(this),
			ChildSlot.Geometry,
			MyCullingRect,
			OutDrawElements,
			MaxLayer,
			InWidgetStyle,
			bShouldBeEnabled
		);
		MaxLayer = FMath::Max(MaxLayer, ChildLayer);
	}

	return MaxLayer;
}

// FGCObject override: keep references to pooled widgets alive
void SStrategyCanvasPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : PoolsByClass)
	{
		Pair.Value.AddReferencedObjects(Collector);
	}
}
