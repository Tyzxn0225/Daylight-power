#include "StationPrototype.h"
#include "StationFrontend.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

namespace StationPresentation
{
    struct FStationVoiceEntry { const TCHAR* Text; const TCHAR* Asset; };
    #include "StationVoiceData.inl"
    USoundBase* Sound(const FString& Name)
    {
        // EV_ is the selected expressive cast, VR_ recorded reactions, HR_ future recordings.
        // Legacy VO_ SAPI placeholders remain excluded.
        if(!Name.StartsWith(TEXT("EV_"))&&!Name.StartsWith(TEXT("VR_"))&&!Name.StartsWith(TEXT("HR_")))return nullptr;
        return LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/BZ/Audio/%s.%s"),*Name,*Name));
    }
}

FString AStationDirector::VoiceForText(const FString& Text) const
{
    const FString ArcVoice=GetArcVoiceForText(Text);if(!ArcVoice.IsEmpty())return ArcVoice;
    for(const auto& Entry:StationPresentation::VoiceEntries)if(Text==Entry.Text)return Entry.Asset;
    const FString NewVoice=GetCrisisVoiceForText(Text);return NewVoice.IsEmpty()?GetMandateVoiceForText(Text):NewVoice;
}
bool AStationDirector::IsVoicePlaying() const { return DialogueAudio&&DialogueAudio->IsPlaying(); }
bool AStationDirector::IsNarrationPlaying() const { return NarrationAudio&&NarrationAudio->IsPlaying(); }
void AStationDirector::StopPresentationVoice()
{
    if(DialogueAudio)DialogueAudio->Stop();
    if(NarrationAudio)NarrationAudio->Stop();
    if(ReactionAudio)ReactionAudio->Stop();
}
void AStationDirector::PlayCharacterReaction(const FString& Key,FVector Position,float Volume)
{
    const float Now=GetWorld()->GetTimeSeconds();
    if(!VoiceEnabled||Starting||IsMenuOpen()||IsVoicePlaying()||IsNarrationPlaying()||(Key.StartsWith(TEXT("VR_LinZhao"))&&Now<NextReactionTime))return;
    auto* Sound=StationPresentation::Sound(Key);if(!Sound)return;
    if(ReactionAudio&&ReactionAudio->IsPlaying())return;
    if(!ReactionAudio)ReactionAudio=MakeLoop(*Key,Position,Volume,true);
    else
    {
        ReactionAudio->SetSound(Sound);ReactionAudio->SetWorldLocation(Position);
        ReactionAudio->SetVolumeMultiplier(Volume*VoiceVolume);ReactionAudio->Play();
    }
    ReactionKey=Key;++ReactionCount;NextReactionTime=Now+6.f;
}
void AStationDirector::SpeakPresentation(const FString& Key)
{
    if(DialogueAudio)DialogueAudio->Stop();
    if(!VoiceEnabled||Key.IsEmpty())return;
    if(auto* Sound=StationPresentation::Sound(Key))
    {
        if(!DialogueAudio)DialogueAudio=MakeLoop(*Key,FVector::ZeroVector,.78f,false);
        else {DialogueAudio->SetSound(Sound);DialogueAudio->SetVolumeMultiplier(.78f*VoiceVolume);DialogueAudio->Play();}
    }
}
void AStationDirector::BeginPresentation(const FString& RawText,bool Typewriter,const FString& ExplicitVoiceKey)
{
    RememberDialogue(RawText);StopPresentationVoice();PresentedText=RawText;
    PresentedSpeaker=Panel==EStationPanel::Conversation?GetPanelTitle():TEXT("档案旁白");
    int32 Colon=INDEX_NONE;
    if(RawText.FindChar(TCHAR(0xff1a),Colon)&&Colon>0&&Colon<8)
    {
        const FString Candidate=RawText.Left(Colon);
        if(Candidate==TEXT("林照")||Candidate==TEXT("叶岑")||Candidate==TEXT("林禾")||Candidate==TEXT("周驰")||Candidate==TEXT("旁白")||Candidate==TEXT("沈砚")||Candidate==TEXT("罗婶")||Candidate==TEXT("阿湛"))
        {
            PresentedSpeaker=Candidate;PresentedText=RawText.Mid(Colon+1);
        }
    }
    PresentedVoiceKey=ExplicitVoiceKey.IsEmpty()?VoiceForText(RawText):ExplicitVoiceKey;
    if(PresentedVoiceKey.StartsWith(TEXT("@")))PresentedVoiceKey=VoiceForText(PresentedVoiceKey);
    if(!StationPresentation::Sound(PresentedVoiceKey))PresentedVoiceKey.Empty();
    VisibleCharacters=Typewriter?0:PresentedText.Len();GlyphClock=0;GlyphDelay=.05f;
    GlyphPeriod=.075f;
    if(auto* Sound=StationPresentation::Sound(PresentedVoiceKey))
    {
        float Pauses=.05f;
        for(const TCHAR C:PresentedText)
        {
            if(C==TCHAR(0xff0c)||C==TCHAR(0xff1b))Pauses+=.14f;
            if(C==TCHAR(0x3002)||C==TCHAR(0xff1f)||C==TCHAR(0xff01))Pauses+=.24f;
            if(C=='\n')Pauses+=.10f;
        }
        GlyphPeriod=FMath::Clamp((Sound->GetDuration()*.9f-Pauses)/FMath::Max(1,PresentedText.Len()-1),.018f,.16f);
    }
    if(SubtitleSpeed==3)VisibleCharacters=PresentedText.Len();
    else GlyphPeriod*=SubtitleSpeed==0?1.4f:SubtitleSpeed==2?.6f:1.f;
    SpeakPresentation(PresentedVoiceKey);
    if(Panel==EStationPanel::Conversation&&!RemoteConversation)
    {
        if(auto* Person=Cast<AStationEngineer>(ConversationTarget))Person->PlayStationAction(PresentedSpeaker==Person->PersonName?TEXT("Talk"):TEXT("Idle"));
        UpdateDialogueCamera();
    }
    if(IsArcChapter()&&Panel==EStationPanel::Conversation)SetArcPerformance(RawText,false);
    if(Starting&&LinHe)LinHe->PlayStationAction(PresentedSpeaker==TEXT("林禾")?TEXT("Talk"):TEXT("Idle"));
}
void AStationDirector::BeginPanelPresentation()
{
    switch(Panel)
    {
        case EStationPanel::SurveyGame:case EStationPanel::SurveyBook:case EStationPanel::SurveyRecord:BeginPresentation(TEXT(""),false);break;
        case EStationPanel::Conversation:BeginPresentation(GetPanelBody());break;
        case EStationPanel::WorldRecord:BeginPresentation(GetUrbanThought(),true,FString::Printf(TEXT("EV_Urban_%03d"),ActiveUrban+1));break;
        case EStationPanel::Exploration:BeginPresentation(GetExplorationThought(),true,ActiveExploration>=0?FString::Printf(TEXT("EV_Explore_%03d"),ActiveExploration+1):TEXT(""));break;
        case EStationPanel::Document:
        {
            auto* Object=Cast<AStationObject>(ConversationTarget);
            FString Key=Progress->Started&&Object&&Object->Kind==EStationObjectKind::Terminal
                ?(Progress->ChosenPlan==0?TEXT("@record0"):TEXT("@record1")):TEXT("");
            if(IsRationChapter()&&Object)
            {
                if(Object->IsServiceRequest())Key=FString::Printf(TEXT("@ration_doc%d"),int32(Object->Kind)-int32(EStationObjectKind::ClinicRequest));
                if(Object->Kind==EStationObjectKind::Terminal)Key=Progress->ReadRecall?TEXT("@ration_recall"):TEXT("@ration_account");
            }
            if(IsConflictChapter()&&Object&&(Object->Kind==EStationObjectKind::Terminal||Object==ConsignmentCase||Object==SupplierReturn))Key.Empty();
            if(IsSupplyChapter()&&IsSupplyDocument(Object)||Object==WorkAccount)Key.Empty();
            if(DistrictObjects.Contains(Object))Key.Empty();
            if(IsDutyChapter()&&IsDutyDocument(Object)||Object&&Object==DutyLoadBank)Key.Empty();
            if(IsMandateDocument(Object))Key.Empty();
            if(IsCrisisDocument(Object)||IsArcDocument(Object)||ArcPendingNode.StartsWith(TEXT("archive:")))Key.Empty();
            BeginPresentation(GetPanelBody(),true,Key);break;
        }
        case EStationPanel::DutyCalibrate:BeginPresentation(TEXT(""),false);break;
        case EStationPanel::RationPlan:BeginPresentation(GetPanelBody(),false);break;
        case EStationPanel::RationConfirm:BeginPresentation(GetPanelBody(),false,TEXT("@ration_confirm"));break;
        case EStationPanel::Plans:BeginPresentation(GetPanelBody(),false,TEXT("@plans"));break;
        case EStationPanel::Confirmation:BeginPresentation(GetPanelBody(),false,ConfirmSelection==0?TEXT("@confirm0"):TEXT("@confirm1"));break;
        default:BeginPresentation(GetPanelBody(),false);break;
    }
}
void AStationDirector::CompletePresentation(){VisibleCharacters=PresentedText.Len();}
FString AStationDirector::GetDisplayedText() const {return PresentedText.Left(VisibleCharacters);}
FString AStationDirector::GetTypedObjective() const
{
    return LastObjective.Left(FMath::Min(LastObjective.Len(),FMath::FloorToInt(ObjectiveElapsed*18)));
}
int32 AStationDirector::GetQuestStage() const
{
    if(IsArcChapter())return GetArcStage();
    if(IsCrisisChapter())return GetCrisisStage();
    if(IsMandateChapter())return GetMandateStage();
    if(IsAftermathChapter())return GetAftermathStage();
    if(IsDistrictChapter())return GetDistrictStage();
    if(IsDutyChapter())return GetDutyStage();
    if(IsSupplyChapter())return GetSupplyStage();
    if(IsConflictChapter())return GetConflictStage();
    if(IsRationChapter())return GetRationStage();
    if(Progress->Started)
    {
        if(!Progress->HeardAllocation)return 0;
        if(!Progress->MetBroker)return 1;
        if(!Progress->ReadLedger)return 2;
        if(Progress->AllocationPolicy<0)return 3;
        if(Progress->AllocationReviewMask!=7)return 4;
        return 5;
    }
    if(!Progress->MetEngineer)return 0;
    if(!Progress->ReadNotice)return 1;
    if(!Progress->HasConnector)return 2;
    if(Progress->Connections!=7)return 3;
    if(!Progress->HeardRisk)return 4;
    if(!Progress->Started)return 5;
    return 6;
}
void AStationDirector::TickPresentation(float Delta)
{
    if(IsStoryCapture())return;
    const FString Objective=GetObjective();
    if(Objective!=LastObjective)
    {
        LastObjective=Objective;ObjectiveElapsed=0;ObjectiveSpoken=false;
        if(NarrationAudio)NarrationAudio->Stop();
    }
    ObjectiveElapsed+=Delta;
    if(Panel==EStationPanel::Conversation&&!RemoteConversation&&!IsVoicePlaying()&&!IsTyping())
        if(auto* Person=Cast<AStationEngineer>(ConversationTarget))Person->PlayStationAction(TEXT("Idle"));
    if(Starting&&LinHe&&C07Line>=0&&!IsVoicePlaying()&&!IsTyping())LinHe->PlayStationAction(TEXT("Idle"));
    if(IsTyping())
    {
        GlyphClock+=Delta;
        while(IsTyping()&&GlyphClock>=GlyphDelay)
        {
            GlyphClock-=GlyphDelay;const TCHAR Character=PresentedText[VisibleCharacters++];
            GlyphDelay=GlyphPeriod;
            if(Character==TCHAR(0xff0c)||Character==TCHAR(0xff1b))GlyphDelay+=.14f;
            if(Character==TCHAR(0x3002)||Character==TCHAR(0xff1f)||Character==TCHAR(0xff01))GlyphDelay+=.24f;
            if(Character=='\n')GlyphDelay+=.10f;
        }
    }
    if(QuietIndex<0&&!IsStoryFilmActive()&&!IsAmbientStoryActive()&&!ObjectiveSpoken&&ObjectiveElapsed>1.2f&&!IsMenuOpen()&&!Starting&&!IsPhysicalActionActive()&&!IsVoicePlaying()&&VoiceEnabled&&!(ReactionAudio&&ReactionAudio->IsPlaying()))
    {
        const FString Cue=VoiceForText(LastObjective);
        if(auto* Sound=StationPresentation::Sound(Cue))
        {
            if(!NarrationAudio)NarrationAudio=MakeLoop(*Cue,FVector::ZeroVector,.63f,false);
            else {NarrationAudio->SetSound(Sound);NarrationAudio->SetVolumeMultiplier(.63f*VoiceVolume);NarrationAudio->Play();}
        }
        ObjectiveSpoken=true;
    }
}
void AStationDirector::UpdateVoiceMix(float Delta)
{
    const bool Speaking=IsVoicePlaying()||IsNarrationPlaying()||(ReactionAudio&&ReactionAudio->IsPlaying());
    VoiceMix=FMath::FInterpTo(VoiceMix,Speaking?.42f:1.f,Delta,7.f);
    const float Alpha=Starting?FMath::Clamp((StartElapsed-10)/8,0.f,1.f):(Progress->Started?1.f:0.f);
    if(AirAudio)AirAudio->SetVolumeMultiplier(.10f*VoiceMix*EffectsVolume);
    if(IdleAudio)IdleAudio->SetVolumeMultiplier((.22f*(1-Alpha)+.06f*Alpha)*VoiceMix*EffectsVolume);
    if(RunAudio)RunAudio->SetVolumeMultiplier(.25f*Alpha*VoiceMix*EffectsVolume);
    if(Starting&&MusicAudio)MusicAudio->SetVolumeMultiplier((StartElapsed<30?.23f:.09f)*VoiceMix*MusicVolume);
}
void AStationDirector::RequestJournal()
{
    if(IsStoryFilmActive()||Starting||IsPhysicalActionActive())return;
    if(Panel==EStationPanel::Journal){ClosePanel();return;}
    if(!IsMenuOpen())OpenPanel(EStationPanel::Journal);
}
void AStationDirector::ToggleVoice()
{
    VoiceEnabled=!VoiceEnabled;
    auto* Preferences=StationFrontend::LoadPreferences(this);Preferences->Voice=VoiceEnabled;StationFrontend::StorePreferences(Preferences);
    if(!VoiceEnabled)StopPresentationVoice();
    else if(IsStoryFilmActive()||IsMenuOpen())SpeakPresentation(PresentedVoiceKey);
    else ObjectiveSpoken=false;
}
FString AStationDirector::GetPresentationStateJson() const
{
    return FString::Printf(TEXT("{\"typing\":%s,\"visible\":%d,\"length\":%d,\"voice_playing\":%s,\"narration_playing\":%s,\"voice_enabled\":%s,\"speaker\":\"%s\",\"voice_asset\":\"%s\",\"line\":%d,\"quest_stage\":%d,\"reaction_asset\":\"%s\",\"reaction_playing\":%s,\"reaction_count\":%d,\"dialogue_camera\":%s,\"sequence_duration\":%.3f,\"c07_line\":%d}"),
        IsTyping()?TEXT("true"):TEXT("false"),VisibleCharacters,PresentedText.Len(),IsVoicePlaying()?TEXT("true"):TEXT("false"),IsNarrationPlaying()?TEXT("true"):TEXT("false"),VoiceEnabled?TEXT("true"):TEXT("false"),*PresentedSpeaker,*PresentedVoiceKey,ConversationLine,GetQuestStage(),*ReactionKey,ReactionAudio&&ReactionAudio->IsPlaying()?TEXT("true"):TEXT("false"),ReactionCount,DialoguePreviousView?TEXT("true"):TEXT("false"),C07Duration,C07Line);
}

void AStationDirector::PrepareC07Timing()
{
    C07Line=-1;C07SpeechTimes[0]=30;
    for(int32 I=0;I<5;++I)
    {
        const FString Key=FString::Printf(TEXT("EV_C07_%02d"),I+1);
        const auto* Sound=StationPresentation::Sound(Key);
        const float Duration=Sound?Sound->GetDuration():2.f;
        if(I<4)C07SpeechTimes[I+1]=C07SpeechTimes[I]+FMath::Max(Duration+.6f,1.8f);
        else C07Duration=FMath::Max(46.f,C07SpeechTimes[I]+Duration+1.f);
    }
}
