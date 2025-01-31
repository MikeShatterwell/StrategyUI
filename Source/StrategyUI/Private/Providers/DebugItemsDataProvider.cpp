// DebugItemsDataProvider.cpp

#include "Providers/DebugItemsDataProvider.h"

#include "StrategyDebugItem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DebugItemsDataProvider)

void UDebugItemsDataProvider::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	GenerateDebugItems();
}

void UDebugItemsDataProvider::GenerateDebugItems()
{
	// Clear old data
	DebugItems.Reset(DebugItemCount);

	// Create new UDebugRadialItems
	for (int32 i = 0; i < DebugItemCount; ++i)
	{
		UStrategyDebugItem* NewItem = NewObject<UStrategyDebugItem>(this, UStrategyDebugItem::StaticClass());
		NewItem->DebugLabel = FString::Printf(TEXT("Debug Item %d"), i);
		NewItem->Id = i;
		DebugItems.Add(NewItem);
	}

	// Notify any listeners (e.g., strategy widget) that data changed through the delegate wrapper
	if (IsValid(DelegateWrapper) && DelegateWrapper->OnDataProviderUpdatedDelegate.IsBound())
	{
		DelegateWrapper->OnDataProviderUpdatedDelegate.Broadcast();
	}
}

void UDebugItemsDataProvider::InitializeDataProvider_Implementation()
{
	DelegateWrapper = NewObject<UOnDataProviderUpdatedDelegateWrapper>(this, TEXT("Debug Delegate Wrapper"));

	// Initialize the array with the specified number of items
	GenerateDebugItems();
}
