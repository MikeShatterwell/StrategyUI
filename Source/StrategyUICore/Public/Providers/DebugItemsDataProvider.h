#pragma once

#include <CoreMinimal.h>
#include <UObject/Object.h>

#include "Interfaces/IStrategyDataProvider.h"
#include "DebugItemsDataProvider.generated.h"

/**
 * Data provider that generates a specified number of debug items, useful for testing layouts without real data.
 */
UCLASS(Blueprintable, EditInlineNew, Category="StrategyUI|Providers")
class STRATEGYUI_API UDebugItemsDataProvider : public UObject, public IStrategyDataProvider
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void GenerateDebugItems();

	/** Number of debug items to generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	int32 DebugItemCount = 25;

	// ~ Begin IStrategyDataProvider interface
	virtual TArray<UObject*> GetDataItems_Implementation() const override
	{
		return DebugItems;
	}

	virtual bool IsProviderReady_Implementation() const override
	{
		return IsValid(DelegateWrapper);
	} 

	virtual void InitializeDataProvider_Implementation() override;
	
	virtual UOnDataProviderUpdatedDelegateWrapper* GetOnDataProviderUpdated_Implementation() override
	{
		return DelegateWrapper;
	};
	// ~ End IStrategyDataProvider interface

private:
	/**
	 * The actual array of debug items. We'll return this from GetDataItems().
	 */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> DebugItems;

	/**
	 * Delegate to broadcast changes to our data set
	 */
	UPROPERTY(Transient)
	TObjectPtr<UOnDataProviderUpdatedDelegateWrapper> DelegateWrapper;
};

