#pragma once

#include <CoreMinimal.h>

class UBaseLayoutStrategy;

class FLayoutStrategyDebugPaintUtil final
{
public:
	~FLayoutStrategyDebugPaintUtil() = default;

	static void DrawLayoutStrategyDebugVisuals(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& AllottedGeometry,
		int32 InLayerId,
		const UBaseLayoutStrategy* LayoutStrategy,
		const FVector2D& Center);
};