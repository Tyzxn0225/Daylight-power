#include "StationPrototype.h"
#include "StationSurveyData.inl"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void AStationDirector::CreateSurveyWorld()
{
    SurveySources.SetNum(StationSurveyData::Count);
    for(int32 I=0;I<StationSurveyData::Count;++I)
    {
        const auto& Site=StationSurveyData::Sites[I];if(Site.Source!=-1)continue;
        // Garden probe faces the open eastern approach, away from the low planting in front.
        const FTransform T(FRotator(0,I==8?-90.f:0.f,0),Site.Position);
        auto* O=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        O->Kind=EStationObjectKind::SurveyCabinet;O->DisplayName=Site.Place;O->Tags.Add(TEXT("SurveyV29"));
        O->FinishSpawning(T);Objects.Add(O);SurveySources[I]=O;
        O->SetIndicator(FLinearColor(.28f,.57f,.52f),.9f);
    }
    Progress->SurveySolvedMask&=(1<<StationSurveyData::Count)-1;
    if(Progress->SurveyTracked<0||Progress->SurveyTracked>=StationSurveyData::Count)Progress->SurveyTracked=-1;
    SyncSurveySources();
}
void AStationDirector::SyncSurveySources()
{
    for(int32 I=0;I<SurveySources.Num();++I)
    {
        const int32 Source=StationSurveyData::Sites[I].Source;
        if(Source>=0&&Source<6&&ExplorationObjects.IsValidIndex(Source))SurveySources[I]=ExplorationObjects[Source];
        if(Source>=10&&Source<13&&UrbanObjects.IsValidIndex(Source-10))SurveySources[I]=UrbanObjects[Source-10];
        if(Source>=20&&Source<26&&DistrictObjects.IsValidIndex(Source-20))SurveySources[I]=DistrictObjects[Source-20];
        if(Source==-1&&SurveySources[I])
        {
            const bool Available=SurveyAvailable(I);
            SurveySources[I]->Active=Available;SurveySources[I]->SetActorHiddenInGame(!Available);SurveySources[I]->SetActorEnableCollision(Available);
            SurveySources[I]->SetIndicator(SurveySolved(I)?FLinearColor(.17f,.3f,.29f):FLinearColor(.28f,.57f,.52f),SurveySolved(I)?.15f:.9f);
        }
    }
}
bool AStationDirector::SurveyAvailable(int32 I) const
{
    return Progress&&StationSurvey::Available(I,Progress->HasConnector,Progress->Started,Progress->ReadRecall,Progress->DutySettled,Progress->SurveySolvedMask);
}
bool AStationDirector::SurveySolved(int32 I) const{return Progress&&StationSurvey::Has(Progress->SurveySolvedMask,I);}
int32 AStationDirector::SurveySiteOf(AActor* Actor) const
{
    if(!Actor)return -1;if(Actor==ConsignmentCase)return 4;
    for(int32 I=0;I<SurveySources.Num();++I)if(SurveySources[I]==Actor)return I;
    return -1;
}
int32 AStationDirector::NextSurveySite() const
{
    if(!Progress)return -1;
    const FVector Here=PC&&PC->GetPawn()?PC->GetPawn()->GetActorLocation():FVector::ZeroVector;
    const int32 Region=Here.Y>6000?2:Here.X>900?1:0;
    int32 AvailableMask=0,ExistingMask=0;
    for(int32 I=0;I<SurveySources.Num();++I){if(SurveyAvailable(I))AvailableMask|=1<<I;if(IsValid(SurveySources[I]))ExistingMask|=1<<I;}
    return StationSurvey::Next(Progress->SurveySolvedMask,AvailableMask,ExistingMask,Progress->SurveyTracked,Region);
}
AActor* AStationDirector::GetSurveyGuidance() const
{
    if(!Progress||!Progress->SurveyRouteEnabled||Panel==EStationPanel::ArcScene||Panel==EStationPanel::ArcCredits||Starting||IsStoryFilmActive()||IsPhysicalActionActive())return nullptr;
    const int32 I=NextSurveySite();return SurveySources.IsValidIndex(I)?SurveySources[I].Get():nullptr;
}
FString AStationDirector::SurveyObjective() const
{
    const int32 I=NextSurveySite();if(I<0)return TEXT("");
    return FString(StationSurveyData::Sites[I].Title)+TEXT("\n前往")+StationSurveyData::Sites[I].Place+(I==6?TEXT("，由放映间外楼梯上行。"):TEXT("。"))+TEXT("K 切回主线。");
}
void AStationDirector::TickSurvey(float Delta)
{
    SurveyPoll-=Delta;if(SurveyPoll>0)return;SurveyPoll=.5f;SyncSurveySources();
    if(Progress&&Progress->SurveyRouteEnabled&&NextSurveySite()<0)
    {
        Progress->SurveyRouteEnabled=false;Progress->SurveyTracked=-1;SaveProgress();
        Notify(SurveySolved(14)?TEXT("巡光档案已连通。已恢复主线指引；K 可补看各处记录。"):TEXT("本片区路线告一段落。已恢复主线指引；K 可主动追踪遗漏现场，无须折返补齐。"));
    }
}
bool AStationDirector::HandleSurveyObject(AStationObject* Object)
{
    const int32 I=SurveySiteOf(Object);if(I<0||!SurveyAvailable(I))return false;
    if(Object==ConsignmentCase&&Progress->ConflictChoice>=0&&!Progress->ConflictDelivered)return false;
    const bool MainTrace=I>=2&&I<=4&&IsConflictChapter()&&Progress->ConflictStarted&&Progress->ConflictChoice<0;
    if(I==2&&MainTrace&&!Progress->KitchenShortcutOpen)return false;
    const int32 Trace=I==2?0:I==3?1:I==4?2:-1;
    if(MainTrace&&Object->Kind!=EStationObjectKind::SurveyCabinet&&StationSurvey::Has(Progress->ConflictTraceMask,Trace)&&!Progress->SurveyRouteEnabled)return false;
    if(!MainTrace&&IsArcChapter()&&GetArcGuidance()==Object&&!Progress->SurveyRouteEnabled)return false;
    if(I>=11&&!Progress->SurveyRouteEnabled&&Object->Kind!=EStationObjectKind::SurveyCabinet)return false;
    if(SurveySolved(I))
    {
        if(Object->Kind!=EStationObjectKind::SurveyCabinet)return false;
        SurveyActive=I;SurveyFeedback.Empty();OpenPanel(EStationPanel::SurveyRecord);return true;
    }
    SurveyActive=I;SurveyState=StationSurvey::Begin(StationSurveyData::Sites[I].Type,I+1);
    SurveyFeedback.Empty();SurveyShowHint=false;OpenPanel(EStationPanel::SurveyGame);
    PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.12f,false);
    if(SurveyState.Type==StationSurvey::Kind::Pulse)PlaySurveyPulse();return true;
}
void AStationDirector::RequestSurveyBook()
{
    if(Panel==EStationPanel::SurveyBook){CloseSurvey();return;}
    if(IsMenuOpen()||IsStoryFilmActive()||IsPhysicalActionActive()||Starting||QuietIndex>=0)return;
    OpenPanel(EStationPanel::SurveyBook);Selection=0;
}
void AStationDirector::SurveySelect(int32 Direction)
{
    if(Panel==EStationPanel::SurveyGame)StationSurvey::Select(SurveyState,Direction);
    else if(Panel==EStationPanel::SurveyBook)Selection=StationSurvey::Wrap(Selection+Direction,StationSurveyData::Count+1);
    else return;
    PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.055f,false);
}
void AStationDirector::SurveyLeft(){SurveyAdjust(-1);}
void AStationDirector::SurveyRight(){SurveyAdjust(1);}
void AStationDirector::SurveyAdjust(int32 Direction)
{
    if(Panel!=EStationPanel::SurveyGame)return;
    StationSurvey::Adjust(SurveyState,Direction);SurveyFeedback.Empty();PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.055f,false);
}
void AStationDirector::SurveyHint()
{
    if(Panel!=EStationPanel::SurveyGame)return;
    SurveyShowHint=!SurveyShowHint;if(SurveyState.Type==StationSurvey::Kind::Pulse)PlaySurveyPulse();
}
void AStationDirector::PressSurvey()
{
    if(Panel==EStationPanel::SurveyBook)
    {
        if(Selection==0){Progress->SurveyRouteEnabled=false;Progress->SurveyTracked=-1;SaveProgress();ClosePanel();Notify(TEXT("已切回主线。调查记录会保留。"));return;}
        const int32 I=Selection-1;
        if(SurveySolved(I)){SurveyActive=I;OpenPanel(EStationPanel::SurveyRecord);return;}
        if(!SurveyAvailable(I)){Notify(TEXT("此现场尚未开放。先推进主线或收集三地各两份记录。"),true);return;}
        Progress->SurveyTracked=I;Progress->SurveyRouteEnabled=true;SaveProgress();ClosePanel();Notify(TEXT("开始追踪：")+FString(StationSurveyData::Sites[I].Place));return;
    }
    if(Panel==EStationPanel::SurveyRecord){CloseSurvey();return;}
    if(Panel!=EStationPanel::SurveyGame||SurveyActive<0||SurveyActive>=StationSurveyData::Count)return;
    const int32 Error=StationSurvey::Check(SurveyState);
    if(Error)
    {
        if(SurveyState.Type==StationSurvey::Kind::Align)SurveyFeedback=TEXT("底片定位十字仍未重合。检查横移、纵移和转角三个调节。");
        else if(SurveyState.Type==StationSurvey::Kind::Pulse)SurveyFeedback=Error==1?TEXT("探针频率与铭牌不一致。先调频率，再对齐脉冲相位。"):TEXT("频率相同，但脉冲峰值仍错开。调整相位。");
        else if(SurveyState.Type==StationSurvey::Kind::Flow)SurveyFeedback=Error==4?TEXT("支出超过 8 格。总量固定，不能把三个阀门全开。"):FString::Printf(TEXT("第 %d 支路扣除管损后仍低于最低需求。"),Error);
        else SurveyFeedback=FString::Printf(TEXT("第 %d 项结论超出了原件能证明的范围。重新对读原文。"),Error);
        PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.09f,false);return;
    }
    const int32 I=SurveyActive;const bool First=!SurveySolved(I);
    Progress->SurveySolvedMask|=1<<I;
    if(Progress->SurveyTracked==I){Progress->SurveyTracked=-1;if(Progress->SurveyRouteEnabled)Progress->SurveyTracked=NextSurveySite();}
    StopSurveyPulse();SaveProgress();SyncSurveySources();if(First)PlayCue(TEXT("UI_Stored"),FVector::ZeroVector,.25f,false);
    if(I>=2&&I<=4&&IsConflictChapter()&&Progress->ConflictStarted&&Progress->ConflictChoice<0&&ConflictTraces.IsValidIndex(I-2))
    {
        ClosePanel();Focus=ConflictTraces[I-2];ConversationTarget=Focus;SurveyActive=-1;
        HandleConflictObject(ConflictTraces[I-2]);Notify(TEXT("实物核验完成。读完原件并按 E，才会记入本章调查。"));return;
    }
    OpenPanel(EStationPanel::SurveyRecord);SurveyFeedback.Empty();
}
void AStationDirector::CloseSurvey(){StopSurveyPulse();SurveyActive=-1;SurveyFeedback.Empty();SurveyShowHint=false;ClosePanel();}
void AStationDirector::StopSurveyPulse(){if(IsValid(SurveyAudio)){SurveyAudio->Stop();SurveyAudio->DestroyComponent();}SurveyAudio=nullptr;}
void AStationDirector::PlaySurveyPulse()
{
    StopSurveyPulse();if(SurveyState.Type!=StationSurvey::Kind::Pulse)return;
    auto* Wave=NewObject<USoundWaveProcedural>(this);Wave->SetSampleRate(22050);Wave->NumChannels=1;Wave->Duration=2.2f;Wave->bLooping=false;
    TArray<int16> PCM;PCM.SetNumZeroed(48510);const auto A=StationSurvey::Answer(SurveyState);
    for(int32 N=0;N<PCM.Num();++N)
    {
        const float T=float(N)/22050.f;const bool Probe=T>=1.1f;const float Local=T-(Probe?1.1f:0.f);
        const int32 F=Probe?SurveyState.Value[0]:A[0];const int32 Phase=Probe?SurveyState.Value[1]:A[1];
        const float Cycle=FMath::Fmod(Local*4.f+Phase/8.f,1.f);
        const float Gate=Cycle<.18f?FMath::Sin(Cycle/.18f*PI):0.f;const float Fade=FMath::Clamp((1.f-Local)*15.f,0.f,1.f);
        PCM[N]=int16(FMath::Sin(2.f*PI*(220.f+F*28.f)*Local)*Gate*Fade*4200.f);
    }
    Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()),PCM.Num()*sizeof(int16));
    SurveyAudio=UGameplayStatics::SpawnSound2D(this,Wave,FMath::Clamp(EffectsVolume,0.f,1.f)*.65f);
}
FString AStationDirector::GetSurveyStateJson() const
{
    auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("solved_mask"),Progress?Progress->SurveySolvedMask:0);
    Root->SetNumberField(TEXT("reserve"),Progress?Progress->Reserve:0);Root->SetNumberField(TEXT("main_trace_mask"),Progress?Progress->ConflictTraceMask:0);
    Root->SetNumberField(TEXT("tracked"),Progress?Progress->SurveyTracked:-1);Root->SetBoolField(TEXT("route_enabled"),Progress&&Progress->SurveyRouteEnabled);
    Root->SetNumberField(TEXT("active"),SurveyActive);Root->SetNumberField(TEXT("panel"),int32(Panel));Root->SetNumberField(TEXT("axis"),SurveyState.Axis);
    TArray<TSharedPtr<FJsonValue>> Values;for(int V:SurveyState.Value)Values.Add(MakeShared<FJsonValueNumber>(V));Root->SetArrayField(TEXT("values"),Values);
    Root->SetNumberField(TEXT("check"),StationSurvey::Check(SurveyState));Root->SetStringField(TEXT("feedback"),SurveyFeedback);
    TArray<TSharedPtr<FJsonValue>> Sites;
    for(int32 I=0;I<StationSurveyData::Count;++I)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("id"),I);Row->SetStringField(TEXT("title"),StationSurveyData::Sites[I].Title);
        Row->SetBoolField(TEXT("available"),SurveyAvailable(I));Row->SetBoolField(TEXT("solved"),SurveySolved(I));
        auto* O=SurveySources.IsValidIndex(I)?SurveySources[I].Get():nullptr;Row->SetStringField(TEXT("actor"),IsValid(O)?O->GetName():TEXT(""));
        if(O){Row->SetNumberField(TEXT("x"),O->GetActorLocation().X);Row->SetNumberField(TEXT("y"),O->GetActorLocation().Y);Row->SetNumberField(TEXT("z"),O->GetActorLocation().Z);}
        Sites.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("sites"),Sites);FString Out;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Out));return Out;
}
void AStationDirector::AutomationSurveyPreview(bool MainInvestigation)
{
#if WITH_EDITOR
    if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return;
    AutomationMandatePreview(0);CloseSurvey();
    Progress->ArcStarted=false;Progress->ArcFinished=false;Progress->CrisisStarted=false;Progress->CrisisEndingHeard=false;
    Progress->MandateStarted=false;Progress->MandateEndingHeard=false;Progress->SurveySolvedMask=0;Progress->SurveyTracked=-1;
    Progress->SurveyRouteEnabled=!MainInvestigation;Progress->DutySettled=!MainInvestigation;
    Progress->Started=true;Progress->HasConnector=true;Progress->ReadRecall=true;
    Progress->SeenAftermathFilm=true;Progress->ConflictStarted=MainInvestigation;
    if(MainInvestigation)
    {
        Progress->SupplyStarted=false;Progress->SupplyComplete=false;Progress->DutyStarted=false;Progress->DistrictStarted=false;Progress->DistrictSettled=false;
        Progress->ConflictTraceMask=0;Progress->ConflictWitness=0;Progress->ConflictEvidence=false;Progress->ConflictHeardMask=0;
        Progress->ConflictOffer=false;Progress->ConflictChoice=-1;Progress->ConflictPartHeld=false;Progress->ConflictDelivered=false;Progress->ConflictComplete=false;Progress->KitchenShortcutOpen=false;
    }
    ApplyProgress();SyncSurveySources();
#endif
}
bool AStationDirector::AutomationSurveyRoundTrip() const
{
#if WITH_EDITOR
    if(!Progress||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return false;
    TArray<uint8> Bytes;if(!UGameplayStatics::SaveGameToMemory(Progress,Bytes))return false;
    const auto* Copy=Cast<UStationProgress>(UGameplayStatics::LoadGameFromMemory(Bytes));
    return Copy&&Copy->SurveySolvedMask==Progress->SurveySolvedMask&&Copy->SurveyTracked==Progress->SurveyTracked&&Copy->SurveyRouteEnabled==Progress->SurveyRouteEnabled&&Copy->Reserve==Progress->Reserve&&Copy->ConflictTraceMask==Progress->ConflictTraceMask;
#else
    return false;
#endif
}
