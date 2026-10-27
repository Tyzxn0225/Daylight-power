#include "StationPrototype.h"
#include "StationCrisisPolicy.h"
#include "StationCrisisData.inl"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/RectLight.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace Crisis26
{
const FVector Locations[]={FVector(-830,-1450,3),FVector(-4700,1840,19),FVector(4240,3400,15)};
const TCHAR* Names[]={TEXT("余热旁路 · 景观隔离与分流阀"),TEXT("院内等候室 · 供热末端表"),TEXT("恒昼夜班等候廊 · 供热末端表")};
const TCHAR* Mode(int Flow){return Flow>=24?TEXT("完整开放"):Flow>=18?TEXT("轮换使用"):TEXT("主厅保底");}
USceneComponent* Detail(AStationObject* O,bool Source)
{
 USceneComponent* Wheel=nullptr;
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_StationDevice.M_StationDevice"));
 auto Add=[&](const TCHAR* Shape,FVector P,FVector Scale,FRotator R,FLinearColor C)
 {
  auto* M=NewObject<UStaticMeshComponent>(O);O->AddInstanceComponent(M);M->SetupAttachment(O->Anchor);
  M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));M->SetRelativeLocation(P);M->SetRelativeRotation(R);M->SetRelativeScale3D(Scale);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->RegisterComponent();
  if(Parent){auto* Mat=UMaterialInstanceDynamic::Create(Parent,O);Mat->SetVectorParameterValue(TEXT("Tint"),C);Mat->SetScalarParameterValue(TEXT("Glow"),0);M->SetMaterial(0,Mat);}return M;
 };
 const FLinearColor Metal(.13,.17,.18),Brass(.43,.25,.07);
 Add(TEXT("Cube"),FVector(0,0,3),FVector(.82,.6,.06),FRotator::ZeroRotator,Metal);
 for(int Side:{-1,1})
 {
  const float X=Side*(Source?48.f:36.f);
  Add(TEXT("Cylinder"),FVector(X,-12,57),FVector(.085,.085,1.14),FRotator::ZeroRotator,Side<0?FLinearColor(.1,.25,.32):Brass);
  for(float Z:{14.f,98.f})Add(TEXT("Cylinder"),FVector(X,-12,Z),FVector(.15,.15,.035),FRotator::ZeroRotator,Metal);
 }
 if(Source)
 {
  // A visible handwheel with a real stem; no paper panel masquerading as a valve.
  Add(TEXT("Cylinder"),FVector(0,27,73),FVector(.045,.045,.24),FRotator(90,0,0),Metal);
  Wheel=NewObject<USceneComponent>(O);O->AddInstanceComponent(Wheel);Wheel->SetupAttachment(O->Anchor);Wheel->SetRelativeLocation(FVector(0,40,73));Wheel->RegisterComponent();
  for(int I=0;I<8;++I)
  {
   const float A=I*PI/4;auto* Rim=Add(TEXT("Sphere"),FVector(FMath::Cos(A)*15,40,73+FMath::Sin(A)*15),FVector(.055),FRotator::ZeroRotator,Brass);Rim->AttachToComponent(Wheel,FAttachmentTransformRules::KeepWorldTransform);
   auto* Spoke=Add(TEXT("Cube"),FVector(FMath::Cos(A)*8,40,73+FMath::Sin(A)*8),FVector(.16,.03,.025),FRotator(-FMath::RadiansToDegrees(A),0,0),Brass);Spoke->AttachToComponent(Wheel,FAttachmentTransformRules::KeepWorldTransform);
  }
 }
 auto* Label=NewObject<UTextRenderComponent>(O);O->AddInstanceComponent(Label);Label->SetupAttachment(O->Anchor);Label->SetRelativeLocation(FVector(0,17,48));Label->SetRelativeRotation(FRotator(0,90,0));Label->SetHorizontalAlignment(EHTA_Center);Label->SetWorldSize(5);Label->SetText(FText::FromString(Source?TEXT("HEAT RECOVERY\n36 kW MAX"):TEXT("HEAT / NOT ELECTRIC\nCHECK LOCAL RETURN")));Label->RegisterComponent();
 return Wheel;
}
}
bool AStationDirector::IsCrisisChapter() const
{
 // Continue from the heard M12 receipt in ordinary play. Merely loading an
 // earlier checkpoint must not unlock the heat-transfer chapter.
 return IsMandateChapter()&&Progress->MandateEndingHeard;
}
bool AStationDirector::IsCrisisMenu() const{return Panel==EStationPanel::CrisisStart||Panel==EStationPanel::CrisisBalance||Panel==EStationPanel::CrisisConfirm;}
bool AStationDirector::IsCrisisDocument(const AStationObject* O) const{return IsCrisisChapter()&&O&&(CrisisObjects.Contains(O)||O->Kind==EStationObjectKind::NodeB);}
int32 AStationDirector::GetCrisisStage() const
{
 if(!Progress||!Progress->CrisisStarted)return 0;
 if(!Progress->CrisisMeasured)return 1;if(!Progress->CrisisBriefed)return 2;if(!Progress->CrisisIsolated)return 3;
 if(Progress->CrisisAllocation<0)return 4;if(!(Progress->CrisisVerifiedMask&1))return 5;if(!(Progress->CrisisVerifiedMask&2))return 6;return 7;
}
FString AStationDirector::GetCrisisObjective() const
{
 static const TCHAR* Steps[]={TEXT("到冷却检修带电台，接听回水异常通知"),TEXT("在连接 B 实测余热旁路容量"),TEXT("找叶岑核对联锁与两端保底负荷"),TEXT("在旁路阀隔离景观加温，不切住宅与通风"),TEXT("调节两处等候室的热量，再亲手锁定阀位"),TEXT("到院内等候室，核验本端实际供热"),TEXT("到恒昼夜班等候廊，核验另一端结果"),TEXT("找林禾谈谈这次分流，各自留下真实感受")};
 return Progress&&Progress->CrisisEndingHeard?TEXT("本班余热已记录 · 院内与恒昼的轮候仍在继续"):Steps[GetCrisisStage()];
}
AActor* AStationDirector::GetCrisisGuidance() const
{
 switch(GetCrisisStage())
 {
 case 0:return FieldRadio;case 1:for(AStationObject* O:Objects)if(O&&O->Kind==EStationObjectKind::NodeB)return O;return nullptr;
 case 2:return Engineer;case 3:case 4:return CrisisObjects.IsValidIndex(0)?CrisisObjects[0].Get():nullptr;
 case 5:return CrisisObjects.IsValidIndex(1)?CrisisObjects[1].Get():nullptr;case 6:return CrisisObjects.IsValidIndex(2)?CrisisObjects[2].Get():nullptr;
 case 7:return Progress->CrisisEndingHeard?nullptr:LinHe;default:return nullptr;
 }
}
void AStationDirector::UpdateCrisisWorld()
{
 const bool Available=IsCrisisChapter();
 if(Available&&CrisisObjects.IsEmpty())
 {
  for(int I=0;I<3;++I)
  {
   FTransform T(Crisis26::Locations[I]);auto* O=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   O->Kind=EStationObjectKind::HeatControl;O->DisplayName=Crisis26::Names[I];O->FinishSpawning(T);auto* Wheel=Crisis26::Detail(O,I==0);if(I==0)CrisisWheel=Wheel;Objects.Add(O);CrisisObjects.Add(O);
   if(I>0)
   {
    auto* L=GetWorld()->SpawnActor<ARectLight>(Crisis26::Locations[I]+FVector(0,20,245),FRotator(-90,0,0));auto* C=CastChecked<URectLightComponent>(L->GetLightComponent());C->SetMobility(EComponentMobility::Movable);C->SetIntensityUnits(ELightUnits::Lumens);C->SetIntensity(0);C->SetSourceWidth(110);C->SetSourceHeight(60);C->SetAttenuationRadius(270);C->SetLightColor(FLinearColor(1,.65,.32));CrisisTaskLights.Add(L);
   }
  }
  CrisisWorldAllocation=-99;
 }
 for(AStationObject* O:CrisisObjects)if(O){O->Active=Available;O->SetActorHiddenInGame(!Available);O->SetActorEnableCollision(Available);}
 const int Allocation=Available?Progress->CrisisAllocation:-1;
 if(CrisisWheel&&Available)
 {
  float Angle=Progress->CrisisIsolated?90.f:0.f;
  if(Allocation>=0)Angle=90+(Allocation-12)*7.5f;
  else if(PendingRepairIndex==-61&&IsPhysicalActionActive())Angle=90*GetPhysicalActionProgress();
  else if(PendingRepairIndex==-62&&IsPhysicalActionActive()&&StationCrisis::Valid(CrisisPending))Angle=90+(CrisisPending-12)*7.5f*GetPhysicalActionProgress();
  CrisisWheel->SetRelativeRotation(FRotator(Angle,0,0));
 }
 if(CrisisWorldAllocation!=Allocation||Allocation<0)
 {
  for(int I=0;I<CrisisObjects.Num();++I)
  {
   const int Flow=I==1?Allocation:I==2&&Allocation>=0?StationCrisis::Other(Allocation):-1;
   CrisisObjects[I]->SetIndicator(Flow<0?FLinearColor(.23,.4,.47):FLinearColor(1,.35f+.015f*Flow,.11),Flow<0?1:3);
   if(I>0&&CrisisTaskLights.IsValidIndex(I-1))CrisisTaskLights[I-1]->GetLightComponent()->SetIntensity(Flow<0?0:120+Flow*10);
  }
  CrisisWorldAllocation=Allocation;
 }
}
FString AStationDirector::GetCrisisBody() const
{
 if(Panel==EStationPanel::CrisisStart)return TEXT("景观支路的回水调节失效，两处等候室准备限热。灯与通风另走支路，亮着灯不代表供暖正常。\n\n先实测备用换热旁路，再核对联锁。本次处理不改写已结算的供光、押金与工资。");
 if(Panel==EStationPanel::CrisisBalance)return TEXT("本班可回收余热 36 kW。两端各需 24 kW 才能完整开放；每端至少保留 12 kW。\n\nW 增加院内、减少恒昼；S 反向。每次调节 2 kW，总量始终守恒。18 / 18 表示轮换使用，并不等于需求全部满足。\n\n这里调的是热功率，不增加右侧公共供光额度。预览不会改变阀位，按 E 复核，退格放弃草稿。");
 if(Panel==EStationPanel::CrisisConfirm)return FString::Printf(TEXT("院内 %d kW · %s；恒昼夜班廊 %d kW · %s。\n合计 36 kW。住宅、通风和旧供光安排保持原状。\n本班临时分流不等于永久资格，下一班仍须复核。\n确认后须完成阀门操作；中途取消不会生效。"),CrisisDraft,Crisis26::Mode(CrisisDraft),StationCrisis::Other(CrisisDraft),Crisis26::Mode(StationCrisis::Other(CrisisDraft)));
 const AStationObject* O=Cast<AStationObject>(ConversationTarget);
 if(O&&O->Kind==EStationObjectKind::NodeB)return TEXT("旁路实测 / 换热隔离\n\n可用热功率上限 36 kW。院内等候室与恒昼夜班等候廊分别需要 24 kW 完整开放；各留 12 kW 可保留主厅，其余座位限用。18 kW 采用轮换使用。\n\n旁路从既有热网回水回收热量，需要原有泵组运行，并非凭空发电。先隔离景观加温，住宅、通风与应急照明不接入本阀。");
 const int I=CrisisObjects.IndexOfByKey(O);
 if(I>0)
 {
  if(Progress->CrisisAllocation<0)return TEXT("供热末端尚未接入本班余热旁路。应急灯仍在独立运行。你可以先查看两处空间，再返回分流阀调节。");
  const int Flow=I==1?Progress->CrisisAllocation:StationCrisis::Other(Progress->CrisisAllocation);
  return FString::Printf(TEXT("%s / 现场读数\n\n本端分流：%d kW / 完整需求 24 kW\n开放档位：%s\n另一端：%d kW；两端合计 36 kW。\n\n%s\n\n读完按 E 收起，记录本端核验。这里只记录本班热量安排，不增减旧供光额度，也不恢复私人住宅资格。"),I==1?TEXT("院内等候室"):TEXT("恒昼夜班等候廊"),Flow,Crisis26::Mode(Flow),StationCrisis::Other(Flow),I==1?TEXT("这里由公共厨房的既有室内空间承担等候。人还会等，给得更多也不是永远够用。"):TEXT("在恒昼等交班的也有保洁、维修和安保人员；不能用建筑所在片区代替判断每个人。"));
 }
 return Progress->CrisisAllocation<0?TEXT("景观加温尚未隔离前，联锁禁止分流。不要把住宅总阀当成景观支路。先核对叶岑的实测说明。"):FString::Printf(TEXT("本班阀位已锁定：院内 %d kW / 恒昼 %d kW。可核验两处末端，本班不能重复领取或叠加热量。"),Progress->CrisisAllocation,StationCrisis::Other(Progress->CrisisAllocation));
}
FString AStationDirector::GetCrisisChoiceLabel(int I) const
{
 if(Panel==EStationPanel::CrisisStart)return I==0?TEXT("接下检修，先实测旁路"):TEXT("暂缓，留在当前社区");
 return I==0?TEXT("确认本班安排，操作阀门"):TEXT("返回调整，暂不生效");
}
void AStationDirector::AdjustCrisis(int Direction)
{
 CrisisDraft=StationCrisis::Clamp(CrisisDraft+Direction*2);PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.08f,false);BeginPanelPresentation();
}
void AStationDirector::PressCrisisChoice()
{
 if(Panel==EStationPanel::CrisisStart)
 {
  const bool Accept=Selection==0;ClosePanel();if(!Accept)return;Progress->CrisisStarted=true;SaveProgress();
  ConversationTarget=Engineer;RemoteConversation=true;ConversationLine=0;OpenPanel(EStationPanel::Conversation);return;
 }
 if(Panel==EStationPanel::CrisisBalance){if(!StationCrisis::Valid(CrisisDraft))return;OpenPanel(EStationPanel::CrisisConfirm);Selection=1;return;}
 if(Panel!=EStationPanel::CrisisConfirm)return;
 if(Selection!=0){OpenPanel(EStationPanel::CrisisBalance);return;}
 if(!Progress->CrisisMeasured||!Progress->CrisisBriefed||!Progress->CrisisIsolated||Progress->CrisisAllocation>=0||!StationCrisis::Valid(CrisisDraft)){ClosePanel();return;}
 CrisisPending=CrisisDraft;ClosePanel();Focus=CrisisObjects[0];if(!BeginPhysicalAction(CrisisObjects[0],TEXT("Repair"),-62))CrisisPending=-1;
}
bool AStationDirector::HandleCrisisObject(AStationObject* O)
{
 if(!IsCrisisChapter()||!O)return false;
 if(O==FieldRadio)
 {
  if(!Progress->CrisisStarted)OpenPanel(EStationPanel::CrisisStart);
  else {ConversationTarget=Engineer;RemoteConversation=true;ConversationLine=0;OpenPanel(EStationPanel::Conversation);}return true;
 }
 const int I=CrisisObjects.IndexOfByKey(O);
 if(O->Kind!=EStationObjectKind::NodeB&&I<0)return false;
 if(!Progress->CrisisStarted){Notify(TEXT("先到检修电台接听回水异常通知。"),true);return true;}
 if(O->Kind==EStationObjectKind::NodeB){if(!Progress->CrisisMeasured)BeginPhysicalAction(O,TEXT("Repair"),-60);else OpenPanel(EStationPanel::Document);return true;}
 if(I>0){OpenPanel(EStationPanel::Document);return true;}
 if(!Progress->CrisisMeasured||!Progress->CrisisBriefed){Notify(TEXT("先实测连接 B，再请叶岑核对联锁。住宅和通风不能误切。"),true);return true;}
 if(!Progress->CrisisIsolated)BeginPhysicalAction(O,TEXT("Reach"),-61);
 else if(Progress->CrisisAllocation<0){CrisisDraft=18;OpenPanel(EStationPanel::CrisisBalance);}
 else OpenPanel(EStationPanel::Document);return true;
}
void AStationDirector::CompleteCrisisAction(int Action)
{
 if(!IsCrisisChapter()||!Progress->CrisisStarted)return;
 if(Action==-60){Progress->CrisisMeasured=true;SaveProgress();OpenPanel(EStationPanel::Document);return;}
 if(Action==-61&&Progress->CrisisMeasured&&Progress->CrisisBriefed){Progress->CrisisIsolated=true;SaveProgress();PlayCue(TEXT("FX_Relay"),TargetPoint(CrisisObjects[0]),.6f);Notify(TEXT("仅景观加温已隔离。住宅与通风保留，可调节 36 kW 余热。"));return;}
 if(Action!=-62||!Progress->CrisisIsolated||Progress->CrisisAllocation>=0||!StationCrisis::Valid(CrisisPending))return;
 Progress->CrisisAllocation=CrisisPending;CrisisPending=-1;SaveProgress();UpdateCrisisWorld();PlayCue(TEXT("FX_Relay"),TargetPoint(CrisisObjects[0]),.6f);Notify(TEXT("阀位已锁定。沿地图到两处末端，核对真实分流。"));
}
void AStationDirector::CompleteCrisisDocument(AStationObject* O)
{
 if(!IsCrisisChapter()||Progress->CrisisAllocation<0)return;const int I=CrisisObjects.IndexOfByKey(O);if(I<1||I>2)return;
 Progress->CrisisVerifiedMask|=1<<(I-1);SaveProgress();Notify(Progress->CrisisVerifiedMask==3?TEXT("两端读数已核验。去听林禾怎样看这次安排。"):TEXT("本端供热已核验，另一端仍需到场查看。"));
}
FString AStationDirector::GetCrisisVoiceForText(const FString& Text) const{for(const auto& L:Crisis26::Lines)if(Text==L.Text)return L.Audio;return TEXT("");}
TArray<FString> AStationDirector::GetCrisisConversation(AActor* Person) const
{
 if(!Progress->CrisisStarted)return Person==Engineer?TArray<FString>{TEXT("叶岑：检修电台刚响过，先把回水告警听清楚。")}:
  Person==ZhouChi?TArray<FString>{TEXT("周驰：等候廊有人等交班。先听电台，别看到恒昼就只想到业主。")}:
  TArray<FString>{TEXT("林禾：电台刚响过。先听清楚出了什么事，不用先替我们答应。")};
 FString Scene;
 if(Person==Engineer)Scene=Progress->CrisisMeasured?TEXT("brief"):TEXT("intake");
 else if(Person==ZhouChi)Scene=TEXT("broker");
 else if(Person==LinHe&&Progress->CrisisAllocation>=0&&Progress->CrisisVerifiedMask==3)Scene=Progress->CrisisAllocation>18?TEXT("high"):Progress->CrisisAllocation==18?TEXT("equal"):TEXT("low");
 else if(Person==LinHe)return {TEXT("林禾：我在厨房这边照看等候室。去看另一边坐的是谁，再回来告诉我，你把阀门调到了哪里。")};
 TArray<FString> Lines;for(const auto& L:Crisis26::Lines)if(Scene==L.Scene)Lines.Add(L.Text);return Lines;
}
bool AStationDirector::CompleteCrisisConversation(AActor* Person)
{
 if(!IsCrisisChapter())return false;
 if(Progress->CrisisStarted)
 {
  if(Person==Engineer&&Progress->CrisisMeasured)Progress->CrisisBriefed=true;
  if(Person==ZhouChi)Progress->CrisisBrokerHeard=true;
  if(Person==LinHe&&Progress->CrisisAllocation>=0&&Progress->CrisisVerifiedMask==3)Progress->CrisisEndingHeard=true;
 }
 ClosePanel();SaveProgress();Notify(GetCrisisObjective());return true;
}
void AStationDirector::TickCrisisCommunity()
{
 for(auto* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(Person)
 {
  Person->Patrol=true;Person->GetCharacterMovement()->MaxWalkSpeed=115;Person->SetCommunityRoutine(Person==Engineer?TEXT("N16_Maintenance"):Person==LinHe?TEXT("N16_Home"):TEXT("N16_Exchange"));
  if(WorkYieldPeople.Contains(Person))continue;
  if(!Progress->CrisisStarted||Progress->CrisisEndingHeard){Person->ClearCommunityTask();continue;}
  if(Person==Engineer)Person->SetCommunityTask(FVector(-760,-1030,91),TEXT("在换热检修带看守住宅与通风联锁"));
  else if(Person==LinHe)Person->SetCommunityTask(FVector(-4760,2090,107),TEXT("到公共厨房照看本班临时等候室"));
  else Person->SetCommunityTask(FVector(4400,3670,107),TEXT("到恒昼夜班廊核对交班人员与等候位置"));
 }
}
FString AStationDirector::GetCrisisStateJson() const
{
 TSharedRef<FJsonObject> S=MakeShared<FJsonObject>();
 S->SetBoolField(TEXT("available"),IsCrisisChapter());S->SetNumberField(TEXT("stage"),GetCrisisStage());S->SetBoolField(TEXT("started"),Progress&&Progress->CrisisStarted);S->SetBoolField(TEXT("measured"),Progress&&Progress->CrisisMeasured);S->SetBoolField(TEXT("briefed"),Progress&&Progress->CrisisBriefed);S->SetBoolField(TEXT("isolated"),Progress&&Progress->CrisisIsolated);S->SetNumberField(TEXT("allocation"),Progress?Progress->CrisisAllocation:-1);S->SetNumberField(TEXT("other"),Progress&&Progress->CrisisAllocation>=0?StationCrisis::Other(Progress->CrisisAllocation):-1);S->SetNumberField(TEXT("verified"),Progress?Progress->CrisisVerifiedMask:0);S->SetBoolField(TEXT("broker"),Progress&&Progress->CrisisBrokerHeard);S->SetBoolField(TEXT("ending"),Progress&&Progress->CrisisEndingHeard);S->SetNumberField(TEXT("draft"),CrisisDraft);S->SetNumberField(TEXT("reserve"),Progress?Progress->Reserve:0);S->SetNumberField(TEXT("mandate_choice"),Progress?Progress->MandateChoice:-1);S->SetStringField(TEXT("objective"),GetCrisisObjective());S->SetStringField(TEXT("mission"),GetMissionTitle());
 FString Result;auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(S,Writer);return Result;
}
void AStationDirector::AutomationCrisisPreview(int32 Choice)
{
#if WITH_EDITOR
 if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return;
 AutomationMandatePreview(0);Progress->MandateStarted=Progress->MandateOwnerHeard=Progress->MandateOriginalHeld=Progress->MandateCompared=Progress->MandateTermsRead=Progress->MandateConsentRecorded=Progress->MandateCommitted=Progress->MandateEndingHeard=true;Progress->MandateWitnessMask=3;Progress->MandateChoice=FMath::Clamp(Choice,0,1);CrisisPending=-1;CrisisDraft=18;UpdateCrisisWorld();
 if(PC&&PC->GetPawn()){PC->GetPawn()->SetActorLocation(FVector(-730,-1060,100));PC->SetControlRotation(FRotator(-10,-90,0));}
 // Explicit read-only fixtures only. Production movement uses the route graph.
 if(Engineer){Engineer->ClearCommunityTask();Engineer->SetActorLocation(FVector(-760,-1030,91));}
 if(LinHe){LinHe->ClearCommunityTask();LinHe->SetActorLocation(FVector(-4760,2090,107));}
 if(ZhouChi){ZhouChi->ClearCommunityTask();ZhouChi->SetActorLocation(FVector(4400,3670,107));}
#endif
}
bool AStationDirector::AutomationCrisisRoundTrip() const
{
#if WITH_EDITOR
 if(!Progress||!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return false;
 TArray<uint8> Data;if(!UGameplayStatics::SaveGameToMemory(Progress,Data))return false;auto* R=Cast<UStationProgress>(UGameplayStatics::LoadGameFromMemory(Data));
 return R&&R->CrisisStarted==Progress->CrisisStarted&&R->CrisisMeasured==Progress->CrisisMeasured&&R->CrisisBriefed==Progress->CrisisBriefed&&R->CrisisIsolated==Progress->CrisisIsolated&&R->CrisisAllocation==Progress->CrisisAllocation&&R->CrisisVerifiedMask==Progress->CrisisVerifiedMask&&R->CrisisBrokerHeard==Progress->CrisisBrokerHeard&&R->CrisisEndingHeard==Progress->CrisisEndingHeard&&R->Reserve==Progress->Reserve&&R->MandateChoice==Progress->MandateChoice;
#else
 return false;
#endif
}
