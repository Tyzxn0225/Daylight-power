#include "StationPrototype.h"
#include "StationNeighborhoodData.inl"
#include "StationExplorationPolicy.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/RectLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

FVector AStationDirector::NeighborhoodPoint(const TCHAR* Key,FVector Fallback) const
{
    return NeighborhoodReady?Neighborhood16::Point(Key,Fallback):Fallback;
}
void AStationDirector::CreateNeighborhood()
{
    TArray<UStaticMesh*> Meshes;TArray<AStaticMeshActor*> Replaced;
    // Resolve every dependency before changing any existing facade or circulation.
    for(const auto& Row:Neighborhood16::Meshes)
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/BZ/ExplorationV16/Geometry/%s.%s"),Row.Id,Row.Id));
        if(!Mesh){UE_LOG(LogTemp,Warning,TEXT("N16 assets not imported; original community retained"));return;}
        AStaticMeshActor* Original=nullptr;
        if(!FString(Row.Replace).IsEmpty())
        {
            for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)if(It->ActorHasTag(FName(Row.Replace))){Original=*It;break;}
            if(!Original){UE_LOG(LogTemp,Warning,TEXT("N16 original actor missing: %s; original community retained"),Row.Replace);return;}
        }
        Meshes.Add(Mesh);Replaced.Add(Original);
    }
    for(int32 I=0;I<Meshes.Num();++I)
    {
        const auto& Row=Neighborhood16::Meshes[I];AStaticMeshActor* Actor=Replaced[I];
        const bool Gate=FString(Row.Id)==TEXT("SM_N16_KitchenGate");
        if(!Actor)Actor=GetWorld()->SpawnActor<AStaticMeshActor>(FVector::ZeroVector,FRotator::ZeroRotator);
        auto* Component=Actor->GetStaticMeshComponent();Component->SetMobility(EComponentMobility::Movable);Component->SetStaticMesh(Meshes[I]);
        Component->SetCollisionProfileName(Row.Blocking?TEXT("BlockAll"):TEXT("NoCollision"));
        if(Gate)
        {
            // The mesh is baked in world coordinates; a separate pivot keeps the hinge real.
            KitchenGate=GetWorld()->SpawnActor<AActor>(FVector(-4857.5f,1298.f,19.f),FRotator::ZeroRotator);
            auto* Pivot=NewObject<USceneComponent>(KitchenGate);KitchenGate->AddInstanceComponent(Pivot);KitchenGate->SetRootComponent(Pivot);Pivot->RegisterComponent();Pivot->SetWorldLocation(FVector(-4857.5f,1298.f,19.f));
            Actor->AttachToActor(KitchenGate,FAttachmentTransformRules::KeepWorldTransform);
        }
    }
    NeighborhoodReady=true;
    for(const auto& F:Neighborhood16::Fixtures)
    {
        // Service allocation controls the clinic/kitchen lights separately.
        const FString Key(F.Key);if(Key.Contains(TEXT("KitchenTask"))||Key.Contains(TEXT("ClinicFunctional")))continue;
        auto* Lamp=GetWorld()->SpawnActor<ARectLight>(F.Position,FRotator(-90,0,0));auto* Component=CastChecked<URectLightComponent>(Lamp->GetLightComponent());Component->SetMobility(EComponentMobility::Movable);
        Component->SetIntensityUnits(ELightUnits::Lumens);Component->SetIntensity(F.Lumens);Component->SetLightColor(F.Color);Component->SetSourceWidth(F.Width);Component->SetSourceHeight(F.Height);Component->SetAttenuationRadius(Key.Contains(TEXT("Family"))?210.f:650.f);
    }
    if(KitchenGate&&Progress->KitchenShortcutOpen)MoveKitchenGate(1,0);
}
void AStationDirector::MoveKitchenGate(float DesiredAlpha,float Delta)
{
    if(!IsValid(KitchenGate))return;
    const float Next=Delta>0?FMath::FInterpConstantTo(KitchenGateAlpha,DesiredAlpha,Delta,.85f):DesiredAlpha;
    if(FMath::IsNearlyEqual(Next,KitchenGateAlpha))return;
    TArray<AActor*> Leaves;KitchenGate->GetAttachedActors(Leaves,true,true);
    const FTransform CurrentPivot=KitchenGate->GetActorTransform();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KitchenGateSafety),false,this);
    // Rotation cannot use the character's translational movement sweep. Check
    // the physical leaf along its arc and wait until people leave that arc.
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FMath::Abs(Next-KitchenGateAlpha)*100.f/2.f));
    for(int32 I=1;I<=Steps;++I)
    {
        const float Alpha=FMath::Lerp(KitchenGateAlpha,Next,float(I)/Steps);
        // Open inward so the leaf does not sweep through the delivery queue.
        const FTransform Pivot(FRotator(0,100*FMath::InterpEaseInOut(0.f,1.f,Alpha,2.f),0),CurrentPivot.GetLocation());
        for(AActor* Leaf:Leaves)
        {
            TArray<UStaticMeshComponent*> Parts;Leaf->GetComponents(Parts);
            for(UStaticMeshComponent* Part:Parts)
            {
                if(Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision)continue;
                FVector Min,Max;Part->GetLocalBounds(Min,Max);
                const FTransform Pose=Part->GetComponentTransform().GetRelativeTransform(CurrentPivot)*Pivot;
                const FVector Center=Pose.TransformPosition((Min+Max)*.5f);
                const FVector Extent=(Max-Min)*.5f*Pose.GetScale3D().GetAbs()+FVector(3);
                if(GetWorld()->OverlapAnyTestByObjectType(Center,Pose.GetRotation(),FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeBox(Extent),Query))return;
            }
        }
    }
    KitchenGateAlpha=Next;
    KitchenGate->SetActorRotation(FRotator(0,100*FMath::InterpEaseInOut(0.f,1.f,Next,2.f),0));
}
void AStationDirector::CreateExplorationObjects()
{
    if(!NeighborhoodReady)return;
    for(int32 I=0;I<UE_ARRAY_COUNT(Neighborhood16::Memories);++I)
    {
        const auto& M=Neighborhood16::Memories[I];FTransform T(M.Position);
        auto* Object=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Object->Kind=EStationObjectKind::NeighborhoodMemory;Object->DisplayName=M.Name;Object->UseExistingGeometry=true;Object->InteractionOffset=FVector(0,18,130);Object->FinishSpawning(T);ExplorationObjects.Add(Object);Objects.Add(Object);
    }
    auto Spawn=[&](const TCHAR* Key,EStationObjectKind Kind,const TCHAR* Name)
    {
        FTransform T(Neighborhood16::Point(Key,FVector::ZeroVector));auto* Object=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Object->Kind=Kind;Object->DisplayName=Name;Object->FinishSpawning(T);Objects.Add(Object);return Object;
    };
    FieldRadio=Spawn(TEXT("FieldRadio"),EStationObjectKind::FieldRadio,TEXT("冷却端检修电台 · 联系叶岑"));
    FieldDesk=Spawn(TEXT("FieldDesk"),EStationObjectKind::Terminal,TEXT("院内公共事务台 · 与旧站联网"));
}
void AStationDirector::TickNeighborhood(float Delta)
{
    if(!NeighborhoodReady||!Progress)return;
    if(FieldDesk){FieldDesk->Active=Progress->Started;FieldDesk->SetActorHiddenInGame(!Progress->Started);FieldDesk->SetActorEnableCollision(Progress->Started);}
    if(FieldRadio){FieldRadio->Active=IsCrisisChapter()||IsArcChapter()||(!Progress->Started&&Progress->Connections==7&&!Progress->HeardRisk);}
    if(KitchenGate&&!IsArcChapter()&&Progress->KitchenShortcutOpen&&KitchenGateAlpha<1)
    {
        MoveKitchenGate(1,Delta);
    }
    for(auto* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(Person)Person->NeighborhoodRoutes=true;
    if(!Progress->Started&&Progress->HasConnector&&Engineer&&!Engineer->ConversationPaused&&!WorkYieldPeople.Contains(Engineer))
        Engineer->SetCommunityTask(Neighborhood16::Point(TEXT("NodeB"),FVector::ZeroVector)+FVector(-90,90,0),TEXT("前往冷却端复核风险 · 电台可联系"),TEXT("Idle"));
    if(!PC||!PC->GetPawn())return;
    const FVector P=PC->GetPawn()->GetActorLocation();
    const bool Home=P.X>-4160&&P.X<-3460&&P.Y>1340&&P.Y<2180&&P.Z<220;
    const bool Quiet=!IsArcChapter()&&QuietIndex<0&&Home&&Progress->Started&&!(Progress->NeighborhoodFilmMask&1)&&!IsMenuOpen()&&!IsPhysicalActionActive()&&!Starting&&!IsStoryFilmActive()&&!IsVoicePlaying()&&PC->GetPawn()->GetVelocity().Size2D()<10;
    HomeQuietTime=Quiet?HomeQuietTime+Delta:0;
    if(HomeQuietTime>1.5f&&StoryAutoplayEnabled()){HomeQuietTime=0;StartStoryFilm(TEXT("C13"));}
}
bool AStationDirector::HandleNeighborhoodObject(AStationObject* Object)
{
    if(!NeighborhoodReady)return false;
    if(Object==FieldDesk&&Progress->Started)
    {
        if(!Progress->ReadRecall&&Progress->AllocationPolicy>=0&&Progress->AllocationReviewMask!=7)return BeginCivicReview(0);
        if(!Progress->ReadRecall&&IsRationChapter()&&Progress->RationMask>=0&&Progress->RationVisitMask==7&&Progress->RationReviewMask!=7)return BeginCivicReview(1);
        if(IsRationChapter()&&Progress->RationMask<0&&Progress->RequestReadMask==7&&!Progress->RationBriefed)
        {
            ClosePanel();RemoteConversation=true;ConversationTarget=Engineer;ConversationLine=0;RiskConversation=false;OpenPanel(EStationPanel::Conversation);PlayCue(TEXT("FX_Relay"),FVector::ZeroVector,.16f,false);return true;
        }
    }
    if(Object==FieldRadio)
    {
        if(Progress->Connections!=7||Progress->HeardRisk)return true;
        ClosePanel();RemoteConversation=true;ConversationTarget=Engineer;ConversationLine=0;RiskConversation=true;OpenPanel(EStationPanel::Conversation);return true;
    }
    const int32 Index=ExplorationObjects.IndexOfByKey(Object);if(Index<0)return false;
    ActiveExploration=Index;
    if(Index==5&&!Progress->KitchenShortcutOpen){Progress->KitchenShortcutOpen=true;SaveProgress();PlayCue(TEXT("FX_Relay"),Object->GetActorLocation(),.2f,true);}
    OpenPanel(EStationPanel::Exploration);return true;
}
bool AStationDirector::BeginCivicReview(int32 Phase)
{
    if(!NeighborhoodReady||!FieldDesk||!Progress||Phase<0||Phase>1)return false;
    if(!StationExplorationPolicy::CanReview(Phase,Progress->AllocationPolicy,Progress->RationMask,Progress->ReadRecall))return false;
    const int32 Mask=Phase==0?Progress->AllocationReviewMask:Progress->RationReviewMask;
    const int32 ReviewPersonIndex=StationExplorationPolicy::NextReviewRole(Mask);
    AStationEngineer* Person=ReviewPersonIndex==0?LinHe.Get():ReviewPersonIndex==1?Engineer.Get():ReviewPersonIndex==2?ZhouChi.Get():nullptr;
    if(!Person)return false;
    ClosePanel();CivicReviewPhase=Phase;RemoteConversation=true;ConversationTarget=Person;ConversationLine=0;RiskConversation=false;OpenPanel(EStationPanel::Conversation);PlayCue(TEXT("FX_Relay"),FVector::ZeroVector,.16f,false);return true;
}
FString AStationDirector::GetExplorationBody() const
{
    return ActiveExploration>=0&&ActiveExploration<UE_ARRAY_COUNT(Neighborhood16::Memories)?FString(Neighborhood16::Memories[ActiveExploration].Body):TEXT("");
}
FString AStationDirector::GetExplorationThought() const
{
    return ActiveExploration>=0&&ActiveExploration<UE_ARRAY_COUNT(Neighborhood16::Memories)?FString(Neighborhood16::Memories[ActiveExploration].Thought):TEXT("");
}
void AStationDirector::CloseExploration()
{
    const int32 Index=ActiveExploration;if(Index>=0&&Index<6){Progress->ExplorationSeenMask|=1<<Index;SaveProgress();}
    ClosePanel();ActiveExploration=-1;
    if(Index==4&&!(Progress->NeighborhoodFilmMask&4)&&StoryAutoplayEnabled())StartStoryFilm(TEXT("C15"));
}
void AStationDirector::RequestNeighborhoodMap()
{
    if(IsStoryFilmActive()||Starting||IsPhysicalActionActive())return;
    if(Panel==EStationPanel::NeighborhoodMap){ClosePanel();return;}
    if(!NeighborhoodReady){Notify(TEXT("社区内街图暂不可用。"));return;}
    if(!IsMenuOpen())OpenPanel(EStationPanel::NeighborhoodMap);
}
AActor* AStationDirector::GetGuidanceTarget() const
{
    if(AActor* Survey=GetSurveyGuidance())return Survey;
    if(IsArcChapter())return GetArcGuidance();
    AActor* Target=GetBaseGuidanceTarget();
    if(NeighborhoodReady)
    {
        if(FieldDesk&&Progress->Started)
        {
            if(!Progress->ReadRecall&&Progress->AllocationPolicy>=0&&Progress->AllocationReviewMask!=7)return FieldDesk;
            if(!Progress->ReadRecall&&IsRationChapter()&&((Progress->RequestReadMask==7&&!Progress->RationBriefed)||(Progress->RationMask>=0&&Progress->RationVisitMask==7&&Progress->RationReviewMask!=7)))return FieldDesk;
        }
        if(Target==Engineer&&FieldRadio&&FieldRadio->Active)return FieldRadio;
        if(auto* Object=Cast<AStationObject>(Target))if(Object->Kind==EStationObjectKind::Terminal&&Progress->Started&&FieldDesk)return FieldDesk;
    }
    return Target;
}
FString AStationDirector::GetNeighborhoodStateJson() const
{
    return FString::Printf(TEXT("{\"ready\":%s,\"exploration_seen\":%d,\"film_seen\":%d,\"shortcut_open\":%s,\"gate_alpha\":%.3f,\"remote_conversation\":%s,\"memories\":%d}"),NeighborhoodReady?TEXT("true"):TEXT("false"),Progress?Progress->ExplorationSeenMask:0,Progress?Progress->NeighborhoodFilmMask:0,Progress&&Progress->KitchenShortcutOpen?TEXT("true"):TEXT("false"),KitchenGateAlpha,RemoteConversation?TEXT("true"):TEXT("false"),ExplorationObjects.Num());
}
