﻿// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <NativeGameplayTags.h>

#define TAG(Name) STRATEGYUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Name)

namespace StrategyUIGameplayTags
{
	namespace StrategyUI
	{
		namespace EntryLifecycle
		{
			TAG(Parent);
			TAG(Loading);
			TAG(Pooled);
			TAG(Deactivated);
			TAG(Active);
		}

		namespace EntryInteraction
		{
			TAG(Parent);
			TAG(Focused);
			TAG(Selected);
		}
	}
}