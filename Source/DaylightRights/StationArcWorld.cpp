#include "StationPrototype.h"
#include "Dom/JsonObject.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/RectLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
FString AS(const TSharedPtr<FJsonObject>& O,const TCHAR* Key){FString S;if(O)O->TryGetStringField(Key,S);return S;}
FVector AP(const TSharedPtr<FJsonObject>& O){const auto& P=O->GetArrayField(TEXT("position"));return FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber());}
void BuildArcGateReader(AStationObject* Gate)
{
 auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_StationDevice.M_StationDevice"));
 auto Add=[&](const TCHAR* Shape,FVector At,FVector Scale,FLinearColor Color,bool Status=false,FRotator Rotation=FRotator::ZeroRotator)
 {
  auto* Part=NewObject<UStaticMeshComponent>(Gate);Gate->AddInstanceComponent(Part);Part->SetupAttachment(Gate->Anchor);
  Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
  Part->SetRelativeLocation(At);Part->SetRelativeScale3D(Scale);Part->SetRelativeRotation(Rotation);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->RegisterComponent();
  if(Material){auto* MID=UMaterialInstanceDynamic::Create(Material,Gate);MID->SetVectorParameterValue(TEXT("Tint"),Color);MID->SetScalarParameterValue(TEXT("Glow"),Status?3.5f:0.f);Part->SetMaterial(0,MID);}
  if(Status)Part->ComponentTags.Add(TEXT("ArcCardStatus"));
 };
 // A separate reader post stands in the clear forecourt, outside the leaf's
 // sweep. The hinge itself is embedded in the existing wall and cannot host
 // a visible interaction point. The post has a bolted base and body collision.
 Add(TEXT("Cube"),FVector(90,78,3),FVector(.28,.28,.06),FLinearColor(.16f,.17f,.16f));
 Add(TEXT("Cube"),FVector(90,78,55),FVector(.12,.12,1.1),FLinearColor(.07f,.08f,.08f));
 Add(TEXT("Cube"),FVector(90,72,110),FVector(.18,.08,.26),FLinearColor(.035f,.055f,.058f));
 Add(TEXT("Cube"),FVector(90,67.5f,117),FVector(.10,.008,.022),FLinearColor(.08f,.3f,.22f),true);
 Add(TEXT("Cylinder"),FVector(90,67.5f,107),FVector(.06,.06,.004),FLinearColor(.28f,.23f,.16f),false,FRotator(0,0,90));
 for(float X:{80.f,100.f})for(float Y:{68.f,88.f})Add(TEXT("Sphere"),FVector(X,Y,6),FVector(.025),FLinearColor(.18f,.2f,.2f));
 auto* Body=NewObject<UBoxComponent>(Gate);Gate->AddInstanceComponent(Body);Body->SetupAttachment(Gate->Anchor);
 Body->SetRelativeLocation(FVector(90,78,60));Body->SetBoxExtent(FVector(10,10,60));Body->SetCollisionProfileName(TEXT("BlockAll"));Body->RegisterComponent();
}
}
AActor* AStationDirector::ResolveArcTarget(const FString& Key) const
{
 if(Key==TEXT("he"))return LinHe;if(Key==TEXT("engineer")||Key==TEXT("ye"))return Engineer;if(Key==TEXT("zhou"))return ZhouChi;
 if(Key==TEXT("joint_ally"))return GetArcFact(TEXT("crew_support"))?Engineer.Get():ResolveArcTarget(TEXT("zhan"));
 const auto* Value=ArcTargets.Find(Key);return Value&&Value->IsValid()?Value->Get():nullptr;
}
void AStationDirector::UpdateArcWorld(float Delta)
{
 if(!LoadArcStory())return;const bool Available=IsArcChapter();
 if(Available)
 {
  const auto World=ArcStory->GetObjectField(TEXT("world"));
  for(const auto& Value:World->GetArrayField(TEXT("people")))
  {
   auto Spec=Value->AsObject();const FString Key=AS(Spec,TEXT("key"));
   if(IsValid(ResolveArcTarget(Key)))continue;
   FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
   auto* Person=GetWorld()->SpawnActor<AStationEngineer>(AP(Spec),FRotator(0,90,0),Params);if(!Person)continue;
   Person->PersonName=AS(Spec,TEXT("name"));Person->ApplyIdentity(AS(Spec,TEXT("base")));Person->NeighborhoodRoutes=true;
   const FString Variant=AS(Spec,TEXT("variant"));
   if(auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/BZ/Characters/%s/SK_%s.SK_%s"),*Variant,*Variant,*Variant)))Person->GetMesh()->SetSkeletalMeshAsset(Mesh);
   ArcPeople.Add(Person);ArcTargets.Add(Key,Person);
  }
  // Aliases use the existing furniture and real meters, not a second cabinet in
  // the same footprint. A gate wrapper has no geometry/collision of its own.
  for(const auto& Value:World->GetArrayField(TEXT("objects")))
  {
   auto Spec=Value->AsObject();const FString Key=AS(Spec,TEXT("key")),Existing=AS(Spec,TEXT("existing"));AActor* Target=nullptr;
   if(Existing==TEXT("field_desk"))Target=FieldDesk;
   else if(Existing==TEXT("work_account"))Target=WorkAccount;
   else if(Existing==TEXT("node_b")){for(AStationObject* O:Objects)if(O&&O->Kind==EStationObjectKind::NodeB){Target=O;break;}}
   else if(Existing==TEXT("urban_0")&&UrbanObjects.Num())Target=UrbanObjects[0];
   else if(Existing==TEXT("crisis_source_rich")&&CrisisObjects.Num()>2)Target=CrisisObjects[2];
   else if(Existing==TEXT("supplier_return"))Target=SupplierReturn;
   else if(Existing==TEXT("field_radio"))Target=FieldRadio;
   else if(Existing==TEXT("crisis_source")&&CrisisObjects.Num())Target=CrisisObjects[0];
   else if(Existing==TEXT("quiet_home")&&QuietObjects.Num())Target=QuietObjects[0];
   else if(Existing.IsEmpty()||Existing==TEXT("kitchen_gate"))
   {
    Target=ResolveArcTarget(Key);
    if(!IsValid(Target))
    {
     FTransform T(AP(Spec));auto* O=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
     O->Kind=EStationObjectKind::SupplyParcel;O->DisplayName=AS(Spec,TEXT("name"));O->UseExistingGeometry=Existing==TEXT("kitchen_gate");
     if(O->UseExistingGeometry)O->InteractionOffset=FVector(90,60,110);
     O->FinishSpawning(T);if(O->UseExistingGeometry)BuildArcGateReader(O);Objects.Add(O);ArcObjects.Add(O);Target=O;
    }
   }
   if(IsValid(Target))ArcTargets.Add(Key,Target);
  }
  if(ArcLights.IsEmpty())
  {
   for(const FVector P:{FVector(-4750,1630,260),FVector(4410,3380,290),FVector(-4700,1190,230)})
   {
    auto* L=GetWorld()->SpawnActor<ARectLight>(P,FRotator(-90,0,0));auto* C=CastChecked<URectLightComponent>(L->GetLightComponent());C->SetMobility(EComponentMobility::Movable);C->SetIntensityUnits(ELightUnits::Lumens);C->SetIntensity(0);C->SetSourceWidth(65);C->SetSourceHeight(35);C->SetAttenuationRadius(280);C->SetLightColor(FLinearColor(1,.69,.4));ArcLights.Add(L);
   }
  }
 }
 for(AStationEngineer* P:ArcPeople)if(IsValid(P)){if(Available&&P->IsHidden())P->SetConversationPaused(false,nullptr);P->SetActorHiddenInGame(!Available);P->SetActorEnableCollision(Available);if(!Available)P->SetConversationPaused(true,nullptr);}
 for(AStationObject* O:ArcObjects)if(IsValid(O)){O->Active=Available;O->SetActorHiddenInGame(!Available);O->SetActorEnableCollision(Available);}
 if(!Available){for(ARectLight* L:ArcLights)if(L)L->GetLightComponent()->SetIntensity(0);return;}
 if(FieldRadio)FieldRadio->Active=true;
 // Only the current story alias changes its label. Old completed paperwork
 // remains accessible; checking it cannot credit an unrelated new objective.
 const FString Current=AS(ArcCurrent(),TEXT("target"));
 for(const auto& V:ArcStory->GetObjectField(TEXT("world"))->GetArrayField(TEXT("objects")))
 {
  auto S=V->AsObject();if(AS(S,TEXT("key"))==Current)if(auto* O=Cast<AStationObject>(ResolveArcTarget(Current))){O->Active=true;O->DisplayName=AS(S,TEXT("name"));}
 }
 if(Progress->ArcFinished)
 {
  if(FieldDesk)FieldDesk->DisplayName=TEXT("院内事务台 · 匿名副本与工牌留存");
  if(WorkAccount)WorkAccount->DisplayName=TEXT("维修铺工作台 · 分租底册与接头");
  if(auto* Meter=Cast<AStationObject>(ResolveArcTarget(TEXT("return_meter"))))Meter->DisplayName=TEXT("连接 B · 回水原始记录");
  if(auto* Offer=Cast<AStationObject>(ResolveArcTarget(TEXT("offer"))))Offer->DisplayName=TEXT("恒昼阅览端 · 供热表与岗位附件");
 }
 if(KitchenGate)
 {
  bool Open=GetArcFact(TEXT("shortcut"))!=0||Progress->KitchenShortcutOpen;
  if(GetArcFact(TEXT("gate_blocked"))&&!GetArcFact(TEXT("gate_resolved")))Open=false;
  if(Panel==EStationPanel::ArcScene&&AS(ArcCurrent(),TEXT("id"))==TEXT("blockade")&&ArcSceneLine>=3)Open=false;
  if(GetArcFact(TEXT("priority_live")))Open=PC&&PC->GetPawn()&&FVector::Dist2D(PC->GetPawn()->GetActorLocation(),KitchenGate->GetActorLocation())<260;
  const float Wanted=Open?1.f:0.f;KitchenGateAlpha=Delta>0?FMath::FInterpConstantTo(KitchenGateAlpha,Wanted,Delta,.85f):Wanted;
  KitchenGate->SetActorRotation(FRotator(0,-100*FMath::InterpEaseInOut(0.f,1.f,KitchenGateAlpha,2.f),0));
 }
 const bool Committed=GetArcFact(TEXT("world_committed"))!=0;
 if(ArcLights.Num()==3)
 {
  // These are local task lamps, not extra generated heat or restored home rights.
  ArcLights[0]->GetLightComponent()->SetIntensity(Committed?(GetArcFact(TEXT("priority_live"))?90:380):140);
  ArcLights[1]->GetLightComponent()->SetIntensity(GetArcFact(TEXT("priority_live"))?650:0);
  ArcLights[2]->GetLightComponent()->SetIntensity(GetArcFact(TEXT("gate_blocked"))&&!Committed?70:160);
 }
 if(auto* Gate=Cast<AStationObject>(ResolveArcTarget(TEXT("gate"))))
 {
  const FLinearColor Status=GetArcFact(TEXT("badge_revoked"))?FLinearColor(.65,.025,.01):GetArcFact(TEXT("gate_blocked"))&&!Committed?FLinearColor(.55,.25,.015):FLinearColor(.08,.3,.22);
  Gate->SetIndicator(Status,1);
  for(auto* Component:Gate->GetComponentsByTag(UStaticMeshComponent::StaticClass(),TEXT("ArcCardStatus")))
   if(auto* Mesh=Cast<UStaticMeshComponent>(Component))if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))MID->SetVectorParameterValue(TEXT("Tint"),Status);
 }
 if(auto* Bag=Cast<AStationObject>(ResolveArcTarget(TEXT("bag")))){const bool Collected=GetArcFact(TEXT("zhan_helped"))!=0;Bag->SetActorHiddenInGame(Collected);Bag->Active=!Collected;Bag->SetActorEnableCollision(!Collected);}
}
void AStationDirector::TickArcCommunity()
{
 if(!IsArcChapter())return;
 const FString Node=Progress->ArcNode;const int Chapter=ArcCurrent()?int(ArcCurrent()->GetNumberField(TEXT("chapter"))):14;
 auto Task=[&](AStationEngineer* P,FVector Where,const TCHAR* Why,const TCHAR* Action=TEXT("Idle"))
 {
  if(!IsValid(P)||WorkYieldPeople.Contains(P))return;P->Patrol=true;P->NeighborhoodRoutes=true;P->SetCommunityRoutine(TEXT("N16_Arc"));P->GetCharacterMovement()->MaxWalkSpeed=115;P->SetCommunityTask(Where,Why,Action);
 };
 // Production transitions are walking tasks. Actors are never teleported to
 // make a conversation appear assembled or to satisfy a cooperative interlock.
 const bool FamilyVisit=Chapter>=18&&!Progress->ArcFinished;
 Task(LinHe,FamilyVisit?FVector(-3760,2050,107):FVector(-4760,1510,107),Progress->ArcFinished?TEXT("回互助厨房收拾夜饭与自己的铺位，原住址尚未恢复"):TEXT("整理等候者的杯子与衣物，保留自己的去留决定"));
 Task(Cast<AStationEngineer>(ResolveArcTarget(TEXT("ro"))),FVector(-5040,1130,107),TEXT("看守送餐入口，核对自己经手的登记"));
 auto* Zhan=Cast<AStationEngineer>(ResolveArcTarget(TEXT("zhan")));
 Task(Zhan,!Progress->ArcFinished&&GetArcFact(TEXT("route"))==1&&!GetArcFact(TEXT("crew_support"))?FVector(-760,-1510,91):FVector(-4730,1170,107),TEXT("取回自己的工作物品，决定是否到现场作证"));
 Task(Cast<AStationEngineer>(ResolveArcTarget(TEXT("shen"))),FVector(4600,3650,107),TEXT("在恒昼廊等待实际方案与核验回执"));
 const bool Courier=Node==TEXT("courier_copy");AActor* Handoff=ResolveArcTarget(TEXT("handoff"));
 Task(ZhouChi,Courier&&Handoff?Handoff->GetActorLocation()+FVector(-100,80,84):FVector(-4390,950,107),Courier?TEXT("步行到交接柜，交还自己留存的轮班底联"):TEXT("等候当事人追问轮班信息外流"));
 const bool DoorDuty=Chapter==16||Node==TEXT("shen_offer")||Node==TEXT("window")||Node==TEXT("offer_terms")||Node==TEXT("offer_decision")||Node==TEXT("family");
 Task(Engineer,DoorDuty?FVector(-4800,980,107):FVector(-920,-1270,91),DoorDuty?TEXT("前往送餐门，核对待撤检修人员与现场秩序"):TEXT("看守换热联锁，等待协作操作"));
}
