#include "Utils/PropertyDebugPaintUtil.h"

#include "Strategies/BaseLayoutStrategy.h"

#if WITH_EDITOR

void FLayoutStrategyDebugPaintUtil::DrawLayoutStrategyDebugVisuals(
	FSlateWindowElementList& OutDrawElements,
	const FGeometry& AllottedGeometry,
	int32 InLayerId,
	const UBaseLayoutStrategy* LayoutStrategy,
	const FVector2D& Center)
{

	if (!ensureMsgf(LayoutStrategy, TEXT("Container is null in %hs!"), __FUNCTION__))
	{
		return;
	}

	// Place the text at some offset from top-left
	FVector2D DrawPos(10.f, 60.f);

	// Draw the class name
	const UClass* Class = LayoutStrategy->GetClass();
	const FString& ClassName = Class->GetName();
	const FString& TitleText = FString::Printf(TEXT("Strategy: %s"), *ClassName);

	// A quick utility to draw a line of text
	auto DrawDebugLine = [&](const FString& Text, const FLinearColor& Color) mutable
	{
		const FSlateLayoutTransform TitleXform(1.f, DrawPos);
		
		FSlateDrawElement::MakeText(
			OutDrawElements,
			InLayerId,
			AllottedGeometry.ToPaintGeometry(TitleXform),
			FText::FromString(Text),
			FCoreStyle::GetDefaultFontStyle("Regular", 12),
			ESlateDrawEffect::None,
			Color
		);
		// Move down for next line
		DrawPos.Y += 18.f;
	};
	
	// First, draw the title in Yellow
	DrawDebugLine(TitleText, FLinearColor(1.f, 1.f, 0.f, 1.f));
	DrawPos.Y += 4.f; // extra spacing

	// 2) Gather all properties, grouped by their Category metadata
	TMap<FString, TArray<FProperty*>> CategoryToProperties;

	// 2) Iterate properties
	for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		// Read the category metadata
		FString CategoryName = Property->GetMetaData(TEXT("Category"));
		if (CategoryName.IsEmpty())
		{
			// If there's no Category, use a default fallback
			CategoryName = TEXT("Uncategorized");
		}

		CategoryToProperties.FindOrAdd(CategoryName).Add(Property);
	}

	// Let's do a simple alphabetical sort of the category keys
	TArray<FString> SortedCategories;
	CategoryToProperties.GetKeys(SortedCategories);
	SortedCategories.Sort();

	// We'll define some color heuristics for different types
	auto GetPropertyColor = [&](FProperty* Prop) -> FLinearColor
	{
		if (Prop->IsA<FFloatProperty>())
		{
			return FLinearColor(0.f, 1.f, 0.f, 1.f);   // Green for floats
		}
		if (Prop->IsA<FIntProperty>() || Prop->IsA<FInt64Property>() || Prop->IsA<FInt16Property>())
		{
			return FLinearColor(0.4f, 0.5f, 1.f, 1.f); // Blue for integers
		}
		if (Prop->IsA<FBoolProperty>())
		{
			return FLinearColor(1.f, .2f, .2f, 1.f);   // Red for bools
		}
		if (Prop->IsA<FStrProperty>() || Prop->IsA<FNameProperty>())
		{
			return FLinearColor(1.f, .7f, 1.f, 1.f);  // Rose for text
		} 
		// fallback
		return FLinearColor(0.8f, 0.8f, 0.8f, 1.f);
	};

	// 4) Now loop each category, print properties
	for (const FString& Cat : SortedCategories)
	{
		// Print a header for the category
		DrawDebugLine(FString::Printf(TEXT("  Category: %s"), *Cat), FLinearColor(1.f, 0.7f, 0.f, 1.f));
		
		const TArray<FProperty*>& Props = CategoryToProperties[Cat];

		// Sort property array by their name or display name, if desired
		TArray<FString> PropNames;
		TMap<FString, FProperty*> NameToProp;
		for (FProperty* P : Props)
		{
			FString DisplayName = P->GetDisplayNameText().ToString(); 
			if (DisplayName.IsEmpty())
			{
				DisplayName = P->GetName(); // fallback to internal name
			}
			PropNames.Add(DisplayName);
			NameToProp.Add(DisplayName, P);
		}
		PropNames.Sort();

		// Print each property in sorted order
		for (const FString& DisplayName : PropNames)
		{
			FProperty* Property = NameToProp[DisplayName];
			if (!Property)
			{
				continue;
			}

			// Retrieve the property value as string
			const void* PropValuePtr = Property->ContainerPtrToValuePtr<void>(LayoutStrategy);
			if (!PropValuePtr)
			{
				continue;
			}

			FString ValueString;
			UObject* PropOwner = Property->GetOwner<UObject>();
			Property->ExportText_Direct(ValueString, PropValuePtr, PropValuePtr, PropOwner, PPF_None);

			// Color
			FLinearColor Color = GetPropertyColor(Property);

			FString Line = FString::Printf(TEXT("    %s = %s"), *DisplayName, *ValueString);
			DrawDebugLine(Line, Color);
		}

		// Extra spacing after each category
		DrawPos.Y += 8.f;
	}
	
	LayoutStrategy->DrawDebugVisuals(AllottedGeometry, OutDrawElements, InLayerId, Center);
}

#endif // WITH_EDITOR