#include "StationPrototype.h"
#include "StationAftermathPolicy.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool AStationDirector::IsAftermathChapter() const
{
 return Progress&&StationAftermath::Available(Progress->DistrictSettled,Progress->SeenDistrictFilm);
}
int32 AStationDirector::GetAftermathStage() const
{
 return Progress?StationAftermath::Next(Progress->AftermathHeardMask):0;
}
int32 AStationDirector::GetAftermathPerson(const AActor* Person) const
{
 if(!Person)return -1;
 return Person==Engineer?0:Person==ZhouChi?1:Person==LinHe?2:-1;
}
AActor* AStationDirector::GetAftermathGuidance() const
{
 switch(GetAftermathStage()){case 0:return Engineer;case 1:return ZhouChi;case 2:return LinHe;default:return nullptr;}
}
FString AStationDirector::GetAftermathObjective() const
{
 if(!Progress)return TEXT("");
 const int32 Count=StationAftermath::Count(Progress->AftermathHeardMask);
 const TCHAR* Steps[]={TEXT("找到叶岑，听检修组如何记这笔处置"),TEXT("找到周驰，问清回执之外的那笔生意"),TEXT("找到林禾，听她本人怎样看这一夜"),TEXT("三份回访已记 · 可回家、重看原件或自由探索")};
 return Count<3?FString::Printf(TEXT("%s  ·  回访 %d / 3"),Steps[GetAftermathStage()],Count):Steps[3];
}
TArray<FString> AStationDirector::GetAftermathConversation(AActor* Person) const
{
 const bool Trial=Progress->DistrictChoice==0;
 switch(GetAftermathPerson(Person))
 {
 case 0:
  if(Trial)return {
   TEXT("叶岑：试亮那会儿，我也站在灯下面。有人问是不是终于不用交钱了，我没接话。"),
   TEXT("林照：押金延期那一栏，是我签的。"),
   TEXT("叶岑：我知道。可下个月要是工位少两个，我会先保留下来的人。别因为我替大家说过话，就以为我不会算自己的账。"),
   TEXT("林照：这句也记下来。不写成检修组一致同意。")};
  return {
   TEXT("叶岑：押金退回去了。有个组员拿了钱，还是没敢退宿舍。搬出去，工位也得重新排。"),
   TEXT("林照：钱到账了，人还被拴在原地。"),
   TEXT("叶岑：也别替他写成白退了。那是他的钱。我替他争，也希望他留下来上班，我的组不能散。"),
   TEXT("林照：把这两件事分开记。都留着。")};
 case 1:
  if(Trial)return {
   TEXT("周驰：你把光留了两个小时。等押金的人，把凭条拿来问我能不能先换饭票。"),
   TEXT("林照：你按多少收？"),
   TEXT("周驰：折价。有人骂我，又有人催我收。我赚的是他们等不起的那几天，你要记就记完整。"),
   TEXT("林照：试亮支出后面，我会补上这笔折价。你的名字也会在。")};
  return {
   TEXT("周驰：退款的人拿回了钱，厂里也开始给这批合格灯重新标价。两边都说这单办妥了。"),
   TEXT("林照：你还接转售？"),
   TEXT("周驰：接。我垫的钱也没自己长回来。你可以看不起我，可他们要买下一次光，还得来找我。"),
   TEXT("林照：回执写了退款。我再留一行：下次买光，价格另算。")};
 case 2:
  if(Trial)return {
   TEXT("林禾：灯亮的时候，我真的想叫你看。然后旁边有人问，押金到底哪天退。"),
   TEXT("林照：你是不是也觉得我替别人答应得太快？"),
   TEXT("林禾：我也想过劝他们再等一等。只要等的人不是我，话就说得特别容易。哥，我也没比你干净。"),
   TEXT("林照：我小时候想给家里留一整天的太阳。现在连两个小时，都有人替我垫着。"),
   TEXT("林禾：先回家吃饭吧。不是说算了，是明天还得把这些名字一个个记住。")};
  return {
   TEXT("林禾：我拿着回执走过检光井，灯都还是好的。我有一瞬间，真想把退款的人叫回来。"),
   TEXT("林照：钱是他们自己的。"),
   TEXT("林禾：我知道。可知道了，也还是会怨。哥，我不想再让你替我说我懂事。"),
   TEXT("林照：那就不写。今天没亮的地方，也留在回执上。"),
   TEXT("林禾：回家吧。饭凉了可以热，别再拿等一等当作谁答应过。")};
 default:return {};
 }
}
bool AStationDirector::CompleteAftermathConversation(AActor* Person)
{
 if(!IsAftermathChapter())return false;
 const int32 RoleIndex=GetAftermathPerson(Person);if(RoleIndex<0)return false;
 const int32 Old=StationAftermath::Mask(Progress->AftermathHeardMask);
 Progress->AftermathHeardMask=StationAftermath::Record(Old,RoleIndex);
 const bool Changed=Old!=Progress->AftermathHeardMask;
 ClosePanel();
 if(Changed)
 {
  SaveProgress();
  Notify(GetAftermathStage()==3?TEXT("三份回访已记。没有写成共同认可。J 可回看这一夜留下的余账。"):TEXT("已记下这位当事人的说法。下一位的位置已更新到地图。"));
 }
 return true;
}
FString AStationDirector::GetAftermathJournal() const
{
 FString Result=TEXT("M11 / 散场以后\n\n回执已经签发，生活还在继续。三人的回访记录分别保留，不合并成共同认可。\n\n");
 const TCHAR* Names[]={TEXT("叶岑 · 工位与工资"),TEXT("周驰 · 垫款与折价"),TEXT("林禾 · 家庭与本人意见")};
 for(int32 I=0;I<3;++I)Result+=(Progress->AftermathHeardMask&(1<<I)?TEXT("[已听完] "):TEXT("[待回访] "))+FString(Names[I])+TEXT("\n");
 Result+=Progress->DistrictChoice==0?TEXT("\n原处置：公共试亮，押金延期。回访不改变已经执行的去向。"):TEXT("\n原处置：押金先退，公共试亮取消。回访不改写原始检验结果。");
 return Result;
}
FString AStationDirector::GetAftermathStateJson() const
{
 const auto* Guide=Cast<AStationEngineer>(GetAftermathGuidance());
 TSharedRef<FJsonObject> S=MakeShared<FJsonObject>();
 S->SetBoolField(TEXT("available"),IsAftermathChapter());S->SetNumberField(TEXT("mask"),Progress?Progress->AftermathHeardMask:0);S->SetNumberField(TEXT("stage"),GetAftermathStage());
 S->SetNumberField(TEXT("count"),Progress?StationAftermath::Count(Progress->AftermathHeardMask):0);S->SetStringField(TEXT("guide"),IsAftermathChapter()&&Guide?Guide->PersonName:TEXT(""));
 S->SetNumberField(TEXT("choice"),Progress?Progress->DistrictChoice:-1);S->SetNumberField(TEXT("reserve"),Progress?Progress->Reserve:0);
 S->SetStringField(TEXT("objective"),GetObjective());S->SetStringField(TEXT("mission"),GetMissionTitle());S->SetStringField(TEXT("body"),GetPanelBody());
 FString Result;const auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(S,Writer);return Result;
}
void AStationDirector::AutomationAftermathPreview(int32 Choice,int32 HeardMask)
{
#if WITH_EDITOR
 if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave"))||Choice<0||Choice>1)return;
 AutomationReset(false);
 Progress->MetEngineer=Progress->ReadNotice=Progress->HasConnector=Progress->HeardRisk=true;
 Progress->Connections=7;Progress->ChosenPlan=0;Progress->Reserve=72;Progress->Started=Progress->CutsceneDone=true;
 Progress->HeardAllocation=Progress->MetBroker=Progress->ReadLedger=true;Progress->AllocationPolicy=1;Progress->AllocationReviewMask=7;
 Progress->RationMask=3;Progress->RationVisitMask=Progress->RationReviewMask=7;Progress->ReadRecall=true;
 Progress->ConflictComplete=true;Progress->ConflictChoice=1;Progress->SupplyComplete=true;Progress->SupplyChoice=1;
 Progress->DutySettled=Progress->SeenDutyFilm=true;Progress->DistrictStarted=Progress->DistrictSettled=Progress->SeenDistrictFilm=true;
 Progress->DistrictChoice=Choice;Progress->DistrictEvidence=7;Progress->AftermathHeardMask=StationAftermath::Mask(HeardMask);RiskConversation=false;
 ApplyProgress();UpdateDistrictState();
#endif
}
bool AStationDirector::AutomationAftermathRoundTrip() const
{
#if WITH_EDITOR
 if(!Progress||!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return false;
 TArray<uint8> Data;if(!UGameplayStatics::SaveGameToMemory(Progress,Data))return false;
 const auto* Reloaded=Cast<UStationProgress>(UGameplayStatics::LoadGameFromMemory(Data));
 return Reloaded&&Reloaded->AftermathHeardMask==Progress->AftermathHeardMask&&Reloaded->DistrictChoice==Progress->DistrictChoice&&Reloaded->Reserve==Progress->Reserve&&Reloaded->SeenDistrictFilm==Progress->SeenDistrictFilm;
#else
 return false;
#endif
}
