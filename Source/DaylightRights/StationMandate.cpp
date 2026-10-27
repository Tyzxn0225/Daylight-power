#include "StationPrototype.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "StationMandateData.inl"

bool AStationDirector::IsMandateChapter() const
{
    return Progress&&Progress->DistrictSettled&&Progress->SeenDistrictFilm&&(Progress->AftermathHeardMask&7)==7;
}
bool AStationDirector::IsMandateMenu() const
{
    return Panel==EStationPanel::MandateStart||Panel==EStationPanel::MandateCompare||Panel==EStationPanel::MandateChoice||Panel==EStationPanel::MandateConfirm;
}
bool AStationDirector::IsMandateDocument(const AStationObject* Object) const
{
    if(!IsMandateChapter()||!Progress->MandateStarted||!Object)return false;
    return Object==FieldDesk||(!Progress->MandateEndingHeard&&(Object==WorkAccount||(UrbanObjects.IsValidIndex(0)&&Object==UrbanObjects[0])));
}
bool AStationDirector::MandateCanSubmit() const
{
    return IsMandateChapter()&&Progress->MandateStarted&&Progress->MandateOwnerHeard&&Progress->MandateOriginalHeld&&Progress->MandateCompared&&(Progress->MandateWitnessMask&3)&&Progress->MandateTermsRead&&Progress->MandateConsentRecorded&&!Progress->MandateCommitted;
}
int32 AStationDirector::GetMandateStage() const
{
    if(!Progress||!Progress->MandateStarted)return 0;
    if(!Progress->MandateOwnerHeard)return 1;
    if(!Progress->MandateCompared)return 2;
    if(!(Progress->MandateWitnessMask&3))return 3;
    if(!Progress->MandateTermsRead)return 4;
    if(!Progress->MandateConsentRecorded)return 5;
    return Progress->MandateCommitted?7:6;
}
FString AStationDirector::GetMandateObjective() const
{
    static const TCHAR* Steps[]={TEXT("到公共院落事务台，查看下一期续供关联通知"),TEXT("找到林禾，先听她本人怎样回应这份申请"),TEXT("到维修铺工时台取出旧底联，找出被扩大的一项"),TEXT("向叶岑或周驰核对形成经过 · 另一人可选追问"),TEXT("到恒昼访客阅览台，读完关联担保与撤回条款"),TEXT("回公共院落听林禾本人意见 · 她会自行从维修铺返回"),TEXT("在院落事务台复核代价，再亲手提交处置"),TEXT("与林禾核对已生效回执，听完这一段")};
    return Progress&&Progress->MandateEndingHeard?TEXT("本章已留档 · 可回看回执或自由探索，旧账与供光不变"):Steps[GetMandateStage()];
}
AActor* AStationDirector::GetMandateGuidance() const
{
    if(!IsMandateChapter()||Progress->MandateEndingHeard)return nullptr;
    switch(GetMandateStage())
    {
    case 0:case 6:return FieldDesk;
    case 1:case 5:case 7:return LinHe;
    case 2:return WorkAccount;
    case 3:return Engineer;
    case 4:return UrbanObjects.IsValidIndex(0)?UrbanObjects[0].Get():nullptr;
    default:return nullptr;
    }
}
FString AStationDirector::GetMandateChoiceLabel(int32 I) const
{
    if(Panel==EStationPanel::MandateStart)return I==0?TEXT("接下核对 · 先问林禾本人"):TEXT("暂缓接取，保留现有进度");
    if(Panel==EStationPanel::MandateCompare)
    {
        const TCHAR* Rows[]={TEXT("地址一栏：仍是同一个居住地址"),TEXT("旧底联日期：仍是原来的登记日期"),TEXT("担保范围：仅本人 → 新增家属关联担保")};
        return I>=0&&I<3?Rows[I]:TEXT("");
    }
    if(Panel==EStationPanel::MandateConfirm)return I==0?TEXT("确认这份代价，并亲手提交"):TEXT("返回，重新核对");
    return I==0?TEXT("撤回整份家庭申请 · 快线名额取消，今后重新轮候"):TEXT("拆分个人申请并交岗位编号 · 新申请转人工复核");
}
FString AStationDirector::GetMandateDecisionPreview() const
{
    if(Panel==EStationPanel::MandateCompare)return TEXT("");
    if(Panel==EStationPanel::MandateStart)return TEXT("这是下一期尚未执行的申请。原有退款、试亮、工时和公共额度均保留。");
    const int32 Choice=Panel==EStationPanel::MandateConfirm?ConfirmSelection:Selection;
    return Choice==0?TEXT("林禾明确拒绝联保，整份新申请退出快线；现有供光不撤销，但之后要重新轮候。"):TEXT("移除林禾的关联栏，只送林照本人申请。匿名岗位编号会触发组内流程复核，尚未结算的新申请暂缓，不倒扣既有工资。");
}
FString AStationDirector::GetMandateBody() const
{
    if(Panel==EStationPanel::MandateStart)return TEXT("M12 / 替你签下的名字\n\n林禾递来一张下一期续供申请。系统按地址带入家属，关联担保栏写着：林禾；授权方式：待户主批量确认。她没有签字。\n\n这张表尚未执行。先听本人，再核对维修铺底联与恒昼访客条款。不要把上章的任何处置改写成她已同意。");
    if(Panel==EStationPanel::MandateCompare)return TEXT("原件并排核对 / 选择被扩大的字段\n\n旧底联：地址相同；原登记日期；签署人林照，授权仅限本人检修及本人工时；林禾无共同授权。\n\n新申请：地址与日期沿用；拟新增林禾为关联担保人，待户主确认后执行。\n\n哪一项不能凭旧签名沿用？W / S 选，E 标出。");
    if(Panel==EStationPanel::MandateChoice)return TEXT("林禾的意见已单独记为：拒绝关联担保。你只能处置自己的新申请。\n\n整单撤回：家庭快线名额取消，今后重新申请。\n拆分送审：林禾退出，保留林照个人申请；附不含个人姓名的值守岗位编号，检修组新申请转人工复核。\n\n两种方式均不扣旧额度、不撤旧供光、不改上章押金。没有当场获赠的新光。");
    if(Panel==EStationPanel::MandateConfirm)return FString(TEXT("提交前最后复核\n\n林禾拒绝联保的意见已独立保留。\n\n"))+(ConfirmSelection==0?TEXT("你选择撤回整份下一期家庭申请。快线名额取消，今后重新轮候。既有合同和已执行处置不变。"):TEXT("你选择拆分申请并附值守岗位编号。林禾移出关联栏；林照个人申请与本组未结算的新申请转人工复核，不提交组员姓名，不倒扣既有工资。"))+TEXT("\n\n确认后还须在事务台完成提交。中途取消不会留下处置结果。");
    if(Panel==EStationPanel::Journal)return TEXT("M12 / 替你签下的名字\n\n不是旧债突然转给妹妹，而是下一期申请正准备把她变成担保人。\n\n在院落接通知 → 亲自问林禾 → 到维修铺取底联并找出授权扩大 → 找叶岑或周驰核对经过 → 到恒昼访客廊读条款 → 回院落听本人意见 → 提交自己的处置 → 兄妹各留回执。\n\n证词可选先后，两位均可追问。调查不会重写前几章。\n\n当前：")+GetMandateObjective();
    if(Progress->MandateCommitted)return Progress->MandateChoice==0?TEXT("下一期申请 / 已受理回执 M12-01\n\n林禾：拒绝关联担保，意见原样保留。\n林照：撤回整份家庭新申请。\n结果：本次快线名额取消，今后重新轮候；没有新增联保，也没有新增光额。\n\n既有供光与旧账不变。兄妹各留一份；收起后找林禾核对，不把受理写成全家和解。"):TEXT("下一期申请 / 已受理回执 M12-02\n\n林禾：拒绝关联担保，已移出关联栏。\n林照：仅以本人提交新申请，附无个人姓名的值守岗位编号。\n结果：本人与本组未结算新申请转人工复核；现有供光、工资与押金不倒扣。\n\n这不是批准供光。形成过程与责任留在原件里，不能写成检修组共同认可。");
    const auto* O=Cast<AStationObject>(ConversationTarget);
    if(UrbanObjects.IsValidIndex(0)&&O==UrbanObjects[0])return TEXT("恒昼访客阅览 / 下一期快线细则增页\n\n按地址自动带入仅用于形成申请草稿。新增家属担保仍需本人单独确认，原地址登记与旧工时签名不能代替。\n\n撤回整单会取消本次快线名额。要求拆分时，若附值守岗位编号，相关新申请进入人工复核；已结算工资及既有供光不追溯扣减。\n\n窗口可承认撤回，并不等于提供另一份便宜的光。读完并按 E 收起，记下这两种代价。");
    if(O==WorkAccount)return TEXT("维修铺 / 已取得原始底联\n\n旧登记只含林照本人，不含林禾的共同授权。系统带入地址时把关联担保一并预填。该条尚未生效。\n\n原件副本与标出的差异已留存。继续询问形成经过，核对撤回条款。原工时结算仍可在旧档案回看。");
    return TEXT("公共院落 / 下一期关联申请\n\n先把本人意见、原件差异、至少一方形成说明及撤回条款核对齐。不是交齐纸张就替她表示同意。\n\n")+GetMandateObjective();
}
FString AStationDirector::GetMandateVoiceForText(const FString& Text) const
{
    for(const auto& Row:Mandate25::Lines)if(Text==Row.Text)return Row.Audio;
    return TEXT("");
}
TArray<FString> AStationDirector::GetMandateConversation(AActor* Person) const
{
    if(!IsMandateChapter()||!Progress->MandateStarted)return {};
    FString Scene;
    if(Person==LinHe)
    {
        if(Progress->MandateCommitted)Scene=Progress->MandateChoice==0?TEXT("ending0"):TEXT("ending1");
        else if(!Progress->MandateOwnerHeard)Scene=TEXT("intro");
        else if(Progress->MandateCompared&&(Progress->MandateWitnessMask&3)&&Progress->MandateTermsRead)Scene=TEXT("owner");
        else return {TEXT("林禾：先去看原件和条款。我到维修铺核对自己的名字，再回院落等你。")};
    }
    else if(Person==Engineer)Scene=TEXT("crew");else if(Person==ZhouChi)Scene=TEXT("broker");else return {};
    TArray<FString> Result;for(const auto& Row:Mandate25::Lines)if(Scene==Row.Scene)Result.Add(Row.Text);return Result;
}
bool AStationDirector::CompleteMandateConversation(AActor* Person)
{
    if(!IsMandateChapter()||!Progress->MandateStarted)return false;
    if(Person==LinHe)
    {
        if(Progress->MandateCommitted)Progress->MandateEndingHeard=true;
        else if(!Progress->MandateOwnerHeard)Progress->MandateOwnerHeard=true;
        else if(Progress->MandateCompared&&(Progress->MandateWitnessMask&3)&&Progress->MandateTermsRead)Progress->MandateConsentRecorded=true;
    }
    else if(Person==Engineer)Progress->MandateWitnessMask|=1;else if(Person==ZhouChi)Progress->MandateWitnessMask|=2;else return false;
    ClosePanel();SaveProgress();Notify(Progress->MandateEndingHeard?TEXT("两份回执已留。没有把今天写成谁终于原谅了谁。"):GetMandateObjective());return true;
}
bool AStationDirector::HandleMandateObject(AStationObject* O)
{
    if(!IsMandateChapter()||!O)return false;
    if(O==FieldDesk)
    {
        if(!Progress->MandateStarted)OpenPanel(EStationPanel::MandateStart);
        else if(Progress->MandateCommitted)OpenPanel(EStationPanel::Document);
        else if(MandateCanSubmit())
        {
            if(!LinHe||FVector::Dist2D(LinHe->GetActorLocation(),FieldDesk->GetActorLocation())>340){Notify(TEXT("等林禾回到院落，或沿人物肖像去接应她。不能远程替她完成提交。"),true);return true;}
            OpenPanel(EStationPanel::MandateChoice);
        }
        else {OpenPanel(EStationPanel::Document);}
        return true;
    }
    if(!Progress->MandateStarted||Progress->MandateEndingHeard)return false;
    if(O==WorkAccount)
    {
        if(!Progress->MandateOwnerHeard){Notify(TEXT("先听林禾本人怎么说，别替她把申请拆好了。"),true);return true;}
        if(Progress->MandateCompared)OpenPanel(EStationPanel::Document);
        else if(Progress->MandateOriginalHeld)OpenPanel(EStationPanel::MandateCompare);
        else BeginPhysicalAction(O,TEXT("Reach"),-50);
        return true;
    }
    if(UrbanObjects.IsValidIndex(0)&&O==UrbanObjects[0]){OpenPanel(EStationPanel::Document);return true;}
    return false;
}
void AStationDirector::CompleteMandateDocument(AStationObject* O)
{
    if(IsMandateChapter()&&Progress->MandateStarted&&!Progress->MandateCommitted&&UrbanObjects.IsValidIndex(0)&&O==UrbanObjects[0])
    {Progress->MandateTermsRead=true;SaveProgress();Notify(TEXT("两种撤回方式与代价已记。原地址登记不能代替本人授权。"));}
}
void AStationDirector::PressMandateChoice()
{
    if(Panel==EStationPanel::MandateStart)
    {
        const bool Accept=Selection==0;ClosePanel();if(!Accept)return;
        Progress->MandateStarted=true;SaveProgress();Notify(GetMandateObjective());return;
    }
    if(Panel==EStationPanel::MandateCompare)
    {
        if(!Progress->MandateOriginalHeld||!Progress->MandateOwnerHeard){Notify(TEXT("先取得底联与本人意见。"),true);return;}
        if(Selection!=2){Notify(Selection==0?TEXT("地址相同只说明同住，不能证明她愿意担保。再看签署范围。"):TEXT("日期沿用不等于授权范围可以扩大。请找新增义务。"),true);return;}
        Progress->MandateCompared=true;ClosePanel();SaveProgress();PlayCue(TEXT("UI_Stored"),FVector::ZeroVector,.3f,false);Notify(TEXT("已标出担保范围扩大。争议尚未受理，没有替任何人签字。"));return;
    }
    if(Panel==EStationPanel::MandateChoice){ConfirmSelection=FMath::Clamp(Selection,0,1);OpenPanel(EStationPanel::MandateConfirm);Selection=1;return;}
    if(Panel!=EStationPanel::MandateConfirm)return;
    if(Selection!=0){OpenPanel(EStationPanel::MandateChoice);Selection=ConfirmSelection;return;}
    if(!MandateCanSubmit()||!FieldDesk||!LinHe||FVector::Dist2D(LinHe->GetActorLocation(),FieldDesk->GetActorLocation())>340){ClosePanel();Notify(TEXT("原件、本人意见或在场条件不齐，未提交。"),true);return;}
    MandatePendingChoice=ConfirmSelection;ClosePanel();Focus=FieldDesk;
    if(!BeginPhysicalAction(FieldDesk,TEXT("Repair"),-51))MandatePendingChoice=-1;
}
void AStationDirector::CompleteMandateAction(int32 Action)
{
    if(!IsMandateChapter()||!Progress->MandateStarted)return;
    if(Action==-50)
    {
        if(!Progress->MandateOwnerHeard)return;Progress->MandateOriginalHeld=true;SaveProgress();Focus=WorkAccount;OpenPanel(EStationPanel::MandateCompare);return;
    }
    if(Action!=-51||!MandateCanSubmit()||MandatePendingChoice<0||MandatePendingChoice>1)return;
    if(!LinHe||!FieldDesk||FVector::Dist2D(LinHe->GetActorLocation(),FieldDesk->GetActorLocation())>340){MandatePendingChoice=-1;Notify(TEXT("林禾离开了提交现场。本次未记结果，请到场后重试。"),true);return;}
    Progress->MandateChoice=MandatePendingChoice;Progress->MandateCommitted=true;MandatePendingChoice=-1;SaveProgress();
    PlayCue(TEXT("UI_Stored"),FVector::ZeroVector,.35f,false);Focus=FieldDesk;OpenPanel(EStationPanel::Document);
}
void AStationDirector::TickMandateCommunity(float Delta)
{
    for(auto* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(Person)
    {
        Person->Patrol=true;Person->GetCharacterMovement()->MaxWalkSpeed=115.f;
        Person->SetCommunityRoutine(Person==Engineer?TEXT("N16_Maintenance"):Person==LinHe?TEXT("N16_Home"):TEXT("N16_Exchange"));
        if(WorkYieldPeople.Contains(Person))continue;
        if(Progress->MandateEndingHeard){Person->ClearCommunityTask();continue;}
        if(Person==LinHe)
        {
            const bool Workshop=Progress->MandateOwnerHeard&&!Progress->MandateCompared;
            Person->SetCommunityTask(Workshop?FVector(-3700,3000,107):FVector(-4660,700,91),Workshop?TEXT("到维修铺核对自己被预填的名字"):TEXT("到院落亲自保留撤回意见，等哥哥核对条款"));
        }
        else Person->ClearCommunityTask();
    }
}
FString AStationDirector::GetMandateStateJson() const
{
    TSharedRef<FJsonObject> S=MakeShared<FJsonObject>();
    S->SetBoolField(TEXT("available"),IsMandateChapter());S->SetNumberField(TEXT("stage"),GetMandateStage());
    S->SetBoolField(TEXT("started"),Progress&&Progress->MandateStarted);S->SetBoolField(TEXT("owner_heard"),Progress&&Progress->MandateOwnerHeard);S->SetBoolField(TEXT("original"),Progress&&Progress->MandateOriginalHeld);S->SetBoolField(TEXT("compared"),Progress&&Progress->MandateCompared);
    S->SetNumberField(TEXT("witnesses"),Progress?Progress->MandateWitnessMask:0);S->SetBoolField(TEXT("terms"),Progress&&Progress->MandateTermsRead);S->SetBoolField(TEXT("consent"),Progress&&Progress->MandateConsentRecorded);S->SetBoolField(TEXT("committed"),Progress&&Progress->MandateCommitted);S->SetBoolField(TEXT("ending"),Progress&&Progress->MandateEndingHeard);S->SetNumberField(TEXT("choice"),Progress?Progress->MandateChoice:-1);
    S->SetStringField(TEXT("objective"),GetMandateObjective());S->SetStringField(TEXT("mission"),GetMissionTitle());S->SetStringField(TEXT("recap"),GetResumeRecap());S->SetNumberField(TEXT("reserve"),Progress?Progress->Reserve:0);S->SetNumberField(TEXT("district_choice"),Progress?Progress->DistrictChoice:-1);
    FString Result;const auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(S,Writer);return Result;
}
void AStationDirector::AutomationMandatePreview(int32 Choice)
{
#if WITH_EDITOR
    if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return;
    AutomationAftermathPreview(FMath::Clamp(Choice,0,1),7);MandatePendingChoice=-1;
    if(PC&&PC->GetPawn()){PC->GetPawn()->SetActorLocation(FVector(-4550,790,99));PC->SetControlRotation(FRotator(-10,-90,0));}
    // Fixture staging is restricted to the explicit, read-only preview. Normal
    // play returns from the previous district through collision-checked paths.
    if(LinHe){LinHe->SetActorLocation(FVector(-4660,700,91));LinHe->ClearCommunityTask();}
    if(Engineer){Engineer->SetActorLocation(FVector(-3680,3000,107));Engineer->ClearCommunityTask();}
    if(ZhouChi){ZhouChi->SetActorLocation(FVector(-4860,1680,107));ZhouChi->ClearCommunityTask();}
    TickMandateCommunity(0);Notify(TEXT("后续章节只读试玩：从院落事务台接取。不会覆盖你的主线存档。"));
#endif
}
bool AStationDirector::AutomationMandateRoundTrip() const
{
#if WITH_EDITOR
    if(!Progress||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return false;
    TArray<uint8> Bytes;if(!UGameplayStatics::SaveGameToMemory(Progress,Bytes))return false;
    const auto* R=Cast<UStationProgress>(UGameplayStatics::LoadGameFromMemory(Bytes));
    return R&&R->MandateStarted==Progress->MandateStarted&&R->MandateOwnerHeard==Progress->MandateOwnerHeard&&R->MandateOriginalHeld==Progress->MandateOriginalHeld&&R->MandateCompared==Progress->MandateCompared&&R->MandateWitnessMask==Progress->MandateWitnessMask&&R->MandateTermsRead==Progress->MandateTermsRead&&R->MandateConsentRecorded==Progress->MandateConsentRecorded&&R->MandateChoice==Progress->MandateChoice&&R->MandateCommitted==Progress->MandateCommitted&&R->MandateEndingHeard==Progress->MandateEndingHeard&&R->Reserve==Progress->Reserve&&R->DistrictChoice==Progress->DistrictChoice;
#else
    return false;
#endif
}
