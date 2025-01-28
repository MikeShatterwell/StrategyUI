#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidgetPool.h>
#include <GameplayTagContainer.h>

#include "BaseStrategyWidget.generated.h"

class UBaseLayoutStrategy;
class UUserWidget;
class UCanvasPanel;

// Delegate broadcast when an item gains focus. Provides the index and the data item object implementing UStrategyInteractiveEntry.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStrategyItemFocusedDelegate, int32, Index, UObject*, Item);

// Delegate broadcast when an item is clicked or selected. Provides the index and the data item object implementing UStrategyInteractiveEntry.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStrategyItemSelectedDelegate, int32, Index, UObject*, Item);

/**
 * A generic container widget that supports a "layout strategy" object.
 * Can be used for radials layouts, floating markers, or any arbitrary layout that the strategy computes.
 *
 * This class owns:
 *   - The items/data to display
 *   - The pool of entry widgets
 *   - The canvas to place the entry widgets on
 *   - The layout strategy that it calls to compute positions
 *   - The focus and selection management for the items
 *
 * Subclasses can override or extend input handling, item events, etc.
 */
UCLASS(Abstract, Blueprintable, ClassGroup="StrategyUI")
class STRATEGYUI_API UBaseStrategyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit UBaseStrategyWidget(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	 * Strategy object used for laying out items according to abstract rules.
	 * 
	 * Assign to a subclass of UBaseLayoutStrategy and override virtual functions as needed to customize behavior.
	 * Included by default are URadialWheelLayoutStrategy and URadialSpiralLayoutStrategy.
	 *
	 * @TODO: Floating marker widget strategy
	 * 
	 * URadialWheelLayoutStrategy: A simple wheel layout with equidistant segments.
	 * URadialSpiralLayoutStrategy: A more complex layout that allows for infinite scrolling and a spiral pattern.
	*/
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="StrategyUI|BaseStrategyWidget")
	TObjectPtr<UBaseLayoutStrategy> LayoutStrategy = nullptr;

	/** Custom entry widget class (must implement IStrategyEntryBase). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget")
	TSubclassOf<UUserWidget> EntryWidgetClass;

#pragma region Public API
	// ---------------------------------------------------------------------------------------------
	// Base setters and getters
	// ---------------------------------------------------------------------------------------------
	/** 
	 * Assign a new layout strategy at runtime.
	 * If the new strategy is valid, we re‐initialize and re‐layout.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget", meta=(DisplayName="Set Layout Strategy"))
	void SetLayoutStrategy(UBaseLayoutStrategy* NewStrategy);

	/** Returns the current layout strategy (may be nullptr). */
	UFUNCTION(BlueprintPure, Category="StrategyUI|BaseStrategyWidget")
	UBaseLayoutStrategy* GetLayoutStrategy() const { return LayoutStrategy; }

	/** Returns the number of data items currently stored. */
	UFUNCTION(BlueprintPure, Category="StrategyUI|BaseStrategyWidget")
	int32 GetItemCount() const { return Items.Num(); }

	// ---------------------------------------------------------------------------------------------
	// Base virtual functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * Sets the item data to display; automatically chooses wheel/spiral based on RadialSegments.
	 * These data objects will be passed via IStrategyEntryBase interface to the EntryWidgetClass.
	 * Then UpdateLayout() will be called automatically to build the visual layout.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget")
	virtual void SetItems(const TArray<UObject*>& InItems);
	
	virtual void Reset();

	UFUNCTION(BlueprintCallable, Category="StrategyUI|Focus")
	virtual void UpdateFocusedGlobalIndex(int32 InNewGlobalFocusIndex);

	UFUNCTION(BlueprintCallable, Category="StrategyUI|Selection")
	virtual void SetSelectedGlobalIndex(int32 InGlobalIndex, bool bShouldBeSelected);

	UFUNCTION(BlueprintCallable, Category="StrategyUI|Selection")
	virtual void ToggleFocusedIndex();

#pragma endregion Public API

protected:
#pragma region UWidget Overrides
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
#pragma endregion UWidget Overrides

	UBaseLayoutStrategy& GetLayoutStrategyChecked() const
	{
		check(LayoutStrategy);
		return *LayoutStrategy;
	}

	template<typename T>
	T& GetLayoutStrategyChecked() const
	{
		T* StrategyType = Cast<T>(LayoutStrategy);
		check(StrategyType);
		return *StrategyType;
	}

	// ---------------------------------------------------------------------------------------------
	// Entry Widgets Pool & Handling
	// ---------------------------------------------------------------------------------------------
	/** Create or retrieve from a pooled widget to display a given item index. */
	virtual UUserWidget* AcquireEntryWidget(int32 GlobalIndex);

	/** Release an entry widget back to the pool if no longer needed. */
	virtual void ReleaseEntryWidget(int32 GlobalIndex);

	virtual void ReleaseUndesiredWidgets(const TSet<int32>& DesiredIndices);

	/** Called when building or updating the entry widget for item at 'Index'. */
	virtual void UpdateEntryWidget(int32 InGlobalIndex);
	
	virtual void NotifyStrategyEntryStateChange(int32 GlobalIndex, UUserWidget* Widget, const FGameplayTagContainer& OldState, const FGameplayTagContainer& NewState);
	virtual void TryHandlePooledEntryStateTransition(int32 GlobalIndex);
	
	virtual void UpdateEntryLifecycleTagState(const int32 GlobalIndex, const FGameplayTag& NewStateTag);
	virtual void UpdateEntryInteractionTagState(const int32 GlobalIndex, const FGameplayTag& InteractionTag, const bool bEnable);

	virtual void UpdateVisibleWidgets();

	virtual void PositionWidget(int32 GlobalIndex);


	// ---------------------------------------------------------------------------------------------
	// Runtime Properties
	// ---------------------------------------------------------------------------------------------
	/** The source of truth data array. Subclasses can interpret these items in any way needed. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|BaseStrategyWidget|Data")
	TArray<TObjectPtr<UObject>> Items;

	/** A reusable pool of entry widgets. */
	UPROPERTY(Transient)
	FUserWidgetPool EntryWidgetPool;

	/**
	 * Mapping from "global item index" -> "widget currently displaying that item".
	 * Items outside the visible window won't be in this map.
	 */
	UPROPERTY(Transient)
	TMap<int32, TWeakObjectPtr<UUserWidget>> IndexToWidgetMap;

	/**
	 * Mapping from "global item index" -> "state of the item entry".
	 * This is used to track whether an item is pooled, deactivated, or active.
	 */
	UPROPERTY(Transient)
	TMap<int32, FGameplayTagContainer> IndexToTagStateMap;

	//----------------------------------------------------------------------------------------------
	// Focus & Selection
	//----------------------------------------------------------------------------------------------
	/*
	 * May contain multiple data indices if the widget supports multi-select.
	 * Otherwise it will only contain a single index. 
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|Selection")
	TSet<int32> SelectedDataIndices;

	/** Currently focused item index without being constrained by data indexes. Can be negative or exceed the number of Items. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|Selection")
	int32 FocusedGlobalIndex = INDEX_NONE;

	/** Currently focused item index (in the entire data Items array). INDEX_NONE (-1) if invalid. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|Selection")
	int32 FocusedDataIndex = INDEX_NONE;

	/** Broadcasts when an item is considered focused (hovered) by this widget. */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|RadialStrategyWidget|Event")
	FStrategyItemFocusedDelegate OnItemFocused;
	
	/** Broadcasts when the focused item is selected (clicked) by this widget. */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|RadialStrategyWidget|Event")
	FStrategyItemSelectedDelegate OnItemSelected;
	// @TODO: Add OnItemDeselected, etc. Update when we have entry-to-container selection support.

	//----------------------------------------------------------------------------------------------
	// Bound Widgets
	//----------------------------------------------------------------------------------------------
	/**
	 * Canvas panel to hold the entry widgets (optionally bound in the UMG designer).
	 *
	 * If one isn't bound in UMG, we create one at runtime and add it to the widget tree ourselves
	 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="StrategyUI|BaseStrategyWidget")
	TObjectPtr<UCanvasPanel> CanvasPanel = nullptr;

	//----------------------------------------------------------------------------------------------
	// Debug
	//----------------------------------------------------------------------------------------------
	/** If true, we debug-draw pointer lines, circles, angles, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget")
	bool bEnableDebugDraw = false;

	/** Number of debug items to show if no other data is supplied via SetItems(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget", meta=(ClampMin="0"))
	int32 DebugItemCount = 0;
};
