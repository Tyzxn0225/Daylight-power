#include "StationAchievements.h"
#include "StationPrototype.h"
#include "StationAchievementPolicy.h"
#include "StationAchievementData.inl"
#include "StationAchievementPhotos.inl"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString AStationDirector::GetAchievementStateJson() const
{
 TSharedRef<FJsonObject> State=MakeShared<FJsonObject>(),Awards=MakeShared<FJsonObject>(),Variants=MakeShared<FJsonObject>();
 State->SetNumberField(TEXT("count"),GetAchievementCount());State->SetStringField(TEXT("photo_key"),GetAchievementPhotoKey(Selection));State->SetStringField(TEXT("caption"),GetAchievementCaption(Selection));
 if(AchievementArchive)
 {
  for(const auto& Row:AchievementArchive->UnlockedAt)Awards->SetStringField(Row.Key.ToString(),Row.Value.ToIso8601());
  for(const auto& Row:AchievementArchive->PhotoVariants)Variants->SetStringField(Row.Key.ToString(),Row.Value.ToString());
 }
 State->SetObjectField(TEXT("awards"),Awards);State->SetObjectField(TEXT("variants"),Variants);
 FString Result;const auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(State,Writer);return Result;
}
void AStationDirector::AutomationAchievementFreshArchive()
{
#if WITH_EDITOR
 // This cannot reset a real player's on-disk archive, even in an editor build.
 if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return;
 AchievementArchive=NewObject<UStationAchievementArchive>(this);AchievementPending.Empty();AchievementShowing=-1;AchievementToastTime=0;AchievementNeedsSave=false;
#endif
}
bool AStationDirector::AutomationAchievementSerialization()
{
#if WITH_EDITOR
 if(!AchievementArchive||!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return false;
 TArray<uint8> Bytes;if(!UGameplayStatics::SaveGameToMemory(AchievementArchive,Bytes))return false;
 const auto* Loaded=Cast<UStationAchievementArchive>(UGameplayStatics::LoadGameFromMemory(Bytes));
 if(!Loaded||Loaded->SchemaVersion!=AchievementArchive->SchemaVersion||Loaded->UnlockedAt.Num()!=AchievementArchive->UnlockedAt.Num()||Loaded->PhotoVariants.Num()!=AchievementArchive->PhotoVariants.Num()||Loaded->WitnessNotes.Num()!=AchievementArchive->WitnessNotes.Num())return false;
 for(const auto& Row:AchievementArchive->UnlockedAt){const auto* Value=Loaded->UnlockedAt.Find(Row.Key);if(!Value||*Value!=Row.Value)return false;}
 for(const auto& Row:AchievementArchive->PhotoVariants){const auto* Value=Loaded->PhotoVariants.Find(Row.Key);if(!Value||*Value!=Row.Value)return false;}
 for(const auto& Row:AchievementArchive->WitnessNotes){const auto* Value=Loaded->WitnessNotes.Find(Row.Key);if(!Value||*Value!=Row.Value)return false;}
 return true;
#else
 return false;
#endif
}

namespace
{
static_assert(UE_ARRAY_COUNT(StoryAchievements20::Entries)==StationAchievements::Count,"Achievement definitions and policy must stay aligned");
const TCHAR* AchievementSlot=TEXT("DaylightRights_StoryAchievements_v1");
bool ReadOnlyAchievementRun()
{
#if !UE_BUILD_SHIPPING
 return FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave"));
#else
 return false;
#endif
}
}
void AStationDirector::LoadAchievements()
{
 AchievementArchive=Cast<UStationAchievementArchive>(UGameplayStatics::LoadGameFromSlot(AchievementSlot,0));
 if(!AchievementArchive)AchievementArchive=NewObject<UStationAchievementArchive>(this);
 // Old checkpoints can prove story facts but not a historical unlock date.
 // Backfill is quiet and labelled as reconstructed from the current checkpoint.
 EvaluateAchievements(false);
}
void AStationDirector::StoreAchievements()
{
 if(!AchievementArchive||ReadOnlyAchievementRun())return;
 if(!UGameplayStatics::SaveGameToSlot(AchievementArchive,AchievementSlot,0))
 {AchievementNeedsSave=true;UE_LOG(LogTemp,Warning,TEXT("Story achievement archive could not be saved"));}
 else AchievementNeedsSave=false;
}
void AStationDirector::EvaluateAchievements(bool Announce)
{
 if(!Progress||!AchievementArchive)return;
 StationAchievements::Facts S;
 S.started=Progress->Started;S.firstLightComplete=Progress->CutsceneDone;S.heardAllocation=Progress->HeardAllocation;S.metBroker=Progress->MetBroker;S.readLedger=Progress->ReadLedger;
 S.allocationPolicy=Progress->AllocationPolicy;S.allocationReviews=Progress->AllocationReviewMask;
 S.rationMask=Progress->RationMask;S.rationVisits=Progress->RationVisitMask;S.rationReviews=Progress->RationReviewMask;S.readRecall=Progress->ReadRecall;
 S.seenFamily=Progress->SeenFamilyFilm;S.conflictComplete=Progress->ConflictComplete;S.conflictEvidence=Progress->ConflictEvidence;S.conflictWitness=Progress->ConflictWitness;S.conflictHeard=Progress->ConflictHeardMask;
 S.supplyComplete=Progress->SupplyComplete;S.supplyEvidence=Progress->SupplyEvidenceMask;S.supplyMeasured=Progress->SupplyMeasuredMask;S.supplyHeard=Progress->SupplyHeardMask;
 S.dutySettled=Progress->DutySettled;S.dutyWork=Progress->DutyWorkMask;
 S.districtStarted=Progress->DistrictStarted;S.districtSettled=Progress->DistrictSettled;S.districtChoice=Progress->DistrictChoice;S.seenDistrictEnding=Progress->SeenDistrictFilm;S.districtEvidence=Progress->DistrictEvidence;
 S.exploration=Progress->ExplorationSeenMask;S.urban=Progress->UrbanReadMask;S.quiet=Progress->QuietVisitMask;S.shortcutOpen=Progress->KitchenShortcutOpen;S.shortcutReady=NeighborhoodReady&&KitchenGateAlpha>=.999f;
 S.heatShiftComplete=Progress->CrisisEndingHeard;S.storyFinished=Progress->ArcFinished;S.familyResolved=GetArcFact(TEXT("family_resolved"))!=0;S.endingRoute=GetArcFact(TEXT("ending_route"));
 bool Changed=false;
 for(int32 I=0;I<StationAchievements::Count;++I)
 {
  const auto& Entry=StoryAchievements20::Entries[I];const FName Id(Entry.Id);
  if(!StationAchievements::Eligible(S,I)||AchievementArchive->UnlockedAt.Contains(Id))continue;
  AchievementArchive->UnlockedAt.Add(Id,FDateTime::UtcNow());
  FString Note=Announce?TEXT("在本次旅程中留下。") :TEXT("由当前存档中的已发生事实补记；并非历史解锁时间。");
  if(I==2)Note+=Progress->AllocationPolicy==0?TEXT("当时公开原始名单与担保关系，三方回应均已听取。"):TEXT("当时仅公开规则并封存个人名单，三方回应均已听取。");
  if(I==7)Note+=Progress->SupplyChoice==0?TEXT("当时使用公共备用担保。"):TEXT("当时抵押林照本人的工时。");
  if(I==11)Note+=Progress->DistrictChoice==0?TEXT("当时先公开试亮，夜班押金延期；异议仍在回执中。"):TEXT("当时先退夜班押金，公共试亮取消；原检验记录仍保留。");
  AchievementArchive->WitnessNotes.Add(Id,Note.Left(240));
  const FString PhotoVariant=ANSI_TO_TCHAR(StationAchievements::FirstPhotoVariant(I,Progress->AllocationPolicy,Progress->SupplyChoice,Progress->DistrictChoice));
  AchievementArchive->PhotoVariants.Add(Id,FName(*PhotoVariant));
  if(Announce)AchievementPending.Add(I);Changed=true;
 }
 if(Changed){AchievementNeedsSave=true;StoreAchievements();}
}
bool AStationDirector::CanShowAchievementToast() const
{
 return PC&&PC->GetPawn()&&GetWorld()->GetTimeSeconds()>=ToastUntil&&GetWorld()->GetTimeSeconds()>=ResumeRecapUntil&&StationAchievements::CanShow(IsMenuOpen(),Starting||IsStoryFilmActive(),QuietIndex>=0,IsPhysicalActionActive(),IsVoicePlaying()||IsNarrationPlaying(),IsAmbientStoryActive());
}
void AStationDirector::TickAchievements(float Delta)
{
 if(!AchievementArchive)return;
 AchievementPoll-=Delta;AchievementRetry-=Delta;
 if(AchievementPoll<=0){AchievementPoll=.5f;EvaluateAchievements(true);}
 if(AchievementNeedsSave&&AchievementRetry<=0){AchievementRetry=15.f;StoreAchievements();}
 if(!CanShowAchievementToast())return;
 if(AchievementShowing>=0)
 {
  AchievementToastTime+=Delta;
  if(AchievementToastTime>=5.0f){AchievementShowing=-1;AchievementToastTime=0;AchievementToastGap=1.f;}
 }
 else if(AchievementToastGap>0)AchievementToastGap-=Delta;
 else if(!AchievementPending.IsEmpty())
 {
  AchievementShowing=AchievementPending[0];AchievementPending.RemoveAt(0);AchievementToastTime=0;
  PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.12f,false);
 }
}
int32 AStationDirector::GetAchievementToastIndex() const{return CanShowAchievementToast()?AchievementShowing:-1;}
bool AStationDirector::IsAchievementUnlocked(int32 Index) const
{
 return AchievementArchive&&Index>=0&&Index<StationAchievements::Count&&AchievementArchive->UnlockedAt.Contains(FName(StoryAchievements20::Entries[Index].Id));
}
int32 AStationDirector::GetAchievementCount() const
{
 int32 Count=0;for(int32 I=0;I<StationAchievements::Count;++I)if(IsAchievementUnlocked(I))++Count;return Count;
}
FString AStationDirector::GetAchievementNote(int32 Index) const
{
 if(!IsAchievementUnlocked(Index))return TEXT("");
 const FString* Note=AchievementArchive->WitnessNotes.Find(FName(StoryAchievements20::Entries[Index].Id));return Note?*Note:TEXT("已在旅程中留下。");
}
FString AStationDirector::GetAchievementPhotoKey(int32 Index) const
{
 if(!IsAchievementUnlocked(Index))return FString();
 const FName Id(StoryAchievements20::Entries[Index].Id);
 const FName* Saved=AchievementArchive->PhotoVariants.Find(Id);
 const FString Variant=Saved?Saved->ToString():TEXT("default");
 const FString Key=Id.ToString()+TEXT("__")+Variant;
 // Only authored keys can address image files. Older archives get neutral art.
 for(const auto& Photo:StoryPhotos20::Entries)if(Key==Photo.Key)return Key;
 return Id.ToString()+TEXT("__default");
}
FString AStationDirector::GetAchievementCaption(int32 Index) const
{
 const FString Key=GetAchievementPhotoKey(Index);
 if(Key.IsEmpty())return TEXT("经历发生之后，这里才会显影。");
 for(const auto& Photo:StoryPhotos20::Entries)if(Key==Photo.Key)return Photo.Caption;
 return TEXT("已在旅程中留下。");
}
void AStationDirector::RequestAchievements()
{
 if(QuietIndex>=0||IsStoryFilmActive()||Starting||IsPhysicalActionActive())return;
 if(Panel==EStationPanel::Achievements){ClosePanel();return;}
 if(!IsMenuOpen()){EvaluateAchievements(false);OpenPanel(EStationPanel::Achievements);StopPresentationVoice();}
}
