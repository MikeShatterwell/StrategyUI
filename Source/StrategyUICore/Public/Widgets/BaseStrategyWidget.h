// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidgetPool.h>
#include <GameplayTagContainer.h>

#include "Interfaces/ILayoutStrategyHost.h"

#include "BaseStrategyWidget.generated.h"

class UBaseLayoutStrategy;
class UUserWidget;
class IStrategyDataProvider;
class SStrategyCanvasPanel;

USTRUCT()
struct FStrategyEntrySlotData
{

	GENERATED_BODY()
	
public:
	virtual ~FStrategyEntrySlotData() = default;
	FStrategyEntrySlotData() = default;
	
	virtual FStrategyEntrySlotData& operator=(const FStrategyEntrySlotData& Other)
	{
		Widget = Other.Widget;
		TagState = Other.TagState;
		Position = Other.Position;
		Depth = Other.Depth;
		LastAssignedItem = Other.LastAssignedItem;
		return *this;
	}
	
	// The pooled or acquired widget for this global index
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "StrategyUI|BaseStrategyWidget")
	TWeakObjectPtr<UUserWidget> Widget = nullptr;
	TSharedPtr<SWidget> CachedSlateWidget = nullptr;

	// The current tag state (lifecycle, interaction, etc.)
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "StrategyUI|BaseStrategyWidget")
	FGameplayTagContainer TagState;

	// The latest position computed by the layout strategy's GetItemPosition()
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "StrategyUI|BaseStrategyWidget")
	FVector2D Position = FVector2D::ZeroVector;

	// A "depth" or Z-order used to sort
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "StrategyUI|BaseStrategyWidget")
	float Depth = 0.f;

	// The last assigned data item, for detecting changes
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "StrategyUI|BaseStrategyWidget")
	TWeakObjectPtr<UObject> LastAssignedItem = nullptr;

	virtual FString ToString() const
	{
		return FString::Printf(TEXT("\n\t\tWidget: %s, \n\t\tTagState: %s, \n\t\tPosition: %s, \n\t\tDepth: %f, \n\t\tLastItem: %s"),
			Widget.IsValid() ? *Widget->GetName() : TEXT("None"),
			*TagState.ToString(),
			*Position.ToString(),
			Depth,
			LastAssignedItem.IsValid() ? *LastAssignedItem->GetName() : TEXT("None"));
	}

	virtual void Reset()
	{
		Widget.Reset();
		CachedSlateWidget.Reset();
		TagState.Reset();
		Position = FVector2D::ZeroVector;
		Depth = 0.f;
		LastAssignedItem.Reset();
	}

	virtual bool operator==(const FStrategyEntrySlotData& Other) const
	{
		return Widget == Other.Widget
			&& TagState == Other.TagState
			&& Position == Other.Position
			&& FMath::IsNearlyEqual(Depth, Other.Depth)
			&& LastAssignedItem == Other.LastAssignedItem;
	}

	virtual bool IsValid() const
	{
		return Widget.IsValid() && TagState.IsValid() && CachedSlateWidget.IsValid();
	}
};

template<>
struct TBaseStructure<FStrategyEntrySlotData>
{
	static const UScriptStruct* Get()
	{
		return FStrategyEntrySlotData::StaticStruct();
	}
};

/**
 * Delegate broadcast when an item gains focus.o
 * Provides the index and the data item object implementing UStrategyInteractiveEntry.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStrategyItemFocusedDelegate, int32, Index, UObject*, Item);

/**
 * Delegate broadcast when an item is clicked or selected.
 * Provides the index and the data item object implementing UStrategyInteractiveEntry.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStrategyItemSelectedDelegate, int32, Index, UObject*, Item);

/**
 * A generic container widget that supports a "layout strategy" object.
 * Can be used for radial layouts, floating markers, or any arbitrary layout that the strategy computes.
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
class STRATEGYUI_API UBaseStrategyWidget : public UUserWidget, public ILayoutStrategyHost
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	/** Validate compiled defaults in the editor. */
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;

	/** Respond to property changes in the editor. */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

#pragma region UBaseStrategyWidget Functions - API Base
	/** Returns the current layout strategy (may be nullptr). */
	UFUNCTION(BlueprintPure, Category="StrategyUI|BaseStrategyWidget|LayoutStrategy")
	UBaseLayoutStrategy* GetLayoutStrategy() const { return LayoutStrategy; }
	
	/**
	 * Assign a new layout strategy at runtime.
	 * If the new strategy is valid, re‐initialize and re‐layout.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget|LayoutStrategy", meta=(DisplayName="Set Layout Strategy"))
	virtual void SetLayoutStrategy(UBaseLayoutStrategy* NewStrategy);

	/** Returns the number of data items currently stored. */
	UFUNCTION(BlueprintPure, Category="StrategyUI|BaseStrategyWidget")
	int32 GetItemCount() const { return Items.Num(); }

	/**
	 * Sets the item data to display.
	 * By default, initializes the layout strategy and calls UpdateVisibleWidgets().
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget")
	virtual void SetItems(const TArray<UObject*>& InItems);

	/**
	 * Sets a new data provider for the strategy widget.
	 * Unbinds from the existing provider (if any) and initializes the new provider.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget|DataProvider")
	virtual void SetDataProvider(UObject* NewProvider);
	
	/** Resets internal state, clearing selection, focus, etc. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget")
	virtual void Reset();

	/**
	 * Updates the currently focused index for the widget and handles related updates,
	 * including broadcasting focus‐change events.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget|Focus")
	virtual void UpdateFocusedIndex(int32 InNewGlobalFocusIndex);

	/**
	 * Updates the "selected" state of the entry at InGlobalIndex.
	 * Broadcasts a selection event if a valid data index changes.
	 */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget|Selection")
	virtual void SetSelectedGlobalIndex(int32 InGlobalIndex, bool bShouldBeSelected);

	/** Toggles selection state of the currently focused data index. */
	UFUNCTION(BlueprintCallable, Category="StrategyUI|BaseStrategyWidget|Selection")
	virtual void ToggleFocusedIndexSelection();
#pragma endregion

protected:
#pragma region UBaseStrategyWidget Properties - Editable
	/** 
	 * Strategy object used for laying out items according to abstract rules.
	 * Assign to a subclass of UBaseLayoutStrategy and override virtual functions as needed.
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
	 * Only recommended if you're not using MVVM.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget", meta=(MustImplement="StrategyDataProvider"))
	TSubclassOf<UObject> DefaultDataProviderClass = nullptr;
#pragma endregion

#pragma region UUserWidget & UWidget Overrides
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
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

#pragma region ILayoutStrategyHost Interface
	virtual int32 GetNumItems_Implementation() const override { return GetItemCount(); }
#pragma endregion

#pragma region UBaseStrategyWidget Functions - Entry Widgets Pool & Handling
	/** Returns the current layout strategy object, checking that it's valid. */
	template<typename T = UBaseLayoutStrategy>
	T& GetLayoutStrategyChecked() const
	{
		T* StrategyType = Cast<T>(LayoutStrategy);
		check(StrategyType);
		return *StrategyType;
	}
	
	/** Create (or retrieve from a pool) a widget for the item at GlobalIndex. */
	virtual UUserWidget* AcquireEntryWidget(int32 GlobalIndex);

	/** Releases an entry widget back to the pool if it's no longer needed. */
	virtual void ReleaseEntryWidget(int32 GlobalIndex);

	/** Releases widgets not found in DesiredIndices, returning them to the pool. */
	virtual void ReleaseUndesiredWidgets(const TSet<int32>& DesiredIndices);

	/** Called to (re)build the entry widget for the item at InGlobalIndex. */
	virtual void UpdateEntryWidget(int32 InGlobalIndex);

	/**
	 * Updates the lifecycle tag on a widget entry and notifies if it implements IStrategyEntryBase.
	 */
	virtual void NotifyStrategyEntryStateChange(
		int32 GlobalIndex,
		UUserWidget* Widget,
		const FGameplayTagContainer& OldState,
		const FGameplayTagContainer& NewState
	);

	/**
	 * Handles activating/deactivating pooled entries as dictated by the layout's 
	 * visible/invisible requirements.
	 */
	virtual void TryHandlePooledEntryStateTransition(int32 GlobalIndex);

	/**
	 * Updates the lifecycle tag state for an entry identified by GlobalIndex.
	 */
	virtual void UpdateEntryLifecycleTagState(const int32 GlobalIndex, const FGameplayTag& NewStateTag);

	/**
	 * Enables or disables an interaction tag on the entry identified by GlobalIndex.
	 */
	virtual void UpdateEntryInteractionTagState(
		const int32 GlobalIndex,
		const FGameplayTag& InteractionTag,
		bool bEnable
	);

	/**
	 * Main point for updating all entry widgets. Releases undesired widgets 
	 * and updates the ones still in use.
	 */
	virtual void UpdateWidgets();

	/**
	 * Checks if the new desired indices are identical to what was used last update.
	 */
	bool HasNewDesiredIndices(const TSet<int32>& NewIndices) const;

	/**
	 * A single function to build the arrays for our Slate panel
	 * and optionally re-acquire/update widgets for each global index.
	 *
	 * @param InIndices           The set of global indices we’re displaying.
	 * @param bForceUpdateWidget  If true, call UpdateEntryWidget(...) (releasing/re-acquiring if needed).
	 *                            If false, only do minimal position/visibility updates.
	 */
	void RebuildSlateForIndices(const TSet<int32>& InIndices, bool bForceUpdateWidget);
#pragma endregion

#pragma region UBaseStrategyWidget Functions - Internal Implementations
	/**
	 * Called internally by SetItems. Sets new items, initializes the layout, etc.
	 * Override in subclasses to handle data differently.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="StrategyUI|BaseStrategyWidget")
	void SetItems_Internal(const TArray<UObject*>& InItems);
	virtual void SetItems_Internal_Implementation(const TArray<UObject*>& InItems);
#pragma endregion

#pragma region UBaseStrategyWidget Functions - Data Provider
	/** Bound to the data provider's update event, forces a refresh of items. */
	UFUNCTION()
	virtual void OnDataProviderUpdated();
	
	/** Creates a default data provider if DefaultDataProviderClass is set and DataProvider is null. */
	virtual void TryCreateDefaultDataProvider();

	/** Fetches items from the DataProvider and calls SetItems with them. */
	virtual void RefreshFromProvider();
#pragma endregion

#pragma region UBaseStrategyWidget Properties - Runtime Data
	/** The array of data objects used by this widget. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|BaseStrategyWidget|Data")
	TArray<TObjectPtr<UObject>> Items;

	/**
	 * Optional data provider object. If set, auto‐fetches data on changes.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|BaseStrategyWidget|DataProvider")
	TObjectPtr<UObject> DataProvider = nullptr;

	//----------------------------------------------------------------------------------------------
	// UBaseStrategyWidget Properties - Entry Widgets & State
	//----------------------------------------------------------------------------------------------
	UPROPERTY(Transient, VisibleInstanceOnly, Category="StrategyUI|BaseStrategyWidget")
	TMap<int32, FStrategyEntrySlotData> GlobalIndexToSlotData;

	UPROPERTY(Transient, VisibleInstanceOnly, Category="StrategyUI|BaseStrategyWidget")
	TMap<TSubclassOf<UUserWidget>, FUserWidgetPool> WidgetPools;

	/** 
	 * Store the last set of indices for quick comparison 
	 * so we skip re-release/re-acquire if they haven’t changed.
	 */
	UPROPERTY(Transient, VisibleInstanceOnly, Category="StrategyUI|BaseStrategyWidget")
	TSet<int32> LastDesiredIndices;
#pragma endregion

#pragma region UBaseStrategyWidget Properties - Focus & Selection
	/**
	 * Contains zero or more selected data indices. Used if multi‐select is allowed;
	 * otherwise it typically has one entry or none.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|Selection")
	TSet<int32> SelectedDataIndices;

	/**
	 * Raw global "cursor" or "focus" index, can be outside the array range.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|Selection")
	int32 FocusedGlobalIndex = INDEX_NONE;

	/** Focused index clamped to the array, or INDEX_NONE if invalid. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="StrategyUI|Selection")
	int32 FocusedDataIndex = INDEX_NONE;

	/** Broadcasts when an item is considered focused (hovered). */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|Event")
	FStrategyItemFocusedDelegate OnItemFocused;
	
	/** Broadcasts when the focused item is selected (clicked). */
	UPROPERTY(BlueprintAssignable, Category="StrategyUI|Event")
	FStrategyItemSelectedDelegate OnItemSelected;
#pragma endregion

#pragma region UBaseStrategyWidget Properties - Slate
	/**
	 * The custom Slate panel that arranges items in one pass, with multiple pools.
	 */
	TSharedPtr<SStrategyCanvasPanel> StrategyCanvasPanel;
	
	/** Cached geometry size for consistent layout math. */
	mutable FVector2D CachedSize = FVector2D::ZeroVector;

	/** Cached canvas center. */
	mutable FVector2D Center = FVector2D::ZeroVector;
#pragma endregion

#pragma region UBaseStrategyWidget - Debug
	/** If true, draw the strategy's debug shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget")
	bool bPaintDebugInfo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StrategyUI|BaseStrategyWidget")
	bool bPaintEntryWidgetBorders = false;

#if WITH_GAMEPLAY_DEBUGGER
	virtual void UpdateReflectedObjectsDebugCategory();
#endif
#pragma endregion
};
