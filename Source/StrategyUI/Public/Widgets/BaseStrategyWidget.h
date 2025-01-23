#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidgetPool.h>
#include <GameplayTagContainer.h>

#include "BaseStrategyWidget.generated.h"

class UBaseLayoutStrategy;
class UUserWidget;
class UCanvasPanel;

// Enum to specify the runtime state of a strategy item entry.
// @TODO: Extend to support more states
UENUM(BlueprintType)
enum class EStrategyEntryState : uint8
{
	/** Not in the CanvasPanel or any parent; no visuals at all. */
	Pooled,

	/** In the CanvasPanel but hidden. */
	Deactivated,

	/** Fully visible. */
	Active,
};

/**
 * A generic container widget that supports a "layout strategy" object.
 * Can be used for radials layouts, floating markers, or any arbitrary layout that the strategy computes.
 *
 * This class owns:
 *   - The items/data to display
 *   - The pool of entry widgets
 *   - The canvas to place the entry widgets on
 *   - The layout strategy that it calls to compute positions
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

	/**
	 * Builds or updates the visual arrangement of items
	 * by consulting the strategy.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget")
	virtual void UpdateLayout();
#pragma endregion Public API

protected:
#pragma region UWidget Overrides
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
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
		check(LayoutStrategy);
		return *static_cast<T*>(LayoutStrategy);
	}

	// ---------------------------------------------------------------------------------------------
	// Entry Widgets Pool & Handling
	// ---------------------------------------------------------------------------------------------
	/** Create or retrieve from a pooled widget to display a given item index. */
	virtual UUserWidget* AcquireEntryWidget(int32 Index);

	/** Release an entry widget back to the pool if no longer needed. */
	virtual void ReleaseEntryWidget(int32 Index);

	virtual void ReleaseUndesiredWidgets(const TSet<int32>& DesiredIndices);

	/** Called when building or updating the entry widget for item at 'Index'. */
	virtual void UpdateEntryWidget(UUserWidget* EntryWidget, int32 Index);


	// ---------------------------------------------------------------------------------------------
	// Properties
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
	TMap<int32, FGameplayTag> IndexToStateMap;

	/**
	 * Strategy object used for laying out items in a radial pattern.
	 * Set to a subclass of UBaseLayoutStrategy and override virtual functions as needed to customize behavior.
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

	// Debug flags, toggles, etc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget")
	bool bEnableDebugDraw = false;
};
