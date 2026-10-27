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
    }
}
