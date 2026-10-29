#include "StationPrototype.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AStationDirector::AutomationConflictPickupPreview()
{
#if WITH_EDITOR
    if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return;
    // Start from a clean checkpoint so later chapter/film flags cannot leak
    // in from an unrelated late-game preview. Resources match the fresh route.
    AutomationReset(false);
    Progress->MetEngineer=Progress->ReadNotice=Progress->HasConnector=Progress->HeardRisk=true;
    Progress->Connections=7;Progress->ChosenPlan=0;Progress->Reserve=16;
    Progress->Started=Progress->CutsceneDone=true;
    Progress->HeardAllocation=Progress->MetBroker=Progress->ReadLedger=true;
    Progress->AllocationPolicy=0;Progress->AllocationReviewMask=7;
    Progress->RequestReadMask=7;Progress->RationBriefed=true;
    Progress->RationMask=3;Progress->RationBudget=72;Progress->RationCost=56;
    Progress->RationVisitMask=Progress->RationReviewMask=7;Progress->ReadRecall=true;
    Progress->SeenWorldFilm=Progress->SeenFamilyFilm=Progress->SeenAftermathFilm=true;
    Progress->ConflictStarted=true;Progress->KitchenShortcutOpen=true;
    Progress->ConflictTraceMask=7;Progress->ConflictWitness=3;
    Progress->ConflictEvidence=true;Progress->ConflictHeardMask=3;
    Progress->ConflictOffer=true;Progress->ConflictChoice=0;
    Progress->ConflictPartHeld=false;Progress->ConflictDelivered=false;Progress->ConflictComplete=false;
    ApplyProgress();
    if(PC&&PC->GetPawn())PC->GetPawn()->SetActorLocation(FVector(-3874.916409,2778.078932,111.150003));
#endif
}

void AStationDirector::UpdateConflictActors()
{
    if(Progress->ConflictTraceMask==0&&Progress->ConflictWitness>0)Progress->ConflictTraceMask=(1<<Progress->ConflictWitness)-1;
    auto Spawn=[&](TObjectPtr<AStationObject>& Object,EStationObjectKind Kind,const TCHAR* Name,FVector Position)
    {
        if(Object)return;
        FTransform T(NeighborhoodReady?Position:PlaceOnPavement(Position));
        Object=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Object->Kind=Kind;Object->DisplayName=Name;Object->FinishSpawning(T);Objects.Add(Object);
    };
    if(Progress->Started)
    {
        Spawn(ConsignmentCase,EStationObjectKind::ConsignmentCase,TEXT("封存稳压器 · 耗材封签"),NeighborhoodPoint(TEXT("ConsignmentCase"),FVector(350,3985,0)));
        Spawn(SupplierReturn,EStationObjectKind::SupplierReturn,TEXT("衡昼退件交接柜"),NeighborhoodPoint(TEXT("SupplierReturn"),FVector(1570,3985,0)));
    }
    if(ConflictTraces.Num()!=3&&ExplorationObjects.Num()>=6&&ConsignmentCase)
        ConflictTraces={ExplorationObjects[5],ExplorationObjects[3],ConsignmentCase};
    if(ConflictTraces.Num()==3)
    {
        const bool Investigating=IsConflictChapter()&&Progress->ConflictStarted&&!Progress->ConflictComplete;
        ConflictTraces[0]->DisplayName=Investigating?TEXT("厨房后巷 · 送货门与领料痕迹"):TEXT("厨房后门的维修插销");
        ConflictTraces[1]->DisplayName=Investigating?TEXT("旧放映间 · 卷轴盒里的抵押联"):TEXT("旧日放映机与观影留言");
    }
    if(ConsignmentCase)
    {
        const bool Visible=Progress->Started&&!Progress->ConflictPartHeld&&!Progress->ConflictDelivered;
        ConsignmentCase->Active=Visible&&Progress->ReadRecall;ConsignmentCase->SetActorHiddenInGame(!Visible);ConsignmentCase->SetActorEnableCollision(Visible);
        ConsignmentCase->SetIndicator(FLinearColor(.95,.45,.10),3);
    }
    if(SupplierReturn)
    {
        const bool Visible=Progress->ReadRecall;
        SupplierReturn->Active=Visible;SupplierReturn->SetActorHiddenInGame(!Visible);SupplierReturn->SetActorEnableCollision(Visible);
        SupplierReturn->SetIndicator(Progress->ConflictDelivered&&Progress->ConflictChoice==1?FLinearColor(.25,.7,.6):FLinearColor(.22,.4,.6),3);
    }
    APawn* Pawn=PC?PC->GetPawn():nullptr;
    if(Pawn&&Progress->ConflictPartHeld&&!CarriedRegulator)
    {
        CarriedRegulator=NewObject<UStaticMeshComponent>(Pawn,TEXT("SealedRegulatorPouch"));Pawn->AddInstanceComponent(CarriedRegulator);
        CarriedRegulator->SetupAttachment(Pawn->GetRootComponent());
        CarriedRegulator->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        CarriedRegulator->SetRelativeLocation(FVector(-9,24,-20));CarriedRegulator->SetRelativeScale3D(FVector(.12,.08,.18));
        CarriedRegulator->SetCollisionEnabled(ECollisionEnabled::NoCollision);CarriedRegulator->RegisterComponent();
        if(auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_StationDevice.M_StationDevice")))
        {auto* M=UMaterialInstanceDynamic::Create(Parent,CarriedRegulator);M->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.18,.22,.23));M->SetScalarParameterValue(TEXT("Glow"),0);CarriedRegulator->SetMaterial(0,M);}
    }
    if(CarriedRegulator)CarriedRegulator->SetVisibility(Progress->ConflictPartHeld);
    if(Progress->ConflictDelivered&&Progress->ConflictChoice==0)
        for(auto& O:Objects)if(O->Kind==EStationObjectKind::NodeB)O->SetIndicator(FLinearColor(.28,.75,.57),4);
    if(!Progress->ConflictStarted){WitnessSeconds=0;if(ZhouChi)ZhouChi->ClearCommunityTask();}
}
bool AStationDirector::HandleConflictObject(AStationObject* Object)
{
    if(!Progress->ReadRecall)return false;
    const int32 Trace=ConflictTraces.IndexOfByKey(Object);
    if(IsConflictChapter()&&Progress->ConflictStarted&&Trace>=0&&Progress->ConflictChoice<0)
    {
        if(Trace==0&&!Progress->KitchenShortcutOpen){BeginPhysicalAction(Object,TEXT("Reach"),-13);return true;}
        ActiveConflictTrace=Trace;OpenPanel(EStationPanel::Document);PlayCue(TEXT("FX_Paper"),TargetPoint(Object),.25f);return true;
    }
    if(Object->Kind==EStationObjectKind::Terminal)
    {
        if(!Progress->ConflictStarted)
        {
            // Let the first settlement close into C09 before offering the next mission.
            if(!Progress->SeenAftermathFilm&&StoryAutoplayEnabled())return false;
            OpenPanel(EStationPanel::ConflictStart);Selection=1;return true;
        }
        if(Progress->ConflictDelivered)
        {
            Progress->ConflictComplete=true;SaveProgress();UpdateConflictActors();OpenPanel(EStationPanel::Document);return true;
        }
        if(Progress->ConflictOffer&&Progress->ConflictChoice<0){OpenPanel(EStationPanel::ConflictChoice);return true;}
        OpenPanel(EStationPanel::Document);return true;
    }
    if(Object==ConsignmentCase)
    {
        if(!Progress->ConflictStarted){Notify(TEXT("先到主控制台查看退件通知。"),true);return true;}
        if(Progress->ConflictWitness<3){Notify(TEXT("先随周驰核对厨房、担保账册与交接封签；他会在每处等你。"),true);return true;}
        if(Progress->ConflictChoice<0)
        {
            Progress->ConflictEvidence=true;SaveProgress();OpenPanel(EStationPanel::Document);PlayCue(TEXT("FX_Paper"),TargetPoint(Object),.35f);return true;
        }
        if(!Progress->ConflictPartHeld&&!Progress->ConflictDelivered)BeginPhysicalAction(Object,TEXT("Reach"),-10);
        return true;
    }
    if(Object==SupplierReturn)
    {
        if(Progress->ConflictPartHeld&&Progress->ConflictChoice==1)BeginPhysicalAction(Object,TEXT("Reach"),-12);
        else {OpenPanel(EStationPanel::Document);}return true;
    }
    if(Object->Kind==EStationObjectKind::NodeB&&Progress->ConflictPartHeld&&Progress->ConflictChoice==0)
    {BeginPhysicalAction(Object,TEXT("Repair"),-11);return true;}
    return false;
}
void AStationDirector::PressConflictChoice()
{
    if(Panel==EStationPanel::ConflictStart)
    {
        if(Selection!=0){ClosePanel();return;}
        if(!Progress->ReadRecall)return;
        Progress->ConflictStarted=true;SaveProgress();ClosePanel();Notify(TEXT("自行调查三处痕迹，顺序不限。周驰可以带路；无需等待他到场。"));return;
    }
    if(Panel==EStationPanel::ConflictChoice)
    {
        ConfirmSelection=Selection;OpenPanel(EStationPanel::ConflictConfirm);Selection=1;return;
    }
    if(Panel==EStationPanel::ConflictConfirm)
    {
        if(Selection!=0){OpenPanel(EStationPanel::ConflictChoice);Selection=ConfirmSelection;return;}
        if(!Progress->ConflictOffer||Progress->ConflictChoice>=0)return;
        Progress->ConflictChoice=ConfirmSelection;SaveProgress();ClosePanel();
        Notify(TEXT("决定已记录。到封存箱取出稳压器，再亲手完成交接。"));
    }
}
void AStationDirector::CompleteConflictAction(int32 Action)
{
    if(!IsConflictChapter()||Progress->ConflictComplete)return;
    if(Action==-13)
    {
        Progress->KitchenShortcutOpen=true;SaveProgress();
        if(ConflictTraces.Num())
        {
            Focus=ConflictTraces[0];
            if(!HandleSurveyObject(ConflictTraces[0])){ConversationTarget=ConflictTraces[0];ActiveConflictTrace=0;OpenPanel(EStationPanel::Document);}
        }
        Notify(TEXT("送货后门已打开。厨房与院落之间多了一条真正可走的捷径。"));return;
    }
    if(Action==-10&&Progress->ConflictChoice>=0&&!Progress->ConflictPartHeld&&!Progress->ConflictDelivered)
    {Progress->ConflictPartHeld=true;Notify(TEXT("稳压器已装入腰侧工具袋。可步行或疾跑，取消操作不会丢失物件。"));PlayCue(TEXT("UI_Stored"),FVector::ZeroVector,.3f,false);}
    if(((Action==-11&&Progress->ConflictChoice==0)||(Action==-12&&Progress->ConflictChoice==1))&&Progress->ConflictPartHeld)
    {Progress->ConflictPartHeld=false;Progress->ConflictDelivered=true;Progress->ConflictComplete=true;Notify(Progress->ConflictChoice==0?TEXT("应急稳压已接通，现场回执已记录。下一批外供暂挂，抵押债须共同核账。 "):TEXT("原封退件已签收。抵押解除，社区担保金暂不退；候供排期不等于已经发货。"));PlayCue(TEXT("FX_Relay"),Action==-12&&SupplierReturn?SupplierReturn->GetActorLocation()+FVector(0,0,90):FVector(750,3985,150),.45f);}
    UpdateConflictActors();SaveProgress();
}
TArray<FString> AStationDirector::GetConflictConversation() const
{
    if(Progress->ConflictComplete)
    {
        if(ConversationTarget==LinHe)return {TEXT("林禾：今天的账先记下。回家吃饭，不等于我同意你做的每件事。"),TEXT("林照：饭别留凉了。我回来，跟你一起算。")};
        if(ConversationTarget==Engineer)return {Progress->ConflictChoice==0?TEXT("叶岑：设备留住了，外供也暂挂了。下回缺件，别只写周驰一个人的名字。"):TEXT("叶岑：退件单有回执，担保金没有退。等下一批药的人，得把这段也等过去。")};
        return {Progress->ConflictChoice==0?TEXT("周驰：欠条还在我身上。我会来找你们摊这笔账。"):TEXT("周驰：抵押解了，货还是他们的。我松了一口气，也就这一口。")};
    }
    if(Progress->ConflictWitness<3||!Progress->ConflictEvidence)
    {
        if(ConversationTarget==Engineer)return {TEXT("叶岑：封签还在箱上。先核对实物，再来问我是谁做的担保。")};
        if(ConversationTarget==LinHe)return {TEXT("林禾：你先看封签。我听到的，未必就是全部。")};
    }
    if(Progress->ConflictWitness<3)return {TEXT("周驰：先跟我走完这三处。你看见我收钱，也看看谁把欠条压在我身上。")};
    if(!Progress->ConflictEvidence)return {TEXT("周驰：稳压器在封存箱里。先看封签，再说谁偷了谁的东西。")};
    if(ConversationTarget==Engineer)return {
        TEXT("叶岑：稳压器是社区预订的，钱却是周驰借的。供应商要住户名册，我们没交，他们就收回设备。"),
        TEXT("林照：拿我们每户的地址，换我们付过钱的东西？"),
        TEXT("叶岑：我拿检修组做了担保。里面有我的人。留住设备，断供的风险也得大家一起认。")};
    if(ConversationTarget==LinHe)return {
        TEXT("林禾：他们说，先登记家属，下一轮就有位置。我差点签了。"),
        TEXT("林照：我想让你少等一晚。可不能每次都拿这个，说服我自己。"),
        TEXT("林禾：我想让你回家，也想灯别灭。别替我把这两件事，写成同意交出别人的地址。")};
    if(Progress->ConflictHeardMask!=3)return {TEXT("周驰：先问叶岑是谁担的保。也问问你妹妹，她拿到的登记表要填什么。")};
    return {
        TEXT("周驰：拆封留下，我的抵押就解不开；原封退回，他们答应保留下一批的排期。两条路，我都要抽搬运费。"),
        TEXT("林照：你怕债压着你。我怕回家时，又得跟她说再等一晚。"),
        TEXT("周驰：那就把共同保管写在纸上。别你做了好人，欠条还只留我一个名字。")};
}
void AStationDirector::CompleteConflictConversation(AActor* Person)
{
    if(!IsConflictChapter()||!Progress->ConflictEvidence||Progress->ConflictComplete)return;
    if(Person==Engineer)Progress->ConflictHeardMask|=1;
    if(Person==LinHe)Progress->ConflictHeardMask|=2;
    if(Person==ZhouChi&&Progress->ConflictHeardMask==3)Progress->ConflictOffer=true;
    SaveProgress();
}
FString AStationDirector::GetConflictObjective() const
{
    if(Progress->ConflictComplete)return TEXT("退件回执已保存 · 在交接柜或连接 B 按 E 查看新批供货要约");
    if(Progress->ConflictWitness<3)return FString::Printf(TEXT("实地调查 %d / 3 · 后巷送货门、旧放映间、封存处；顺序不限，无需等待周驰"),Progress->ConflictWitness);
    if(!Progress->ConflictEvidence)return TEXT("E 查看封存稳压器上的退件封签");
    if(!(Progress->ConflictHeardMask&1))return TEXT("询问叶岑：谁付钱，谁担保，谁要名册");
    if(!(Progress->ConflictHeardMask&2))return TEXT("找林禾核对家属登记表");
    if(!Progress->ConflictOffer)return TEXT("带着核验结果与周驰交涉");
    if(Progress->ConflictChoice<0)return TEXT("与周驰现场交涉，选择共同保管或原封退件");
    if(Progress->ConflictDelivered)return TEXT("现场回执已留存，可继续探索或查看下一批供货");
    if(!Progress->ConflictPartHeld)return TEXT("E 从封存箱取出稳压器，装入工具袋");
    return Progress->ConflictChoice==0?TEXT("携稳压器前往连接 B · E 安装应急稳压"):TEXT("携稳压器前往东侧交接柜 · E 原封退件");
}
int32 AStationDirector::GetConflictStage() const
{
    if(Progress->ConflictComplete)return 6;
    if(Progress->ConflictDelivered)return 5;
    if(Progress->ConflictChoice>=0)return 4;
    if(Progress->ConflictOffer)return 3;
    if(Progress->ConflictEvidence)return 2;
    return 1;
}
AActor* AStationDirector::GetConflictGuidance() const
{
    if(Progress->ConflictComplete)
    {
        if(Progress->ConflictChoice==1)return SupplierReturn;
        for(auto& O:Objects)if(O->Kind==EStationObjectKind::NodeB)return O;
        return nullptr;
    }
    if(Progress->ConflictWitness<3&&ConflictTraces.Num()==3)
    {
        AActor* Best=nullptr;double Distance=BIG_NUMBER;
        const FVector Here=PC&&PC->GetPawn()?PC->GetPawn()->GetActorLocation():FVector::ZeroVector;
        for(int32 I=0;I<3;++I)if(!(Progress->ConflictTraceMask&(1<<I)))
        {const double D=FVector::DistSquared2D(Here,ConflictTraces[I]->GetActorLocation());if(D<Distance){Distance=D;Best=ConflictTraces[I];}}
        return Best;
    }
    if(!Progress->ConflictEvidence)return ConsignmentCase;
    if(!(Progress->ConflictHeardMask&1))return Engineer;
    if(!(Progress->ConflictHeardMask&2))return LinHe;
    if(!Progress->ConflictOffer)return ZhouChi;
    if(Progress->ConflictChoice<0)return ZhouChi;
    if(Progress->ConflictChoice>=0&&!Progress->ConflictDelivered)
    {
        if(!Progress->ConflictPartHeld)return ConsignmentCase;
        if(Progress->ConflictChoice==1)return SupplierReturn;
        for(auto& O:Objects)if(O->Kind==EStationObjectKind::NodeB)return O;
    }
    for(auto& O:Objects)if(O->Kind==EStationObjectKind::Terminal)return O;
    return nullptr;
}
FString AStationDirector::GetConflictMenuChoice(int32 Index) const
{
    if(Panel==EStationPanel::ConflictStart)return Index==0?TEXT("调查退件 · 自主探索街区"):TEXT("暂缓 · 返回社区");
    if(Panel==EStationPanel::ConflictConfirm)return Index==0?TEXT("确认并承担这项处置"):TEXT("返回 · 重新考虑");
    return Index==0?TEXT("拆封共同保管 · 现场安装，承担停供与抵押债"):TEXT("原封退回 · 保留后续排期，本轮失去备用设备");
}
FString AStationDirector::GetConflictBody() const
{
    if(Panel==EStationPanel::ConflictStart)return TEXT("设备没有坏，衡昼却要求原封退回。周驰说他替社区垫了钱，厨房领料记录里却有被划掉的一行。\n\n厨房后巷留着送货痕迹；旧放映间的片盒里藏着抵押联；封存处仍有供应商的原始封签。\n\n三处可以按自己的路线调查。周驰愿意带路，也可能只想让你看到对他有利的部分。找齐实物，再问每个人。没有追赶时限，不扣除照明额度。");
    if(Panel==EStationPanel::ConflictChoice)return TEXT("留下设备，可以稳定旧回路，却会触发外供暂挂；周驰借来的抵押款仍须共同追认。\n\n原封退回，保住的是供应商承诺的排期，不是免费供货。社区已经支付的担保金暂不退回。\n\n两种处置都不改变首夜配给；选择后还须实际取件、安装或送回，再签收回执。");
    if(Panel==EStationPanel::ConflictConfirm)return ConfirmSelection==0?TEXT("拆封后由社区共同保管。取出稳压器，在连接 B 亲手安装。\n\n衡昼将暂挂下一批外供。周驰的抵押债记为待共同核账，不能靠重复安装抹去；本次不从照明余额扣款。\n\n这项处置完成后不能改成另一条路线。"):TEXT("保持封签，把稳压器交到东侧退件柜。\n\n本轮失去备用稳压器，下一批排期仍受衡昼条件约束；担保金暂不退。名册尚未提交，本任务不会自动同意提交。\n\n这项处置完成后不能改成另一条路线。");
    const auto* O=Cast<AStationObject>(ConversationTarget);
    if(O==ConsignmentCase)return TEXT("耗材封签 / 稳压器 HZ-17\n\n采购用途：社区旧站应急稳压。\n垫付与抵押人：周驰。\n担保小组：叶岑检修组。\n撤回原因：受益住户名册未提交。设备检测：合格。\n\n厨房确已领用同批耗材，说明周驰并非凭空索债；已付款，也没有让社区获得自行保管的权利。");
    if(O==SupplierReturn&&!Progress->ConflictComplete)return TEXT("衡昼退件交接柜\n\n仅接收原封 HZ-17 稳压器。退件后保留下一批候供排期，是否发货仍须协商合同与名册条件。\n\n当前首夜服务不撤回；本柜不发放照明额度，也不自动退还担保金。");
    if(!Progress->ConflictDelivered)return GetConflictObjective()+TEXT("\n\n退件核验尚未签收。设备只能选择一次去向；操作中途退出不提交交接。");
    return Progress->ConflictChoice==0?TEXT("共同保管回执 / HZ-17\n\n稳压器已在连接 B 接通。已批准的首夜服务照常，候补服务未自动获批。\n\n衡昼外供：暂挂。周驰抵押债：列入社区共同核账，金额须凭原单确认。\n受益住户名册：未提交。\n\n林照留住了设备，也把原本压在周驰身上的风险摊到了社区。林禾没有因此自动原谅他；下一批耗材仍待解决。\n\n任务完成：谁的备用。余下债务不因重复阅读而清零。"):TEXT("原封退件回执 / HZ-17\n\n稳压器已入东侧交接柜。社区本轮无新增备用设备，首夜原有配给继续。\n\n衡昼候供排期：保留，发货仍待合同。周驰该件抵押：解除。社区担保金：暂不退。\n受益住户名册：未提交。\n\n林照保住了谈下一批的机会，也让今晚担心故障的人继续等待。周驰松了一口气，社区为这口气付了代价。\n\n任务完成：谁的备用。排期不是光，回执也不是免责书。");
}
bool AStationDirector::CompleteConflictTraceDocument(AStationObject* Object)
{
    const int32 I=ConflictTraces.IndexOfByKey(Object);
    if(I<0||!IsConflictChapter()||!Progress->ConflictStarted||Progress->ConflictChoice>=0)return false;
    Progress->ConflictTraceMask|=1<<I;
    Progress->ConflictWitness=FMath::CountBits(uint32(Progress->ConflictTraceMask&7));
    if(Progress->ConflictTraceMask==7)Progress->ConflictEvidence=true;
    SaveProgress();ClosePanel();ActiveConflictTrace=-1;
    Notify(Progress->ConflictWitness==3?TEXT("三处实物已互相印证。现在去问叶岑：担保栏里为什么有她的人。"):TEXT("证据已放入任务册。其余地点可自由选择；周驰无需到场。"));
    return true;
}
FString AStationDirector::GetConflictTraceDocument(AStationObject* Object) const
{
    const int32 I=ConflictTraces.IndexOfByKey(Object);
    if(I==0)return TEXT("厨房后巷 / 油污下的领料存根\n\n门后有同批耗材的空箱。厨房确实收过货，周驰并非凭空索债。\n\n领用一栏却被改过：一部分写作‘家属留用’，没有领取人的签字。有人需要这批货，也有人利用每个人都缺货这件事。\n\n插销已经复位，后门通向院落。你获得了一条能反复使用的捷径，而不是额外的照明额度。\n\n还缺的证据：谁抵押，谁担保，供应商为什么撤回一台合格设备。");
    if(I==1)return TEXT("旧放映间 / 藏进卷轴盒的抵押联\n\n旧观众留言下压着一张新票。周驰把它折进片盒，避开了公共账册。\n\n抵押人：周驰。担保栏：叶岑检修组。运费旁另写着一行：先给家里留一份，余数再分。\n\n他替社区垫过钱，也替自己留下了退路。两件事可以同时是真的。叶岑为什么让同事一起担保，纸上没有解释。\n\n片盒里的旧字仍在：下午太阳太大，等阴一点再走。这里曾经让人暂时忘记生活，现在替一个怕失去生活的人藏着欠条。");
    return TEXT("封存处 / 稳压器 HZ-17 原始封签\n\n用途：社区旧站应急稳压。设备检测：合格。\n\n撤回原因：受益住户名册未提交。垫付与抵押人：周驰；担保小组：叶岑检修组。\n\n钱已经付了，设备却不能由付款的人决定去留。供应商要的还有每户的地址，以及以后向谁追索。\n\n厨房的领料、片盒里的抵押联和这张封签应当放在一起看。留下设备会让谁欠债，原封退回又让谁继续等：接下来要问当事人。");
}
FString AStationDirector::GetConflictStateJson() const
{
    return FString::Printf(TEXT("{\"started\":%s,\"witness\":%d,\"witness_seconds\":%.2f,\"evidence\":%s,\"heard\":%d,\"offer\":%s,\"choice\":%d,\"held\":%s,\"delivered\":%s,\"complete\":%s,\"stage\":%d,\"reserve\":%d,\"supply_suspended\":%s,\"registry_submitted\":false}"),Progress->ConflictStarted?TEXT("true"):TEXT("false"),Progress->ConflictWitness,WitnessSeconds,Progress->ConflictEvidence?TEXT("true"):TEXT("false"),Progress->ConflictHeardMask,Progress->ConflictOffer?TEXT("true"):TEXT("false"),Progress->ConflictChoice,Progress->ConflictPartHeld?TEXT("true"):TEXT("false"),Progress->ConflictDelivered?TEXT("true"):TEXT("false"),Progress->ConflictComplete?TEXT("true"):TEXT("false"),GetConflictStage(),Progress->Reserve,Progress->ConflictDelivered&&Progress->ConflictChoice==0?TEXT("true"):TEXT("false"));
}
