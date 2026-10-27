#pragma once
// Engine-independent rules shared by production code and the local policy test.
namespace StationExplorationPolicy
{
    constexpr int CleanMemories(int Mask){return Mask&63;}
    constexpr int CleanFilms(int Mask){return Mask&7;}
    constexpr int NextReviewRole(int Mask)
    {return !(Mask&1)?0:!(Mask&2)?1:!(Mask&4)?2:-1;}
    constexpr bool CanReview(int Phase,int Allocation,int Ration,bool Recall)
    {return !Recall&&(Phase==0?Allocation>=0&&Allocation<2:Phase==1&&Ration>=0&&Ration<=7);}
    constexpr bool CanShowConsequences(bool InService,bool Signed,int VisitMask,bool Recall,int Seen,bool Autoplay)
    {return InService&&Signed&&(VisitMask&7)==7&&!Recall&&!(Seen&2)&&Autoplay;}
    constexpr bool HasMemory(int Mask,int Index)
    {return Index>=0&&Index<6&&(Mask&(1<<Index))!=0;}
}
