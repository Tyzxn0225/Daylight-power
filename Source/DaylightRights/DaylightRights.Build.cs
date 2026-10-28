using UnrealBuildTool;
using System.IO;
public class DaylightRights : ModuleRules
{
    public DaylightRights(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "Json", "NavigationSystem", "MediaAssets", "AudioMixer" });
        // Stage exactly the runtime PNGs. Whole-directory staging would also
        // copy editor-created .uasset files and their source-import metadata.
        string RuntimeImages = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../Content/BZ/RuntimeImages"));
        if (Directory.Exists(RuntimeImages))
        {
            foreach (string Image in Directory.GetFiles(RuntimeImages, "*.png", SearchOption.AllDirectories))
                RuntimeDependencies.Add(Image, StagedFileType.NonUFS);
        }
    }
}
