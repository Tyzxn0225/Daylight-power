#pragma once
// Engine-independent policy: test this without mutating or launching the UE project.
namespace StationNarrative
{
enum class Region { Street, Rich, ReturnLight };
struct Outcome
{
    bool started=false, readRecall=false, conflictComplete=false, supplyComplete=false, dutySettled=false, districtSettled=false;
    int allocation=-1, ration=-1, conflict=-1, supply=-1, district=-1;
};
enum class Memory { None, OriginalGuarantee, NamedListPublished, RulesOnly, HousingLit, HousingWaiting, RegulatorKept, RegulatorReturned, LaborSettled, ReserveRestored, TrialAndDeferredRefund, RefundAndNoTrial };
inline Memory LatestMemory(const Outcome& s)
{
    if(s.districtSettled&&s.district>=0&&s.district<2)return s.district==0?Memory::TrialAndDeferredRefund:Memory::RefundAndNoTrial;
    if(s.dutySettled&&s.supply>=0&&s.supply<2)return s.supply==1?Memory::LaborSettled:Memory::ReserveRestored;
    if(s.conflictComplete&&s.conflict>=0&&s.conflict<2)return s.conflict==0?Memory::RegulatorKept:Memory::RegulatorReturned;
    if(s.readRecall&&s.ration>=0)return s.ration&4?Memory::HousingLit:Memory::HousingWaiting;
    if(s.allocation>=0&&s.allocation<2)return s.allocation==0?Memory::NamedListPublished:Memory::RulesOnly;
    return s.started?Memory::OriginalGuarantee:Memory::None;
}
inline Region NextRegion(Region current,double x,double y)
{
    if(current==Region::ReturnLight&&y>6000)return current;
    if(y>6400)return Region::ReturnLight;
    if(current==Region::Rich&&x>1050&&y<4150)return current;
    if(x>1350&&y<3850)return Region::Rich;
    return Region::Street;
}
inline bool ExplorationMusicWindow(double seconds)
{
    if(seconds<0)return false;
    const int cycles=static_cast<int>(seconds/96.0);
    const double phase=seconds-cycles*96.0;
    return phase<64.0; // Leave room for wind, footsteps and the lived environment.
}
inline bool CanStartVignette(bool enabled,bool blocked,bool voicePlaying,double quietSeconds,double cooldown,int seenMask,int bit,double distance,double speed)
{
    return enabled&&!blocked&&!voicePlaying&&quietSeconds>=3.0&&cooldown<=0&&(seenMask&bit)==0&&bit>0&&distance<=650&&speed<180;
}
inline bool MayRememberVignette(bool typing,bool voicePlaying,double elapsed,double minimum)
{
    return !typing&&!voicePlaying&&elapsed>=minimum;
}
}
