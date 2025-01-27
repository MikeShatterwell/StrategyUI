#pragma once

#include <NativeGameplayTags.h>

#define TAG(Name) STRATEGYUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Name)

namespace StrategyUIGameplayTags
{
	TAG(StrategyUI_EntryState_Pooled);
	TAG(StrategyUI_EntryState_Deactivated);
	TAG(StrategyUI_EntryState_Active);
}