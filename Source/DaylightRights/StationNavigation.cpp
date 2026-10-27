#include "StationNavigation.h"
#include "StationPrototype.h"
#include "StationNavigationPolicy.h"
#include "StationNavigationData.inl"
#include "StationNeighborhoodData.inl"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
int32 PortraitOf(const AActor* Actor)
{
 const auto* Person=Cast<AStationEngineer>(Actor);if(!Person)return -1;
 return Person->PersonName==TEXT("林照")?0:Person->PersonName==TEXT("叶岑")?1:Person->PersonName==TEXT("林禾")?2:Person->PersonName==TEXT("周驰")?3:Person->PersonName==TEXT("沈砚")?4:Person->PersonName==TEXT("罗婶")?5:Person->PersonName==TEXT("阿湛")?6:-1;
}
const FLinearColor NavGold(.87f,.69f,.40f,1),NavPaper(.94f,.92f,.85f,1),NavTeal(.48f,.74f,.70f,1),NavDim(.51f,.57f,.56f,1);
StationNavigation::Point XY(FVector V){return {V.X,V.Y};}
FVector2D V2(StationNavigation::Point P){return FVector2D(P.x,P.y);}
}

TArray<FStationMapPin> AStationDirector::GetNavigationPins() const
{
 TArray<FStationMapPin> Pins;if(!Progress||!PC||!PC->GetPawn())return Pins;
 AActor* Main=GetGuidanceTarget();const FVector Player=PC->GetPawn()->GetActorLocation();
 auto Add=[&](AActor* Actor,bool Optional=false,bool Seen=false)
 {
  if(!IsValid(Actor))return;
  if(auto* Object=Cast<AStationObject>(Actor);Object&&!Object->Active)return;
  for(const auto& P:Pins)if(P.Actor.Get()==Actor)return;
  FStationMapPin P;P.Actor=Actor;P.Position=Actor->GetActorLocation();P.Portrait=PortraitOf(Actor);P.Primary=Actor==Main;P.Optional=Optional;P.Discovered=Seen;
  if(auto* Person=Cast<AStationEngineer>(Actor))P.Name=Person->PersonName;
  else if(auto* Object=Cast<AStationObject>(Actor))P.Name=Object->DisplayName;
  Pins.Add(P);
 };
 if(IsArcChapter())for(AStationEngineer* Person:ArcPeople)Add(Person);
 for(int32 I=0;I<SurveySources.Num();++I)
  if(SurveyAvailable(I)&&!SurveySolved(I)&&IsValid(SurveySources[I])&&
    (SurveySources[I]==Main||FVector::Dist2D(Player,SurveySources[I]->GetActorLocation())<900.f))Add(SurveySources[I],true);
 if(IsConflictChapter()&&Progress->ConflictStarted)for(int32 I=0;I<ConflictTraces.Num();++I)
  if(!(Progress->ConflictTraceMask&(1<<I)))Add(ConflictTraces[I],true);
 Add(Main);Add(Engineer);if(Progress->Started){Add(LinHe);Add(ZhouChi);}
 if(IsSupplyChapter()&&!Progress->SupplyComplete&&Progress->SupplyChoice<0)
 {
  if(!(Progress->SupplyEvidenceMask&1))Add(SupplierReturn,true);
  if(!(Progress->SupplyEvidenceMask&2))Add(AllocationLedger,true);
  if(!(Progress->SupplyEvidenceMask&4))Add(WorkAccount,true);
  for(auto& O:Objects)
   if((O->Kind==EStationObjectKind::NodeA&&!(Progress->SupplyMeasuredMask&1))||
      (O->Kind==EStationObjectKind::NodeC&&!(Progress->SupplyMeasuredMask&2))||
      (O->Kind==EStationObjectKind::NodeB&&Progress->ConflictChoice==1&&!(Progress->SupplyMeasuredMask&4)))Add(O,true);
 }
 if(IsDutyChapter()&&!Progress->DutySettled&&(!Progress->DutyFaultRaised||Progress->DutyEmergencyChoice==1||Progress->DutyCommunityRestored))
  for(int32 I=0;I<4;++I)if((GetDutyRequiredMask()&(1<<I))&&!(Progress->DutyWorkMask&(1<<I)))Add(GetDutyOrderObject(I),true);
 if(IsCrisisChapter()&&Progress->CrisisStarted)
 {
  if(CrisisObjects.IsValidIndex(0)&&Progress->CrisisAllocation<0)Add(CrisisObjects[0]);
  if(Progress->CrisisAllocation>=0)for(int32 I=1;I<3;++I)
   if(CrisisObjects.IsValidIndex(I)&&!(Progress->CrisisVerifiedMask&(1<<(I-1))))Add(CrisisObjects[I]);
 }
 for(int32 I=0;I<ExplorationObjects.Num();++I)
 {
  auto* O=ExplorationObjects[I].Get();if(!IsValid(O))continue;const bool Seen=(Progress->ExplorationSeenMask&(1<<I))!=0;
  if(StationNavigation::ShowOptional(Seen,FVector::Dist2D(Player,O->GetActorLocation()),CanSeeTarget(O)))Add(O,true,Seen);
 }
 for(int32 I=0;I<UrbanObjects.Num();++I)
 {
  auto* O=UrbanObjects[I].Get();if(!IsValid(O))continue;const bool Seen=(Progress->UrbanReadMask&(1<<I))!=0;
  if(StationNavigation::ShowOptional(Seen,FVector::Dist2D(Player,O->GetActorLocation()),CanSeeTarget(O)))Add(O,true,Seen);
 }
 return Pins;
}
FString AStationDirector::GetNavigationPlace(AActor* Target) const
{
 if(!IsValid(Target))return TEXT("社区自由探索");
 if(Target->ActorHasTag(TEXT("SurveyV29")))if(const auto* O=Cast<AStationObject>(Target))return O->DisplayName;
 if(Target==FieldDesk)return TEXT("公共院落 · 事务台");if(Target==FieldRadio)return TEXT("冷却检修带 · 电台");
 const FVector P=Target->GetActorLocation();
 if(P.Y>6000)
 {
  if(DistrictObjects.IsValidIndex(0)&&Target==DistrictObjects[0])return TEXT("回光埠 · 入口广场");
  if(DistrictObjects.IsValidIndex(1)&&Target==DistrictObjects[1])return TEXT("回光埠 · 退光井");
  if(P.X<-3300)return TEXT("回光埠 · 夜班宿舍");
  if(P.X>1600&&P.Y>11500)return TEXT("回光埠 · 交易厂房");
  return TEXT("回光埠 · 回收与检验区");
 }
 if(P.X>1000)return TEXT("恒昼区 · 访客步道");
 if(P.X>-4200&&P.X<-3400&&P.Y>1300&&P.Y<2300)return TEXT("旧光里 · 林家一层");
 if(P.X<-5200)return TEXT("旧光里 · 诊疗深巷");
 if(P.X<-4400&&P.Y>1200&&P.Y<2200)return TEXT("旧光里 · 互助厨房");
 if(P.Y<-2500&&P.X<-2400)return TEXT("旧光里 · 后巷计量庭");
 if(P.Y>2500&&P.X<-3300)return TEXT("旧光里 · 维修铺");
 if(P.X<-3300)return TEXT("旧光里 · 公共院落");
 return TEXT("旧站 · 服务与控制区");
}

void AStationHUD::LoadNavigationPortraits()
{
 const FString MapFile=FPaths::Combine(FPaths::ProjectContentDir(),TEXT("BZ/NavigationV19/Maps/CommunityMap.png"));
 NavigationBasemap=FPaths::FileExists(MapFile)?FImageUtils::ImportFileAsTexture2D(MapFile):nullptr;
 const TCHAR* Roles[]={TEXT("LinZhao"),TEXT("YeCen"),TEXT("LinHe"),TEXT("ZhouChi"),TEXT("ShenYan"),TEXT("LuoShen"),TEXT("AZhan")};
 for(const auto* PortraitRole:Roles)
 {
  const FString File=FPaths::Combine(FPaths::ProjectContentDir(),TEXT("BZ/NavigationV19/Portraits"),FString::Printf(TEXT("T_N19_%s.png"),PortraitRole));
  UTexture2D* Texture=FPaths::FileExists(File)?FImageUtils::ImportFileAsTexture2D(File):nullptr;
  if(!Texture)Texture=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/BZ/NavigationV19/T_N19_%s.T_N19_%s"),PortraitRole,PortraitRole));
  NavigationPortraits.Add(Texture);
 }
}

FString AStationHUD::GetNavigationStateJson(AActor* Target)
{
 auto* D=Cast<AStationDirector>(UGameplayStatics::GetActorOfClass(GetWorld(),AStationDirector::StaticClass()));
 if(!D)return TEXT("{}");
 RefreshNavigation(D,Target);
 TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();
 Root->SetBoolField(TEXT("verified"),NavigationVerified);
 Root->SetStringField(TEXT("target"),NavigationTarget.IsValid()?NavigationTarget->GetName():TEXT(""));
 TArray<TSharedPtr<FJsonValue>> Points;
 for(const FVector& P:NavigationRoute){TArray<TSharedPtr<FJsonValue>> Values;Values.Add(MakeShared<FJsonValueNumber>(P.X));Values.Add(MakeShared<FJsonValueNumber>(P.Y));Values.Add(MakeShared<FJsonValueNumber>(P.Z));Points.Add(MakeShared<FJsonValueArray>(Values));}
 Root->SetArrayField(TEXT("points"),Points);
 FString Result;auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(Root,Writer);return Result;
}

void AStationHUD::RefreshNavigation(AStationDirector* D,AActor* TargetOverride)
{
 APawn* Pawn=PlayerOwner?PlayerOwner->GetPawn():nullptr;if(!Pawn)return;
 AActor* Guide=IsValid(TargetOverride)?TargetOverride:D->GetGuidanceTarget();const float Now=GetWorld()->GetTimeSeconds();
 if(!IsValid(Guide)){NavigationRoute.Empty();NavigationTarget.Reset();NavigationVerified=false;return;}
 const FVector Here=Pawn->GetActorLocation(),Target=Guide->GetActorLocation();
 if(!StationNavigation::Replan(NavigationTarget.Get()==Guide,Now-NavigationUpdated,FVector::Dist2D(Here,NavigationStart),FVector::Dist2D(Target,NavigationGoal)))return;
 NavigationUpdated=Now;NavigationTarget=Guide;NavigationGoal=Target;NavigationStart=Here;NavigationRoute.Empty();NavigationVerified=false;
 FVector Goal=Target;
 if(auto* Object=Cast<AStationObject>(Guide))Goal+=Object->GetActorRightVector()*72.f;
 else Goal+=(Here-Target).GetSafeNormal2D()*145.f;
 auto* Character=Cast<ACharacter>(Pawn);const float Half=Character?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():88.f;
 const float Radius=Character?FMath::Max(30.f,Character->GetCapsuleComponent()->GetScaledCapsuleRadius()):34.f;
 const auto Shape=FCollisionShape::MakeCapsule(Radius,FMath::Max(Radius,Half-18.f));
 FCollisionQueryParams Q(SCENE_QUERY_STAT(StoryNavigation),false,Pawn);Q.AddIgnoredActor(Guide);Q.AddIgnoredActor(D);
 FCollisionQueryParams MovementQ(SCENE_QUERY_STAT(StoryNavigationBody),false,Pawn);MovementQ.AddIgnoredActor(D);
 // Prefer the engine's walkable navigation data when the level provides it.
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
 {
  FNavLocation From,To;
  if(Nav->ProjectPointToNavigation(Here,From,FVector(100,100,180))&&Nav->ProjectPointToNavigation(Goal,To,FVector(120,120,180)))
   if(auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),From.Location,To.Location,Pawn);Path&&Path->IsValid()&&!Path->IsPartial()&&Path->PathPoints.Num()>1)
   {
    bool Clear=true;
    for(int32 I=1;Clear&&I<Path->PathPoints.Num();++I)
    {FHitResult Hit;Clear=!GetWorld()->SweepSingleByChannel(Hit,Path->PathPoints[I-1]+FVector(0,0,Half+5),Path->PathPoints[I]+FVector(0,0,Half+5),FQuat::Identity,ECC_Pawn,Shape,MovementQ);}
    if(Clear){NavigationRoute=Path->PathPoints;NavigationVerified=true;return;}
   }
 }
 // The existing community's authored circulation graph is the fallback. Every
 // traversed edge is swept against the current world, including the kitchen gate.
 // Upper levels without valid navigation intentionally receive bearing-only cues.
 const float Feet=Here.Z-Half;
 auto Ground=[&](FVector P,FVector& Result)
 {
  FHitResult Hit;
  if(!GetWorld()->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Feet+65),FVector(P.X,P.Y,Feet-70),ECC_WorldStatic,Q)||Hit.ImpactNormal.Z<.7f)return false;
  Result=Hit.ImpactPoint+FVector(0,0,Half);return true;
 };
 FVector GroundGoal;if(!Ground(Goal,GroundGoal))return;
 TArray<FVector> Positions={Here,GroundGoal};TArray<bool> Grounded={true,true};
 for(const auto& P:Neighborhood16::RouteNodes){FVector G;const bool OK=Ground(P,G);Positions.Add(OK?G:FVector(P.X,P.Y,Here.Z));Grounded.Add(OK);}
 // Transit spines are shared with the existing NPC routines, not drawn through buildings.
 const FVector Transit[]={FVector(-700,4200,0),FVector(-400,4800,0),FVector(400,4800,0),FVector(-400,5650,0),FVector(400,5650,0),FVector(-400,6500,0),FVector(400,6500,0),FVector(-400,8200,0),FVector(400,8200,0),FVector(-2200,8700,0),FVector(-2200,9800,0),FVector(900,9900,0),FVector(900,11900,0),FVector(2200,12300,0),FVector(-4200,11500,0),FVector(500,11700,0),FVector(1200,10500,0),FVector(4400,2000,15),FVector(4400,800,15),FVector(4400,-400,15),FVector(4400,-1600,15),FVector(4400,-2800,15),FVector(4400,-3350,15)};
 const int32 Extra=Positions.Num();
 for(const auto& P:Transit){FVector G;const bool OK=Ground(P,G);Positions.Add(OK?G:FVector(P.X,P.Y,Here.Z));Grounded.Add(OK);}
 // Reader stands and people remain physical obstacles. Local approach nodes
 // allow a short route around their body instead of drawing through it.
 const int32 ApproachStart=Positions.Num();
 for(const FVector Center:{Here,GroundGoal})for(int32 I=0;I<8;++I)
 {
  const float Angle=I*PI/4;const FVector P=Center+FVector(FMath::Cos(Angle)*185,FMath::Sin(Angle)*185,0);
  FVector G;const bool OK=Ground(P,G);Positions.Add(OK?G:P);Grounded.Add(OK);
 }
 std::vector<StationNavigation::Point> Nodes;for(const FVector& P:Positions)Nodes.push_back(XY(P));
 std::vector<StationNavigation::EdgeLink> Edges;
 for(const auto& E:Neighborhood16::RouteEdges)Edges.push_back({E.X+2,E.Y+2});
 for(int32 I=2;I<Positions.Num();++I)
 {
  if(FVector::Dist2D(Positions[0],Positions[I])<1400)Edges.push_back({0,I});
  if(FVector::Dist2D(Positions[1],Positions[I])<1400)Edges.push_back({1,I});
  if(I>=Extra)for(int32 J=2;J<I;++J)if(FVector::Dist2D(Positions[I],Positions[J])<(I>=ApproachStart?1400:2250))Edges.push_back({I,J});
 }
 if(FVector::Dist2D(Here,Goal)<2200)Edges.push_back({0,1});
 int32 Queries=0;bool Exhausted=false;TMap<uint64,bool> Cache;
 auto Clear=[&](int32 I,int32 J)
 {
  const uint64 Key=(uint64(FMath::Min(I,J))<<32)|uint64(FMath::Max(I,J));if(const bool* Found=Cache.Find(Key))return *Found;
  if(!Grounded[I]||!Grounded[J])return false;
  if(++Queries>320){Exhausted=true;return false;}
  const FVector A=Positions[I],B=Positions[J];FHitResult Hit;
  bool OK=!GetWorld()->SweepSingleByChannel(Hit,A+FVector(0,0,5),B+FVector(0,0,5),FQuat::Identity,ECC_Pawn,Shape,MovementQ);
  const int32 Samples=FMath::CeilToInt(FVector::Dist2D(A,B)/110.f);
  for(int32 K=1;OK&&K<Samples;++K)
  {FVector G;const FVector P=FMath::Lerp(A,B,float(K)/Samples);OK=Ground(P,G)&&FMath::Abs(G.Z-P.Z)<40.f;}
  Cache.Add(Key,OK);return OK;
 };
 const auto Path=StationNavigation::Path(Nodes,Edges,Clear);
 if(!Exhausted&&Path.size()>1){for(int32 I:Path)NavigationRoute.Add(Positions[I]-FVector(0,0,Half-5));NavigationVerified=true;}
}

void AStationHUD::DrawNavigation(AStationDirector* D,float X,float Y,float Width,float Height,bool Expanded)
{
 using namespace StationNavigation;
 APawn* Pawn=PlayerOwner->GetPawn();if(!Pawn)return;
 RefreshNavigation(D);const FVector Player=Pawn->GetActorLocation();
 const StationNavigation::Rect Area{X,Y,Width,Height-38};Point Center=XY(Player);double Cpp=5800./Width;
 if(Expanded)
 {
  const bool District=Player.Y>6000;Center=District?Point{0,11400}:Point{-250,-500};
  Cpp=std::max((District?13600.:15600.)/Width,(District?11400.:13600.)/Area.h);
  if(!Inside(StationNavigation::Project(XY(Player),Center,Area,Cpp),Area,26))Center=XY(Player);
 }
 auto Plot=[&](FVector P){return StationNavigation::Project(XY(P),Center,Area,Cpp);};
 auto Box=[&](double A,double B,double C,double E,FLinearColor Color){DrawRect(Color,A*Unit,B*Unit,C*Unit,E*Unit);};
 auto Stroke=[&](Point A,Point B,FLinearColor C,float Thick=1.f){if(Clip(A,B,Area))DrawLine(A.x*Unit,A.y*Unit,B.x*Unit,B.y*Unit,C,Thick*Unit);};
 auto Circle=[&](Point P,double R,FLinearColor C,float Thick=1.f){for(int32 I=0;I<40;++I){const double A=I*2*PI/40,B=(I+1)*2*PI/40;Stroke({P.x+std::cos(A)*R,P.y+std::sin(A)*R},{P.x+std::cos(B)*R,P.y+std::sin(B)*R},C,Thick);}};
 auto Diamond=[&](Point P,double R,FLinearColor C){Stroke({P.x,P.y-R},{P.x+R,P.y},C,2);Stroke({P.x+R,P.y},{P.x,P.y+R},C,2);Stroke({P.x,P.y+R},{P.x-R,P.y},C,2);Stroke({P.x-R,P.y},{P.x,P.y-R},C,2);};
 auto Portrait=[&](Point P,int32 Id,float R,FLinearColor Color,bool Primary)
 {
  Circle(P,R+3,Color,Primary?3.f:1.5f);Circle(P,R+1,FLinearColor(.025f,.035f,.035f,1),3.f);
  if(NavigationPortraits.IsValidIndex(Id)&&NavigationPortraits[Id]&&NavigationPortraits[Id]->GetResource())
  {
   TArray<FCanvasUVTri> Triangles;
   for(int32 I=0;I<36;++I)
   {
    const double A=I*2*PI/36,B=(I+1)*2*PI/36;FCanvasUVTri T;
    T.V0_Pos=V2(P)*Unit;T.V1_Pos=V2({P.x+std::cos(A)*R,P.y+std::sin(A)*R})*Unit;T.V2_Pos=V2({P.x+std::cos(B)*R,P.y+std::sin(B)*R})*Unit;
    T.V0_UV=FVector2D(.5,.5);T.V1_UV=FVector2D(.5+std::cos(A)*.5,.5+std::sin(A)*.5);T.V2_UV=FVector2D(.5+std::cos(B)*.5,.5+std::sin(B)*.5);
    T.V0_Color=T.V1_Color=T.V2_Color=FLinearColor::White;Triangles.Add(T);
   }
   FCanvasTriangleItem Item(Triangles,NavigationPortraits[Id]->GetResource());Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
  }
  else
  {
   // A missing portrait remains a named marker, never a silent empty location.
   const TCHAR* Names[]={TEXT("照"),TEXT("岑"),TEXT("禾"),TEXT("驰")};Text(Id>=0&&Id<4?Names[Id]:TEXT("人"),P.x-8,P.y-12,18,Color);
  }
 };
 Box(X-5,Y-5,Width+10,Height+10,FLinearColor(.008f,.014f,.016f,.6f));
 Box(X-1,Y-1,Width+2,Height+2,FLinearColor(.40f,.39f,.30f,.9f));Box(X,Y,Width,Height,FLinearColor(.022f,.037f,.040f,.98f));
 if(NavigationBasemap&&NavigationBasemap->GetResource())
 {
  constexpr double MinX=-8500,MinY=-7500,MaxX=8500,MaxY=16500;
  const double Left=Center.x-Area.w*.5*Cpp,Top=Center.y-Area.h*.5*Cpp;
  const double L=std::max(Left,MinX),T=std::max(Top,MinY),R=std::min(Left+Area.w*Cpp,MaxX),B=std::min(Top+Area.h*Cpp,MaxY);
  if(R>L&&B>T)
  {
   FCanvasTileItem Item(FVector2D(Area.x+(L-Left)/Cpp,Area.y+(T-Top)/Cpp)*Unit,NavigationBasemap->GetResource(),FVector2D((R-L)/Cpp,(B-T)/Cpp)*Unit,FLinearColor::White);
   Item.UV0=FVector2D((L-MinX)/(MaxX-MinX),(T-MinY)/(MaxY-MinY));Item.UV1=FVector2D((R-MinX)/(MaxX-MinX),(B-MinY)/(MaxY-MinY));Item.BlendMode=SE_BLEND_Opaque;Canvas->DrawItem(Item);
  }
 }
 else for(const auto& O:Navigation19::Outlines)
 {
  Point A=Plot(FVector(O.Bounds.X,O.Bounds.Y,0)),B=Plot(FVector(O.Bounds.Z,O.Bounds.W,0));
  const double L=std::max(A.x,Area.x),T=std::max(A.y,Area.y),R=std::min(B.x,Area.x+Area.w),Btm=std::min(B.y,Area.y+Area.h);
  const FLinearColor C=O.Zone==1?FLinearColor(.22f,.24f,.20f,1):O.Zone==2?FLinearColor(.16f,.21f,.22f,1):FLinearColor(.15f,.19f,.19f,1);
  if(R>L&&Btm>T)Box(L,T,R-L,Btm-T,C);
  Stroke(A,{B.x,A.y},NavDim.CopyWithNewOpacity(.24f));Stroke({B.x,A.y},B,NavDim.CopyWithNewOpacity(.24f));Stroke(B,{A.x,B.y},NavDim.CopyWithNewOpacity(.24f));Stroke({A.x,B.y},A,NavDim.CopyWithNewOpacity(.24f));
 }
 // Only a verified walkable route gets a solid line. Direction-only fallback
 // keeps the objective marker but never invents a line across a building.
 if(D->SceneGuides&&NavigationVerified)for(int32 I=1;I<NavigationRoute.Num();++I)
 {Stroke(Plot(NavigationRoute[I-1]),Plot(NavigationRoute[I]),FLinearColor(.025f,.03f,.028f,1),6);Stroke(Plot(NavigationRoute[I-1]),Plot(NavigationRoute[I]),NavGold,2.5f);}
 const auto Pins=D->GetNavigationPins();
 TArray<Point> Occupied;
 // The player is reserved first, then the active story target; remaining pins
 // receive short leader lines if crowded. World positions are never changed.
 Point Self=Plot(Player);Occupied.Add(Self);
 auto Pin=[&](const FStationMapPin& P)
 {
  if(!Expanded&&!P.Primary&&P.Portrait>=0&&FVector::Dist2D(Player,P.Position)>1100)return;
  const Point Actual=Plot(P.Position);const float R=P.Portrait>=0?(P.Primary?15.f:11.f):5.f;
  const bool Outside=!Inside(Actual,Area,R+7);
  if(Outside&&!P.Primary)return;
  Point Pos=Outside?Edge(Actual,Area,R+8):Actual;
  if(!Outside)
  {
   const Point Original=Pos;
   for(int32 Trial=0;Trial<16;++Trial)
   {
    bool Free=true;for(auto Other:Occupied)if(Length(Pos-Other)<R+23){Free=false;break;}if(Free)break;
    const double Angle=Trial*2.399963;Pos=Edge(Original+Point{std::cos(Angle)*34,std::sin(Angle)*34},Area,R+8);
   }
   if(Length(Pos-Original)>2){Stroke(Actual,Pos,NavDim,1);Diamond(Actual,2,NavDim);}
  }
  Occupied.Add(Pos);const FLinearColor C=P.Primary?NavGold:P.Optional?NavTeal:NavDim;
  if(P.Portrait>=0)Portrait(Pos,P.Portrait,R,C,P.Primary);else{Diamond(Pos,R,C);if(P.Discovered)Stroke({Pos.x-3,Pos.y},{Pos.x,Pos.y+3},C);}
  if(Outside)
  {
   const Point Direction=StationNavigation::Unit(Actual-Pos),Side{-Direction.y,Direction.x},Tip=Pos+Direction*(R+8);
   Stroke(Tip,Pos+Direction*(R+1)+Side*4,NavGold,2);Stroke(Tip,Pos+Direction*(R+1)-Side*4,NavGold,2);
  }
  if(P.Primary)
  {
   if(P.Portrait>=0)Diamond({Pos.x,Pos.y-R-13},4,NavGold);
   const FString HeightHint=P.Position.Z-Player.Z>190?TEXT(" ↑ 上层"):P.Position.Z-Player.Z<-190?TEXT(" ↓ 下层"):TEXT("");
   const float LabelY=Pos.y+R+8<Y+Area.h-20?Pos.y+R+8:Pos.y-R-28;
   const FString Label=(Outside?TEXT("前往 "):TEXT(""))+P.Name.Left(P.Portrait>=0?5:9)+HeightHint;
   const float LabelWidth=FMath::Max(60.f,Label.Len()*15.f);
   const float LabelX=FMath::Clamp(float(Pos.x-36),X+7,X+Width-LabelWidth-7);
   Text(Label,LabelX,LabelY,15,NavGold);Occupied.Add({LabelX+LabelWidth*.5,LabelY+9});
  }
 };
 for(const auto& P:Pins)if(P.Primary)Pin(P);for(const auto& P:Pins)if(!P.Primary)Pin(P);
 Portrait(Self,0,13,FLinearColor(.67f,.82f,.86f,1),true);
 const Point Facing=StationNavigation::Unit(XY(Pawn->GetActorForwardVector())),Tip=Self+Facing*29,Side{-Facing.y,Facing.x};
 Stroke(Tip,Self+Facing*21+Side*5,NavPaper,2);Stroke(Tip,Self+Facing*21-Side*5,NavPaper,2);
 Text(TEXT("北"),X+Width-27,Y+9,15,NavPaper);Stroke({X+Width-20,Y+34},{X+Width-20,Y+44},NavDim);
 const double ScaleMeters=Expanded?20:10,Bar=ScaleMeters*100/Cpp;
 DrawLine((X+12)*Unit,(Y+Height-13)*Unit,(X+12+Bar)*Unit,(Y+Height-13)*Unit,NavDim,2*Unit);Text(FString::Printf(TEXT("%.0f m"),ScaleMeters),X+12,Y+Height-34,12,NavDim);
 if(!Expanded)
 {
  Text(Player.Y>6000?TEXT("回光埠"):Player.X>1000?TEXT("恒昼区"):TEXT("旧光里"),X,Y-30,17,NavPaper);
  Text(TEXT("M  展开地图"),X+Width-113,Y-28,15,NavDim);
 }
}

void AStationHUD::DrawNavigationInstruction(AStationDirector* D,float X,float Y,float Width)
{
 APawn* Pawn=PlayerOwner->GetPawn();AActor* Guide=D->GetGuidanceTarget();if(!Pawn||!IsValid(Guide))return;
 RefreshNavigation(D);const FVector Here=Pawn->GetActorLocation(),Goal=Guide->GetActorLocation();
 FString Name=Cast<AStationEngineer>(Guide)?Cast<AStationEngineer>(Guide)->PersonName:Cast<AStationObject>(Guide)?Cast<AStationObject>(Guide)->DisplayName:TEXT("剧情目标");
 double Distance=FVector::Dist2D(Here,Goal)/100.;FVector Next=Goal;
 if(NavigationVerified&&NavigationRoute.Num()>1)
 {
  int32 First=1;while(First+1<NavigationRoute.Num()&&FVector::Dist2D(Here,NavigationRoute[First])<145)++First;
  Next=NavigationRoute[First];Distance=FVector::Dist2D(Here,Next)/100.;for(int32 I=First+1;I<NavigationRoute.Num();++I)Distance+=FVector::Dist2D(NavigationRoute[I-1],NavigationRoute[I])/100.;
 }
 const bool Focused=D->Focus.Get()==Guide,Visible=D->CanSeeTarget(Guide),SameFloor=FMath::Abs(Goal.Z-Here.Z)<190;
 const bool Arrive=StationNavigation::Arrived(Focused,FVector::Dist2D(Here,Goal),Visible,SameFloor);
 FString Cue;
 if(Focused)Cue=TEXT("E  ")+D->GetContextPrompt().RightChop(3);
 else if(Arrive)Cue=TEXT("已到目标附近 · 面向目标按 E");
 else if(!SameFloor&&FVector::Dist2D(Here,Goal)<700)Cue=Goal.Z>Here.Z?TEXT("↑  目标在上层 · 寻找楼梯入口"):TEXT("↓  目标在下层 · 沿坡道进入");
 else
 {
  const auto Dir=StationNavigation::Direction(StationNavigation::SignedBearing(XY(PlayerOwner->GetControlRotation().Vector()),XY(Next-Here)));
  Cue=Dir==StationNavigation::Turn::Left?TEXT("←  向左前进"):Dir==StationNavigation::Turn::Right?TEXT("→  向右前进"):Dir==StationNavigation::Turn::Behind?TEXT("↶  转身沿通道前进"):TEXT("↑  沿通道前进");
  if(!NavigationVerified)Cue+=TEXT(" · 路线待确认");
 }
 DrawRect(FLinearColor(.008f,.016f,.018f,.91f),X*Unit,Y*Unit,Width*Unit,118*Unit);
 DrawRect(NavGold,X*Unit,Y*Unit,3*Unit,118*Unit);
 Text(D->GetNavigationPlace(Guide),X+17,Y+12,17,NavDim);
 Text(Cue,X+17,Y+41,22,NavPaper);
 Text(FString::Printf(TEXT("%s  ·  %s %.0f m"),*Name.Left(16),NavigationVerified?TEXT("路径"):TEXT("方位距"),Distance),X+17,Y+80,17,NavGold);
}
