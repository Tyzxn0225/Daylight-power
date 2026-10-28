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
            // Keep only the symbol filename in the PE debug directory. An explicit
            // alternate path also applies to Development game executables.
            WindowsPlatform.PdbAlternatePath = "%_PDB%";
            AdditionalCompilerArguments = (AdditionalCompilerArguments ?? "") +
                " /pathmap:\"" + ProjectFile.Directory.FullName + "=DaylightRights\"";
        }
    }
}
