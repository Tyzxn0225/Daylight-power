#include "StationPrototype.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/RectLight.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace Ration
{
    const int32 Costs[]={32,24,24};
    const TCHAR* Names[]={TEXT("诊疗点"),TEXT("互助厨房"),TEXT("居民共用照明")};
    const TCHAR* Keys[]={TEXT("Clinic"),TEXT("Kitchen"),TEXT("Housing")};
    const TCHAR* Hours[]={TEXT("4 小时"),TEXT("3 小时"),TEXT("3 小时")};
    int32 Cost(int32 Mask){int32 Total=0;for(int32 I=0;I<3;++I)if(Mask&(1<<I))Total+=Costs[I];return Total;}
    int32 Index(const AStationObject* Object){return Object?int32(Object->Kind)-int32(EStationObjectKind::ClinicRequest):-1;}
    FString Summary(int32 Mask)
    {
        FString Text;for(int32 I=0;I<3;++I)if(Mask&(1<<I)){if(!Text.IsEmpty())Text+=TEXT("、");Text+=Names[I];}
        return Text.IsEmpty()?TEXT("尚未选择"):Text;
    }
}

int32 AStationDirector::GetRationDraftCost() const {return Ration::Cost(RationDraftMask);}
void AStationDirector::UpdateRationActors()
{
    if(!IsRationChapter())
    {
        for(auto& Point:RationPoints)if(Point){Objects.Remove(Point);Point->Destroy();}
        for(auto& Light:RationLights)if(Light)Light->Destroy();
        RationPoints.Empty();RationLights.Empty();RationDraftMask=0;PendingRationMask=0;return;
    }
    if(RationPoints.IsEmpty())
    {
        const float Xs[]={-2550,-250,1070};
        for(int32 I=0;I<3;++I)
        {
            const TCHAR* LocalKeys[]={TEXT("ClinicRequest"),TEXT("KitchenRequest"),TEXT("HousingRequest")};
            FTransform Transform(NeighborhoodReady?NeighborhoodPoint(LocalKeys[I],FVector(Xs[I],3985,7)):PlaceOnPavement(FVector(Xs[I],3985,7)));
            auto* Point=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            Point->Kind=EStationObjectKind(int32(EStationObjectKind::ClinicRequest)+I);
            Point->DisplayName=FString(Ration::Names[I])+TEXT(" · 现场回执");Point->Indoor=NeighborhoodReady&&I<2;Point->FinishSpawning(Transform);
            Objects.Add(Point);RationPoints.Add(Point);
            auto* Light=GetWorld()->SpawnActor<ARectLight>(Transform.GetLocation()+(NeighborhoodReady&&I<2?FVector(0,0,240):FVector(0,120,290)),FRotator(-90,0,0));
            Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            auto* Rect=CastChecked<URectLightComponent>(Light->GetLightComponent());
            Rect->SetIntensityUnits(ELightUnits::Lumens);Rect->SetSourceWidth(180);Rect->SetSourceHeight(80);
            Rect->SetAttenuationRadius(560);Rect->SetLightColor(FLinearColor(1,.72,.38));Rect->SetIntensity(0);
            RationLights.Add(Light);
        }
    }
    for(int32 I=0;I<RationPoints.Num();++I)
    {
        const FString Status=Progress->RationMask<0?TEXT("Pending"):(Progress->RationMask&(1<<I))?TEXT("On"):TEXT("Wait");
        const FString Name=FString::Printf(TEXT("M_Ration%s%s"),Ration::Keys[I],*Status);
        if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/BZ/Gameplay/Materials/%s.%s"),*Name,*Name)))RationPoints[I]->Screen->SetMaterial(0,Mat);
        RationPoints[I]->SetServicePower(Progress->RationMask>=0&&(Progress->RationMask&(1<<I)));
    }
    TickRationLights(0);
}
void AStationDirector::TickRationLights(float Delta)
{
    for(int32 I=0;I<RationLights.Num();++I)
    {
        const float Goal=Progress->RationMask>=0&&(Progress->RationMask&(1<<I))?24000.f:0.f;
        auto* Component=RationLights[I]->GetLightComponent();
        Component->SetIntensity(Delta>0?FMath::FInterpTo(Component->Intensity,Goal,Delta,1.6f):Goal);
    }
}
bool AStationDirector::HandleRationObject(AStationObject* Object)
{
    if(!IsRationChapter())return false;
    const int32 I=Ration::Index(Object);
    if(I>=0&&I<3)
    {
        if(Progress->RationMask<0)Progress->RequestReadMask|=1<<I;
        else Progress->RationVisitMask|=1<<I;
        SaveProgress();OpenPanel(EStationPanel::Document);PlayCue(TEXT("FX_Paper"),TargetPoint(Object),.3f);return true;
    }
    if(Object->Kind!=EStationObjectKind::Terminal)return false;
    if(Progress->RationMask<0)
    {
        if(Progress->RequestReadMask!=7){Notify(TEXT("先到三处核验牌阅读申请，不能只凭总价分配。"),true);return true;}
        if(!Progress->RationBriefed){Notify(TEXT("先请叶岑核对三份申请与剩余额度。"),true);return true;}
        OpenRationPlan();return true;
    }
    if(Progress->RationVisitMask==7&&Progress->RationReviewMask==7)
    {
        Progress->ReadRecall=true;SaveProgress();
    }
    OpenPanel(EStationPanel::Document);return true;
}
void AStationDirector::OpenRationPlan()
{
    RationDraftMask=0;PendingRationMask=0;OpenPanel(EStationPanel::RationPlan);
}
void AStationDirector::PressRationChoice()
{
    if(Selection<3)
    {
        const int32 Next=RationDraftMask^(1<<Selection);
        if(Ration::Cost(Next)>Progress->Reserve){Notify(TEXT("额度不足。先取消其他申请，再调整分配。"),true);return;}
        RationDraftMask=Next;BeginPanelPresentation();return;
    }
    if(!RationDraftMask){Notify(TEXT("至少选择一处供光服务，才能提交本轮分配。"),true);return;}
    PendingRationMask=RationDraftMask;OpenPanel(EStationPanel::RationConfirm);Selection=1;
}
void AStationDirector::CommitRation()
{
    const int32 Cost=Ration::Cost(PendingRationMask);
    if(!IsRationChapter()||!Progress->RationBriefed||Progress->RequestReadMask!=7||Progress->RationMask>=0||PendingRationMask<=0||PendingRationMask>7||Cost>Progress->Reserve)return;
    Progress->RationBudget=Progress->Reserve;Progress->RationCost=Cost;Progress->Reserve-=Cost;
    Progress->RationMask=PendingRationMask;Progress->RationVisitMask=0;Progress->RationReviewMask=0;Progress->ReadRecall=false;
    SaveProgress();ClosePanel();UpdateRationActors();
    // Start from dark only on the actual relay event. Loading a checkpoint does not spend again.
    for(auto& Light:RationLights)Light->GetLightComponent()->SetIntensity(0);
    PlayCue(TEXT("FX_Relay"),FVector(-580,3765,180),.65f);
    Notify(FString::Printf(TEXT("首夜分配已生效：%s。去三处核验实际结果。"),*Ration::Summary(Progress->RationMask)));
}
TArray<FString> AStationDirector::GetRationConversation() const
{
    if(Progress->RationMask<0)
    {
        if(ConversationTarget==Engineer)
        {
            if(Progress->RequestReadMask!=7)return {TEXT("叶岑：先看三份申请。有人等救治，有人等开饭，也有人想把自己的损失算在别人头上。")};
            return {TEXT("叶岑：三份申请，八十额度。账上的余额，你已经看到了。"),
                TEXT("林照：开站时照得越远，现在能续上的越少。"),
                TEXT("叶岑：诊疗点照样收担保金。厨房名单里，也有我的人。"),
                TEXT("林照：那你凭什么替他们担保？"),
                TEXT("叶岑：因为明天还得有人修灯。现在轮到你签字了。")};
        }
        if(ConversationTarget==LinHe)return {TEXT("林禾：居民共用的灯，不是谁家独占。我会排队，也会抢位置。别把我写得太高尚。")};
        return {TEXT("周驰：饭票、担保、排队的位置，都能卖。你决定点哪盏灯，我决定去哪条队伍。")};
    }
    if(ConversationTarget==LinHe)return Progress->RationMask&4
        ?TArray<FString>{TEXT("林禾：我会去领。但别拿我的名字，当你公平的证据。"),TEXT("林照：这次没人替你插队。"),TEXT("林禾：有灯的时候，我也想多占一会。这件事，你也记上。")}
        :TArray<FString>{TEXT("林禾：我排队了，没轮到。"),TEXT("林照：我想先保住更急的。"),TEXT("林禾：你不用躲着我。也别让我替你说，没关系。")};
    if(ConversationTarget==Engineer)return Progress->RationMask&1
        ?TArray<FString>{TEXT("叶岑：诊疗点亮了。明天交不起担保金的人，还是会被挡在门外。"),TEXT("林照：你早就知道？"),TEXT("叶岑：我知道。我还是签了。你也一样。")}
        :TArray<FString>{TEXT("叶岑：他们把没担保的病人，送去了付费区。"),TEXT("林照：付费区愿意接？"),TEXT("叶岑：先签欠条。你省下的额度，最后是谁付了？")};
    return Progress->RationMask&2
        ?TArray<FString>{TEXT("周驰：厨房领到了光。登记册是我递的，我当然得留个位置。"),TEXT("林照：互助也要收你的钱？"),TEXT("周驰：搬耗材的是我。你们谈互助的时候，怎么总把这段略掉？")}
        :TArray<FString>{TEXT("周驰：厨房停了。我手里的饭票，今晚涨价。"),TEXT("林照：你就等着这个。"),TEXT("周驰：我等着吃饭。你可以骂我，空着肚子再骂。")};
}
void AStationDirector::CompleteRationConversation(AActor* Person)
{
    if(Progress->RationMask<0)
    {
        if(Person==Engineer&&Progress->RequestReadMask==7)Progress->RationBriefed=true;
    }
    else
    {
        if(Person==LinHe)Progress->RationReviewMask|=1;
        if(Person==Engineer)Progress->RationReviewMask|=2;
        if(Person==ZhouChi)Progress->RationReviewMask|=4;
    }
    SaveProgress();
}
FString AStationDirector::GetRationDocument(const AStationObject* Object) const
{
    const int32 I=Ration::Index(Object);
    if(I>=0&&I<3)
    {
        static const TCHAR* Claims[]={
          TEXT("申请：稳定照明与药柜供电，4 小时 / 32 额度。\n\n院方承诺延长急诊，却保留担保金门槛。批注写着：不收钱，下一批药谁来进？没有担保的病人将被转介到付费区，转介并不意味着免费。"),
          TEXT("申请：备餐照明、育苗与保温，3 小时 / 24 额度。\n\n厨房承诺按登记发餐。耗材由周驰垫付，登记册上却留有一栏优先饭票。叶岑的维修小组也在优先栏内。批注：撤掉他们，明天谁送货？"),
          TEXT("申请：居民共用灯与轮候休息区，3 小时 / 24 额度。\n\n按户轮候，不设检修家属优先。林禾重新登记；有人已经收钱替人占位。批注：门口的灯归大家，门口的位置归谁？")};
        FString Text=FString(Ration::Names[I])+TEXT(" · 首夜申请\n\n")+Claims[I];
        if(Progress->RationMask<0)Text+=TEXT("\n\n本牌只记录申请，不等于批准。三项合计 80；请到主控制台核对余额。");
        else
        {
            const bool On=(Progress->RationMask&(1<<I))!=0;
            Text+=On?FString::Printf(TEXT("\n\n回执：本轮获得 %s，已付 %d 额度。服务时段锁定，供光已经派出。"),Ration::Hours[I],Ration::Costs[I]):TEXT("\n\n回执：本轮候补，无新增服务供光，不收本项费用。街道照明不代表本项服务获准。");
            if(!On)Text+=I==0?TEXT("\n转介登记：付费区接收前须签欠条。"):I==1?TEXT("\n停餐登记：原饭票未作废，二手转让价已上调。"):TEXT("\n轮候登记：队列保留，居民不能进入富人区的常昼花园。");
        }
        return Text;
    }
    if(Progress->RationMask<0)return FString::Printf(TEXT("首夜配给总账\n\n可用额度：%d / 100。诊疗 32、厨房 24、居民共用照明 24，合计 80。\n\n每份申请不可拆分；已承诺的服务包含最低稳压、耗材与人员时段。此前开站的街道基础照明另计，不能退回额度。\n\n先阅读三处核验牌，再请叶岑核账。"),Progress->Reserve);
    FString Text=FString::Printf(TEXT("首夜结算 · 已生效\n获准：%s。\n分配前 %d − 本轮 %d = 现余 %d。"),*Ration::Summary(Progress->RationMask),Progress->RationBudget,Progress->RationCost,Progress->Reserve);
    if(Progress->RationVisitMask!=7||Progress->RationReviewMask!=7)return Text+TEXT("\n\n待核验：到三处核验牌查看结果，再听林禾、叶岑与周驰的回应。外部复核回执尚未送达。");
    return Text+TEXT("\n\n衡昼资产管理 · 抵押复核通知\n下轮耗材须采用指定供应商，并提交受益住户登记；签约前保留中止供应权。\n\n原件附言：你们可以自行决定谁先见光，供应商由我们决定。\n\n待办：连续供光协议 / 富人区维护通道。\n协议尚未签署，现余款项由社区保管。");
}
FString AStationDirector::GetRationChoiceBody() const
{
    if(Panel==EStationPanel::RationConfirm)return FString::Printf(TEXT("批准：%s。\n\n本轮消耗 %d，确认后余额 %d。未选服务继续候补。\n\n确认后派出首夜服务，本轮不能撤回；返回可继续调整。"),*Ration::Summary(PendingRationMask),Ration::Cost(PendingRationMask),Progress->Reserve-Ration::Cost(PendingRationMask));
    return FString::Printf(TEXT("可用 %d 额度 · 已选 %d · 预计余 %d\n三项合计 80，无法全保。E 勾选或取消，选好后核对并提交。"),Progress->Reserve,GetRationDraftCost(),Progress->Reserve-GetRationDraftCost());
}
int32 AStationDirector::GetRationStage() const
{
    if(Progress->RequestReadMask!=7)return 0;if(!Progress->RationBriefed)return 1;
    if(Progress->RationMask<0)return 2;if(Progress->RationVisitMask!=7)return 3;
    if(Progress->RationReviewMask!=7)return 4;if(!Progress->ReadRecall)return 5;return 6;
}
FString AStationDirector::GetRationObjective() const
{
    static const TCHAR* Texts[]={TEXT("核验诊疗点、互助厨房和居民照明的申请"),TEXT("请叶岑核对三份申请与余额"),TEXT("在主控制台勾选服务，复核并提交首夜配给"),TEXT("到三处核验牌查看实际供光结果"),TEXT("回访三人，听完首夜配给后的回应"),TEXT("返回主控制台，阅读结算与抵押复核通知"),TEXT("首夜已结算 · 连续供光协议成为下一条线索")};
    return Texts[GetRationStage()];
}
AActor* AStationDirector::GetRationGuidance() const
{
    const int32 Stage=GetRationStage();
    if(Stage==0||Stage==3)
    {
        const int32 Mask=Stage==0?Progress->RequestReadMask:Progress->RationVisitMask;
        for(int32 I=0;I<RationPoints.Num();++I)if(!(Mask&(1<<I)))return RationPoints[I];
    }
    if(Stage==1)return Engineer;
    if(Stage==4){if(!(Progress->RationReviewMask&1))return LinHe;if(!(Progress->RationReviewMask&2))return Engineer;return ZhouChi;}
    // Settlement is also the entry to the consignment investigation. Keep the
    // same physical desk marked until that next mission is accepted.
    if(Stage==2||Stage==5||Stage==6)for(auto& Object:Objects)if(Object->Kind==EStationObjectKind::Terminal)return Object;
    return nullptr;
}
FString AStationDirector::GetRationStateJson() const
{
    FString Intensities;for(auto& Light:RationLights){if(!Intensities.IsEmpty())Intensities+=TEXT(",");Intensities+=FString::Printf(TEXT("%.1f"),Light->GetLightComponent()->Intensity);}
    return FString::Printf(TEXT("{\"active\":%s,\"requests\":%d,\"briefed\":%s,\"mask\":%d,\"draft\":%d,\"draft_cost\":%d,\"budget\":%d,\"cost\":%d,\"reserve\":%d,\"visits\":%d,\"reviews\":%d,\"complete\":%s,\"stage\":%d,\"service_lights\":[%s]}"),IsRationChapter()?TEXT("true"):TEXT("false"),Progress->RequestReadMask,Progress->RationBriefed?TEXT("true"):TEXT("false"),Progress->RationMask,RationDraftMask,GetRationDraftCost(),Progress->RationBudget,Progress->RationCost,Progress->Reserve,Progress->RationVisitMask,Progress->RationReviewMask,Progress->ReadRecall?TEXT("true"):TEXT("false"),GetRationStage(),*Intensities);
}
