#pragma once
namespace StationExperience
{
    constexpr int SettingsCount=16,SettingsPerPage=8,LogCapacity=64,LogPerPage=4;
    constexpr int CleanUrbanMask(int Value){return Value&7;}
    constexpr int CleanQuietMask(int Value){return Value&7;}
    constexpr int ClampVolume(int Value){return Value<0?0:Value>10?10:Value;}
    constexpr int SettingsPage(int Selected){return Selected>=0&&Selected<SettingsCount?Selected/SettingsPerPage:0;}
    constexpr bool RowVisible(int Selected,int Row){return Row>=0&&Row<SettingsCount&&SettingsPage(Selected)==SettingsPage(Row);}
    constexpr double FocusScore(double Aim,double Distance,bool Previous){return Aim*100-Distance/1000+(Previous?1.75:0);}
    constexpr bool ShowGuide(int Mode,bool LegacyEnabled,double Distance,bool Visible)
    {return LegacyEnabled&&(Mode==2||(Mode==1&&Distance<=1200&&Visible));}
    constexpr bool RememberQuiet(double Seconds,bool Interrupted){return Seconds>=10&&!Interrupted;}
    constexpr int ServiceState(int RationMask,int Index,bool Fault,int FaultIndex,bool Restored)
    {return Index<0||Index>2||RationMask<0?-1:!(RationMask&(1<<Index))?0:Fault&&Index==FaultIndex&&!Restored?3:1;}
    constexpr int WorkPoster(bool Settled,int SupplyChoice){return !Settled||SupplyChoice<0||SupplyChoice>1?-1:SupplyChoice;}
    constexpr int LogPages(int Count){return Count>0?((Count>LogCapacity?LogCapacity:Count)+LogPerPage-1)/LogPerPage:1;}
}
