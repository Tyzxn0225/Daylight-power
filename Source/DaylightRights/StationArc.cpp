#include "StationPrototype.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"

namespace Arc27
{
FString S(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,const FString& Default=TEXT("")){FString V;return O&&O->TryGetStringField(Key,V)?V:Default;}
int32 N(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,int32 Default=0){double V;return O&&O->TryGetNumberField(Key,V)?int32(V):Default;}
TSharedPtr<FJsonObject> J(const TSharedPtr<FJsonObject>& O,const TCHAR* Key){const TSharedPtr<FJsonObject>* V=nullptr;return O&&O->TryGetObjectField(Key,V)?*V:nullptr;}
const TArray<TSharedPtr<FJsonValue>>& A(const TSharedPtr<FJsonObject>& O,const TCHAR* Key){static const TArray<TSharedPtr<FJsonValue>> Empty;const TArray<TSharedPtr<FJsonValue>>* V=nullptr;return O&&O->TryGetArrayField(Key,V)?*V:Empty;}
USoundBase* Sound(const FString& Key){return Key.IsEmpty()?nullptr:LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/BZ/Audio/%s.%s"),*Key,*Key));}
FVector Focus(AActor* Subject)
{
 if(auto* Person=Cast<ACharacter>(Subject))if(auto* Mesh=Person->GetMesh())
  if(Mesh->GetBoneIndex(TEXT("head"))!=INDEX_NONE)return Mesh->GetSocketLocation(TEXT("head"))+FVector(0,0,7);
 return Subject->GetActorLocation()+FVector(0,0,125);
}
TArray<FString> ScenePeople(const TSharedPtr<FJsonObject>& Node)
{
 const FString Id=S(Node,TEXT("id"));
 if(Id==TEXT("gate_scene"))return {TEXT("he"),TEXT("ro"),TEXT("zhan")};
 if(Id==TEXT("blockade"))return {TEXT("he"),TEXT("ro"),TEXT("zhan"),TEXT("ye")};
 if(Id==TEXT("window"))return {TEXT("shen")};
 if(Id==TEXT("aftermath"))return {TEXT("he")};return {};
}
}
bool AStationDirector::LoadArcStory()
{
 if(ArcStory)return !ArcNodes.IsEmpty();FString Text;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("BZ/StoryV27/campaign.json"))))return false;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),ArcStory)||!ArcStory)return false;
 for(const auto& Value:Arc27::A(ArcStory,TEXT("nodes")))
 {
  auto Node=Value->AsObject();const FString Id=Arc27::S(Node,TEXT("id"));if(Id.IsEmpty()||ArcNodes.Contains(Id)){ArcNodes.Empty();ArcStory.Reset();return false;}ArcNodes.Add(Id,Node);
  for(const auto& V:Arc27::A(Node,TEXT("lines"))){auto L=V->AsObject();ArcVoices.Add(Arc27::S(L,TEXT("text")),Arc27::S(L,TEXT("asset")));}
 }
 return ArcNodes.Contains(TEXT("queue"));
}
TSharedPtr<FJsonObject> AStationDirector::ArcCurrent() const
{
 if(!Progress)return nullptr;const FString Key=Progress->ArcNode.IsEmpty()?TEXT("queue"):Progress->ArcNode;
 const auto* Found=ArcNodes.Find(Key);return Found?*Found:nullptr;
}
bool AStationDirector::IsArcChapter() const
{
 // Finishing the heat-transfer aftermath opens the continuation in ordinary
 // play. Started and finished saves retain the same prerequisites and facts.
 return Progress&&Progress->CrisisEndingHeard&&ArcStory&&!ArcNodes.IsEmpty();
}
bool AStationDirector::IsArcMenu() const{return Panel==EStationPanel::ArcChoice||Panel==EStationPanel::ArcConfirm;}
int32 AStationDirector::GetArcFact(const FString& Key) const
{
 if(!Progress)return 0;
 if(Key==TEXT("old_refund"))return Progress->DistrictChoice==1;
 if(Key==TEXT("old_public_names"))return Progress->AllocationPolicy==0;
 if(Key==TEXT("old_regulator_kept"))return Progress->ConflictChoice==0;
 if(Key==TEXT("old_labor"))return Progress->SupplyChoice==1;
 if(Key==TEXT("old_family_split"))return Progress->MandateChoice==1;
 if(Key==TEXT("witness_allowed"))return GetArcFact(TEXT("privacy_trade"))==0||GetArcFact(TEXT("zhan_helped"))==1;
 if(Key==TEXT("manager_available"))return GetArcFact(TEXT("offer_position"))!=0;
 if(Key==TEXT("joint_available"))return GetArcFact(TEXT("evidence_roster"))&&GetArcFact(TEXT("evidence_meter"))&&GetArcFact(TEXT("evidence_offer"))&&GetArcFact(TEXT("sanitized"))&&(GetArcFact(TEXT("crew_support"))||GetArcFact(TEXT("zhan_witness")));
 if(const int32* V=Progress->ArcFlags.Find(Key))return *V;
 if(Key==TEXT("ending_route")||Key==TEXT("route"))return -1;
 if(Key==TEXT("offer_position"))return 2;return 0;
}
bool AStationDirector::ArcMeets(const TSharedPtr<FJsonObject>& R) const
{
 if(!R)return true;
 for(const auto& Pair:R->Values)if(GetArcFact(FString(Pair.Key.ToView()))!=int32(Pair.Value->AsNumber()))return false;return true;
}
FString AStationDirector::GetArcTitle() const
{
 if(ArcPendingNode.StartsWith(TEXT("archive:")))if(const auto* Node=ArcNodes.Find(ArcPendingNode.Mid(8)))return Arc27::S(*Node,TEXT("title"))+TEXT(" · 已阅原件");
 return Arc27::S(ArcCurrent(),TEXT("title"),TEXT("白昼产权"));
}
FString AStationDirector::GetArcObjective() const
{
 auto Node=ArcCurrent();if(Arc27::S(Node,TEXT("kind"))==TEXT("scene"))
  if(auto* Person=Cast<AStationEngineer>(GetArcGuidance());Person&&Person!=ResolveArcTarget(Arc27::S(Node,TEXT("target"))))return Person->PersonName+TEXT("正在前往现场 · M 可查看实际位置，到场后再开始");
 return Arc27::S(Node,TEXT("objective"));
}
FString AStationDirector::GetArcVoiceForText(const FString& Text) const{const FString* Found=ArcVoices.Find(Text);return Found?*Found:TEXT("");}
int32 AStationDirector::GetArcChoiceCount() const{return Panel==EStationPanel::ArcConfirm?2:Arc27::A(ArcCurrent(),TEXT("options")).Num();}
bool AStationDirector::IsArcChoiceEnabled(int32 I) const
{
 const auto& Choices=Arc27::A(ArcCurrent(),TEXT("options"));return Choices.IsValidIndex(I)&&ArcMeets(Arc27::J(Choices[I]->AsObject(),TEXT("requires")));
}
FString AStationDirector::GetArcChoiceLabel(int32 I) const
{
 if(Panel==EStationPanel::ArcConfirm)return I==0?TEXT("确认这项行动"):TEXT("返回，先不做决定");
 const auto& Choices=Arc27::A(ArcCurrent(),TEXT("options"));return Choices.IsValidIndex(I)?Arc27::S(Choices[I]->AsObject(),TEXT("label")):TEXT("");
}
FString AStationDirector::GetArcChoicePreview() const
{
 const int32 I=Panel==EStationPanel::ArcConfirm?ArcPendingChoice:Selection;const auto& Options=Arc27::A(ArcCurrent(),TEXT("options"));if(!Options.IsValidIndex(I))return TEXT("");
 FString Text=Arc27::S(Options[I]->AsObject(),TEXT("description"));
 if(!IsArcChoiceEnabled(I))
 {
  const FString Id=Arc27::S(ArcCurrent(),TEXT("id"));
  Text+=Id==TEXT("crew")?TEXT("\n当前不可选：此前没有优先退还检修组的押金。这一班不能再由他们无偿承担。"):GetArcFact(TEXT("offer_position"))==0&&I==0?TEXT("\n当前不可选：你已明确拒绝这份优待。"):TEXT("\n还缺完整记录、匿名副本，或愿意出面的检修组／证人。可以选择本班临时暂留。");
 }return Text;
}
TArray<FString> AStationDirector::GetArcSteps() const
{
 TArray<FString> Result;const auto Chapters=Arc27::J(Arc27::J(ArcStory,TEXT("world")),TEXT("chapters"));const FString Key=FString::FromInt(Arc27::N(ArcCurrent(),TEXT("chapter"),14));
 const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(Chapters&&Chapters->TryGetArrayField(Key,Values))for(const auto& V:*Values)Result.Add(V->AsString());return Result;
}
int32 AStationDirector::GetArcStepCount() const{return GetArcSteps().Num();}
int32 AStationDirector::GetArcStage() const
{
 const FString Id=Progress?Progress->ArcNode:TEXT("");
 if(Id==TEXT("queue")||Id==TEXT("return_meter")||Id==TEXT("blockade")||Id==TEXT("decision")||Id==TEXT("aftermath"))return 0;
 if(Id==TEXT("free")||Id==TEXT("credits"))return FMath::Max(0,GetArcStepCount()-1);
 if(Id.StartsWith(TEXT("ending_")))return 1;
 if(Id==TEXT("gate_scene")||Id==TEXT("shen_offer")||Id==TEXT("broker")||Id==TEXT("manager_terms")||Id==TEXT("joint_terms")||Id==TEXT("emergency_terms"))return 1;
 if(Id==TEXT("workcard")||Id==TEXT("roster_path")||Id==TEXT("backdoor")||Id==TEXT("window")||Id==TEXT("offer_terms")||Id==TEXT("courier_copy")||Id==TEXT("worker_help")||Id==TEXT("worker_bag")||Id==TEXT("worker_witness"))return 2;
 return FMath::Max(0,GetArcStepCount()-1);
}
FString AStationDirector::GetArcRecap() const
{
 if(!Progress||!Progress->ArcStarted)return TEXT("林禾先一步打开了送餐通道。门外还有没被登记承认的人。");
 if(GetArcFact(TEXT("ending_route"))>=0)return GetArcBody();
 FString Text=TEXT("已发生的事仍然有效。\n");
 if(GetArcFact(TEXT("privacy_trade")))Text+=TEXT("你交出过临时等候名单，信息无法靠取回一份副本抹去。\n");
 if(GetArcFact(TEXT("badge_revoked")))Text+=TEXT("你留下了自己的临时工牌。\n");
 if(GetArcFact(TEXT("offer_position"))==0)Text+=TEXT("你已拒绝稳定地址的优待。\n");
 if(GetArcFact(TEXT("extra_hours")))Text+=TEXT("你承诺了四小时本人的返工。\n");
 return Text+GetArcObjective();
}
AActor* AStationDirector::GetArcGuidance() const
{
 if(Progress&&Progress->ArcFinished)return nullptr;auto Node=ArcCurrent();
 const FString Near=Arc27::S(Node,TEXT("near_person"));AActor* Target=ResolveArcTarget(Arc27::S(Node,TEXT("target")));
 if(Arc27::S(Node,TEXT("kind"))==TEXT("scene"))for(const FString& Key:Arc27::ScenePeople(Node))
  if(auto* Person=ResolveArcTarget(Key);Person&&Target&&FVector::Dist2D(Person->GetActorLocation(),Target->GetActorLocation())>450)return Person;
 if(!Near.IsEmpty())if(auto* Person=ResolveArcTarget(Near);Person&&Target&&FVector::Dist2D(Person->GetActorLocation(),Target->GetActorLocation())>220)return Person;return Target;
}
FString AStationDirector::GetArcBody() const
{
 if(ArcPendingNode.StartsWith(TEXT("archive:")))if(const auto* Archived=ArcNodes.Find(ArcPendingNode.Mid(8)))return Arc27::S(*Archived,TEXT("body"));
 auto Node=ArcCurrent();
 if(Panel==EStationPanel::ArcConfirm)return TEXT("这项决定会被记下。\n\n")+GetArcChoicePreview()+TEXT("\n\n你仍可以返回。需要设备操作的结果，要在动作完成后才生效。");
 if(Panel==EStationPanel::ArcChoice)return TEXT("眼前的人有自己的打算。你已经看过的事，会改变能做的选择。");
 if(Progress&&GetArcFact(TEXT("ending_route"))>=0&&(Panel==EStationPanel::ArcCredits||Progress->ArcFinished))
 {
  FString Text;
  switch(GetArcFact(TEXT("ending_route")))
  {
   case 0:Text=TEXT("常昼管理人\n\n稳定地址与管理岗位已启用。原来的家并未因此赎回，林禾选择另住。你拥有新的门卡，也成为准入核验的经手人。");break;
   case 1:Text=TEXT("把账留下\n\n个人优待已经放弃。旁路与地址核验进入限期共同复核，轮候和维护仍然存在。原住宅份额没有自动赎回，兄妹暂用公共空间，各自保留申请和决定。");break;
   default:Text=TEXT("只保住今晚\n\n已测旁路维持本班暂留。临时工牌停用，长期供光资格未恢复；下一班需要新的安排。林禾自己寻找工作与住处，今晚没有被写成所有人都同意的明天。");break;
  }
  if(GetArcFact(TEXT("privacy_trade")))Text+=TEXT("\n\n曾交出的等候名单仍已外流。");
  if(GetArcFact(TEXT("badge_revoked")))Text+=TEXT("\n林照的临时通行权限已停用。");
  if(GetArcFact(TEXT("extra_hours")))Text+=TEXT("\n林照承诺的四小时返工没有抹去。");
  return Text;
 }
 return Arc27::S(Node,TEXT("body"),GetArcObjective());
}
TArray<FString> AStationDirector::GetArcConversation(AActor* Person) const
{
 TArray<FString> Result;
 if(ArcDialogueMatched)
 {
  for(const auto& V:Arc27::A(ArcCurrent(),TEXT("lines"))){auto L=V->AsObject();if(ArcMeets(Arc27::J(L,TEXT("when"))))Result.Add(Arc27::S(L,TEXT("text")));}return Result;
 }
 const FString Key=Cast<AStationEngineer>(Person)?Cast<AStationEngineer>(Person)->PersonName:TEXT("");
 if(Key==TEXT("林禾"))
 {
  if(Progress->ArcFinished)
  {
   // Reuse the performed ending lines so the free-roam voice and intent stay
   // consistent with her independent housing decision.
   if(GetArcFact(TEXT("ending_route"))==0)return {TEXT("林禾：先放着。你回来的时候，把饭吃了，别又放一夜。")};
   if(GetArcFact(TEXT("ending_route"))==1)return {TEXT("林禾：锅边。你先洗，我把饭翻一下。")};
   return {TEXT("林禾：先坐。汤不多了，你别全舀给我。")};
  }
  return {TEXT("林禾：你先去看眼前那件事。我这边有人，忙完过来。")};
 }
 if(Key==TEXT("叶岑"))return {GetArcFact(TEXT("crew_support"))?TEXT("叶岑：人我排了。多答应的活，别给我塞回来。"):TEXT("叶岑：别站风口。我还得看着这班设备。")};
 if(Key==TEXT("周驰"))return {GetArcFact(TEXT("evidence_courier"))?TEXT("周驰：底联拿到了。已经出去的那份，我没说能拿回来。"):TEXT("周驰：我在这儿。要问什么，你直说。")};
 if(Key==TEXT("沈砚"))return {GetArcFact(TEXT("ending_route"))==0?TEXT("沈砚：门卡会认你。下一班的核验表，也会先到你手里。"):TEXT("沈砚：我还在等你那边的实际安排。")};
 if(Key==TEXT("罗婶"))return {GetArcFact(TEXT("ending_route"))==0?TEXT("罗婶：小林，名单那一栏，我明早来找你。"):TEXT("罗婶：米袋靠里面，别堵门。你们有话，等我腾出手。")};
 return {GetArcFact(TEXT("zhan_helped"))?TEXT("阿湛：袋子我拿回来了。别的，你还得自己办。"):TEXT("阿湛：我就在这儿等一会儿。你忙你的。")};
}
bool AStationDirector::HandleArcPerson(AStationEngineer* Person)
{
 if(!IsArcChapter()||!Person)return false;auto Node=ArcCurrent();ArcDialogueMatched=Arc27::S(Node,TEXT("kind"))==TEXT("talk")&&ResolveArcTarget(Arc27::S(Node,TEXT("target")))==Person;
 Person->SetConversationPaused(true,PC->GetPawn());ConversationTarget=Person;ConversationLine=0;RiskConversation=false;
 if(ArcDialogueMatched&&GetArcFact(TEXT("heard:")+Arc27::S(Node,TEXT("id")))&&Arc27::A(Node,TEXT("options")).Num())OpenPanel(EStationPanel::ArcChoice);
 else OpenPanel(EStationPanel::Conversation);return true;
}
bool AStationDirector::HandleArcObject(AStationObject* O)
{
 if(!IsArcChapter()||!O)return false;auto Node=ArcCurrent();AActor* Target=ResolveArcTarget(Arc27::S(Node,TEXT("target")));
 if(O!=Target)
 {
  for(const auto& Pair:ArcNodes)if(Arc27::S(Pair.Value,TEXT("kind"))==TEXT("inspect")&&GetArcFact(TEXT("done:")+Pair.Key)&&ResolveArcTarget(Arc27::S(Pair.Value,TEXT("target")))==O)
  {ArcPendingNode=TEXT("archive:")+Pair.Key;OpenPanel(EStationPanel::Document);return true;}
  if(ArcObjects.Contains(O)){Notify(O==ResolveArcTarget(TEXT("bag"))?TEXT("这是阿湛暂存的工作物品。先问问他的打算。"):GetArcFact(TEXT("priority_live"))?TEXT("读卡器只识别目前有效的优先门卡。"):TEXT("送餐通道保留当前门禁安排；这里没有待完成的设备操作。"));return true;}
  return false;
 }
 const FString Kind=Arc27::S(Node,TEXT("kind"));
 if(Kind==TEXT("talk"))
 {
  if(auto* Person=Cast<AStationEngineer>(ResolveArcTarget(Arc27::S(Node,TEXT("remote")))))
  {
   RemoteConversation=true;ConversationTarget=Person;ArcDialogueMatched=true;ConversationLine=0;OpenPanel(EStationPanel::Conversation);return true;
  }
 }
 if(Kind==TEXT("inspect")){OpenPanel(EStationPanel::Document);return true;}
 if(Kind==TEXT("operate"))
 {
  if(const FString Near=Arc27::S(Node,TEXT("near_person"));!Near.IsEmpty())
   if(auto* Person=ResolveArcTarget(Near);!Person||FVector::Dist2D(Person->GetActorLocation(),O->GetActorLocation())>220){Notify(TEXT("等协作者实际到场，再操作。地图会跟随他的位置。"),true);return true;}
  if(BeginPhysicalAction(O,Arc27::S(Node,TEXT("action"),TEXT("Repair")),-70))ArcPendingNode=Arc27::S(Node,TEXT("id"));return true;
 }
 if(Kind==TEXT("scene"))
 {
  if(GetArcGuidance()!=Target){Notify(GetArcObjective(),true);return true;}
  BeginArcScene();return true;
 }
 if(Kind==TEXT("credits")){OpenPanel(EStationPanel::ArcCredits);ArcSceneElapsed=0;return true;}
 return false;
}
bool AStationDirector::IsArcDocument(const AStationObject* O) const{return IsArcChapter()&&Arc27::S(ArcCurrent(),TEXT("kind"))==TEXT("inspect")&&ResolveArcTarget(Arc27::S(ArcCurrent(),TEXT("target")))==O;}
void AStationDirector::CompleteArcDocument(AStationObject* O){if(IsArcDocument(O))FinishArcNode();}
void AStationDirector::CompleteArcAction(int32 Action)
{
 if(Action!=-70||!IsArcChapter()||ArcPendingNode!=Arc27::S(ArcCurrent(),TEXT("id")))return;ArcPendingNode.Empty();FinishArcNode();
}
bool AStationDirector::CompleteArcConversation(AActor* Person)
{
 if(!IsArcChapter())return false;
 if(!ArcDialogueMatched){ClosePanel();return true;}
 const auto Node=ArcCurrent();Progress->ArcFlags.Add(TEXT("heard:")+Arc27::S(Node,TEXT("id")),1);
 if(Arc27::A(Node,TEXT("options")).Num()){ClosePanel();Focus=ResolveArcTarget(Arc27::S(Node,TEXT("target")));OpenPanel(EStationPanel::ArcChoice);SaveProgress();}
 else FinishArcNode();return true;
}
void AStationDirector::ApplyArcEffects(const TSharedPtr<FJsonObject>& Effects)
{
 if(!Progress||!Effects)return;
 for(const auto& P:Effects->Values)Progress->ArcFlags.Add(FString(P.Key.ToView()),int32(P.Value->AsNumber()));
 if(Effects->HasField(TEXT("record_witness")))Progress->ArcFlags.Add(TEXT("zhan_witness"),GetArcFact(TEXT("witness_allowed")));
 if(Effects->HasField(TEXT("ending_route")))Progress->ArcFlags.Add(TEXT("world_committed"),1);
}
void AStationDirector::AdvanceArc(const FString& Next)
{
 if(!Progress)return;Progress->ArcStarted=true;Progress->ArcNode=Next;ArcDialogueMatched=false;ArcPendingChoice=-1;
 for(int I=0;I<12&&Arc27::S(ArcCurrent(),TEXT("kind"))==TEXT("branch");++I)
 {
  bool Found=false;for(const auto& V:Arc27::A(ArcCurrent(),TEXT("branches"))){auto B=V->AsObject();if(ArcMeets(Arc27::J(B,TEXT("requires")))){Progress->ArcNode=Arc27::S(B,TEXT("next"));Found=true;break;}}if(!Found)break;
 }
 SaveProgress();UpdateArcWorld(0);Notify(GetArcObjective());
 if(Arc27::S(ArcCurrent(),TEXT("kind"))==TEXT("credits")){OpenPanel(EStationPanel::ArcCredits);ArcSceneElapsed=0;}
}
void AStationDirector::FinishArcNode()
{
 auto Node=ArcCurrent();if(!Node)return;const FString Next=Arc27::S(Node,TEXT("next"));ApplyArcEffects(Arc27::J(Node,TEXT("set")));Progress->ArcFlags.Add(TEXT("done:")+Arc27::S(Node,TEXT("id")),1);ClosePanel();AdvanceArc(Next);
}
void AStationDirector::PressArcChoice()
{
 auto Node=ArcCurrent();const auto& Options=Arc27::A(Node,TEXT("options"));
 if(Panel==EStationPanel::ArcConfirm)
 {
  if(Selection!=0){OpenPanel(EStationPanel::ArcChoice);Selection=FMath::Max(0,ArcPendingChoice);return;}
 }
 else
 {
  ArcPendingChoice=Selection;if(!IsArcChoiceEnabled(Selection)){Notify(GetArcChoicePreview(),true);return;}
  if(Options.IsValidIndex(Selection)&&Options[Selection]->AsObject()->GetBoolField(TEXT("confirm"))){OpenPanel(EStationPanel::ArcConfirm);Selection=1;return;}
 }
 if(!Options.IsValidIndex(ArcPendingChoice)||!IsArcChoiceEnabled(ArcPendingChoice))return;
 const auto Chosen=Options[ArcPendingChoice]->AsObject();ApplyArcEffects(Arc27::J(Node,TEXT("set")));ApplyArcEffects(Arc27::J(Chosen,TEXT("set")));Progress->ArcFlags.Add(TEXT("done:")+Arc27::S(Node,TEXT("id")),1);const FString Next=Arc27::S(Chosen,TEXT("next"));ClosePanel();AdvanceArc(Next);
}
void AStationDirector::BeginArcScene()
{
 auto Node=ArcCurrent();ArcSceneLines.Empty();ArcLineStarts.Empty();ArcSceneDuration=.5f;
 for(const auto& V:Arc27::A(Node,TEXT("lines")))
 {
  auto L=V->AsObject();if(!ArcMeets(Arc27::J(L,TEXT("when"))))continue;
  FString Text=Arc27::S(L,TEXT("text"));ArcSceneLines.Add(Text);ArcLineStarts.Add(ArcSceneDuration);
  auto* Sound=Arc27::Sound(Arc27::S(L,TEXT("asset")));ArcSceneDuration+=(Sound?Sound->GetDuration():FMath::Max(2.f,Text.Len()/5.5f))+.35f;
 }
 ArcSceneDuration=FMath::Max(ArcSceneDuration+1.f,float(Arc27::N(Node,TEXT("duration"),8)));ArcSceneElapsed=0;ArcSceneLine=-1;ArcSkipTime=0;ArcSkipHeld=false;
 ArcPreviousView=PC?PC->GetViewTarget():nullptr;OpenPanel(EStationPanel::ArcScene);BeginPresentation(TEXT(""),false);
 for(auto* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(Person&&PC&&FVector::Dist2D(Person->GetActorLocation(),PC->GetPawn()->GetActorLocation())<900){Person->SetConversationPaused(true,PC->GetPawn());ArcPausedPeople.Add(Person);}
 for(AStationEngineer* Person:ArcPeople)if(Person&&PC&&FVector::Dist2D(Person->GetActorLocation(),PC->GetPawn()->GetActorLocation())<900){Person->SetConversationPaused(true,PC->GetPawn());ArcPausedPeople.Add(Person);}
 UpdateArcCamera(TEXT(""));
}
void AStationDirector::FinishArcScene()
{
 for(auto& Person:ArcPausedPeople)if(Person)Person->SetConversationPaused(false,nullptr);ArcPausedPeople.Empty();
 if(PC&&ArcPreviousView)PC->SetViewTargetWithBlend(IsValid(ArcPreviousView)?ArcPreviousView.Get():PC->GetPawn(),ReducedMotion?0.f:.4f);ArcPreviousView=nullptr;
 ArcSkipHeld=false;ArcSkipTime=0;FinishArcNode();
}
void AStationDirector::FinishArcCredits()
{
 if(Panel!=EStationPanel::ArcCredits||ArcSceneElapsed<5.f)return;
 Progress->ArcFinished=true;Progress->ArcStarted=true;Progress->ArcNode=TEXT("free");ClosePanel();SaveProgress();UpdateArcWorld(0);Notify(TEXT("这段故事已留下回执。你可以在社区继续走走。"));
}
void AStationDirector::TickArc(float Delta)
{
 if(!LoadArcStory()||!IsArcChapter())return;
 if(Panel==EStationPanel::ArcCredits){ArcSceneElapsed+=Delta;return;}
 if(Panel!=EStationPanel::ArcScene)return;
 ArcSceneElapsed+=Delta;ArcSkipTime=ArcSkipHeld?ArcSkipTime+Delta:0;
 if(ArcSkipTime>=.9f||ArcSceneElapsed>=ArcSceneDuration){FinishArcScene();return;}
 int Next=-1;for(int I=0;I<ArcLineStarts.Num();++I)if(ArcSceneElapsed>=ArcLineStarts[I])Next=I;
 if(Next!=ArcSceneLine&&ArcSceneLines.IsValidIndex(Next)){ArcSceneLine=Next;BeginPresentation(ArcSceneLines[Next],true);SetArcPerformance(ArcSceneLines[Next],true);UpdateArcCamera(ArcSceneLines[Next]);}
 if(ArcCamera&&IsValid(ArcCameraSubject))
 {
  // A reach or leaning gesture moves the head relative to the capsule. Follow
  // the performed pose instead of framing an empty point above the person.
  const FVector Aim=Arc27::Focus(ArcCameraSubject);FHitResult Hit;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(ArcPoseCamera),false,this);Q.AddIgnoredActor(ArcCameraSubject);
  if(GetWorld()->LineTraceSingleByChannel(Hit,ArcCamera->GetActorLocation(),Aim,ECC_Visibility,Q))
   UpdateArcCamera(ArcSceneLines.IsValidIndex(ArcSceneLine)?ArcSceneLines[ArcSceneLine]:TEXT(""));
  else ArcCamera->SetActorRotation((Aim-ArcCamera->GetActorLocation()).Rotation());
 }
}
void AStationDirector::SetArcPerformance(const FString& Raw,bool Scene)
{
 int32 Colon;const FString SpeakerName=Raw.FindChar(TCHAR(0xff1a),Colon)?Raw.Left(Colon):TEXT("");
 AStationEngineer* Speaker=nullptr;for(auto* P:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(P&&P->PersonName==SpeakerName)Speaker=P;
 for(AStationEngineer* P:ArcPeople)if(P&&P->PersonName==SpeakerName)Speaker=P;
 AActor* FacingActor=Speaker?static_cast<AActor*>(Speaker):PC?PC->GetPawn():nullptr;
 for(AStationEngineer* P:ArcPausedPeople)if(P&&P!=Speaker){P->PlayStationAction(TEXT("Idle"));P->SetConversationPaused(true,FacingActor);}
 if(SpeakerName==TEXT("林照"))PlayPlayerAction(TEXT("Talk"));
 if(Speaker)
 {
  Speaker->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  AActor* Listener=PC?PC->GetPawn():nullptr;
  if(Scene)for(AStationEngineer* P:ArcPausedPeople)if(P&&P!=Speaker&&FVector::Dist2D(P->GetActorLocation(),Speaker->GetActorLocation())<400){Listener=P;break;}
  Speaker->SetConversationPaused(true,Listener);ArcPausedPeople.AddUnique(Speaker);
  if(!Scene&&PC&&PC->GetPawn()&&FVector::Dist2D(Speaker->GetActorLocation(),PC->GetPawn()->GetActorLocation())<700)
  {Speaker->SetConversationPaused(true,PC->GetPawn());ArcPausedPeople.AddUnique(Speaker);}
  FString Gesture=TEXT("Talk");for(const auto& V:Arc27::A(ArcCurrent(),TEXT("lines"))){auto L=V->AsObject();if(Arc27::S(L,TEXT("text"))==Raw)Gesture=Arc27::S(L,TEXT("gesture"),TEXT("Talk"));}
  Speaker->PlayStationAction(Gesture,Gesture==TEXT("Talk"));
 }
}
void AStationDirector::UpdateArcCamera(const FString& Raw)
{
 if(!PC||!PC->GetPawn())return;AActor* Subject=ResolveArcTarget(Arc27::S(ArcCurrent(),TEXT("target")));int32 Colon;
 const FString SpeakerName=Raw.FindChar(TCHAR(0xff1a),Colon)?Raw.Left(Colon):TEXT("");
 if(SpeakerName==TEXT("林照"))Subject=PC->GetPawn();
 for(auto* P:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(P&&P->PersonName==SpeakerName&&FVector::Dist2D(P->GetActorLocation(),PC->GetPawn()->GetActorLocation())<900)Subject=P;
 for(AStationEngineer* P:ArcPeople)if(P&&P->PersonName==SpeakerName&&FVector::Dist2D(P->GetActorLocation(),PC->GetPawn()->GetActorLocation())<900)Subject=P;
 if(!Subject)return;const FVector Aim=Arc27::Focus(Subject);
 FVector Axis=Subject->GetActorForwardVector().GetSafeNormal2D();
 if(!Cast<ACharacter>(Subject))Axis=(PC->GetPawn()->GetActorLocation()-Subject->GetActorLocation()).GetSafeNormal2D();
 if(Axis.IsNearlyZero())Axis=FVector(1,0,0);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ArcSceneCamera),false,this);Q.AddIgnoredActor(Subject);
 FVector From=FVector::ZeroVector;bool Found=false;
 for(float Radius:{240.f,180.f,135.f,105.f})
 {
  for(float Angle:{25.f,-25.f,55.f,-55.f,90.f,-90.f,135.f,-135.f,180.f})
  {
   const FVector Candidate=Aim+Axis.RotateAngleAxis(Angle,FVector::UpVector)*Radius+FVector(0,0,20);
   FHitResult Hit;
   if(GetWorld()->SweepSingleByChannel(Hit,Aim,Candidate,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(10.f),Q))continue;
   From=Candidate;Found=true;break;
  }
  if(Found)break;
 }
 // Never keep a previous person's camera when the preferred side is blocked.
 // The close fallback remains directed at this speaker and is exposed to QA.
 if(!Found)From=Aim+Axis*95+FVector(0,0,20);
 if(!ArcCamera){ArcCamera=GetWorld()->SpawnActor<ACameraActor>();ArcCamera->GetCameraComponent()->SetFieldOfView(49);}
 ArcCameraSubject=Subject;ArcCamera->GetCameraComponent()->SetFieldOfView(Found?49.f:65.f);
 ArcCamera->SetActorLocationAndRotation(From,(Aim-From).Rotation());PC->SetViewTargetWithBlend(ArcCamera,ReducedMotion?0.f:.45f);
}
FString AStationDirector::GetArcCameraStateJson() const
{
 auto J=MakeShared<FJsonObject>();const bool Ready=PC&&PC->PlayerCameraManager&&IsValid(ArcCameraSubject)&&IsValid(ArcCamera);
 J->SetBoolField(TEXT("ready"),Ready);
 if(Ready)
 {
  const AActor* Subject=ArcCameraSubject;const auto* Person=Cast<AStationEngineer>(Subject);
  J->SetStringField(TEXT("subject_name"),Subject==PC->GetPawn()?TEXT("林照"):Person?Person->PersonName:Subject->GetName());
  const FVector Aim=Arc27::Focus(ArcCameraSubject),From=PC->PlayerCameraManager->GetCameraLocation();
  FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ArcCameraReview),false,this);Q.AddIgnoredActor(Subject);
  J->SetBoolField(TEXT("line_of_sight"),!GetWorld()->LineTraceSingleByChannel(Hit,From,Aim,ECC_Visibility,Q));
  FVector2D Screen;int32 W=0,H=0;PC->GetViewportSize(W,H);
  const bool Projected=PC->ProjectWorldLocationToScreen(Aim,Screen);
  J->SetBoolField(TEXT("in_frame"),Projected&&Screen.X>=W*.1f&&Screen.X<=W*.9f&&Screen.Y>=H*.1f&&Screen.Y<=H*.9f);
  J->SetBoolField(TEXT("current_view"),PC->GetViewTarget()==ArcCamera);J->SetNumberField(TEXT("distance"),FVector::Dist(From,Aim));
 }
 FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return Text;
}
FString AStationDirector::GetArcStateJson() const
{
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("available"),IsArcChapter());J->SetBoolField(TEXT("started"),Progress&&Progress->ArcStarted);J->SetStringField(TEXT("node"),Progress?Progress->ArcNode:TEXT(""));J->SetStringField(TEXT("kind"),Arc27::S(ArcCurrent(),TEXT("kind")));J->SetStringField(TEXT("target"),Arc27::S(ArcCurrent(),TEXT("target")));J->SetStringField(TEXT("objective"),GetArcObjective());J->SetNumberField(TEXT("chapter"),Arc27::N(ArcCurrent(),TEXT("chapter")));J->SetNumberField(TEXT("ending"),GetArcFact(TEXT("ending_route")));J->SetBoolField(TEXT("finished"),Progress&&Progress->ArcFinished);J->SetNumberField(TEXT("reserve"),Progress?Progress->Reserve:0);auto Flags=MakeShared<FJsonObject>();if(Progress)for(const auto& P:Progress->ArcFlags)Flags->SetNumberField(P.Key,P.Value);J->SetObjectField(TEXT("flags"),Flags);J->SetBoolField(TEXT("joint_available"),GetArcFact(TEXT("joint_available"))!=0);J->SetBoolField(TEXT("manager_available"),GetArcFact(TEXT("manager_available"))!=0);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return Text;
}
void AStationDirector::AutomationArcPreview(int32 Variant)
{
#if WITH_EDITOR
 if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return;
 AutomationCrisisPreview(Variant%2);Progress->CrisisStarted=Progress->CrisisMeasured=Progress->CrisisBriefed=Progress->CrisisIsolated=Progress->CrisisEndingHeard=true;Progress->CrisisAllocation=Variant==1?12:Variant==2?24:18;Progress->CrisisVerifiedMask=3;Progress->DistrictChoice=Variant%2;Progress->SupplyInstalled=true;LoadArcStory();Progress->ArcNode=TEXT("queue");Progress->ArcStarted=false;Progress->ArcFinished=false;Progress->ArcFlags.Empty();ArcPendingNode.Empty();ArcPendingChoice=-1;UpdateCrisisWorld();UpdateArcWorld(0);
 if(Variant>=4){Progress->KitchenShortcutOpen=true;UpdateArcWorld(0);}
 if(PC&&PC->GetPawn()){PC->GetPawn()->SetActorLocation(FVector(-4550,790,99));PC->SetControlRotation(FRotator(-8,-90,0));}
#endif
}
void AStationDirector::AutomationArcAlbumPreview(int32 EndingRoute)
{
#if WITH_EDITOR
 // An explicit, read-only editor fixture for reviewing the rendered album.
 // The ordinary campaign and route tests never use this shortcut.
 if(EndingRoute<0||EndingRoute>2||!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightPhotoReview")))return;
 AutomationArcPreview(0);Progress->ArcStarted=true;Progress->ArcFinished=true;Progress->ArcNode=TEXT("free");
 Progress->ArcFlags.Add(TEXT("family_resolved"),1);Progress->ArcFlags.Add(TEXT("ending_route"),EndingRoute);Progress->ArcFlags.Add(TEXT("world_committed"),1);
 ClosePanel();UpdateArcWorld(0);EvaluateAchievements(false);
#endif
}
bool AStationDirector::AutomationArcRoundTrip() const
{
#if WITH_EDITOR
 if(!Progress||!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return false;TArray<uint8> Data;
 if(!UGameplayStatics::SaveGameToMemory(Progress,Data))return false;auto* R=Cast<UStationProgress>(UGameplayStatics::LoadGameFromMemory(Data));return R&&R->ArcNode==Progress->ArcNode&&R->ArcStarted==Progress->ArcStarted&&R->ArcFinished==Progress->ArcFinished&&R->ArcFlags.OrderIndependentCompareEqual(Progress->ArcFlags)&&R->Reserve==Progress->Reserve&&R->MandateChoice==Progress->MandateChoice&&R->CrisisAllocation==Progress->CrisisAllocation;
#else
 return false;
#endif
}
