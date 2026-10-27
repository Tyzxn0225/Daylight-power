using UnrealBuildTool;
public class DaylightRights : ModuleRules
{
    public DaylightRights(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "Json", "NavigationSystem", "MediaAssets", "AudioMixer" });
    }
}
