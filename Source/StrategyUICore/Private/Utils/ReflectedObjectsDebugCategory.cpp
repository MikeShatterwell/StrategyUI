#include "Utils/ReflectedObjectsDebugCategory.h"

#include <Engine/Canvas.h>
#include <Engine/Font.h>

#if WITH_GAMEPLAY_DEBUGGER

#include <Engine/Engine.h>
#include <UObject/UnrealType.h>	   // for TFieldIterator, FProperty, etc.
#include <UObject/Class.h>
#include <GameplayDebuggerAddonBase.h> // for AddTextLine(), AddShape(), etc.
#include <GameplayTagContainer.h>
#include <Containers/Map.h>

TSharedPtr<FReflectedObjectsDebugCategory> FReflectedObjectsDebugCategory::ActiveInstance = nullptr;


FReflectedObjectsDebugCategory::FReflectedObjectsDebugCategory()
{
	// How often to collect data (in seconds). 0 = every tick.
	CollectDataInterval = 0.f;
	bAllowLocalDataCollection = true;

	const FGameplayDebuggerInputHandlerConfig NextKey(TEXT("NextPage"), EKeys::PageUp.GetFName());
	const FGameplayDebuggerInputHandlerConfig PrevKey(TEXT("PrevPage"), EKeys::PageDown.GetFName());
	BindKeyPress(NextKey, this, &FReflectedObjectsDebugCategory::NextPage);
	BindKeyPress(PrevKey, this, &FReflectedObjectsDebugCategory::PrevPage);
}

void FReflectedObjectsDebugCategory::AddTargetObject(UObject* InObject)
{
	if (InObject)
	{
		TargetObjects.AddUnique(InObject);
	}
}

void FReflectedObjectsDebugCategory::ClearTargets()
{
	TargetObjects.Reset();
	ResetReplicatedData(); // clears old lines
}

void FReflectedObjectsDebugCategory::SetCategoryFilters(const TArray<FString>& InFilters)
{
	PropertyCategoryFilters = InFilters;
}

void FReflectedObjectsDebugCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	// 1) Clear old lines
	ResetReplicatedData();

	CachedLines.Empty(128);
	//CurrentPage = 0;

	// 2) If no target objects, note that
	if (TargetObjects.Num() == 0)
	{
		CachedLines.Add(TEXT("No target objects assigned to FStrategyUILocalDebugCategory."));
		return;
	}

	// 3) For each target, reflect
	for (TWeakObjectPtr<UObject>& WeakObj : TargetObjects)
	{
		UObject* Obj = WeakObj.Get();
		if (!Obj)
		{
			CachedLines.Add(TEXT("TargetObject is invalid (GCed?)."));
			continue;
		}

		// Title
		CachedLines.Add(FString::Printf(TEXT("=== Reflecting: %s (%s) ==="),
			*Obj->GetName(), *Obj->GetClass()->GetName()));

		// Reflect
		ReflectObjectProperties(Obj);

		// Blank line
		CachedLines.Add(TEXT(""));
	}
}

void FReflectedObjectsDebugCategory::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	// Determine how many lines fit on the screen based on canvas size and font height.
	const UCanvas* Canvas = CanvasContext.Canvas.Get();
	const float ScreenHeight = Canvas ? Canvas->SizeY : 1080.f;
	const int32 TotalLines = CachedLines.Num();
	
	LinesPerPage = FMath::FloorToInt(ScreenHeight / CharHeight);
	TotalPages = (LinesPerPage > 0) ? FMath::CeilToInt(static_cast<float>(TotalLines) / LinesPerPage) : 1;
	TotalPages = FMath::Max(TotalPages, 1);
	CurrentPage = FMath::Clamp(CurrentPage, 0, TotalPages - 1);

	const int32 StartIndex = CurrentPage * LinesPerPage;
	const int32 EndIndex = FMath::Min(StartIndex + LinesPerPage, TotalLines);

	// Draw only the lines for the current page.
	for (int32 i = StartIndex; i < EndIndex; i++)
	{
		CanvasContext.Printf(TEXT("%s"), *CachedLines[i]);
	}

	// Draw the page indicator on the bottom line.
	CanvasContext.Printf(TEXT("{gray}Page %d/%d"), CurrentPage + 1, TotalPages);
}

void FReflectedObjectsDebugCategory::NextPage()
{
	if (CurrentPage < TotalPages - 1)
	{
		++CurrentPage;
	}
}

void FReflectedObjectsDebugCategory::PrevPage()
{
	if (CurrentPage > 0)
	{
		--CurrentPage;
	}
}

void FReflectedObjectsDebugCategory::ReflectObjectProperties(UObject* Obj)
{
	if (!Obj)
	{
		CachedLines.Add(TEXT("Cannot reflect a null object."));
		return;
	}

	const UClass* Class = Obj->GetClass();
	if (!Class)
	{
		CachedLines.Add(TEXT("Object has no valid UClass?"));
		return;
	}

	// Group properties by Category
	TMap<FString, TArray<FProperty*>> CategoryMap;
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		FString PropCategoryName = Prop->GetMetaData(TEXT("Category"));
		if (PropCategoryName.IsEmpty())
		{
			PropCategoryName = TEXT("Uncategorized");
		}

		CategoryMap.FindOrAdd(PropCategoryName).Add(Prop);
	}

	// Sort category names
	TArray<FString> SortedCategories;
	CategoryMap.GetKeys(SortedCategories);
	SortedCategories.Sort();

	// For each Category
	for (const FString& Cat : SortedCategories)
	{
		bool bPassesFilter = (PropertyCategoryFilters.Num() == 0);
		if (!bPassesFilter)
		{
			for (const FString& Filter : PropertyCategoryFilters)
			{
				if (Cat.MatchesWildcard(*Filter))
				{
					bPassesFilter = true;
					break;
				}
			}
		}

		if (!bPassesFilter)
		{
			// skip this category
			continue;
		}

		// Print category header
		CachedLines.Add(FString::Printf(TEXT("[Category: %s]"), *Cat));

		// Sort the properties by display name
		TArray<FString> PropNames;
		TMap<FString, FProperty*> NameToProp;
		for (FProperty* P : CategoryMap[Cat])
		{
			if (!P) continue;
			FString DisplayName = P->GetDisplayNameText().ToString();
			if (DisplayName.IsEmpty())
			{
				DisplayName = P->GetName();
			}
			PropNames.Add(DisplayName);
			NameToProp.Add(DisplayName, P);
		}
		PropNames.Sort();

		// Print each property
		for (const FString& DisplayName : PropNames)
		{
			FProperty* Property = NameToProp[DisplayName];
			if (!Property)
			{
				continue;
			}

			// If property is a TMap, reflect it specially.
			if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
			{
				ReflectTMapProperty(MapProp, Obj, DisplayName);
			}
			else
			{
				// Normal property reflection
				const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Obj);
				if (!ValuePtr)
				{
					continue;
				}

				// Export property value to string
				FString ValueStr;
				Property->ExportText_Direct(ValueStr, ValuePtr, ValuePtr, Obj, PPF_None);

				// If it’s a GameplayTagContainer, convert it to string specially.
				if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
				{
					if (StructProp->Struct == TBaseStructure<FGameplayTagContainer>::Get())
					{
						const FGameplayTagContainer* TagContainer = static_cast<const FGameplayTagContainer*>(ValuePtr);
						if (TagContainer)
						{
							ValueStr = TagContainer->ToString();
						}
					}
				}

				// Get a color for this property type
				FLinearColor PropColor = GetPropertyColor(Property);
				const FString ColorMarkup = ToHexMarkup(PropColor);

				CachedLines.Add(
					 FString::Printf(TEXT("%s   %s = %s"), 
						 *ColorMarkup, // start color
						 *DisplayName, 
						 *ValueStr
					 )
				 );
			}
		}

		// Blank line after the category.
		CachedLines.Add(TEXT(""));
	}
}

void FReflectedObjectsDebugCategory::ReflectTMapProperty(const FMapProperty* MapProp, UObject* Obj, const FString& DisplayName)
{
	if (!MapProp || !Obj)
	{
		return;
	}
	const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(Obj);
	if (!MapPtr)
	{
		return;
	}

	FScriptMapHelper MapHelper(MapProp, MapPtr);
	const int32 Num = MapHelper.Num();

	// Print a summary line for the TMap.
	CachedLines.Add(FString::Printf(TEXT("   %s (TMap) has %d entries:"), *DisplayName, Num));

	FProperty* KeyProp = MapProp->KeyProp;
	FProperty* ValueProp = MapProp->ValueProp;

	for (int32 i = 0; i < MapHelper.GetMaxIndex(); i++)
	{
		if (!MapHelper.IsValidIndex(i))
		{
			continue;
		}

		const void* KeyPtr   = MapHelper.GetKeyPtr(i);
		const void* ValuePtr = MapHelper.GetValuePtr(i);

		// Convert the key to string.
		FString KeyStr;
		if (KeyProp)
		{
			if (const FClassProperty* ClassKeyProp = CastField<FClassProperty>(KeyProp))
			{
				FClassProperty::TCppType ClassVal = ClassKeyProp->GetPropertyValue(KeyPtr);
				KeyStr = ClassVal ? ClassVal->GetName() : TEXT("None");
			}
			else
			{
				KeyProp->ExportText_Direct(KeyStr, KeyPtr, KeyPtr, nullptr, PPF_None);
			}
		}

		// Convert the value to string.
		FString ValStr;
		if (ValueProp)
		{
			if (FStructProperty* StructValProp = CastField<FStructProperty>(ValueProp))
			{
				if (StructValProp->Struct == TBaseStructure<FGameplayTagContainer>::Get())
				{
					const FGameplayTagContainer* Tags = static_cast<const FGameplayTagContainer*>(ValuePtr);
					ValStr = Tags ? Tags->ToString() : TEXT("None");
				}
				else
				{
					ValueProp->ExportText_Direct(ValStr, ValuePtr, ValuePtr, nullptr, PPF_None);
				}
			}
			else if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(ValueProp))
			{
				const UObject* RefObj = ObjProp->GetObjectPropertyValue(ValuePtr);
				ValStr = RefObj ? RefObj->GetName() : TEXT("null");
			}
			else
			{
				ValueProp->ExportText_Direct(ValStr, ValuePtr, ValuePtr, nullptr, PPF_None);
			}
		}

		FLinearColor KeyColor = GetPropertyColor(KeyProp);
		FLinearColor ValColor = GetPropertyColor(ValueProp);

		const FString KeyMarkup = ToHexMarkup(KeyColor);
		const FString ValMarkup = ToHexMarkup(ValColor);

		CachedLines.Add(FString::Printf(TEXT("      %s[%s] => %s%s"),
			*KeyMarkup, *KeyStr,
			*ValMarkup, *ValStr
		));
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
