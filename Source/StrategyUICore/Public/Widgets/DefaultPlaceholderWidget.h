// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidget.h>

#include "DefaultPlaceholderWidget.generated.h"

class SThrobber;

/**
 * A minimalist loading throbber widget used as a fallback placeholder
 * during async widget loading when no custom placeholder is specified.
 */
UCLASS()
class STRATEGYUI_API UDefaultPlaceholderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDefaultPlaceholderWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SThrobber> Throbber = nullptr;
};