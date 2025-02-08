// Copyright Mike Desrosiers 2025, All Rights Reserved.

#include "Utils/StrategyUIGameplayTags.h"

namespace StrategyUIGameplayTags
{
	namespace StrategyUI
	{
		namespace EntryLifecycle
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Parent,
				"StrategyUI.EntryLifecycle",
				"Used to refer to the StrategyUI.EntryState parent tag.");
	
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Pooled,
				"StrategyUI.EntryLifecycle.Pooled",
				"Not in the viewport or any parent; no visuals at all. Mutually exclusive with other StrategyUI.EntryLifecycle.* tags.");
	
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Deactivated,
				"StrategyUI.EntryLifecycle.Deactivated",
				"In the viewport but hidden. Mutually exclusive with other StrategyUI.EntryLifecycle.* tags.");
	
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Active,
				"StrategyUI.EntryLifecycle.Active",
				"Fully visible. Mutually exclusive with other StrategyUI.EntryLifecycle.* tags.");
		}

		namespace EntryInteraction
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Parent,
				"StrategyUI.EntryInteraction",
				"Used to refer to the StrategyUI.EntryInteraction parent tag.");
			
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Focused,
				"StrategyUI.EntryInteraction.Focused",
				"Currently focused/hovered. Additive with other StrategyUI.EntryInteraction.* tags.");

			UE_DEFINE_GAMEPLAY_TAG_COMMENT(
				Selected,
				"StrategyUI.EntryInteraction.Selected",
				"Currently selected. Additive with other StrategyUI.EntryInteraction.* tags.");
		}
	}
}