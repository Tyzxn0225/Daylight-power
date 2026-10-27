#include "StationPrototype.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

StationNarrative::Outcome AStationDirector::GetNarrativeOutcome() const
{
    StationNarrative::Outcome S;if(!Progress)return S;
    S.started=Progress->Started;S.readRecall=Progress->ReadRecall;S.conflictComplete=Progress->ConflictComplete;
    S.supplyComplete=Progress->SupplyComplete;S.dutySettled=Progress->DutySettled;S.districtSettled=Progress->DistrictSettled;
    S.allocation=Progress->AllocationPolicy;S.ration=Progress->RationMask;S.conflict=Progress->ConflictChoice;S.supply=Progress->SupplyChoice;S.district=Progress->DistrictChoice;return S;
}
FString AStationDirector::GetSocialMemoryBody() const
{
    if(!Progress)return TEXT("");
    FString Family=TEXT("八年前，你把家庭供光额度换成工位保证金。林禾没有共同签字。她是你的妹妹，也是一位独立申请人。");
    if(Progress->AllocationPolicy==0)Family+=TEXT("\n你公开了带有她姓名的原始名单；公开不等于取得她的同意。");
    if(Progress->AllocationPolicy==1)Family+=TEXT("\n你只公开了规则。申请人的隐私被保留，排队关系也仍然看不见。");
    if(Progress->ReadRecall&&Progress->RationMask>=0)Family+=(Progress->RationMask&4)?TEXT("\n她本轮排到了共用灯，后面仍有人在等。"):TEXT("\n她本轮没有排到共用灯，候补次序保留。");
    FString Crew=Progress->ChosenPlan==1?TEXT("第一次供光用了临时过载，叶岑保留着风险记录。她既帮助社区，也保护检修组的工作。"):TEXT("叶岑掌握检修与工位安排。她愿意合作，但不会替组员答应承担一切。");
    FString Broker=TEXT("周驰垫过耗材与饭票。他能提供帮助，也从流通和担保关系中获利。");
    if(Progress->ConflictComplete&&Progress->ConflictChoice>=0&&Progress->ConflictChoice<2)Broker+=Progress->ConflictChoice==0?TEXT("\n你留下了稳压器，供货曾被中止。这笔处置已署名。"):TEXT("\n你原封退回了稳压器，保住排期，也放弃了当时的可用设备。");
    FString Latest=TEXT("眼下还没有一份能够替所有人作答的名单。");
    if(Progress->DutySettled&&Progress->SupplyChoice>=0&&Progress->SupplyChoice<2)Latest=Progress->SupplyChoice==1?TEXT("你已用本人工时核销这批抵押，没有把工时转给林禾。旧债与其他申请仍在。"):TEXT("本班劳动补回了六个公共备用额度；这是劳动所得，没有消除旧欠条。");
    if(Progress->DistrictSettled&&Progress->DistrictChoice>=0&&Progress->DistrictChoice<2)Latest=Progress->DistrictChoice==0?TEXT("你签字让本批六个额度先用于公共试亮，夜班押金延期。领取人的异议仍在。"):TEXT("你签字先退夜班押金，取消公共试亮。合格灯具将进入复售，停订原因没有被删去。");
    if(Progress->MandateCommitted)Latest=Progress->MandateChoice==0?TEXT("下一期家庭申请整单撤回，快线名额取消。林禾拒绝联保的意见独立保留，旧账与供光没有撤销。"):TEXT("林禾退出关联栏，林照个人申请与相关新申请转人工复核。未提交组员姓名，没有倒扣既有工资。");
    if(Progress->CrisisAllocation>=0)Latest+=FString::Printf(TEXT("\n本班余热分流：院内 %d kW，恒昼夜班廊 %d kW。热量总额 36 kW，旧供光与地址资格不变。"),Progress->CrisisAllocation,36-Progress->CrisisAllocation);
    FString Others;
    if(Progress->ArcStarted)
    {
        if(GetArcFact(TEXT("done:queue")))Family+=TEXT("\n林禾自己打开了送餐通道，留下等候的人；她也承担着拥挤和停餐的后果。");
        if(GetArcFact(TEXT("done:family")))Family+=GetArcFact(TEXT("family_honest"))?TEXT("\n你把清退附件交给了她。她知道其中的条件，仍保留自己的去留决定。"):TEXT("\n你把清退附件留在自己手里。门关上以后，她已经知道你隐瞒过什么。");
        if(GetArcFact(TEXT("family_resolved")))Family+=GetArcFact(TEXT("family_separate"))?TEXT("\n她没有跟你迁入稳定地址。兄妹仍会联系，住在哪里由她自己决定。"):TEXT("\n她留下了自己的申请、工作与住处安排；今晚一起吃饭没有取消这些决定。");
        if(GetArcFact(TEXT("done:crew")))Crew+=GetArcFact(TEXT("crew_support"))?TEXT("\n检修组同意到场共同核对。合作有实际条件，不是无限承担责任。"):TEXT("\n本班没有达成共同到场的安排。叶岑保留着检修组的责任边界。");
        if(GetArcFact(TEXT("extra_hours")))Crew+=TEXT("\n你亲自承诺补四小时返工，这笔工时仍在。");
        if(GetArcFact(TEXT("heard:broker")))Broker+=GetArcFact(TEXT("evidence_courier"))?TEXT("\n周驰交回了自己保留的底联；已经卖出的轮班信息无法收回。"):TEXT("\n他承认轮班信息外流。你没有取回他保留的底联。");
        Others=TEXT("\n\n罗婶 / 登记与分租\n她经手等候名单，也收取租金。家里的份额、邻里的情面和别人的资格，在她手里相互牵连。");
        if(GetArcFact(TEXT("privacy_trade")))Others+=TEXT("\n你给过她等候名单，姓名已经传出去。取回文件不能抹掉这次泄露。");
        Others+=TEXT("\n\n阿湛 / 有工牌的等候者\n工牌允许他进恒昼清扫，不等于承认他在这里居住。");
        if(GetArcFact(TEXT("zhan_helped")))Others+=TEXT("\n你拿自己的临时工牌换回了他的工作物品。他愿意说出自己看到的事，仍不愿被当成宣传材料。");
        else if(GetArcFact(TEXT("done:worker_witness")))Others+=GetArcFact(TEXT("zhan_witness"))?TEXT("\n他同意以匿名方式说出经过。"):TEXT("\n他拒绝继续出面。名单外流使他不再相信这次保护。");
        if(GetArcFact(TEXT("done:offer_terms")))Others+=TEXT("\n\n沈砚 / 运营与例外\n他能兑现稳定住处，也要求林照经手清退核验。他母亲的护理地址列在豁免栏。");
        if(GetArcFact(TEXT("world_committed")))Latest=GetArcBody().Replace(TEXT("\n\n"),TEXT("\n"));
    }
    return TEXT("林禾 / 家庭与共同决定\n")+Family+TEXT("\n\n叶岑 / 工作与检修组\n")+Crew+TEXT("\n\n周驰 / 垫款与流通\n")+Broker+Others+TEXT("\n\n林照 / 最近留下的签名\n")+Latest;
}
void AStationDirector::RequestRelationships()
{
    if(IsStoryFilmActive()||Starting||IsPhysicalActionActive())return;
    if(Panel==EStationPanel::Relationships){ClosePanel();return;}
    if(!IsMenuOpen())OpenPanel(EStationPanel::Relationships);
}
FString AStationDirector::GetConversationEcho() const
{
    if(!ConversationTarget)return TEXT("");
    const bool Sister=ConversationTarget==LinHe,Foreman=ConversationTarget==Engineer,Broker=ConversationTarget==ZhouChi;
    using M=StationNarrative::Memory;
    switch(StationNarrative::LatestMemory(GetNarrativeOutcome()))
    {
        case M::TrialAndDeferredRefund:if(Foreman)return TEXT("叶岑：你把异议留下了。可我的组员今天还是没拿到押金，这件事也要留下。");break;
        case M::RefundAndNoTrial:if(Sister)return TEXT("林禾：押金退了，检光井也没亮。两件事都是真的，我没法只替你高兴。");break;
        case M::LaborSettled:if(Sister)return TEXT("林禾：工单结清了。你欠自己的那顿饭，总得找一天吃完。");break;
        case M::ReserveRestored:if(Foreman)return TEXT("叶岑：备用补回来了，是你做的工。别让下一张表把这笔劳动省略掉。");break;
        case M::RegulatorKept:if(Broker)return TEXT("周驰：稳压器还在你们手里。下次谈供货，他们会从这句话开始。");break;
        case M::RegulatorReturned:if(Foreman)return TEXT("叶岑：退件柜的封签我看见了。设备没坏，退回去的人也不是不知道。");break;
        case M::HousingLit:if(Sister)return TEXT("林禾：我记得那一轮我坐到了灯下。也记得有人一直没轮到。");break;
        case M::HousingWaiting:if(Sister)return TEXT("林禾：那一轮我还在等。你来问我后果，我就先告诉你这个。");break;
        case M::NamedListPublished:if(Sister)return TEXT("林禾：你贴出的那张名单上有我的名字。我不是现在才记得。");break;
        case M::RulesOnly:if(Broker)return TEXT("周驰：原名单还封着。规矩能公开，谁替谁排的队就不一定了。");break;
        default:break;
    }
    return TEXT("");
}
TArray<FString> AStationDirector::GetConversation() const
{
    if(IsArcChapter())return GetArcConversation(ConversationTarget);
    if(IsCrisisChapter())return GetCrisisConversation(ConversationTarget);
    if(!IsMandateChapter()&&IsAftermathChapter()&&GetAftermathPerson(ConversationTarget)>=0)return GetAftermathConversation(ConversationTarget);
    const auto NewLines=GetMandateConversation(ConversationTarget);if(!NewLines.IsEmpty())return NewLines;
    auto Lines=GetBaseConversation();const FString Echo=GetConversationEcho();
    if(!Echo.IsEmpty()&&!Lines.Contains(Echo))Lines.Insert(Echo,0);return Lines;
}
FString AStationDirector::GetCommunityNarrativeStateJson() const
{
    return FString::Printf(TEXT("{\"active_bit\":%d,\"seen\":%d,\"enabled\":%s,\"cooldown\":%.2f,\"music_volume\":%.2f,\"subtitle_scale\":%d,\"subtitle_backdrop\":%s,\"guides\":%s,\"memory\":%d,\"region\":%d}"),AmbientStoryBit,Progress?Progress->AmbientSeenMask:0,AmbientStoriesEnabled?TEXT("true"):TEXT("false"),AmbientStoryCooldown,MusicVolume,SubtitleScale,SubtitleBackdrop?TEXT("true"):TEXT("false"),SceneGuides?TEXT("true"):TEXT("false"),int32(StationNarrative::LatestMemory(GetNarrativeOutcome())),ScoreRegion);
}
void AStationDirector::CancelCommunityVignette()
{
    if(!AmbientStoryBit)return;
    AmbientStoryBit=0;AmbientStoryTarget=nullptr;AmbientStoryCooldown=8.f;
    StopPresentationVoice();PresentedText.Empty();PresentedSpeaker.Empty();VisibleCharacters=0;
}
void AStationDirector::TickCommunityVignette(float Delta)
{
    AmbientStoryCooldown=FMath::Max(0.f,AmbientStoryCooldown-Delta);
    if(!PC||!PC->GetPawn()||!Progress)return;
    const bool Blocked=QuietIndex>=0||IsStoryFilmActive()||Starting||IsMenuOpen()||IsPhysicalActionActive();
    const float Speed=PC->GetPawn()->GetVelocity().Size2D();
    if(AmbientStoryBit)
    {
        if(Blocked||!IsValid(AmbientStoryTarget)||FVector::Dist2D(PC->GetPawn()->GetActorLocation(),AmbientStoryTarget->GetActorLocation())>900)
        {CancelCommunityVignette();return;}
        AmbientStoryElapsed+=Delta;
        if(StationNarrative::MayRememberVignette(IsTyping(),IsVoicePlaying(),AmbientStoryElapsed,AmbientStoryMinimum))
        {
            Progress->AmbientSeenMask|=AmbientStoryBit;SaveProgress();CancelCommunityVignette();AmbientStoryCooldown=45.f;
        }
        return;
    }
    AmbientQuietSeconds=!Blocked&&Speed<180?AmbientQuietSeconds+Delta:0.f;
    if(!AmbientStoriesEnabled||Blocked||IsArcChapter()||IsCrisisChapter()||IsVoicePlaying()||IsNarrationPlaying()||AmbientStoryCooldown>0)return;
    struct Cue{int32 Bit;AStationEngineer* Person;bool Enabled;const TCHAR* Text;const TCHAR* Voice;};
    const Cue Cues[]={
        {1,LinHe.Get(),Progress->Started&&!Progress->ReadRecall,TEXT("林禾：你每次都说，先熬过这个月。后来，我们就不再问下个月了。"),TEXT("EV_Story_007")},
        {2,ZhouChi.Get(),Progress->ReadRecall&&(Progress->RationMask&2)!=0,TEXT("周驰：厨房有人做饭。我垫的耗材，也该有人付账了。"),TEXT("EV_Story_021")},
        {4,ZhouChi.Get(),Progress->ReadRecall&&(Progress->RationMask&2)==0,TEXT("周驰：厨房停了。今晚的饭票，我会卖给出得起价的人。"),TEXT("EV_Story_022")},
        {8,LinHe.Get(),Progress->DistrictStarted&&!Progress->DistrictSettled,TEXT("林禾：他们想拿回押金，我也想。可住在黑里的时候，我又盼着有人别把灯全关掉。"),TEXT("EV_District_008")},
        {16,LinHe.Get(),Progress->SeenDutyFilm&&!Progress->DistrictStarted,TEXT("林禾：那就回家吧。别急着答应明天，把今晚这顿饭吃完。"),TEXT("EV_Duty_016")}};
    for(const Cue& C:Cues)
    {
        if(!C.Enabled||!IsValid(C.Person)||C.Person->ConversationPaused||!CanSeeTarget(C.Person))continue;
        const float Distance=FVector::Dist2D(PC->GetPawn()->GetActorLocation(),C.Person->GetActorLocation());
        if(!StationNarrative::CanStartVignette(AmbientStoriesEnabled,Blocked,false,AmbientQuietSeconds,AmbientStoryCooldown,Progress->AmbientSeenMask,C.Bit,Distance,Speed))continue;
        AmbientStoryBit=C.Bit;AmbientStoryTarget=C.Person;AmbientStoryElapsed=0;AmbientStoryMinimum=FMath::Max(7.f,FString(C.Text).Len()*.13f+1.f);
        BeginPresentation(C.Text,true,C.Voice);break;
    }
}
