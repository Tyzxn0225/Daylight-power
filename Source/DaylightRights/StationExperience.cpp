#include "StationPrototype.h"
#include "StationExperiencePolicy.h"
#include "StationExperienceData.inl"
#include "StationFrontend.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"

void AStationDirector::CreateExperienceWorld()
{
    HadResumeAtEntry=Progress->MetEngineer;PaperMaterials.SetNum(6);PaperOwners.SetNum(6);PaperStates.Init(-99,6);
    if(!NeighborhoodReady)return;
    TArray<UStaticMesh*> Meshes;TArray<AStaticMeshActor*> Originals;
    for(const auto& Row:Experience17::Meshes)
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/BZ/ExperienceV17/Geometry/%s.%s"),Row.Id,Row.Id));if(!Mesh)return;
        AStaticMeshActor* Original=nullptr;
        if(!FString(Row.Replace).IsEmpty())
        {
            for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)if(It->ActorHasTag(FName(Row.Replace))){Original=*It;break;}
            if(!Original)return;
        }
        Meshes.Add(Mesh);Originals.Add(Original);
    }
    for(int32 I=0;I<Meshes.Num();++I)
    {
        auto* Actor=Originals[I];if(!Actor)Actor=GetWorld()->SpawnActor<AStaticMeshActor>();
        auto* Component=Actor->GetStaticMeshComponent();Component->SetMobility(EComponentMobility::Movable);Component->SetStaticMesh(Meshes[I]);Component->SetCollisionProfileName(TEXT("BlockAll"));
    }
    auto Spawn=[&](FVector Position,EStationObjectKind Kind,const TCHAR* Name)
    {
        FTransform T(Position);auto* Object=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Object->Kind=Kind;Object->DisplayName=Name;Object->UseExistingGeometry=true;Object->InteractionOffset=FVector(0,0,117);Object->FinishSpawning(T);Objects.Add(Object);return Object;
    };
    for(const auto& Row:Experience17::Records)UrbanObjects.Add(Spawn(Row.Position,EStationObjectKind::UrbanRecord,Row.Name));
    for(const auto& Row:Experience17::Quiet)QuietObjects.Add(Spawn(Row.Position,EStationObjectKind::Reflect,Row.Name));
    ExperienceReady=true;
}
bool AStationDirector::HandleExperienceObject(AStationObject* Object)
{
    const int32 Urban=UrbanObjects.IndexOfByKey(Object);
    if(Urban>=0){ActiveUrban=Urban;OpenPanel(EStationPanel::WorldRecord);return true;}
    const int32 Quiet=QuietObjects.IndexOfByKey(Object);
    if(Quiet>=0){StartQuietMoment(Quiet);return true;}return false;
}
void AStationDirector::CloseUrbanRecord()
{
    if(ActiveUrban>=0&&ActiveUrban<3){Progress->UrbanReadMask|=1<<ActiveUrban;SaveProgress();}
    ClosePanel();ActiveUrban=-1;
}
FString AStationDirector::GetUrbanBody() const{return ActiveUrban>=0&&ActiveUrban<3?Experience17::Records[ActiveUrban].Body:TEXT("");}
FString AStationDirector::GetUrbanThought() const{return ActiveUrban>=0&&ActiveUrban<3?Experience17::Records[ActiveUrban].Thought:TEXT("");}
void AStationDirector::StartQuietMoment(int32 Index)
{
    if(Index<0||Index>=3||!PC||!PC->GetPawn()||IsStoryFilmActive()||Starting||IsMenuOpen()||IsPhysicalActionActive())return;
    if(IsVoicePlaying()||IsNarrationPlaying()){Notify(TEXT("先听完眼前这句话，再留一会儿。"));return;}
    CancelCommunityVignette();QuietIndex=Index;QuietElapsed=0;QuietRemembered=false;QuietPreviousView=PC->GetViewTarget();
    if(!QuietCamera)QuietCamera=GetWorld()->SpawnActor<ACameraActor>();
    const auto& Row=Experience17::Quiet[Index];FVector From=Row.Camera;FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(QuietView),false,PC->GetPawn());Q.AddIgnoredActor(this);
    if(GetWorld()->LineTraceSingleByChannel(Hit,Row.Aim,From,ECC_Visibility,Q))From=Hit.Location+(Row.Aim-Hit.Location).GetSafeNormal()*18.f;
    QuietCamera->SetActorLocationAndRotation(From,(Row.Aim-From).Rotation());QuietCamera->GetCameraComponent()->SetFieldOfView(55);
    PC->SetViewTargetWithBlend(QuietCamera,ReducedMotion?0.f:.8f);
    // No pose is manufactured. The pawn stays standing, colliding and under input.
}
void AStationDirector::EndQuietMoment()
{
    if(QuietIndex<0)return;
    if(PC)PC->SetViewTargetWithBlend(IsValid(QuietPreviousView)?QuietPreviousView.Get():PC->GetPawn(),ReducedMotion?0.f:.4f);
    QuietIndex=-1;QuietElapsed=0;QuietPreviousView=nullptr;
}
FString AStationDirector::GetQuietMomentText() const{return QuietIndex>=0?Experience17::Quiet[QuietIndex].Body:TEXT("");}
void AStationDirector::TickExperience(float Delta)
{
    if(!PC||!PC->GetPawn()||!Progress)return;
    if(QuietIndex>=0)
    {
        if(IsStoryFilmActive()||Starting||IsMenuOpen()||IsPhysicalActionActive()||PC->GetPawn()->GetVelocity().Size2D()>25){EndQuietMoment();return;}
        QuietElapsed+=Delta;
        if(!QuietRemembered&&StationExperience::RememberQuiet(QuietElapsed,false)){QuietRemembered=true;Progress->QuietVisitMask|=1<<QuietIndex;SaveProgress();}
    }
    if(!ResumeRecapShown&&HadResumeAtEntry&&!IsMenuOpen()&&!IsStoryFilmActive()&&!Starting&&QuietIndex<0&&!IsVoicePlaying()&&GetWorld()->GetTimeSeconds()>5)
    {ResumeRecapShown=true;ResumeRecapUntil=GetWorld()->GetTimeSeconds()+9;}
    PaperClock-=Delta;if(!ExperienceReady||PaperClock>0)return;PaperClock=.5f;
    auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/ExperienceV17/Materials/M_N17_Paper.M_N17_Paper"));if(!Parent)return;
    AStationObject* Targets[]={AllocationLedger.Get(),RationPoints.IsValidIndex(0)?RationPoints[0].Get():nullptr,RationPoints.IsValidIndex(1)?RationPoints[1].Get():nullptr,RationPoints.IsValidIndex(2)?RationPoints[2].Get():nullptr,WorkAccount.Get(),DistrictObjects.IsValidIndex(5)?DistrictObjects[5].Get():nullptr};
    int32 States[]={Progress->AllocationPolicy,-1,-1,-1,StationExperience::WorkPoster(Progress->DutySettled,Progress->SupplyChoice),Progress->DistrictSettled?Progress->DistrictChoice:-1};
    for(int32 I=0;I<3;++I)States[I+1]=StationExperience::ServiceState(Progress->RationMask,I,Progress->DutyFaultRaised,Progress->DutyFaultIndex,Progress->DutyCommunityRestored);
    const TCHAR* Keys[]={TEXT("Publication"),TEXT("Clinic"),TEXT("Kitchen"),TEXT("Housing"),TEXT("Work"),TEXT("Return")};
    for(int32 I=0;I<6;++I)
    {
        auto* Object=Targets[I];if(!IsValid(Object)||!Object->Screen)continue;
        if(PaperOwners[I]!=Object||!PaperMaterials[I]){PaperOwners[I]=Object;PaperMaterials[I]=UMaterialInstanceDynamic::Create(Parent,this);Object->Screen->SetMaterial(0,PaperMaterials[I]);PaperStates[I]=-99;}
        if(PaperStates[I]==States[I])continue;
        const FString Key=States[I]<0?TEXT("Pending"):States[I]==3?TEXT("Undervoltage"):FString::Printf(TEXT("%s_%d"),Keys[I],States[I]);
        auto* Texture=LoadObject<UTexture>(nullptr,*FString::Printf(TEXT("/Game/BZ/ExperienceV17/Textures/T_N17_%s.T_N17_%s"),*Key,*Key));
        if(Texture){PaperMaterials[I]->SetTextureParameterValue(TEXT("Paper"),Texture);PaperStates[I]=States[I];}
    }
}
FString AStationDirector::GetResumeRecap() const
{
    if(!HadResumeAtEntry)return TEXT("");
    if(IsArcChapter())return GetArcRecap();
    if(IsCrisisChapter())
    {
        if(Progress->CrisisEndingHeard)return TEXT("本班余热安排与林禾的真实感受都已留档。后续主线仍待总体剧情设计，现在可以继续探索社区。");
        if(!Progress->CrisisStarted)return TEXT("林禾已独立留下本人意见。冷却检修电台出现回水告警，到现场问清热路出了什么问题。");
        if(Progress->CrisisAllocation<0)return TEXT("回水异常仍待处理。先实测、核对联锁并隔离景观加温，分流草稿还没有生效。");
        return FString::Printf(TEXT("本班余热已分流：院内 %d kW / 恒昼夜班廊 %d kW。两端需要到场核验，再听林禾本人的回应；原供光与旧账不变。"),Progress->CrisisAllocation,36-Progress->CrisisAllocation);
    }
    if(IsMandateChapter())
    {
        if(!Progress->MandateStarted)return TEXT("三份回访已经记下。林禾又收到下一期关联申请，到公共院落的事务台查看通知。");
        if(!Progress->MandateOwnerHeard)return TEXT("你接下了下一期关联通知。林禾还没有表达本人意见，先找到她，听她回应这份申请。");
        if(Progress->MandateCommitted)
        {
            const FString Outcome=Progress->MandateChoice==0?TEXT("整份家庭新申请已撤回，本次快线名额取消。"):TEXT("林禾已移出关联栏，林照个人申请与本组新申请转人工复核。");
            return Outcome+(Progress->MandateEndingHeard?TEXT("兄妹分别留了回执，既有供光与旧账不变。"):TEXT("还要找林禾核对回执，听完她对这份结果的回应。"));
        }
        if(Progress->MandateConsentRecorded)return TEXT("林禾拒绝联保的本人意见已记录。你还没有提交处置，回院落事务台复核两种代价，再决定自己的申请。");
        if(Progress->MandateCompared)return TEXT("原件中扩大担保的差异已标出，申请尚未提交。核对经手人的说明和撤回条款，再回院落听林禾本人意见。");
        return TEXT("林禾没有授权户主替她担保。到维修铺取出旧底联，对照新增担保；原件、经手说明与撤回条款仍待核对。");
    }
    if(IsAftermathChapter())return GetAftermathStage()==3?TEXT("三位当事人的回访都已记下。原处置仍然生效，他们各自的余账也没有被写成共同认可。"):TEXT("回光埠的回执已经签发。叶岑、周驰和林禾还在各自的工作与生活中，地图会指向尚未听完的当事人。");
    if(Progress->DistrictSettled)return Progress->DistrictChoice==0?TEXT("你签字保留公共试亮，夜班押金延期。异议仍在回执里，押金没有被写成已退。"):TEXT("你签字先退夜班押金，公共试亮取消。合格的灯仍将复售，两件事都被留下。");
    if(Progress->DutySettled)return Progress->SupplyChoice==1?TEXT("你的抵押工时已按验收记账。没有把林禾的姓名填进担保，也没有替她说已经原谅。"):TEXT("本班报酬已补回公共备用。账上的增加来自已完成的劳动，后面仍有下一笔账。");
    if(Progress->ConflictChoice>=0)return Progress->ConflictChoice==0?TEXT("你选择保留稳压器，交接与担保的代价仍需核对。设备去向以实际交接回执为准。"):TEXT("你选择原封退回稳压器。后续供货仍需新的担保，尚未交接的物件不会自动归账。");
    if(Progress->RationMask>=0)return FString::Printf(TEXT("首夜分配已生效：支出 %d，结余 %d。没有获准的服务仍在等待；已生效和已核验是两件事。"),Progress->RationCost,Progress->Reserve);
    if(Progress->Started)return TEXT("旧站已经启动。林照八年前替家庭签过保证书，林禾没有共同签字；下一次仍需要她自己的决定。");
    return TEXT("林照接下旧站检修，家里的那顿饭还在等。先核对告示、设备与风险，再决定这轮光送到哪里。");
}
FString AStationDirector::GetObjective() const
{
    if(GetSurveyGuidance())return SurveyObjective();
    FString Text=GetBaseObjective();
    if(NeighborhoodReady&&Progress->Started)Text=Text.Replace(TEXT("主控制台"),TEXT("院内公共事务台"));
    if(NeighborhoodReady&&!Progress->Started&&Progress->Connections==7&&!Progress->HeardRisk)return TEXT("在冷却端检修电台联系叶岑，问清启动风险");
    return Text;
}
FString AStationDirector::GetPlaceHint() const
{
    auto* Guide=GetGuidanceTarget();if(!Guide)return TEXT("可以回家、重看旧物，或进入恒昼访客步道");
    if(IsCrisisChapter()&&CrisisObjects.Contains(Cast<AStationObject>(Guide)))return Guide==CrisisObjects[0]?TEXT("冷却检修带 / 余热隔离与分流阀"):Guide==CrisisObjects[1]?TEXT("公共厨房 / 本班室内等候区"):TEXT("恒昼访客廊 / 夜班人员等候区");
    if(Guide==FieldDesk)return TEXT("公共院落 / 联网事务台");if(Guide==FieldRadio)return TEXT("冷却检修带 / 叶岑的电台");
    if(auto* Person=Cast<AStationEngineer>(Guide))return Person->PersonName+TEXT("会沿生活与工作路线移动，靠近后按 E 交谈");
    const FVector P=Guide->GetActorLocation();
    if(P.Y>6000)return TEXT("隔街回光埠 / 沿步道查找现场原件");
    if(P.X>-4200&&P.X<-3400&&P.Y>1300&&P.Y<2300)return TEXT("林家一层 / 从南侧家门进入");
    if(P.X<-5200)return TEXT("西侧深巷 / 诊疗登记与候诊空间");
    if(P.X<-4400&&P.Y>1200&&P.Y<2200)return TEXT("互助厨房 / 灶台与送餐通道");
    if(P.Y<-2500&&P.X<-2400)return TEXT("后巷计量庭 / 上层露台为可选探索");
    if(P.Y>2500&&P.X<-3300)return TEXT("旧灯维修铺 / 双向入口与工作台");
    return TEXT("沿院落、巷道与设备通道前进；M 查看内街图");
}
bool AStationDirector::ShouldShowWorldGuide(AActor* Target) const
{
    return Target&&PC&&PC->GetPawn()&&StationExperience::ShowGuide(GuidanceDetail,SceneGuides,FVector::Dist2D(Target->GetActorLocation(),PC->GetPawn()->GetActorLocation()),CanSeeTarget(Target));
}
FString AStationDirector::GetContextPrompt() const
{
    if(IsArcChapter()&&Focus==GetArcGuidance())return TEXT("E  ")+GetArcObjective();
    if(IsCrisisChapter()&&Focus==FieldRadio)return Progress->CrisisStarted?TEXT("E  与叶岑核对换热联锁"):TEXT("E  接听回水异常通知");
    if(IsCrisisChapter()&&CrisisObjects.Contains(Cast<AStationObject>(Focus)))return Focus==CrisisObjects[0]?TEXT("E  隔离景观 / 调节本班余热"):TEXT("E  核验本端供热读数");
    if(IsMandateChapter()&&Focus==FieldDesk)return Progress->MandateStarted?TEXT("E  核对下一期申请与本人意见"):TEXT("E  接取林禾的关联担保通知");
    if(IsMandateChapter()&&Progress->MandateStarted&&!Progress->MandateEndingHeard&&Focus==WorkAccount)return TEXT("E  抽取旧底联 / 对照担保范围");
    if(auto* Person=Cast<AStationEngineer>(Focus))return TEXT("E  与")+Person->PersonName+TEXT("交谈");
    auto* Object=Cast<AStationObject>(Focus);if(!Object)return TEXT("");
    const int32 SurveyIndex=SurveySiteOf(Object);
    if(SurveyIndex>=0&&SurveyAvailable(SurveyIndex)&&!SurveySolved(SurveyIndex)&&
       (Object->Kind==EStationObjectKind::SurveyCabinet||Progress->SurveyRouteEnabled))return TEXT("E  核验现场 · K 巡光手记");
    if(IsConflictChapter()&&!Progress->ConflictComplete&&ConflictTraces.Contains(Object))return Object==ConflictTraces[0]&&!Progress->KitchenShortcutOpen?TEXT("E  复位送货门插销，调查门后痕迹"):TEXT("E  调查实物证据 · ")+Object->DisplayName;
    if(Object->Kind==EStationObjectKind::Reflect)return TEXT("E  驻足片刻 · ")+Object->DisplayName;
    if(Object->Kind==EStationObjectKind::UrbanRecord)return TEXT("E  阅读条款 · ")+Object->DisplayName;
    if(Object->Kind==EStationObjectKind::NeighborhoodMemory)return TEXT("E  查看旧物 · ")+Object->DisplayName;
    if(Object->Kind==EStationObjectKind::FieldRadio)return TEXT("E  联系叶岑 · 检修电台");
    if(Object->Kind==EStationObjectKind::Connector)return TEXT("E  取出备用接口");
    if(Object->Kind>=EStationObjectKind::NodeA&&Object->Kind<=EStationObjectKind::NodeC)return TEXT("E  检查设备 · ")+Object->DisplayName;
    if(Object==FieldDesk)return TEXT("E  核对公共事务与回访");
    return TEXT("E  查看 · ")+Object->DisplayName;
}
FString AStationDirector::GetDecisionPreview() const
{
    if(IsArcMenu())return GetArcChoicePreview();
    if(IsCrisisMenu())return Panel==EStationPanel::CrisisStart?TEXT("实测与调阀不会自动恢复私人住宅资格，也不会改写前章选择。"):TEXT("两端热量总和固定，最低负荷受联锁保护。草稿只有在阀门操作完成后才生效。");
    if(IsMandateMenu())return GetMandateDecisionPreview();
    const bool Confirm=Panel==EStationPanel::Confirmation||Panel==EStationPanel::AllocationConfirm||Panel==EStationPanel::ConflictConfirm||Panel==EStationPanel::SupplyConfirm||Panel==EStationPanel::DutyConfirm||Panel==EStationPanel::DistrictConfirm;
    const int32 Choice=Confirm?ConfirmSelection:Selection;
    if(Panel==EStationPanel::Confirmation)return Choice==0?TEXT("近街先亮，远街继续等；负荷较稳。启动后的额度以预览和回执为准。"):TEXT("首轮覆盖更广；临时过载会留下风险记录，后续续供能用的额度更少。");
    if(Panel==EStationPanel::Allocation||Panel==EStationPanel::AllocationConfirm)return Choice==0?TEXT("担保关系更容易被追问；名单中的个人也会暴露。公开不等于取得她的同意。"):TEXT("个人姓名保留在封存原件；公开规则不能自动证明这份名单公平。");
    if(Panel==EStationPanel::RationConfirm)return FString::Printf(TEXT("本次支出 %d，结余 %d。未选中的服务继续等待；获准之后仍需到场核验。"),GetRationDraftCost(),Progress->Reserve-GetRationDraftCost());
    if(Panel==EStationPanel::ConflictChoice||Panel==EStationPanel::ConflictConfirm)return Choice==0?TEXT("保留稳压器，承担供货排期的代价。需要亲手安装，选择本身不等于已交接。"):TEXT("按封签退回稳压器，保住后续排期。设备可用也仍可能被退回。");
    if(Panel==EStationPanel::SupplyChoice||Panel==EStationPanel::SupplyConfirm)return Choice==0?TEXT("支出 6 个公共备用额度作担保。后续由完成验收的劳动补回，没有额外获赠。"):TEXT("抵押林照本人的 8 小时劳动。没有提交林禾的姓名，也没有将她一并担保。");
    if(Panel==EStationPanel::DutyChoice||Panel==EStationPanel::DutyConfirm)return Choice==0?TEXT("先恢复社区服务，再补测受影响的验收。辅助模式与正常操作同额记账。"):TEXT("先完成本班验收，再等叶岑到场抢修。原配给不撤销，欠压的等待仍然发生。");
    if(Panel==EStationPanel::DistrictChoice||Panel==EStationPanel::DistrictConfirm)return Choice==0?TEXT("本批 6 个额度用于公共试亮，夜班押金延期。异议留存不能替工人表示同意。"):TEXT("本批 6 个额度先退夜班押金，公共试亮取消。合格灯具进入封存复售流程。");
    return TEXT("");
}
void AStationDirector::RememberDialogue(const FString& Line)
{
    if(StoryManualCapture)return;
    if(!(Panel==EStationPanel::Conversation||Panel==EStationPanel::ArcScene||IsStoryFilmActive()||IsAmbientStoryActive()))return;
    if(!Line.StartsWith(TEXT("林照："))&&!Line.StartsWith(TEXT("林禾："))&&!Line.StartsWith(TEXT("叶岑："))&&!Line.StartsWith(TEXT("周驰："))&&!Line.StartsWith(TEXT("沈砚："))&&!Line.StartsWith(TEXT("罗婶："))&&!Line.StartsWith(TEXT("阿湛：")))return;
    const FString Clean=Line.Replace(TEXT("\r"),TEXT(" ")).Replace(TEXT("\n"),TEXT(" ")).Left(160);
    if(Progress->DialogueHistory.Num()&&Progress->DialogueHistory.Last()==Clean)return;
    Progress->DialogueHistory.Add(Clean);while(Progress->DialogueHistory.Num()>StationExperience::LogCapacity)Progress->DialogueHistory.RemoveAt(0);SaveProgress();
}
void AStationDirector::RequestDialogueLog()
{
    if(IsStoryFilmActive()||Starting||IsPhysicalActionActive())return;
    if(Panel==EStationPanel::DialogueLog){ClosePanel();return;}if(IsMenuOpen())return;
    LogPage=0;OpenPanel(EStationPanel::DialogueLog);
}
FString AStationDirector::GetDialogueLogBody() const
{
    if(Progress->DialogueHistory.IsEmpty())return TEXT("还没有留下对话记录。这里保留实际听到的对白，不提前展示未触发的剧情。");
    FString Text;const int32 Last=Progress->DialogueHistory.Num()-LogPage*4;
    for(int32 I=FMath::Max(0,Last-4);I<Last;++I)Text+=Progress->DialogueHistory[I]+TEXT("\n\n");return Text.TrimEnd();
}
FString AStationDirector::GetExperienceStateJson() const
{
    return FString::Printf(TEXT("{\"ready\":%s,\"urban_seen\":%d,\"quiet_seen\":%d,\"quiet_active\":%d,\"history_count\":%d,\"guide_mode\":%d,\"reduced_motion\":%s,\"voice_volume\":%.2f,\"effects_volume\":%.2f}"),ExperienceReady?TEXT("true"):TEXT("false"),Progress?Progress->UrbanReadMask:0,Progress?Progress->QuietVisitMask:0,QuietIndex,Progress?Progress->DialogueHistory.Num():0,GuidanceDetail,ReducedMotion?TEXT("true"):TEXT("false"),VoiceVolume,EffectsVolume);
}
