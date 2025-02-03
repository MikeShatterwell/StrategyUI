#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidgetPool.h>
#include <GameplayTagContainer.h>

#include "Interfaces/IStrategyDataProvider.h"
#include "BaseStrategyWidget.generated.h"

class UBaseLayoutStrategy;
class UUserWidget;
class UCanvasPanel;

class IStrategyDataProvider;

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
	TSubclassOf<UUserWidget> DefaultEntryWidgetClass = nullptr;
	
	/**
	 * Optional data provider. If set, the widget will automatically fetch
	 * items from the provider and refresh whenever the provider signals data changes.
	 *
	 * Only recommended to use this if working in a non-MVVM context.
	 * If using the MVVM plugin for your UI, consider using a view model to provide data directly to SetItems.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget", meta=(MustImplement="StrategyDataProvider"))
	TSubclassOf<UObject> DefaultDataProviderClass = nullptr;

#pragma region Public API
	
	// ---------------------------------------------------------------------------------------------
	// Base setters and getters
	// ---------------------------------------------------------------------------------------------
	/** 
	 * Assign a new layout strategy at runtime.
	 * If the new strategy is valid, we re‐initialize and re‐layout.
	 *
	 * @param NewStrategy The new layout strategy instance to assign.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget", meta=(DisplayName="Set Layout Strategy"))
	void SetLayoutStrategy(UBaseLayoutStrategy* NewStrategy);

	/** Returns the current layout strategy (may be nullptr). */
	UFUNCTION(BlueprintPure, Category="StrategyUI|BaseStrategyWidget")
	UBaseLayoutStrategy* GetLayoutStrategy() const { return LayoutStrategy; }

	/** Returns the number of data items currently stored. */
	UFUNCTION(BlueprintPure, Category="StrategyUI|BaseStrategyWidget")
	int32 GetItemCount() const { return Items.Num(); }
	
	/**
	 * Sets the item data to display.
	 * - If using MVVM: Bind this to your view model's data in UMG.
	 * - If not using MVVM: Assign a DefaultDataProviderClass or call SetDataProvider(...) and this widget will automatically fetch data via the IStrategyDataProvider interface.
	 * - Alternatively, call this directly with your data objects to update the widget manually.
	 * 
	 * The provided data objects will be passed via IStrategyEntryBase interface to the assigned EntryWidgetClass.
	 * 
	 * Override SetItems_Internal to handle the data in a custom way.
	 * By default, we initialize the layout strategy and call UpdateVisibleWidgets().
	 */
	UFUNCTION(BlueprintCallable, Category = "StrategyUI|BaseStrategyWidget")
	void SetItems(const TArray<UObject*>& InItems);

	/**
	 * Sets the new data provider for the strategy widget. Unbinds from the existing data provider,
	 * if any, and initializes and listens to updates from the new data provider.
	 *
	 * Any UObject implementing the IStrategyDataProvider interface can be used as a data provider.
	 *
	 * @param NewProvider The new object to act as the data provider for this widget.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget")
	void SetDataProvider(UObject* NewProvider);

	// ---------------------------------------------------------------------------------------------
	// Base public virtual functions
	// ---------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget")
	virtual void Reset();

	/**
	 * Updates the currently focused index for the widget and handles all related updates, including
	 * broadcasting changes and refocusing entries based on the provided global focus index.
	 *
	 * @param InNewGlobalFocusIndex The new global index to set as the focused index.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|Focus")
	virtual void UpdateFocusedIndex(int32 InNewGlobalFocusIndex);

	/**
	 * Updates the "selected" state of the associated entry widget and manages the internal selection records.
	 * Notifies listeners if a selection state changes for a valid data index.
	 *
	 * @param InGlobalIndex The global index of the entry to modify the selection state for.
	 * @param bShouldBeSelected Indicates whether the entry should be marked as selected or not.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|Selection")
	virtual void SetSelectedGlobalIndex(int32 InGlobalIndex, bool bShouldBeSelected);

	/** Toggles the selection state of the currently focused data index. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|Selection")
	virtual void ToggleFocusedIndexSelection();
#pragma endregion Public API

protected:
#pragma region UWidget Overrides
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
#pragma endregion UWidget Overrides

	/** 
	 * Returns a reference to the current layout strategy object, ensuring it is valid.
	 * There must always be a valid layout strategy.
	 */
	template<typename T = UBaseLayoutStrategy>
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

	/** Release all entry widgets back to the pool that aren't found in the DesiredIndices set. */
	virtual void ReleaseUndesiredWidgets(const TSet<int32>& DesiredIndices);

	/** Called when building or updating the entry widget for item at 'Index'. */
	virtual void UpdateEntryWidget(int32 InGlobalIndex);

	/**
	 * Notifies the entry widget of a state change, updating the state mapping and notifying the widget if it implements IStrategyEntryBase.
	 *
	 * @param GlobalIndex The global index identifier for the strategy entry whose state is being changed.
	 * @param Widget The UI widget associated with the strategy entry.
	 * @param OldState The previous gameplay tag container state of the strategy entry.
	 * @param NewState The new gameplay tag container state of the strategy entry.
	 */
	virtual void NotifyStrategyEntryStateChange(int32 GlobalIndex, UUserWidget* Widget, const FGameplayTagContainer& OldState, const FGameplayTagContainer& NewState);

	/**
	 * Attempts to handle the state transition of a pooled entry widget based on the visibility
	 * requirements dictated by the layout strategy's computation.
	 *
	 * This method determines whether a specific pooled entry should be in an active or deactivated
	 * state and updates its lifecycle gameplay tag accordingly.
	 *
	 * @param GlobalIndex The unique global index of the pooled entry widget to process.
	 */
	virtual void TryHandlePooledEntryStateTransition(int32 GlobalIndex);

	/**
	 * Updates the lifecycle state tag for a specific entry identified by a global index.
	 * This method ensures that the specified entry receives the new lifecycle tag and
	 * adjusts the tag container so that only one "EntryLifecycle" tag is active at any time.
	 * It also notifies the associated widget of state tag changes if applicable.
	 *
	 * @param GlobalIndex The index that uniquely identifies the entry to update.
	 * @param NewStateTag The new lifecycle state tag to apply to the entry. This tag must
	 *                    be a child of "StrategyUI.EntryLifecycle.*".
	 */
	virtual void UpdateEntryLifecycleTagState(const int32 GlobalIndex, const FGameplayTag& NewStateTag);

	/**
	 * Updates the interaction tag state for a specific entry identified by its global index.
	 * Responsible for enabling or disabling interaction tags on the entry and notifying the widget of state changes.
	 *
	 * @param GlobalIndex The unique index used to identify the specific entry whose tag state is being updated.
	 * @param InteractionTag The gameplay tag that represents the specific interaction state to be updated.
	 * @param bEnable A boolean value indicating whether to enable (true) or disable (false) the specified interaction tag.
	 */
	virtual void UpdateEntryInteractionTagState(const int32 GlobalIndex, const FGameplayTag& InteractionTag, const bool bEnable);

	/**
	 * Point of entry for updating all entry widgets managed by this widget.
	 * The main flow is to release any undesired widgets, then update the desired ones.
	 */
	virtual void UpdateWidgets();

	/**
	 * Positions the widget on the canvas panel based on the layout strategy. This is where the entry widget's CanvasSlot is set up.
	 *
	 * @param GlobalIndex The global index representing the item to be positioned. Used to compute the widget's location and size within the layout.
	 */
	virtual void PositionWidget(int32 GlobalIndex);

	// ---------------------------------------------------------------------------------------------
	// Data Provider
	// ---------------------------------------------------------------------------------------------
	/** Callback that runs when the data provider signals that its data has been updated. */
	UFUNCTION()
	void OnDataProviderUpdated();

	/** Refreshes the widget's data from the data provider by fetching the latest items. */
	void RefreshFromProvider();

	/**
	 * Sets new items, initializes the layout strategy, and refreshes the widget.
	 *
	 * Call SetItems() as the main entry point for setting new data in the widget. This method
	 * is intended to be overridden by subclasses to handle the data in a custom way.
	 *
	 * @param InItems The array of UObject ptrs representing the data items to be set in the widget.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "StrategyUI|BaseStrategyWidget")
	void SetItems_Internal(const TArray<UObject*>& InItems);
	virtual void SetItems_Internal_Implementation(const TArray<UObject*>& InItems);
	FUserWidgetPool& GetOrCreatePoolForClass(const TSubclassOf<UUserWidget>& WidgetClass);

	// ---------------------------------------------------------------------------------------------
	// Runtime Data
	// ---------------------------------------------------------------------------------------------
	/** The source of truth data array. Subclasses can interpret these items in any way needed. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|BaseStrategyWidget|Data")
	TArray<TObjectPtr<UObject>> Items;

	/**
	 * Optional data provider. If set, the widget will automatically fetch
	 * items from the provider and refresh whenever the provider signals data changes.
	 *
	 * Only recommended to use this if working in a non-MVVM context.
	 * If using the MVVM plugin for your UI, consider using a view model to provide data directly to SetItems.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|BaseStrategyWidget")
	TObjectPtr<UObject> DataProvider = nullptr;

	/**
	 * Constructs and assigns a data provider object using the default class specified in DefaultDataProviderClass.
	 * Only constructs the DataProvider object if it is not already set and the DefaultDataProviderClass is valid.
	 */
	void ConstructDataProviderObjectFromDefaultClass();

	// ---------------------------------------------------------------------------------------------
	// Entry Widgets & State
	// ---------------------------------------------------------------------------------------------
	/**
	 * A collection of pools of reusable entry widgets.
	 * These are mapped by the class of the widget they contain.
	 * For the majority of cases, there will only be one pool of widgets for one desired class.
	 * In more complex cases, there may be multiple pools for different classes (e.g., different types of world markers).
	 */
	UPROPERTY(Transient)
	TMap<TSubclassOf<UUserWidget>, FUserWidgetPool> PooledWidgetsMap;

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
	bool bPaintDebugInfo = false;
};
