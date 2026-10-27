#pragma once
#include <array>
#include <algorithm>

// Pure puzzle rules: no save writes, time pressure, resource debit or world side effects.
namespace StationSurvey
{
enum class Kind { Align, Pulse, Flow, Seal };
struct State
{
    Kind Type=Kind::Align;
    int Seed=0, Axis=0;
    std::array<int,3> Value{0,0,0};
};
inline int Wrap(int V,int N){return (V%N+N)%N;}
inline int Channels(Kind K){return K==Kind::Pulse?2:3;}
inline std::array<int,3> Answer(const State& S)
{
    if(S.Type==Kind::Pulse)return {3+S.Seed%10,S.Seed%8,0};
    if(S.Type==Kind::Flow)return {2+S.Seed%2,2+(S.Seed/2)%2,1};
    if(S.Type==Kind::Seal)return {S.Seed%3,(S.Seed+1)%3,(S.Seed+2)%3};
    return {0,0,0};
}
inline State Begin(Kind K,int Seed)
{
    State S;S.Type=K;S.Seed=Seed;
    if(K==Kind::Align)S.Value={3+Seed%3,-3-Seed%2,2+Seed%3};
    if(K==Kind::Flow)S.Value={1,1,1};
    return S;
}
inline void Select(State& S,int Direction){S.Axis=Wrap(S.Axis+Direction,Channels(S.Type));}
inline void Adjust(State& S,int Direction)
{
    int& V=S.Value[S.Axis];
    if(S.Type==Kind::Align)V=std::clamp(V+Direction,-8,8);
    else if(S.Type==Kind::Pulse)V=Wrap(V+Direction,S.Axis==0?16:8);
    else if(S.Type==Kind::Flow)V=std::clamp(V+Direction,0,6);
    else V=Wrap(V+Direction,3);
}
// Zero is success. Positive values identify the unmet condition shown to the player.
inline int Check(const State& S)
{
    const auto A=Answer(S);
    if(S.Type==Kind::Flow)
    {
        if(S.Value[0]+S.Value[1]+S.Value[2]>8)return 4;
        for(int I=0;I<3;++I)if(S.Value[I]<A[I])return I+1;
        return 0;
    }
    for(int I=0;I<Channels(S.Type);++I)if(S.Value[I]!=A[I])return I+1;
    return 0;
}
inline bool Has(int Mask,int I){return I>=0&&I<15&&(Mask&(1<<I));}
inline int Count(int Mask,int Start,int End){int N=0;for(int I=Start;I<End;++I)N+=Has(Mask,I)?1:0;return N;}
inline bool CanCloseArchive(int Mask){return Count(Mask,0,7)>=2&&Count(Mask,7,11)>=2&&Count(Mask,11,14)>=2;}
inline bool Available(int I,bool Connector,bool Started,bool Recall,bool DutySettled,int Mask)
{
    if(I<0||I>=15)return false;
    if(I==2||I==3||I==4)return Recall;
    if(I==6||I>=7&&I<=10)return Started;
    if(I==14)return DutySettled&&CanCloseArchive(Mask);
    if(I>=11)return DutySettled;
    return Connector;
}
inline int Next(int Solved,int AvailableMask,int ExistingMask,int Tracked,int Region)
{
    const auto Eligible=[&](int I){return Has(AvailableMask,I)&&Has(ExistingMask,I)&&!Has(Solved,I);};
    if(Eligible(Tracked))return Tracked;
    static const int Circuit[]={0,1,5,3,6,2,4,7,9,10,8,11,12,13,14};
    for(int R=std::clamp(Region,0,2);R<3;++R)for(int I:Circuit)
        if((I>=11?2:I>=7?1:0)==R&&Eligible(I))return I;
    return -1;
}
}
