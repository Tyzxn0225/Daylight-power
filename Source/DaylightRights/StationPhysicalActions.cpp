#include "StationPrototype.h"
#include "Animation/AnimationAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

FString AStationDirector::GetPhysicalActionLabel() const
{
    if(PendingActionTarget&&PendingFinishTime<=0&&GetWorld()->GetTimeSeconds()>PendingAlignUntil+.15f)return TEXT("等待前方让行");
    if(PendingRepairIndex==-70)return GetArcObjective();
    if(PendingRepairIndex==-60)return TEXT("接入回水表，实测余热换热上限");
    if(PendingRepairIndex==-61)return TEXT("隔离景观加温，保留住宅与通风");
    if(PendingRepairIndex==-62)return TEXT("按已复核分流锁定阀门");
    if(PendingRepairIndex==-50)return TEXT("抽取原始底联并保留副本");
    if(PendingRepairIndex==-51)return TEXT("提交独立本人意见与已复核申请");
    if(PendingRepairIndex<=-30&&PendingRepairIndex>=-33||PendingRepairIndex==-36)return TEXT("接入现场调压表");
    if(PendingRepairIndex==-34)return TEXT("取出分流接头并收纳");
    if(PendingRepairIndex==-40)return TEXT("检验回收灯与停订原因");
    if(PendingRepairIndex==-41)return TEXT("按已复核处置接线或封存");
    if(PendingRepairIndex==-35)return TEXT("接入分流接头恢复社区服务");
    if(PendingRepairIndex==-20)return TEXT("记录近街端设备读数");
    if(PendingRepairIndex==-21)return TEXT("记录应急端设备读数");
    if(PendingRepairIndex==-22)return TEXT("接通并检测人工旁路");
    if(PendingRepairIndex==-23)return TEXT("核对封签并收纳耗材袋");
    if(PendingRepairIndex==-24)return TEXT("接入新批熔断芯与冷却耗材");
    if(PendingRepairIndex==-10)return TEXT("取出稳压器并装入工具袋");
    if(PendingRepairIndex==-11)return TEXT("接入应急稳压器");
    if(PendingRepairIndex==-12)return TEXT("核对封签并存入交接柜");
    if(PendingRepairIndex==-13)return TEXT("复位送货后门插销，检查门后的领料痕迹");
    return PendingRepairIndex>=0?TEXT("检查并锁紧接口"):TEXT("取出备用接口");
}
float AStationDirector::GetPhysicalActionProgress() const
{
    if(!PendingActionTarget||PendingFinishTime<=PendingAlignUntil)return 0;
    return FMath::Clamp((GetWorld()->GetTimeSeconds()-PendingAlignUntil)/(PendingFinishTime-PendingAlignUntil),0.f,1.f);
}

bool AStationDirector::BeginPhysicalAction(AStationObject* Target,const FString& Action,int32 RepairIndex)
{
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    auto* Clip=LoadObject<UAnimationAsset>(nullptr,*FString::Printf(TEXT("/Game/BZ/Characters/LinZhao/Animations/A_LinZhao_%s.A_LinZhao_%s"),*Action,*Action));
    if(!Character||!IsValid(Target)||PendingActionTarget||!Clip)return false;
    if(Character->GetCharacterMovement()->IsFalling()){Notify(TEXT("请先站稳，再操作设备。"),true);return false;}
    // Each device faces its local +Y. Sweep along the existing floor; never teleport
    // through a wall or alter height just to manufacture a contact pose.
    PendingStandPoint=Target->GetActorLocation()+Target->GetActorRightVector()*72;PendingStandPoint.Z=Character->GetActorLocation().Z;
    if(FVector::Dist2D(PendingStandPoint,Character->GetActorLocation())>145)
    {
        Notify(TEXT("请走到设备正面，再开始操作。"),true);return false;
    }
    CancelCommunityVignette();
    PendingActionTarget=Target;PendingAction=Action;PendingRepairIndex=RepairIndex;PendingContactSound=false;
    PendingAlignUntil=GetWorld()->GetTimeSeconds()+.38f;PendingAlignmentDeadline=GetWorld()->GetTimeSeconds()+2.7f;PendingFinishTime=0;
    PlayerAction.Empty();PlayerActionUntil=0;StopPresentationVoice();SetInputLock(true);
    Character->GetCharacterMovement()->StopMovementImmediately();UpdateWorkYield();return true;
}
void AStationDirector::CancelPhysicalAction(bool ShowMessage)
{
    if(!PendingActionTarget)return;
    if(ShowMessage&&PendingRepairIndex==-70)ArcPendingNode.Empty();
    if(ShowMessage&&PendingRepairIndex<=-50)MandatePendingChoice=-1;
    if(ShowMessage&&PendingRepairIndex<=-60)CrisisPending=-1;
    PendingActionTarget=nullptr;PendingAction.Empty();PendingRepairIndex=-1;PendingFinishTime=0;
    UpdateWorkYield();
    PlayerAction.Empty();PlayerActionUntil=0;PlayerMotionKey.Empty();
    if(!Starting&&!IsMenuOpen())SetInputLock(false);
    if(ShowMessage)Notify(TEXT("操作已中止，物件和连接状态未改变。"));
}
void AStationDirector::TickPhysicalAction(float Delta)
{
    if(!PendingActionTarget)return;
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    if(!Character||!IsValid(PendingActionTarget)||FVector::Dist2D(Character->GetActorLocation(),PendingActionTarget->GetActorLocation())>170)
    {
        CancelPhysicalAction();return;
    }
    const float Now=GetWorld()->GetTimeSeconds();
    if(Character->GetCharacterMovement()->IsFalling())
    {
        if(PendingFinishTime>0||Now>=PendingAlignmentDeadline)CancelPhysicalAction();
        return;
    }
    Character->GetCharacterMovement()->StopMovementImmediately();
    FRotator Facing=(PendingActionTarget->GetActorLocation()-Character->GetActorLocation()).Rotation();Facing.Pitch=0;Facing.Roll=0;
    Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),Facing,Delta,12));
    if(PendingFinishTime<=0)
    {
        PendingStandPoint.Z=Character->GetActorLocation().Z;
        const FVector Next=FMath::VInterpTo(Character->GetActorLocation(),PendingStandPoint,Delta,12);
        FHitResult Hit;Character->SetActorLocation(Next,true,&Hit);
        if(Now<PendingAlignUntil)return;
        if(FVector::Dist2D(Character->GetActorLocation(),PendingStandPoint)>12)
        {
            if(Now<PendingAlignmentDeadline)return;
            if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))UE_LOG(LogTemp,Display,TEXT("STATION_ALIGN_BLOCK actor=%s component=%s start_penetrating=%d current=%s goal=%s target=%s people=%s"),Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("none"),Hit.GetComponent()?*Hit.GetComponent()->GetName():TEXT("none"),Hit.bStartPenetrating,*Character->GetActorLocation().ToString(),*PendingStandPoint.ToString(),*PendingActionTarget->GetActorLocation().ToString(),*GetCommunityStateJson());
            CancelPhysicalAction(false);Notify(TEXT("设备前方被挡住了，请换一个站位。"),true);return;
        }
        PendingAlignUntil=Now;PlayPlayerAction(PendingAction);PendingFinishTime=PlayerActionUntil;return;
    }
    if(!PendingContactSound&&Now>=PendingAlignUntil+(PendingFinishTime-PendingAlignUntil)*.48f)
    {
        PendingContactSound=true;
        if(PendingRepairIndex>=0||PendingRepairIndex==-11||PendingRepairIndex<=-20&&PendingRepairIndex!=-23&&PendingRepairIndex!=-34)
        {
            PlayCue(TEXT("FX_Repair"),TargetPoint(PendingActionTarget),.55);
            PlayCharacterReaction(TEXT("VR_LinZhao_Repair"),Character->GetActorLocation()+FVector(0,0,60),.36f);
        }
    }
    if(Now<PendingFinishTime)return;
    const int32 Index=PendingRepairIndex;
    if(Index==-70){CancelPhysicalAction(false);CompleteArcAction(Index);return;}
    if(Index<=-60){CancelPhysicalAction(false);CompleteCrisisAction(Index);return;}
    if(Index<=-50){CancelPhysicalAction(false);CompleteMandateAction(Index);return;}
    if(Index<=-40){CancelPhysicalAction(false);CompleteDistrictAction(Index);return;}
    if(Index<=-30){CancelPhysicalAction(false);CompleteDutyAction(Index);return;}
    if(Index<=-20){CancelPhysicalAction(false);CompleteSupplyAction(Index);return;}
    if(Index<=-10)
    {
        CancelPhysicalAction(false);CompleteConflictAction(Index);return;
    }
    if(Index>=0&&Index<3)Progress->Connections|=1<<Index;
    else if(PendingAction==TEXT("Reach"))Progress->HasConnector=true;
    // Full completion is the only checkpoint/quest commit. Cancelling never grants
    // a connector or repair credit, even if the hand already touched the device.
    CancelPhysicalAction(false);ApplyProgress();SaveProgress();
    if(Index>=0)Notify(Progress->Connections==7?TEXT("三处连接就绪。回到叶岑身边，先听启动风险。"):TEXT("连接已确认。其余节点仍需分别检查。"));
    else {PlayCue(TEXT("UI_Stored"),FVector::ZeroVector,.3,false);Notify(TEXT("已取得一套备用接口。"));}
}
