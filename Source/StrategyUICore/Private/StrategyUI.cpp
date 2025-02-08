// Copyright Mike Desrosiers 2025, All Rights Reserved.

#include "StrategyUI.h"
#include <Modules/ModuleManager.h>

#if WITH_GAMEPLAY_DEBUGGER
#include <GameplayDebugger.h>

#include "Utils/ReflectedObjectsDebugCategory.h"
static const FName StrategyUIDebugCategoryName("StrategyUI_Debug");
#endif

#define LOCTEXT_NAMESPACE "FStrategyUIModule"


void FStrategyUIModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	// Register the gameplay debugger category
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory(
		StrategyUIDebugCategoryName,
		IGameplayDebugger::FOnGetCategory::CreateStatic(&FReflectedObjectsDebugCategory::MakeInstance),
		EGameplayDebuggerCategoryState::EnabledInGame,
		9
	);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FStrategyUIModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory(StrategyUIDebugCategoryName);
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FStrategyUIModule, StrategyUI)
