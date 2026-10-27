#include "StationPrototype.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"

void AStationDirector::UpdateAllocationActors()
{
    if(!Progress->Started)
    {
        if(ZhouChi){ZhouChi->Destroy();ZhouChi=nullptr;}
        if(AllocationLedger){Objects.Remove(AllocationLedger);AllocationLedger->Destroy();AllocationLedger=nullptr;}
        return;
    }
    if(!ZhouChi)
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ZhouChi=GetWorld()->SpawnActor<AStationEngineer>(NeighborhoodPoint(TEXT("BrokerHome"),FVector(900,4140,15))+FVector(0,0,90),FRotator(0,90,0),Params);
        ZhouChi->PersonName=TEXT("周驰");ZhouChi->ApplyIdentity(TEXT("ZhouChi"));ZhouChi->Patrol=true;
    }
    if(!AllocationLedger)
    {
        FTransform Transform(NeighborhoodReady?NeighborhoodPoint(TEXT("AllocationLedger"),FVector(-1280,3985,7)):PlaceOnPavement(FVector(-1280,3985,7)));
        AllocationLedger=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        AllocationLedger->Kind=EStationObjectKind::AllocationLedger;AllocationLedger->DisplayName=TEXT("暂定配给原始名单");
        AllocationLedger->FinishSpawning(Transform);Objects.Add(AllocationLedger);
    }
    const FString Variant=Progress->AllocationPolicy<0?TEXT("Draft"):Progress->AllocationPolicy==0?TEXT("Public"):TEXT("Rules");
    const FString Name=TEXT("M_Allocation")+Variant;
    if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/BZ/Gameplay/Materials/%s.%s"),*Name,*Name)))AllocationLedger->Screen->SetMaterial(0,Mat);
}
FString AStationDirector::GetMissionTitle() const
{
    if(IsArcChapter())return GetArcTitle();
    if(IsCrisisChapter())return TEXT("闸门另一边 · 本班余热");
    if(IsMandateChapter())return TEXT("替你签下的名字 · 下一期申请");
    if(IsAftermathChapter())return TEXT("散场以后 · 三份回访");
    if(IsDistrictChapter())return TEXT("光退到哪里 · 回光埠");
    if(IsDutyChapter())return TEXT("借来的下班");
    if(IsSupplyChapter())return TEXT("签在谁的名字下");
    if(IsConflictChapter())return TEXT("谁的备用 · 退件风波");
    return IsRationChapter()?TEXT("谁先见光 · 首夜配给"):Progress&&Progress->Started?TEXT("谁先见光"):TEXT("旧站复燃");
}
TArray<FString> AStationDirector::GetAllocationConversation() const
{
    if(Progress->AllocationPolicy>=0)
    {
        if(ConversationTarget==LinHe)return Progress->AllocationPolicy==0
          ?TArray<FString>{TEXT("林禾：你把我的名字也贴出去了。好。现在轮到别人问，我凭什么。")}
          :TArray<FString>{TEXT("林禾：规矩贴得很整齐。名单不让看，我还是不知道自己站在哪一边。")};
        if(ConversationTarget==Engineer)return Progress->AllocationPolicy==0
          ?TArray<FString>{TEXT("叶岑：名单是我让人填的。我得让修站的人相信，留下来有用。你说这算不算交易？")}
          :TArray<FString>{TEXT("叶岑：你保住了申请人的体面，也保住了替他们排队的人。别只记前一半。")};
        return Progress->AllocationPolicy==0
          ?TArray<FString>{TEXT("周驰：名字公开了，交易就得换个地方。你别以为它消失了。")}
          :TArray<FString>{TEXT("周驰：放心，明天会有一张更好看的名单。谁先看见，照旧得另谈。")};
    }
    if(ConversationTarget==ZhouChi)return {
      TEXT("周驰：你要的是公平，还是先让自己人亮起来？"),
      TEXT("林照：名单上为什么有林禾？她没递过申请。"),
      TEXT("周驰：你修了站。有人替你填了。每个人都说，先照顾自己人不算贪。"),
      TEXT("林照：谁填的？"),
      TEXT("周驰：别看我。名单是他们写的，价钱也是。"),
      TEXT("周驰：公告架上有原件。想公开，就把你妹妹的名字一起公开。")};
    return {};
}
void AStationDirector::CompleteAllocationConversation(AActor* Person)
{
    if(!Progress->Started)return;
    if(IsRationChapter()){CompleteRationConversation(Person);return;}
    if(Progress->AllocationPolicy<0)
    {
        if(Person==LinHe)Progress->HeardAllocation=true;
        if(Person==ZhouChi)Progress->MetBroker=true;
    }
    else
    {
        if(Person==LinHe)Progress->AllocationReviewMask|=1;
        if(Person==Engineer)Progress->AllocationReviewMask|=2;
        if(Person==ZhouChi)Progress->AllocationReviewMask|=4;
    }
    SaveProgress();UpdateRationActors();
}
FString AStationDirector::GetAllocationDocument() const
{
    if(Progress->AllocationPolicy==0)return TEXT("第一轮配给公示 · 原始名单\n\n社区诊疗室：公共服务。\n林禾：优先列入，担保关系为光站检修。\n临街冷库：以耗材与维修支持换取优先。\n互助厨房：候补，尚无有效担保。\n\n原件批注：先保住能担保的人，否则下一轮谁出钱？\n\n公示附记：关系与申请一并公开。任何人都能质询，也都能看见他人的处境。此公示没有重新分配供光额度。");
    if(Progress->AllocationPolicy==1)return TEXT("第一轮配给公示 · 配额规则\n\n优先级：公共服务、设备连续运行、已登记申请。\n供光额度与耗材成本另行核定。\n\n申请人姓名与担保关系暂不公开。居民可私下提交异议，由原有登记人员复核。\n\n原始名单封存，未销毁；当前规则无法证明名单按此执行。此公示没有重新分配供光额度。");
    return TEXT("第一轮配给 · 暂定原始名单\n\n社区诊疗室：公共服务。\n林禾：优先列入，担保关系为光站检修。\n临街冷库：以耗材与维修支持换取优先。\n互助厨房：候补，尚无有效担保。\n\n原件批注：先保住能担保的人，否则下一轮谁出钱？\n\n本表尚未公示，不构成最终配额。林禾的申请没有本人签字。决定公示方式前，先听她和周驰怎么说。");
}
FString AStationDirector::GetAllocationChoiceBody() const
{
    if(Panel==EStationPanel::AllocationConfirm)
        return ConfirmSelection==0
          ?TEXT("公开完整原件，包括林禾的优先关系、申请人与担保信息。居民可以追问，但申请人的私人处境也会暴露。\n\n此决定会保存，完成后回访三人；不会再次扣除或增加供光额度。")
          :TEXT("只公布配额规则，封存姓名与担保关系。申请人的处境得到保护，替他们排队的人也继续掌握解释权。\n\n原件仍保留。此决定会保存，完成后回访三人；不会再次扣除或增加供光额度。");
    return TEXT("名单已经排好，规则才准备贴出来。\n\n公开原件，让所有人看见优先关系；或只公布规则，暂时保留申请人的私人信息。\n\n两种公示都不等于重新分配。决定之后，还要听当事人怎么说。");
}
void AStationDirector::CommitAllocation()
{
    if(!Progress->Started||!Progress->HeardAllocation||!Progress->MetBroker||!Progress->ReadLedger||Progress->AllocationPolicy>=0)return;
    Progress->AllocationPolicy=ConfirmSelection;Progress->AllocationReviewMask=0;
    UpdateAllocationActors();SaveProgress();ClosePanel();
    Notify(Progress->AllocationPolicy==0?TEXT("原始名单已公开。去听林禾、叶岑和周驰的回应。"):TEXT("配额规则已公开，原件封存。去听林禾、叶岑和周驰的回应。"));
}
FString AStationDirector::GetAllocationStateJson() const
{
    return FString::Printf(TEXT("{\"heard_linhe\":%s,\"met_broker\":%s,\"read_ledger\":%s,\"policy\":%d,\"review_mask\":%d,\"complete\":%s,\"broker\":\"%s\",\"ledger\":\"%s\"}"),
      Progress->HeardAllocation?TEXT("true"):TEXT("false"),Progress->MetBroker?TEXT("true"):TEXT("false"),Progress->ReadLedger?TEXT("true"):TEXT("false"),Progress->AllocationPolicy,Progress->AllocationReviewMask,
      Progress->AllocationPolicy>=0&&Progress->AllocationReviewMask==7?TEXT("true"):TEXT("false"),ZhouChi?*ZhouChi->GetName():TEXT(""),AllocationLedger?*AllocationLedger->GetName():TEXT(""));
}
