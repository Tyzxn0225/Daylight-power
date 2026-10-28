using UnrealBuildTool;
using System.Collections.Generic;
public class DaylightRightsTarget : TargetRules
{
    public DaylightRightsTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("DaylightRights");

        if (Platform == UnrealTargetPlatform.Win64 && ProjectFile != null &&
            (WindowsPlatform.Compiler == WindowsCompiler.Default || WindowsPlatform.Compiler.IsMSVC()))
        {
            // Preserve useful source locations without embedding this workstation's
            // project directory in assertions, diagnostics, or packaged binaries.
            // The override is limited to path/debug information; no ABI settings
            // or installed engine source are changed by this game target.
            bOverrideBuildEnvironment = true;
            bDeterministic = true;
            AdditionalCompilerArguments = (AdditionalCompilerArguments ?? "") +
                " /pathmap:\"" + ProjectFile.Directory.FullName + "=DaylightRights\"";
        }
    }
}
