using UnrealBuildTool;

public class StrategyUIExamples : ModuleRules
{
    public StrategyUIExamples(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", 
                "StrategyUI",
                "StrategyUIExamples"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "UMG",
                "GameplayTags",
                "DeveloperSettings"
            }
        );
    }
}