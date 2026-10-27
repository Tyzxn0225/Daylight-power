#include "StationPrototype.h"
#include "StationNeighborhoodData.inl"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"

namespace Supply
{
    #include "StationSupplyData.inl"
    FString Line(int32 I){return Lines[I-1];}
}
bool AStationDirector::SupplyMeasurementsComplete() const
{
    const int32 Needed=Progress->ConflictChoice==1?7:3;
    return (Progress->SupplyMeasuredMask&Needed)==Needed;
}
bool AStationDirector::IsSupplyDocument(const AStationObject* O) const
{
    return O&&(O->Kind==EStationObjectKind::Terminal||O==SupplierReturn||O==AllocationLedger||O==WorkAccount||O==SupplyParcel);
}
void AStationDirector::UpdateSupplyActors()
{
    auto Spawn=[&](TObjectPtr<AStationObject>& O,EStationObjectKind Kind,const TCHAR* Name,FVector Position)
    {
        if(O)return;FTransform T(NeighborhoodReady?Position:PlaceOnPavement(Position));
        O=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        O->Kind=Kind;O->DisplayName=Name;O->FinishSpawning(T);Objects.Add(O);
    };
    if(Progress->ConflictComplete)
    {
        Spawn(WorkAccount,EStationObjectKind::WorkAccount,TEXT("检修工时账户 · 原始抵押单"),NeighborhoodPoint(TEXT("WorkAccount"),FVector(-2300,3985,0)));
        Spawn(SupplyParcel,EStationObjectKind::SupplyParcel,TEXT("新批耗材 · 封存交接袋"),NeighborhoodPoint(TEXT("SupplyParcel"),FVector(350,3985,0)));
    }
    if(WorkAccount)
    {
        const bool Show=Progress->ConflictComplete;
        WorkAccount->Active=Show;WorkAccount->SetActorHiddenInGame(!Show);WorkAccount->SetActorEnableCollision(Show);
    }
    if(SupplyParcel)
    {
        const bool Show=IsSupplyChapter()&&Progress->SupplyChoice>=0&&!Progress->SupplyPartHeld&&!Progress->SupplyInstalled;
        SupplyParcel->Active=Show;SupplyParcel->SetActorHiddenInGame(!Show);SupplyParcel->SetActorEnableCollision(Show);
        SupplyParcel->SetIndicator(FLinearColor(.8,.63,.25),3);
    }
    // Reuse the small sealed tool pouch, never a floating heavy crate or a carried rigid body.
    if(PC&&PC->GetPawn()&&Progress->SupplyPartHeld&&!CarriedRegulator)
    {
        APawn* Pawn=PC->GetPawn();CarriedRegulator=NewObject<UStaticMeshComponent>(Pawn,TEXT("SupplyToolPouch"));Pawn->AddInstanceComponent(CarriedRegulator);
        CarriedRegulator->SetupAttachment(Pawn->GetRootComponent());
        CarriedRegulator->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        CarriedRegulator->SetRelativeLocation(FVector(-9,24,-20));CarriedRegulator->SetRelativeScale3D(FVector(.12,.08,.18));
        CarriedRegulator->SetCollisionEnabled(ECollisionEnabled::NoCollision);CarriedRegulator->RegisterComponent();
    }
    if(CarriedRegulator)CarriedRegulator->SetVisibility(Progress->ConflictPartHeld||Progress->SupplyPartHeld);
    if(IsSupplyChapter())
    {
        if(SupplierReturn)SupplierReturn->SetIndicator(Progress->SupplyChoice>=0?FLinearColor(.25,.7,.6):FLinearColor(.8,.55,.18),3);
        for(auto& O:Objects)
        {
            if((O->Kind==EStationObjectKind::NodeA&&(Progress->SupplyMeasuredMask&1))||(O->Kind==EStationObjectKind::NodeC&&(Progress->SupplyMeasuredMask&2)))O->SetIndicator(FLinearColor(.25,.55,.8),3);
            if(O->Kind==EStationObjectKind::NodeB&&Progress->SupplyInstalled)O->SetIndicator(FLinearColor(.3,.8,.6),4);
        }
    }
}
void AStationDirector::TickSupplyCommunity()
{
    if(!IsSupplyChapter())return;
    auto Task=[&](AStationEngineer* Person,FVector Goal,const TCHAR* Purpose)
    {if(Person&&!WorkYieldPeople.Contains(Person))Person->SetCommunityTask(Goal,Purpose);};
    if(Progress->SupplyComplete)
    {
        for(auto* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(Person&&!WorkYieldPeople.Contains(Person))Person->ClearCommunityTask();
        return;
    }
    // Read and wait from the side/rear; never occupy the player's device frontage.
    Task(LinHe,NeighborhoodReady?Neighborhood16::Stand(TEXT("WorkAccount"),FVector(-2180,4210,0))+FVector(-100,0,0):FVector(-2180,4210,0),TEXT("核对哥哥的工时抵押 · 等待本人说明"));
    Task(ZhouChi,NeighborhoodReady?Neighborhood16::Stand(TEXT("SupplierReturn"),FVector(1430,4210,0))+FVector(-100,0,0):FVector(1430,4210,0),TEXT("在交接柜侧核对新批货单与旧债"));
    Task(Engineer,NeighborhoodReady?Neighborhood16::Stand(TEXT("NodeB"),FVector(900,4210,0))+FVector(-100,0,0):FVector(900,4210,0),Progress->SupplyPartHeld?TEXT("等候新耗材接入 · 留出操作位"):TEXT("核对现场测试与耗材接收"));
}
bool AStationDirector::HandleSupplyObject(AStationObject* O)
{
    if(!Progress->ConflictComplete)return false;
    if(!IsSupplyChapter())
    {
        if(O->Kind==EStationObjectKind::Terminal||O==SupplierReturn||O==WorkAccount||O->Kind==EStationObjectKind::NodeB){OpenPanel(EStationPanel::SupplyStart);Selection=1;return true;}
        if(O==WorkAccount){OpenPanel(EStationPanel::Document);return true;}
        return false;
    }
    if(O->Kind==EStationObjectKind::Terminal)
    {
        if(Progress->SupplyInstalled)
        {
            Progress->SupplyComplete=true;SaveProgress();UpdateSupplyActors();OpenPanel(EStationPanel::Document);return true;
        }
        if(Progress->SupplyEvidenceMask==7&&SupplyMeasurementsComplete()&&Progress->SupplyHeardMask==7&&Progress->SupplyChoice<0)OpenPanel(EStationPanel::SupplyChoice);
        else OpenPanel(EStationPanel::Document);
        return true;
    }
    int32 Bit=O==SupplierReturn?1:O==AllocationLedger?2:O==WorkAccount?4:0;
    if(Bit)
    {
        OpenPanel(EStationPanel::Document);PlayCue(TEXT("FX_Paper"),TargetPoint(O),.35f);return true;
    }
    if(O==SupplyParcel)
    {
        if(Progress->SupplyChoice>=0&&!Progress->SupplyPartHeld&&!Progress->SupplyInstalled)BeginPhysicalAction(O,TEXT("Reach"),-23);
        return true;
    }
    if(O->Kind==EStationObjectKind::NodeB&&Progress->SupplyPartHeld&&!Progress->SupplyInstalled)
    {BeginPhysicalAction(O,TEXT("Repair"),-24);return true;}
    const int32 Index=O->Kind==EStationObjectKind::NodeA?0:O->Kind==EStationObjectKind::NodeC?1:O->Kind==EStationObjectKind::NodeB&&Progress->ConflictChoice==1?2:-1;
    if(Index>=0)
    {
        if(Progress->SupplyMeasuredMask&(1<<Index)){Notify(TEXT("本端检测已记录；首夜获准服务保持运行。"));return true;}
        BeginPhysicalAction(O,TEXT("Repair"),-20-Index);return true;
    }
    return false;
}
void AStationDirector::PressSupplyChoice()
{
    if(Panel==EStationPanel::SupplyStart)
    {
        if(Selection!=0){ClosePanel();return;}
        if(!Progress->ConflictComplete)return;
        Progress->SupplyStarted=true;SaveProgress();ClosePanel();UpdateSupplyActors();
        if(StoryAutoplayEnabled()&&!Progress->SeenSupplyFilm)StartStoryFilm(TEXT("C10"));
        else Notify(TEXT("新单在东侧交接柜；旧账与工时原单在西侧。先核对，后签字。"));
        return;
    }
    if(Panel==EStationPanel::SupplyChoice){ConfirmSelection=Selection;OpenPanel(EStationPanel::SupplyConfirm);Selection=1;return;}
    if(Panel!=EStationPanel::SupplyConfirm)return;
    if(Selection!=0){OpenPanel(EStationPanel::SupplyChoice);Selection=ConfirmSelection;return;}
    if(Progress->SupplyChoice>=0||Progress->SupplyEvidenceMask!=7||!SupplyMeasurementsComplete()||Progress->SupplyHeardMask!=7)return;
    if(ConfirmSelection==0&&Progress->Reserve<6){Notify(TEXT("公共备用额度不足 6。不能透支；返回后可选择个人工时担保。"),true);return;}
    Progress->SupplyChoice=ConfirmSelection;
    if(ConfirmSelection==0){Progress->Reserve-=6;Progress->SupplyDeposit=6;}
    else Progress->SupplyLaborHours=8;
    SaveProgress();ClosePanel();UpdateSupplyActors();PlayCue(TEXT("FX_StoryStamp"),FVector::ZeroVector,.24f,false);Notify(TEXT("本批担保已记录，不能重复扣费或换签。去封存交接袋取出耗材。"));
}
void AStationDirector::CompleteSupplyAction(int32 Action)
{
    if(!IsSupplyChapter()||Progress->SupplyComplete)return;
    if(Action>=-22&&Action<=-20)
    {
        Progress->SupplyMeasuredMask|=1<<(-20-Action);
        Notify(Action==-22?TEXT("人工旁路检测完成。退回稳压器的后果由现场检修补上；首夜配给未变。"):TEXT("本端检测完成，数据只含设备编号与读数，不包含住户名册。"));
    }
    if(Action==-23&&Progress->SupplyChoice>=0&&!Progress->SupplyPartHeld&&!Progress->SupplyInstalled)
    {Progress->SupplyPartHeld=true;PlayCue(TEXT("UI_Stored"),FVector::ZeroVector,.3f,false);Notify(TEXT("密封耗材袋已收进工具袋。到连接 B 完成接入。"));}
    if(Action==-24&&Progress->SupplyPartHeld)
    {
        Progress->SupplyPartHeld=false;Progress->SupplyInstalled=true;Progress->SupplyComplete=true;
        PlayCue(TEXT("FX_Relay"),FVector(750,3985,95),.45f);Notify(TEXT("本批耗材已接入，现场回执已保存。可在这里接本班检修，也可先回访当事人。"));
    }
    SaveProgress();UpdateSupplyActors();
}
TArray<FString> AStationDirector::GetSupplyConversation() const
{
    if(Progress->SupplyComplete)
    {
        if(ConversationTarget==LinHe)return {Supply::Line(Progress->SupplyChoice==0?18:19),Supply::Line(20)};
        if(ConversationTarget==Engineer)return {Supply::Line(21)};
        return {Supply::Line(Progress->ConflictChoice==0?22:23)};
    }
    if(ConversationTarget==LinHe)return (Progress->SupplyEvidenceMask&4)?TArray<FString>{Supply::Line(12),Supply::Line(13)}:TArray<FString>{Supply::Line(8)};
    if(ConversationTarget==Engineer)return SupplyMeasurementsComplete()&&Progress->SupplyEvidenceMask==7?TArray<FString>{Supply::Line(9),Supply::Line(10),Supply::Line(11)}:TArray<FString>{Supply::Line(7)};
    if(Progress->SupplyEvidenceMask!=7)return {Supply::Line(6)};
    if(!SupplyMeasurementsComplete()||(Progress->SupplyHeardMask&3)!=3)return {Supply::Line(17)};
    return {Supply::Line(14),Supply::Line(15),Supply::Line(16)};
}
void AStationDirector::CompleteSupplyConversation(AActor* Person)
{
    if(!IsSupplyChapter()||Progress->SupplyComplete)return;
    if(Person==LinHe&&(Progress->SupplyEvidenceMask&4))Progress->SupplyHeardMask|=2;
    if(Person==Engineer&&SupplyMeasurementsComplete()&&Progress->SupplyEvidenceMask==7)Progress->SupplyHeardMask|=1;
    if(Person==ZhouChi&&Progress->SupplyEvidenceMask==7&&SupplyMeasurementsComplete()&&(Progress->SupplyHeardMask&3)==3)Progress->SupplyHeardMask|=4;
    SaveProgress();
}
bool AStationDirector::CompleteSupplyDocument(AStationObject* O)
{
    if(!IsSupplyChapter()||Progress->SupplyComplete)return false;
    const int32 Bit=O==SupplierReturn?1:O==AllocationLedger?2:O==WorkAccount?4:0;
    if(!Bit)return false;
    Progress->SupplyEvidenceMask|=Bit;SaveProgress();ClosePanel();
    Notify(TEXT("原单已核验。可沿途检测尚未完成的设备，或继续查阅其余原单；顺序不限。"));return true;
}
FString AStationDirector::GetSupplyObjective() const
{
    if(Progress->SupplyComplete)return TEXT("本批供货已接入 · 就地在连接 B 按 E 接本班检修，也可先自由回访");
    if(Progress->SupplyEvidenceMask!=7||!SupplyMeasurementsComplete())return FString::Printf(TEXT("沿途核验 · 原单 %d / 3，设备检测 %d / %d；两类工作可交错完成"),FMath::CountBits(uint32(Progress->SupplyEvidenceMask)),FMath::CountBits(uint32(Progress->SupplyMeasuredMask)),Progress->ConflictChoice==1?3:2);
    if(!(Progress->SupplyHeardMask&1))return TEXT("请叶岑核对检测结果与公共备用金的代价");
    if(!(Progress->SupplyHeardMask&2))return TEXT("找林禾谈工时抵押：工牌是自己的，等待属于谁");
    if(!(Progress->SupplyHeardMask&4))return TEXT("与周驰对质：新单运费里藏着哪一笔旧债");
    if(Progress->SupplyChoice<0)return TEXT("与周驰现场核对条款，选择备用金或个人工时担保");
    if(Progress->SupplyInstalled)return TEXT("到主控制台核对供货回执，旧债不会自动清零");
    return Progress->SupplyPartHeld?TEXT("携耗材袋到连接 B · E 接入本批耗材"):TEXT("到封存交接袋按 E 取出新批耗材");
}
int32 AStationDirector::GetSupplyStage() const
{
    if(Progress->SupplyComplete)return 6;
    if(Progress->SupplyInstalled)return 5;
    if(Progress->SupplyChoice>=0)return 4;
    if(Progress->SupplyHeardMask==7)return 3;
    if(SupplyMeasurementsComplete()&&Progress->SupplyEvidenceMask==7)return 2;
    return Progress->SupplyEvidenceMask==7?1:0;
}
AActor* AStationDirector::GetSupplyGuidance() const
{
    auto Node=[&](EStationObjectKind K)->AActor*{for(auto& O:Objects)if(O->Kind==K)return O;return nullptr;};
    if(Progress->SupplyComplete)return Node(EStationObjectKind::NodeB);
    TArray<AActor*> Available;
    if(!(Progress->SupplyEvidenceMask&1))Available.Add(SupplierReturn);
    if(!(Progress->SupplyEvidenceMask&2))Available.Add(AllocationLedger);
    if(!(Progress->SupplyEvidenceMask&4))Available.Add(WorkAccount);
    if(!(Progress->SupplyMeasuredMask&1))Available.Add(Node(EStationObjectKind::NodeA));
    if(!(Progress->SupplyMeasuredMask&2))Available.Add(Node(EStationObjectKind::NodeC));
    if(Progress->ConflictChoice==1&&!(Progress->SupplyMeasuredMask&4))Available.Add(Node(EStationObjectKind::NodeB));
    if((Progress->SupplyEvidenceMask&4)&&!(Progress->SupplyHeardMask&2))Available.Add(LinHe);
    AActor* Best=nullptr;double BestDistance=BIG_NUMBER;
    const FVector Here=PC&&PC->GetPawn()?PC->GetPawn()->GetActorLocation():FVector::ZeroVector;
    for(auto* Candidate:Available)if(IsValid(Candidate))
    {const double Distance=FVector::DistSquared2D(Here,Candidate->GetActorLocation());if(Distance<BestDistance){BestDistance=Distance;Best=Candidate;}}
    if(Best)return Best;
    if(!(Progress->SupplyHeardMask&1))return Engineer;
    if(!(Progress->SupplyHeardMask&2))return LinHe;
    if(!(Progress->SupplyHeardMask&4))return ZhouChi;
    if(Progress->SupplyChoice<0)return ZhouChi;
    if(Progress->SupplyChoice>=0&&!Progress->SupplyInstalled)return Progress->SupplyPartHeld?Node(EStationObjectKind::NodeB):SupplyParcel.Get();
    return Node(EStationObjectKind::Terminal);
}
FString AStationDirector::GetSupplyMenuChoice(int32 I) const
{
    if(Panel==EStationPanel::SupplyStart)return I==0?TEXT("核对新批供货 · 继续追查担保条件"):TEXT("暂缓 · 保留当前进度");
    if(Panel==EStationPanel::SupplyConfirm)return I==0?TEXT("确认签署 · 承担本批代价"):TEXT("返回 · 重新核对");
    return I==0?TEXT("动用公共备用金 6 · 缩小故障缓冲"):TEXT("抵押本人检修工时 8 小时 · 保留公共额度");
}
FString AStationDirector::GetSupplyBody() const
{
    if(Panel==EStationPanel::SupplyStart)return TEXT("衡昼递来一张新单：允许以设备检测与担保替代整份住户名册。\n\n留下稳压器的社区需要解除本批供货暂挂；退回设备的社区虽保留排期，却缺少现场稳压。两种结果都不会被新单抹去。\n\n先核对原单与检测，再听完三人，决定本批风险签在谁的名字下。接受后播放新片段《空白收货栏》。");
    if(Panel==EStationPanel::SupplyChoice)return FString::Printf(TEXT("两端检测已核验，周驰承认新运费含旧材料垫款。该笔旧债并未全清，也未凭新单自动摊给住户。\n\n公共预付：扣除 6 个备用额度，降低以后故障的缓冲。个人担保：保留公共额度，抵押林照本人 8 小时检修收入，不代签林禾或其他住户。\n\n当前备用：%d。首夜获准服务不撤销，候补不自动获批。"),Progress->Reserve);
    if(Panel==EStationPanel::SupplyConfirm)return ConfirmSelection==0?TEXT("签署公共预付，将立即从备用额度扣除 6。本批到账后仍需亲手取件、安装并核对回执。\n\n支出属于公共账，不能宣称已获得所有居民同意；担保金在本章不退。原有欠条也不自动清偿。\n\n确认后不可改签，取消动作不会重复扣费。"):TEXT("签署林照本人的检修工时账户：8 小时收入担保。当前公共额度不扣，林禾与邻居没有被写进担保名单。\n\n这仍会影响回家和劳动的时间。8 小时属于保存的合同负担，本章不会要求玩家原地等候 8 小时，也不会将其自动标成还清。\n\n确认后不可改签；仍需完成实际交接。");
    const auto* O=Cast<AStationObject>(ConversationTarget);
    if(O==SupplierReturn)return (Progress->ConflictChoice==0?TEXT("本批补充供货要约 / 前单：设备留置，外供暂挂\n\n"):TEXT("本批补充供货要约 / 前单：设备已退，候供排期保留\n\n"))+FString(TEXT("本批仅含熔断芯与冷却耗材，不提供新的照明时段。接受设备编号与端点读数，不索取完整住户名册。\n\n担保二选一：社区备用预付 6；或林照本人检修工时收入 8 小时。先前担保金与抵押欠条不随本单全部结清。\n\n本协议只解除本批发货限制，不代表衡昼放弃后续合同条件。"));
    if(O==AllocationLedger)return TEXT("垫款旧账 / 新批运费附页\n\n厨房曾领用周驰垫付的同批耗材；部分垫款被转列进本批运费。它不是一笔纯粹的新运输费用。\n\n周驰既承担过真实支出，也从匮乏中抽成。具体旧债仍待逐单核账；本章不会将其自动清零，也不会擅自计入全体住户个人账户。\n\n原先的配给名单与公示政策保留，未因这张新单改写。");
    if(O==WorkAccount)return TEXT("林照 / 检修工时账户原单\n\n八年前家庭照明额度转作工位押金的凭证仍在，旧决定没有被新合同抵销。\n\n当前要约抵押林照本人后续 8 小时检修收入，担保人只能签自己的账户。林禾未同意抵押她的账户，社区也未授权转交全部住户名册。\n\n签自己的字仍有家庭后果；动用公共备用金也不是没有代价。先与本人交谈，不能以文件代替她的态度。");
    if(Progress->SupplyInstalled)return FString::Printf(TEXT("本批耗材接收回执 / 设备检测代替名册\n\n新批耗材已在连接 B 接入。原首夜配给与候补状态保持不变；本次没有增加照明额度。\n\n担保方式：%s。\n当前公共备用：%d。\n住户名册：未提交。林禾账户：未代签。\n\n前单：%s。原欠条与旧担保金不自动清零；本单只恢复这一批供货。\n\n回访林禾、叶岑和周驰，听他们怎样记这笔账。"),Progress->SupplyChoice==0?TEXT("公共备用预付 6，未退"):TEXT("林照本人 8 小时检修收入，尚未清偿"),Progress->Reserve,Progress->ConflictChoice==0?TEXT("稳压器仍由社区保管，共同核账未结束"):TEXT("稳压器已退回，旧担保金仍未退还"));
    return GetSupplyObjective()+TEXT("\n\n三份原单、现场检测、三名当事人的说明均须实际完成。先前配给和设备去向不会被本任务改写。");
}
FString AStationDirector::GetSupplyStateJson() const
{
    return FString::Printf(TEXT("{\"started\":%s,\"seen_film\":%s,\"evidence\":%d,\"measured\":%d,\"measurements_complete\":%s,\"heard\":%d,\"choice\":%d,\"deposit\":%d,\"labor_hours\":%d,\"held\":%s,\"installed\":%s,\"complete\":%s,\"stage\":%d,\"reserve\":%d,\"prior_disposition\":%d,\"ration_mask\":%d,\"registry_submitted\":false,\"sister_pledged\":false}"),Progress->SupplyStarted?TEXT("true"):TEXT("false"),Progress->SeenSupplyFilm?TEXT("true"):TEXT("false"),Progress->SupplyEvidenceMask,Progress->SupplyMeasuredMask,SupplyMeasurementsComplete()?TEXT("true"):TEXT("false"),Progress->SupplyHeardMask,Progress->SupplyChoice,Progress->SupplyDeposit,Progress->SupplyLaborHours,Progress->SupplyPartHeld?TEXT("true"):TEXT("false"),Progress->SupplyInstalled?TEXT("true"):TEXT("false"),Progress->SupplyComplete?TEXT("true"):TEXT("false"),GetSupplyStage(),Progress->Reserve,Progress->ConflictChoice,Progress->RationMask);
}
