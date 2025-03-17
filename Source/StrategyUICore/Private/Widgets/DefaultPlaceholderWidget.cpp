// Copyright Mike Desrosiers 2025, All Rights Reserved.

#include "Widgets/DefaultPlaceholderWidget.h"

#include <Widgets/Images/SThrobber.h>

#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SBorder.h>

UDefaultPlaceholderWidget::UDefaultPlaceholderWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDefaultPlaceholderWidget::RebuildWidget()
{
	Throbber = SNew(SThrobber)
		.PieceImage(FCoreStyle::Get().GetBrush("Throbber.Chunk"))
		.NumPieces(3)
		.Animate(SThrobber::EAnimation::Horizontal);

	// Wrap the throbber in a box to center it and give it some size
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.WidthOverride(32)
		.HeightOverride(32)
		.Padding(FMargin(5))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(4)
			[
				Throbber.ToSharedRef()
			]
		];
}