#include "StationPrototype.h"
#include "StationNeighborhoodData.inl"
#include "StationMovementTuning.h"
#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/RectLight.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"

namespace Duty
{
    #include "StationDutyData.inl"
    FString Line(int32 I){return Lines[I-1];}
    const TCHAR* Jobs[]={TEXT("近街保护端 · 欠压校验"),TEXT("负载箱 · 回压校验"),TEXT("应急端 · 切换校验"),TEXT("冷却端 · 持续值守"),TEXT("负载箱 · 作废验收重测")};
    const TCHAR* Services[]={TEXT("诊疗点"),TEXT("互助厨房"),TEXT("居民共用照明")};
}
int32 AStationDirector::GetDutyRequiredMask() const {return Progress->SupplyChoice==1?15:7;}
bool AStationDirector::DutyBaseWorkComplete() const {return (Progress->DutyWorkMask&GetDutyRequiredMask())==GetDutyRequiredMask();}
int32 AStationDirector::GetDutyValidJobs() const
{
    const int32 Count=FMath::CountBits(uint32(Progress->DutyWorkMask&GetDutyRequiredMask()));
    return FMath::Max(0,Count-(Progress->DutyEmergencyChoice==0&&!Progress->DutyRecheckDone?1:0));
}
void AStationDirector::SyncDutyCredit()
{
    Progress->DutyCreditedHours=Progress->SupplyChoice==1?FMath::Min(Progress->SupplyLaborHours,GetDutyValidJobs()*2):0;
}
bool AStationDirector::DutyReadyToSettle() const
{
    return DutyBaseWorkComplete()&&Progress->DutyCommunityRestored&&Progress->DutyEmergencyChoice>=0&&(Progress->DutyEmergencyChoice!=0||Progress->DutyRecheckDone);
}
AStationObject* AStationDirector::GetDutyOrderObject(int32 I) const
{
    if(I==1||I==4)return DutyLoadBank;
    const EStationObjectKind K=I==0?EStationObjectKind::NodeA:I==2?EStationObjectKind::NodeC:EStationObjectKind::NodeB;
    for(auto& O:Objects)if(O->Kind==K)return O;return nullptr;
}
AStationObject* AStationDirector::GetDutyFaultObject() const
{
    return RationPoints.IsValidIndex(Progress->DutyFaultIndex)?RationPoints[Progress->DutyFaultIndex].Get():GetDutyOrderObject(3);
}
int32 AStationDirector::GetDutyNextOrder() const
{
    int32 Best=-1;double Distance=BIG_NUMBER;
    const FVector Here=PC&&PC->GetPawn()?PC->GetPawn()->GetActorLocation():FVector::ZeroVector;
    for(int32 I=0;I<4;++I)if((GetDutyRequiredMask()&(1<<I))&&!(Progress->DutyWorkMask&(1<<I)))
        if(auto* O=GetDutyOrderObject(I)){const double D=FVector::DistSquared2D(Here,O->GetActorLocation());if(D<Distance){Best=I;Distance=D;}}
    if(Best>=0)return Best;
    return Progress->DutyEmergencyChoice==0&&!Progress->DutyRecheckDone?4:-1;
}
bool AStationDirector::IsDutyDocument(const AStationObject* O) const
{
    return O&&(O->Kind==EStationObjectKind::Terminal||O==DutyLoadBank||O==WorkAccount||O==SupplierReturn||O==AllocationLedger||O==GetDutyFaultObject());
}
void AStationDirector::UpdateDutyActors()
{
    if(Progress->SupplyComplete&&!DutyLoadBank)
    {
        FTransform T(NeighborhoodReady?NeighborhoodPoint(TEXT("DutyLoadBank"),FVector(-800,3985,0)):PlaceOnPavement(FVector(-800,3985,0)));
        DutyLoadBank=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        DutyLoadBank->Kind=EStationObjectKind::DutyLoadBank;DutyLoadBank->DisplayName=TEXT("检修负载箱 · 分流接头");DutyLoadBank->FinishSpawning(T);Objects.Add(DutyLoadBank);
    }
    if(DutyLoadBank)
    {
        DutyLoadBank->Active=Progress->SupplyComplete;DutyLoadBank->SetActorHiddenInGame(!Progress->SupplyComplete);DutyLoadBank->SetActorEnableCollision(Progress->SupplyComplete);
        DutyLoadBank->SetIndicator(Progress->DutySettled?FLinearColor(.25f,.7f,.55f):FLinearColor(.75f,.52f,.18f),3);
    }
    if(IsDutyChapter()&&PC&&PC->GetPawn()&&Progress->DutyBridgeHeld&&!CarriedRegulator)
    {
        APawn* Pawn=PC->GetPawn();CarriedRegulator=NewObject<UStaticMeshComponent>(Pawn,TEXT("DiversionConnectorPouch"));Pawn->AddInstanceComponent(CarriedRegulator);CarriedRegulator->SetupAttachment(Pawn->GetRootComponent());
        CarriedRegulator->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));CarriedRegulator->SetRelativeLocation(FVector(-9,24,-20));CarriedRegulator->SetRelativeScale3D(FVector(.12,.08,.18));
        CarriedRegulator->SetCollisionEnabled(ECollisionEnabled::NoCollision);CarriedRegulator->RegisterComponent();
    }
    if(CarriedRegulator)CarriedRegulator->SetVisibility(Progress->ConflictPartHeld||Progress->SupplyPartHeld||Progress->DutyBridgeHeld);
    if(IsDutyChapter())for(int32 I=0;I<4;++I)if(auto* O=GetDutyOrderObject(I))if(Progress->DutyWorkMask&(1<<I))O->SetIndicator(FLinearColor(.25f,.7f,.55f),3);
}
void AStationDirector::TickDutyCommunity()
{
    if(!IsDutyChapter())return;
    auto Task=[&](AStationEngineer* P,FVector Goal,const TCHAR* Purpose,const TCHAR* Action=TEXT("Idle"))
    {if(P&&!WorkYieldPeople.Contains(P)){P->GetCharacterMovement()->MaxWalkSpeed=FString(Purpose).Contains(TEXT("抢修"))?170.f:StationMovement::Stroll;P->SetCommunityTask(Goal,Purpose,Action);}};
    if(Progress->DutySettled)
    {
        for(auto* P:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(P&&!WorkYieldPeople.Contains(P)){P->GetCharacterMovement()->MaxWalkSpeed=StationMovement::Stroll;P->ClearCommunityTask();}
        return;
    }
    Task(LinHe,NeighborhoodReady?Neighborhood16::Stand(TEXT("SisterHome"),FVector(-1480,4210,0))+FVector(-100,0,0):FVector(-1480,4210,0),TEXT("等候回家 · 留住家庭原单"));
    Task(ZhouChi,NeighborhoodReady?Neighborhood16::Stand(TEXT("WorkAccount"),FVector(-940,4210,0))+FVector(-100,0,0):FVector(-940,4210,0),TEXT("核对本班验收与作废工单"));
    if(Progress->DutyEmergencyChoice==1&&DutyBaseWorkComplete()&&!Progress->DutyCommunityRestored)
    {
        if(auto* O=GetDutyFaultObject())Task(Engineer,O->GetActorLocation()+FVector(0,72,0),TEXT("抢修欠压的社区服务端"),TEXT("Repair"));
    }
    else Task(Engineer,NeighborhoodReady?Neighborhood16::Stand(TEXT("NodeB"),FVector(900,4210,0))+FVector(-100,0,0):FVector(900,4210,0),TEXT("监测验收与社区欠压"));
}
float AStationDirector::GetDutyBandMin() const {return DutyPhase==0?(DutyAssist?35.f:45.f):(DutyAssist?50.f:60.f);}
float AStationDirector::GetDutyBandMax() const {return DutyPhase==0?(DutyAssist?75.f:70.f):(DutyAssist?85.f:80.f);}
FString AStationDirector::GetDutyCalibrationTitle() const {return DutyCalibrationOrder>=0&&DutyCalibrationOrder<5?Duty::Jobs[DutyCalibrationOrder]:TEXT("现场校准");}
void AStationDirector::BeginDutyCalibration(int32 Order)
{
    DutyCalibrationOrder=Order;DutyPhase=0;DutyStable=0;DutyPressure=12;DutyPressureHeld=false;OpenPanel(EStationPanel::DutyCalibrate);
}
void AStationDirector::FinishDutyCalibration()
{
    const int32 Order=DutyCalibrationOrder;ClosePanel();
    if(!IsDutyChapter()||Progress->DutySettled||Order<0)return;
    if(Order==4)Progress->DutyRecheckDone=true;
    else Progress->DutyWorkMask|=1<<Order;
    if(!Progress->DutyFaultRaised&&FMath::CountBits(uint32(Progress->DutyWorkMask))>=2)
    {
        Progress->DutyFaultRaised=true;Progress->DutyFaultIndex=-1;
        for(int32 I=0;I<3;++I)if(Progress->RationMask&(1<<I)){Progress->DutyFaultIndex=I;break;}
        PlayCue(TEXT("UI_Unavailable"),FVector::ZeroVector,.22f,false);
        Notify(TEXT("电台传来社区欠压报警。先抢修，还是先保住本班验收？现在就得回应。"));
    }
    else Notify(Order==4?TEXT("作废验收已重测，未额外增加报酬。核对本班结算。"):TEXT("两阶段校准完成，本张验收已保存。"));
    SyncDutyCredit();SaveProgress();UpdateDutyActors();PlayCue(TEXT("UI_Stored"),FVector::ZeroVector,.25f,false);
    if(Progress->DutyFaultRaised&&Progress->DutyEmergencyChoice<0)OpenPanel(EStationPanel::DutyChoice);
}
void AStationDirector::TickDuty(float Delta)
{
    if(!IsDutyChapter())return;
    if(Panel==EStationPanel::DutyCalibrate&&DutyCalibrationOrder>=0)
    {
        DutyPressure=FMath::Clamp(DutyPressure+Delta*(DutyPressureHeld?(DutyAssist?18.f:24.f):-18.f),0.f,100.f);
        const bool Inside=DutyPressure>=GetDutyBandMin()&&DutyPressure<=GetDutyBandMax();
        DutyStable=Inside?DutyStable+Delta:0.f;
        if(DutyStable>=1.25f)
        {
            PlayCue(TEXT("FX_Relay"),FVector::ZeroVector,.18f,false);DutyPhase++;DutyStable=0;
            if(DutyPhase>=2){FinishDutyCalibration();return;}
            DutyPressure=22.f;
        }
    }
    if(Progress->DutyFaultRaised&&!Progress->DutyCommunityRestored)
    {
        const int32 I=Progress->DutyFaultIndex;
        if(RationLights.IsValidIndex(I)&&RationPoints.IsValidIndex(I))
        {
            // A slow undervoltage dip, never a rapid strobe or a revoked allocation.
            RationLights[I]->GetLightComponent()->SetIntensity(3000.f+500.f*FMath::Sin(GetWorld()->GetTimeSeconds()*.6f));
            RationPoints[I]->SetServicePower(false);
        }
        if(Progress->DutyEmergencyChoice==1&&DutyBaseWorkComplete()&&Engineer&&!IsMenuOpen()&&!IsPhysicalActionActive()&&!Engineer->ConversationPaused&&!WorkYieldPeople.Contains(Engineer))
        {
            auto* O=GetDutyFaultObject();
            if(O&&FVector::Dist2D(Engineer->GetActorLocation(),O->GetActorLocation()+FVector(0,72,0))<25&&Engineer->GetVelocity().Size2D()<5)DutyRepairClock+=Delta;
            else DutyRepairClock=0;
            if(DutyRepairClock>=5.f)
            {
                Progress->DutyCommunityRestored=true;DutyRepairClock=0;SaveProgress();UpdateRationActors();PlayCue(TEXT("FX_Relay"),TargetPoint(O),.3f);Notify(TEXT("叶岑已到场完成抢修。原首夜获准服务恢复，候补状态未改变。"));
            }
        }
    }
}
bool AStationDirector::HandleDutyObject(AStationObject* O)
{
    if(!Progress->SupplyComplete)return false;
    if(!IsDutyChapter())
    {
        if(O->Kind==EStationObjectKind::Terminal||O->Kind==EStationObjectKind::NodeB||O==DutyLoadBank){OpenPanel(EStationPanel::DutyStart);Selection=1;return true;}
        if(O==DutyLoadBank){OpenPanel(EStationPanel::Document);return true;}return false;
    }
    if(O->Kind==EStationObjectKind::Terminal||(O==DutyLoadBank&&DutyReadyToSettle()))
    {
        if(Progress->DutySettled){OpenPanel(EStationPanel::Document);return true;}
        if(Progress->DutyFaultRaised&&Progress->DutyEmergencyChoice<0){OpenPanel(EStationPanel::DutyChoice);return true;}
        if(DutyReadyToSettle())
        {
            SyncDutyCredit();
            if(Progress->SupplyChoice==0){Progress->DutyReward=GetDutyValidJobs()*2;Progress->Reserve+=Progress->DutyReward;}
            Progress->DutySettled=true;SaveProgress();UpdateDutyActors();
        }
        OpenPanel(EStationPanel::Document);return true;
    }
    if(Progress->DutySettled){if(IsDutyDocument(O)){OpenPanel(EStationPanel::Document);return true;}return false;}
    if(Progress->DutyFaultRaised&&Progress->DutyEmergencyChoice<0&&(O==GetDutyFaultObject()||O==DutyLoadBank))
    {OpenPanel(EStationPanel::DutyChoice);return true;}
    if(O==DutyLoadBank&&Progress->DutyEmergencyChoice==0&&!Progress->DutyCommunityRestored)
    {
        if(!Progress->DutyBridgeHeld)BeginPhysicalAction(O,TEXT("Reach"),-34);
        else Notify(TEXT("分流接头已在工具袋里。到欠压服务端安装。"));return true;
    }
    if(O==GetDutyFaultObject()&&Progress->DutyFaultRaised)
    {
        if(Progress->DutyEmergencyChoice==0&&Progress->DutyBridgeHeld&&!Progress->DutyCommunityRestored)BeginPhysicalAction(O,TEXT("Repair"),-35);
        else {OpenPanel(EStationPanel::Document);}return true;
    }
    for(int32 I=0;I<4;++I)if(O==GetDutyOrderObject(I))
    {
        if(Progress->DutyFaultRaised&&Progress->DutyEmergencyChoice<0){OpenPanel(EStationPanel::DutyChoice);return true;}
        if(Progress->DutyEmergencyChoice==0&&!Progress->DutyCommunityRestored){Notify(TEXT("已选择先抢修。到负载箱取分流接头，装到欠压服务端。"),true);return true;}
        const int32 Next=GetDutyNextOrder();
        if(Next==4&&O==DutyLoadBank){BeginPhysicalAction(O,TEXT("Repair"),-36);return true;}
        if(!(GetDutyRequiredMask()&(1<<I))||(Progress->DutyWorkMask&(1<<I))){Notify(Next>=0?TEXT("本端已完成。其余工单可自行选择顺序，已完成的单不重复记账。"):TEXT("本班验收已齐，等待社区服务端恢复后在负载箱结算。"),true);return true;}
        BeginPhysicalAction(O,TEXT("Repair"),-30-I);return true;
    }
    if(IsDutyDocument(O)){OpenPanel(EStationPanel::Document);return true;}
    return false;
}
void AStationDirector::PressDutyChoice()
{
    if(Panel==EStationPanel::DutyStart)
    {
        if(Selection!=0){ClosePanel();return;}
        if(!Progress->SupplyComplete)return;
        Progress->DutyStarted=true;SaveProgress();ClosePanel();UpdateDutyActors();Notify(TEXT("本班工单已领。自行选择尚未完成的验收端；先检修，再按住/松开 E 稳住两段读数。"));return;
    }
    if(Panel==EStationPanel::DutyChoice){ConfirmSelection=Selection;OpenPanel(EStationPanel::DutyConfirm);Selection=1;return;}
    if(Panel!=EStationPanel::DutyConfirm)return;
    if(Selection!=0){OpenPanel(EStationPanel::DutyChoice);Selection=ConfirmSelection;return;}
    if(!Progress->DutyFaultRaised||Progress->DutyEmergencyChoice>=0)return;
    Progress->DutyEmergencyChoice=ConfirmSelection;SyncDutyCredit();SaveProgress();ClosePanel();
    Notify(ConfirmSelection==0?TEXT("先抢修社区。到负载箱取分流接头；恢复后补测一张作废验收。"):TEXT("先交本班验收。最后一张完成后，叶岑会实际赶往欠压端抢修。"));
}
void AStationDirector::CompleteDutyAction(int32 A)
{
    if(!IsDutyChapter()||Progress->DutySettled)return;
    if(A>=-33&&A<=-30){BeginDutyCalibration(-30-A);return;}
    if(A==-36){BeginDutyCalibration(4);return;}
    if(A==-34&&Progress->DutyEmergencyChoice==0&&!Progress->DutyCommunityRestored)Progress->DutyBridgeHeld=true;
    if(A==-35&&Progress->DutyBridgeHeld)
    {
        Progress->DutyBridgeHeld=false;Progress->DutyCommunityRestored=true;UpdateRationActors();PlayCue(TEXT("FX_Relay"),TargetPoint(GetDutyFaultObject()),.35f);
        Notify(TEXT("欠压服务恢复，首夜名单未改变。回到验收路线，完成剩余项并补测作废工单。"));
    }
    SaveProgress();UpdateDutyActors();
}
TArray<FString> AStationDirector::GetDutyConversation() const
{
    if(Progress->DutySettled)
    {
        if(ConversationTarget==LinHe)return {Duty::Line(13),Duty::Line(22)};
        if(ConversationTarget==Engineer)return {Duty::Line(18)};
        return {Duty::Line(Progress->SupplyChoice==1?11:12)};
    }
    if(Progress->DutyFaultRaised)
    {
        if(ConversationTarget==LinHe)return {Duty::Line(7),Duty::Line(8)};
        if(ConversationTarget==Engineer)return {Duty::Line(Progress->DutyEmergencyChoice<0?5:Progress->DutyEmergencyChoice==0?9:10)};
        return Progress->DutyEmergencyChoice==1?TArray<FString>{TEXT("周驰：新单上的运费，有一部分是我垫的旧材料钱。供应商准我在这一批里收。")} : TArray<FString>{Duty::Line(Progress->DutyEmergencyChoice<0?6:21)};
    }
    if(ConversationTarget==Engineer)return {Duty::Line(Progress->SupplyChoice==1?1:2),Duty::Line(20)};
    if(ConversationTarget==LinHe)return {Duty::Line(3),Duty::Line(4)};
    return {TEXT("周驰：新单上的运费，有一部分是我垫的旧材料钱。供应商准我在这一批里收。")};
}
void AStationDirector::CompleteDutyConversation(AActor*) {}
FString AStationDirector::GetDutyObjective() const
{
    if(Progress->DutySettled)return Progress->SeenDutyFilm?TEXT("本班已结算 · 可回访和自由探索，家庭旧票可重播尾声"):TEXT("本班已结算 · 在负载箱或事务台查看下班回执，关闭后播放兄妹尾声");
    if(Progress->DutyFaultRaised&&Progress->DutyEmergencyChoice<0)return TEXT("社区端欠压 · 可先询问当事人，再在故障端或负载箱回应电台报警");
    if(Progress->DutyEmergencyChoice==0&&!Progress->DutyCommunityRestored)return Progress->DutyBridgeHeld?TEXT("携分流接头到欠压服务端 · E 安装抢修"):TEXT("到检修负载箱 · E 取出分流接头");
    const int32 Next=GetDutyNextOrder();
    if(Next>=0)return FString::Printf(TEXT("建议就近：%s · 其余未完成工单也可先做；E 检修与调压"),Duty::Jobs[Next]);
    if(!Progress->DutyCommunityRestored)return TEXT("叶岑正赶往欠压端抢修 · 可前去查看或先探索，恢复后在负载箱结算");
    return TEXT("到检修负载箱 · E 核销本班工时与报酬，无需返回主控制台");
}
int32 AStationDirector::GetDutyStage() const
{
    if(Progress->DutySettled)return 6;
    if(DutyReadyToSettle())return 5;
    if(DutyBaseWorkComplete())return 4;
    if(Progress->DutyEmergencyChoice>=0)return 3;
    return Progress->DutyFaultRaised?2:1;
}
AActor* AStationDirector::GetDutyGuidance() const
{
    auto Terminal=[&]()->AActor*{for(auto& O:Objects)if(O->Kind==EStationObjectKind::Terminal)return O;return nullptr;};
    if(Progress->DutySettled)return DutyLoadBank?DutyLoadBank.Get():Terminal();
    if(Progress->DutyFaultRaised&&Progress->DutyEmergencyChoice<0)return GetDutyFaultObject();
    if(Progress->DutyEmergencyChoice==0&&!Progress->DutyCommunityRestored)return Progress->DutyBridgeHeld?GetDutyFaultObject():DutyLoadBank.Get();
    const int32 Next=GetDutyNextOrder();if(Next>=0)return GetDutyOrderObject(Next);
    return !Progress->DutyCommunityRestored?static_cast<AActor*>(Engineer.Get()):DutyLoadBank.Get();
}
FString AStationDirector::GetDutyMenuChoice(int32 I) const
{
    if(Panel==EStationPanel::DutyStart)return I==0?TEXT("接本班检修 · 把签过的负担做到现场"):TEXT("暂缓 · 保留当前进度");
    if(Panel==EStationPanel::DutyConfirm)return I==0?TEXT("确认这项行动 · 承担其后果"):TEXT("返回 · 重新考虑");
    return I==0?TEXT("先抢修社区 · 一张验收作废，之后补测"):TEXT("先完成验收 · 居民继续等候，叶岑随后到场");
}
FString AStationDirector::GetDutyBody() const
{
    if(Panel==EStationPanel::DutyStart)return (Progress->SupplyChoice==1?FString(TEXT("上批抵押：林照本人 8 小时检修收入。\n本班四张验收单，每张核销两小时；完成现场校准才能记入。\n\n")):FString(TEXT("上批公共预付已支出 6。本班是另接的检修委托，三张单各挣 2 个额度；报酬不等于退回原预付款。\n\n")))+TEXT("工单以检修动作与两段现场调压表示时段推进，毋须现实等待八小时。按住 E 加压，松开 E 泄压，读数稳在标线里才计入验收；S 可切换慢速辅助，同额结算。中途退出不记未完成的单。");
    if(Panel==EStationPanel::DutyChoice)return TEXT("社区端出现欠压，获准服务暂时低功率运行。验收方要求先交当前班次，否则作废其中一张。\n\n先抢修：取分流接头，亲手恢复社区服务；已做的一张验收须重测，不能重复领报酬。\n\n先验收：不用补测，社区继续等候；本班工单齐后，叶岑会实际走到现场检修。首夜获准名单不会被撤销。");
    if(Panel==EStationPanel::DutyConfirm)return ConfirmSelection==0?TEXT("立即把行动转向欠压服务端。一张已完成验收失去效力，需要在负载箱重新校准。\n\n社区服务可提前恢复；你多做的这一张不另算报酬。选择保存后不能改成先验收。"):TEXT("继续本班验收，欠压服务维持低功率。最后一张验收完成后，叶岑才会赶去现场，检修到场完成才恢复。\n\n不新增补测；居民在这段时间确实无法正常使用原服务。选择保存后不能改为先抢修。");
    if(Progress->DutySettled)return FString::Printf(TEXT("下班核销回执 / 本班已结\n\n%s\n\n欠压处置：%s。\n首夜配给名单：保留。原欠条与旧担保金：未自动清零。\n\n当前公共备用：%d。\n\n关闭这份回执，回家前听完兄妹的这一段。重读、重播均不重复加钱或核销工时。"),Progress->SupplyChoice==1?TEXT("本人原抵押 8 小时，本班已核销 8，剩余 0。没有将工时转到林禾或其他住户。 "):TEXT("本班劳动报酬 6 已入公共备用。原预付没有被退款，而是通过新劳动补回这笔缓冲。"),Progress->DutyEmergencyChoice==0?TEXT("林照先抢修，并补测作废验收"):TEXT("先交验收，叶岑随后步行到场抢修"),Progress->Reserve);
    const auto* O=Cast<AStationObject>(ConversationTarget);
    if(O==GetDutyFaultObject()&&Progress->DutyFaultRaised)return FString::Printf(TEXT("社区欠压记录 / %s\n\n状态：%s。\n原获准服务仍保留，候补未自动获批。\n\n%s\n\n验收与抢修没有一起消失：每个人都在把等待转给别人。"),Progress->DutyFaultIndex>=0?Duty::Services[Progress->DutyFaultIndex]:TEXT("公共回路"),Progress->DutyCommunityRestored?TEXT("已到场修复"):TEXT("暂时低功率，服务不能正常使用"),*GetDutyObjective());
    return FString::Printf(TEXT("本班验收与工时记录\n\n有效验收：%d / %d。\n本人抵押剩余：%d 小时。\n本班已入账报酬：%d。\n\n当前：%s\n\n未完成的校准退出后重做，已完成验收随存档保留。辅助模式不减少记账。"),GetDutyValidJobs(),Progress->SupplyChoice==1?4:3,FMath::Max(0,Progress->SupplyLaborHours-Progress->DutyCreditedHours),Progress->DutyReward,*GetDutyObjective());
}
FString AStationDirector::GetDutyStateJson() const
{
    const float Light=RationLights.IsValidIndex(Progress->DutyFaultIndex)?RationLights[Progress->DutyFaultIndex]->GetLightComponent()->Intensity:-1;
    return FString::Printf(TEXT("{\"started\":%s,\"work_mask\":%d,\"required_mask\":%d,\"valid_jobs\":%d,\"next_order\":%d,\"fault\":%s,\"fault_index\":%d,\"choice\":%d,\"bridge_held\":%s,\"restored\":%s,\"recheck\":%s,\"credited_hours\":%d,\"hours_remaining\":%d,\"reward\":%d,\"settled\":%s,\"seen_film\":%s,\"reserve\":%d,\"calibration_order\":%d,\"phase\":%d,\"pressure\":%.3f,\"stable\":%.3f,\"held\":%s,\"assisted\":%s,\"repair_clock\":%.3f,\"service_light\":%.2f,\"ration_mask\":%d}"),Progress->DutyStarted?TEXT("true"):TEXT("false"),Progress->DutyWorkMask,GetDutyRequiredMask(),GetDutyValidJobs(),GetDutyNextOrder(),Progress->DutyFaultRaised?TEXT("true"):TEXT("false"),Progress->DutyFaultIndex,Progress->DutyEmergencyChoice,Progress->DutyBridgeHeld?TEXT("true"):TEXT("false"),Progress->DutyCommunityRestored?TEXT("true"):TEXT("false"),Progress->DutyRecheckDone?TEXT("true"):TEXT("false"),Progress->DutyCreditedHours,FMath::Max(0,Progress->SupplyLaborHours-Progress->DutyCreditedHours),Progress->DutyReward,Progress->DutySettled?TEXT("true"):TEXT("false"),Progress->SeenDutyFilm?TEXT("true"):TEXT("false"),Progress->Reserve,DutyCalibrationOrder,DutyPhase,DutyPressure,DutyStable,DutyPressureHeld?TEXT("true"):TEXT("false"),DutyAssist?TEXT("true"):TEXT("false"),DutyRepairClock,Light,Progress->RationMask);
}
