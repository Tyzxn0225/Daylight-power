#include "StationPrototype.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"

void AStationDirector::CreateScore()
{
    const TCHAR* Names[]={TEXT("MU14_Streets"),TEXT("MU14_Rich"),TEXT("MU14_Return"),TEXT("MU14_Pressure"),TEXT("MU14_Family")};
    for(const TCHAR* Name:Names)
    {
        auto* Audio=MakeLoop(Name,FVector::ZeroVector,0,false);Audio->Stop();ScoreLayers.Add(Audio);ScoreGains.Add(0.f);
    }
}
void AStationDirector::TickScore(float Delta)
{
    if(ScoreLayers.Num()!=5||!PC||!PC->GetPawn())return;
    const bool InFilm=IsStoryFilmActive()||Starting;
    if(!InFilm)
    {
        const FVector P=PC->GetPawn()->GetActorLocation();
        const int32 Next=int32(StationNarrative::NextRegion(static_cast<StationNarrative::Region>(ScoreRegion),P.X,P.Y));
        if(Next!=ScoreRegion){ScoreRegion=Next;ScoreRegionElapsed=0;}
        ScoreRegionElapsed+=Delta;
    }
    const bool Pressure=!GetDecisionPreview().IsEmpty()||Panel==EStationPanel::RationPlan||(IsCrisisChapter()&&Progress->CrisisStarted&&Progress->CrisisAllocation<0)||(Progress->DutyFaultRaised&&!Progress->DutyCommunityRestored);
    const bool ArcScene=Panel==EStationPanel::ArcScene;
    const bool ArcPressure=IsArcChapter()&&!Progress->ArcFinished&&GetArcFact(TEXT("gate_blocked"))&&!GetArcFact(TEXT("world_committed"));
    const int32 Target=ArcPressure?3:ArcScene?4:Pressure?3:(QuietIndex==0||QuietIndex==2)?4:ScoreRegion;
    const bool Window=ArcScene||Panel==EStationPanel::ArcCredits||ArcPressure||QuietIndex>=0||Pressure||StationNarrative::ExplorationMusicWindow(ScoreRegionElapsed);
    for(int32 I=0;I<5;++I)
    {
        const float Goal=!InFilm&&Window&&I==Target?(I==3?.18f:I==4?.15f:.21f):0.f;
        ScoreGains[I]=FMath::FInterpTo(ScoreGains[I],Goal,Delta,.7f);
        if(auto* Audio=ScoreLayers[I].Get())
        {
            const float Gain=ScoreGains[I]*VoiceMix*MusicVolume;Audio->SetVolumeMultiplier(Gain);
            if(Gain>.001f){if(!Audio->IsPlaying())Audio->Play();}
            else if(Audio->IsPlaying())Audio->Stop();
        }
    }
}
