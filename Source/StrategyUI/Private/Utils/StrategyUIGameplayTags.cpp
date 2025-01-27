#include "Utils/StrategyUIGameplayTags.h"

namespace StrategyUIGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		StrategyUI_EntryState_Pooled,
		"StrategyUI.EntryState.Pooled",
		"Not in the viewport or any parent; no visuals at all.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		StrategyUI_EntryState_Deactivated,
		"StrategyUI.EntryState.Deactivated",
		"In the viewport but hidden.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		StrategyUI_EntryState_Active,
		"StrategyUI.EntryState.Active",
		"Fully visible.");
}