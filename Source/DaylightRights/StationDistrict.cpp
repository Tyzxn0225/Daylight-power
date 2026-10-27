#include "StationPrototype.h"
#include "Components/RectLightComponent.h"
#include "Engine/RectLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

void AStationDirector::CreateDistrict()
{
    if(!DistrictObjects.IsEmpty())return;
    const FVector Positions[]={FVector(0,7000,8),FVector(-2200,10130,8),FVector(-4200,11500,8),FVector(2200,12700,8),FVector(500,11700,8),FVector(1200,10500,8)};
    const TCHAR* Names[]={TEXT("回光埠导览与夜班委托"),TEXT("退光井 · 现场检验"),TEXT("夜班宿舍 · 押金申领册"),TEXT("交易厂房 · 复售合同"),TEXT("回收光柜 · 处置复核"),TEXT("留影亭 · 处置回执")};
    for(int32 I=0;I<6;++I)
    {
        FTransform T(Positions[I]);auto* O=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        O->Kind=EStationObjectKind::DistrictRecord;O->DisplayName=Names[I];O->UseExistingGeometry=I==1;O->FinishSpawning(T);DistrictObjects.Add(O);Objects.Add(O);
    }
    for(const FVector P:{FVector(-2600,10900,770),FVector(-1800,10900,770)})
    {
        auto* L=GetWorld()->SpawnActor<ARectLight>(P,FRotator(-90,0,0));auto* C=CastChecked<URectLightComponent>(L->GetLightComponent());C->SetMobility(EComponentMobility::Movable);
        C->SetIntensityUnits(ELightUnits::Lumens);C->SetSourceWidth(160);C->SetSourceHeight(70);C->SetAttenuationRadius(1900);C->SetLightColor(FLinearColor(1,.74f,.40f));C->SetIntensity(0);DistrictTrialLights.Add(L);
    }
    UpdateDistrictState();
}
void AStationDirector::UpdateDistrictState()
{
    const bool On=Progress->DistrictSettled&&Progress->DistrictChoice==0;
    for(auto& L:DistrictTrialLights)if(L)L->GetLightComponent()->SetIntensity(On?42000.f:0.f);
    for(auto& O:DistrictObjects)if(O)O->SetIndicator(On?FLinearColor(.73f,.52f,.24f):FLinearColor(.13f,.25f,.28f),2.f);
}
bool AStationDirector::HandleDistrictObject(AStationObject* O)
{
    const int32 I=DistrictObjects.IndexOfByKey(O);if(I<0)return false;
    if(!Progress->DutySettled)
    {OpenPanel(EStationPanel::Document);return true;}
    if(!Progress->DistrictStarted)
    {
        if(I!=0){Notify(TEXT("先到回光埠入口读取夜班委托。"),true);return true;}
        Progress->DistrictStarted=true;SaveProgress();Notify(TEXT("夜班委托已登记。检验、押金、复售原单，缺一份都不能替人决定。"));
    }
    if(I==1&&!(Progress->DistrictEvidence&1))
    {BeginPhysicalAction(O,TEXT("Repair"),-40);return true;}
    if(I==2||I==3){Progress->DistrictEvidence|=I==2?2:4;SaveProgress();}
    if(I==4&&!Progress->DistrictSettled)
    {
        if(Progress->DistrictEvidence!=7){Notify(TEXT("请先完成检光，并读取宿舍押金与厂房复售原单。"),true);return true;}
        OpenPanel(EStationPanel::DistrictChoice);return true;
    }
    OpenPanel(EStationPanel::Document);return true;
}
void AStationDirector::PressDistrictChoice()
{
    if(Panel==EStationPanel::DistrictChoice){ConfirmSelection=Selection;OpenPanel(EStationPanel::DistrictConfirm);Selection=1;return;}
    if(Panel!=EStationPanel::DistrictConfirm)return;
    if(Selection!=0){OpenPanel(EStationPanel::DistrictChoice);Selection=ConfirmSelection;return;}
    if(Progress->DistrictEvidence!=7||Progress->DistrictSettled||!Progress->DutySettled)return;
    auto* Target=DistrictObjects.IsValidIndex(4)?DistrictObjects[4].Get():nullptr;
    DistrictPendingChoice=ConfirmSelection;ClosePanel();Focus=Target;
    if(!BeginPhysicalAction(Target,TEXT("Repair"),-41))DistrictPendingChoice=-1;
}
void AStationDirector::CompleteDistrictAction(int32 A)
{
    if(!Progress->DistrictStarted||!Progress->DutySettled)return;
    if(A==-40)
    {
        Progress->DistrictEvidence|=1;SaveProgress();Notify(TEXT("检光完成：设备合格。被终止的是地址订阅，灯并没有损坏。"));
        if(DistrictObjects.IsValidIndex(1)){Focus=DistrictObjects[1];OpenPanel(EStationPanel::Document);}return;
    }
    if(A!=-41||Progress->DistrictSettled||Progress->DistrictEvidence!=7||DistrictPendingChoice<0||DistrictPendingChoice>1)return;
    Progress->DistrictChoice=DistrictPendingChoice;DistrictPendingChoice=-1;Progress->DistrictSettled=true;SaveProgress();UpdateDistrictState();
    Notify(Progress->DistrictChoice==0?TEXT("试亮回路已接入。夜班押金延期，异议和签名已保留。前往留影亭核对回执。"):TEXT("试亮回路已封存。夜班押金优先退回，公共试亮取消。前往留影亭核对回执。"));
}
int32 AStationDirector::GetDistrictStage() const
{
    if(Progress->SeenDistrictFilm)return 6;
    if(Progress->DistrictSettled)return 5;
    if(Progress->DistrictEvidence==7)return 4;
    return 1+FMath::CountBits(uint32(Progress->DistrictEvidence&7));
}
FString AStationDirector::GetDistrictObjective() const
{
    if(Progress->SeenDistrictFilm)return TEXT("回光埠处置已留档 · 可自由探索并查看两片社区");
    if(Progress->DistrictSettled)return TEXT("到留影亭读取处置回执，听完兄妹的这一段");
    if(!(Progress->DistrictEvidence&1))return TEXT("到退光井亲手检验被回收的灯具");
    if(!(Progress->DistrictEvidence&2))return TEXT("读取夜班宿舍门外的押金申领册");
    if(!(Progress->DistrictEvidence&4))return TEXT("进入交易厂房，核对灯具复售合同");
    return TEXT("到回收光柜复核代价，并亲手接入或封存回路");
}
AActor* AStationDirector::GetDistrictGuidance() const
{
    if(DistrictObjects.Num()!=6||Progress->SeenDistrictFilm)return nullptr;
    return DistrictObjects[Progress->DistrictSettled?5:!(Progress->DistrictEvidence&1)?1:!(Progress->DistrictEvidence&2)?2:!(Progress->DistrictEvidence&4)?3:4];
}
FString AStationDirector::GetDistrictChoiceLabel(int32 I) const
{
    if(Panel==EStationPanel::DistrictConfirm)return I==0?TEXT("确认这笔代价，并亲手操作回路"):TEXT("返回，重新核对");
    return I==0?TEXT("6 额度用于公共试亮两小时 · 工人押金延期"):TEXT("6 额度先退夜班押金 · 取消公共试亮");
}
FString AStationDirector::GetDistrictBody() const
{
    if(Panel==EStationPanel::DistrictChoice)return TEXT("同一批退灯，有两笔互不相容的申请。\n\n本批清算额只有 6。公共检验试亮两小时需要 6；夜班工人待退押金合计也是 6。不能同时支出。\n\n先试亮：保住一次公共见光和公开检验，押金顺延，领取人没有同意替你担保。\n先退押金：工人拿回自己的钱，检光井取消试亮，灯具进入封存与复售流程。原始检验记录仍然保留。");
    if(Panel==EStationPanel::DistrictConfirm)return ConfirmSelection==0?TEXT("你选择先试亮两小时。\n\n清算额 6 用于试亮，已申领的夜班押金延期。回执会记录林照的签名与领取人的异议，不能写成“全体自愿”。\n\n确认后还需在柜前完成接线。中途取消不扣款、不记结算。原社区的余款和名单不变。"):TEXT("你选择先退押金。\n\n本批清算额 6 退给已登记的夜班工人。检光井取消公共试亮，灯具封存待复售，检验合格与停订原因仍可查。\n\n确认后还需在柜前完成封存。中途取消不结算。原社区的余款和名单不变。");
    const auto* Doc=Cast<AStationObject>(ConversationTarget);const int32 I=DistrictObjects.IndexOfByKey(Doc);
    if(!Progress->DutySettled)return TEXT("回光埠 / 旧水厂回收片区\n\n原用途：冷却水沉淀与维修。现用途：光源回收、检验、估值、复售；宿舍与工位捆绑出租。\n\n可自由参观下沉检光井、交易厂房、夜班宿舍与恒昼样本厅。完成旧站本班结算后，可在入口接取夜班委托。\n\n被收回的灯，不一定坏了。");
    if(I==0)return TEXT("M10 / 光退到哪里\n\n衡昼把回收、估值和押金清算委托给同一家公司。夜班工人要求退款，居民要求公开试亮。厂方都同意受理，但只给了一笔 6 额度的清算额。\n\n先去检光井检查设备，再核对宿舍押金册与厂房复售合同。读完三份原件后，到回收光柜选择处置。\n\n叶岑保检修组的工资，周驰保垫款能收回，林禾也想要光。没人自动站在你这边。");
    if(I==1)return TEXT("退光井 / 现场检验副本\n\n结果：灯具合格；光效仍在可用范围。停用原因：原地址下一期订阅未获支付，并非报废。\n\n旧水池成为公开检光井。四根检验桅杆悬挂灯具；环廊供检验与旁听，低处保留原池壁水线。\n\n合格不等于免费。公开的技术结果，也可能被拿去抬高下一次转售价。");
    if(I==2)return TEXT("夜班宿舍 / 押金申领册\n\n已登记待退押金：6。领取人意见：先还本人垫付的钱，公共试亮另行筹资。\n\n附注：宿舍与检修工位捆绑。有人愿意先等，是怕丢工作；有人坚决退款，家里也等着这笔钱。\n\n叶岑的检修组也有人在名单上。周驰提出折价收购退押金凭条，不是无偿垫款。\n\n异议已登记，不允许由检验人代签同意。");
    if(I==3)return TEXT("交易厂房 / 本批复售合同\n\n灯具所有权：衡昼资产管理。原住户购买的是按地址供应的服务。停订后收回光源，转售收入按合同归设备所有人。\n\n本批核定清算额：6。可用于押金退款，或公共检验试亮两小时；同一笔金额不得重复支出。\n\n恒昼样本厅展示稳定舒适的光照。维修与估值在旧厂房完成，客户看到的只是最后那一扇明亮的窗。");
    if(I==5&&!Progress->DistrictSettled)return TEXT("留影亭 / 回执尚未签发\n\n太阳消失后，这里的旧时刻表再也没有更新。人们留下的，是窗框曾经投下的影子。\n\n本轮处置尚未执行。先核对三份原件，再到回收光柜完成接线或封存。这里不会提前写成已退款或已供光。");
    if(Progress->DistrictSettled)return FString::Printf(TEXT("回光埠 / 已生效处置回执\n\n本批总额：6。试亮支出：%d；押金退款：%d；结余：0。\n\n%s\n\n签署人：林照。原始检验、押金申领与异议均保留。没有追加公共备用，没有免除旧债，没有代替领取人表示原谅。\n\n收起回执后播放本章尾声，可在家庭旧票重播。"),Progress->DistrictChoice==0?6:0,Progress->DistrictChoice==1?6:0,Progress->DistrictChoice==0?TEXT("公共试亮已接入；夜班押金延期。两小时为本轮获准服务，后续不自动续期。"):TEXT("夜班押金优先退款；公共试亮取消。灯具封存待复售，停订原因仍然可查。"));
    return TEXT("回收光柜 / 尚未结算\n\n请先完成现场检验，并核对押金与复售原单。检验结果和退款要求都不是空白。选择处置之前，先把这两边的人看见。");
}
FString AStationDirector::GetDistrictStateJson() const
{
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))&&PC&&PC->GetPawn())
    {
        FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(DistrictRouteAudit),false,PC->GetPawn());
        const FVector P=PC->GetPawn()->GetActorLocation();
        if(auto* C=Cast<ACharacter>(PC->GetPawn()))
        {
            auto* Move=C->GetCharacterMovement();
            UE_LOG(LogTemp,Display,TEXT("DISTRICT_MOTION position=%s acceleration=%s input=%s normal=%s walkable=%d ignored=%d"),*P.ToString(),*Move->GetCurrentAcceleration().ToString(),*C->GetLastMovementInputVector().ToString(),*Move->CurrentFloor.HitResult.ImpactNormal.ToString(),Move->CurrentFloor.IsWalkableFloor(),PC->IsMoveInputIgnored());
        }
        if(GetWorld()->SweepSingleByChannel(H,P,P+PC->GetPawn()->GetActorForwardVector()*180,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(40,75),Q))
            UE_LOG(LogTemp,Display,TEXT("DISTRICT_ROUTE_BLOCK actor=%s component=%s at=%s"),H.GetActor()?*H.GetActor()->GetName():TEXT("none"),H.GetComponent()?*H.GetComponent()->GetName():TEXT("none"),*H.ImpactPoint.ToString());
    }
    return FString::Printf(TEXT("{\"started\":%s,\"evidence\":%d,\"choice\":%d,\"settled\":%s,\"seen_film\":%s,\"stage\":%d,\"stock\":6,\"trial_cost\":%d,\"refund\":%d,\"old_reserve\":%d,\"trial_light\":%.1f}"),Progress->DistrictStarted?TEXT("true"):TEXT("false"),Progress->DistrictEvidence,Progress->DistrictChoice,Progress->DistrictSettled?TEXT("true"):TEXT("false"),Progress->SeenDistrictFilm?TEXT("true"):TEXT("false"),GetDistrictStage(),Progress->DistrictSettled&&Progress->DistrictChoice==0?6:0,Progress->DistrictSettled&&Progress->DistrictChoice==1?6:0,Progress->Reserve,DistrictTrialLights.Num()?DistrictTrialLights[0]->GetLightComponent()->Intensity:0.f);
}
