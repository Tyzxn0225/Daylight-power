using UnrealBuildTool;
using System.Collections.Generic;
public class DaylightRightsEditorTarget : TargetRules
{
    public DaylightRightsEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("DaylightRights");
    }
}
