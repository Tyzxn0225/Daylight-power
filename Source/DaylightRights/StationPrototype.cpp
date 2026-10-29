#include "StationPrototype.h"
#include "StationBackgroundFilm.h"
#include "UnrealClient.h"
#include "StationExplorationPolicy.h"
#include "StationExperiencePolicy.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/RectLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "CanvasItem.h"
#include "StationMovementTuning.h"
#include "StationFrontend.h"
#include "StationAchievementPolicy.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

namespace Station
{
    const TCHAR* Slot=TEXT("DaylightRights_Station_v1");
    const FLinearColor Gold(1.f,.64f,.26f,1.f), White(.92f,.91f,.86f,1.f), Muted(.58f,.63f,.65f,1.f);
    USoundBase* Sound(const TCHAR* Name)
    {
        if(FString(Name).StartsWith(TEXT("VO_")))return nullptr;
        return LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/BZ/Audio/%s.%s"),Name,Name));
    }
    UMaterialInstanceDynamic* Material(UStaticMeshComponent* Mesh,const FLinearColor& Color,float Glow)
    {
        auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_StationDevice.M_StationDevice"));
        if(!Parent)return nullptr;
        auto* Mat=UMaterialInstanceDynamic::Create(Parent,Mesh);
        Mat->SetVectorParameterValue(TEXT("Tint"),Color);Mat->SetScalarParameterValue(TEXT("Glow"),Glow);
        Mesh->SetMaterial(0,Mat);return Mat;
    }
    void Cube(UStaticMeshComponent* Mesh,FVector Location,FVector Scale)
    {
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetRelativeLocation(Location);Mesh->SetRelativeScale3D(Scale);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

AStationObject::AStationObject()
{
    Anchor=CreateDefaultSubobject<USceneComponent>(TEXT("InteractionAnchor"));SetRootComponent(Anchor);
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceBody"));Body->SetupAttachment(Anchor);
    Screen=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusScreen"));Screen->SetupAttachment(Anchor);
    Collision=CreateDefaultSubobject<UBoxComponent>(TEXT("DeviceCollision"));Collision->SetupAttachment(Anchor);
    Collision->SetRelativeLocation(FVector(0,0,60));Collision->SetBoxExtent(FVector(30,17,60));
    Collision->SetCollisionProfileName(TEXT("BlockAll"));
}
void AStationObject::BeginPlay()
{
    Super::BeginPlay();
    Station::Cube(Body,FVector(0,0,60),FVector(.56,.28,1.2));
    Station::Material(Body,FLinearColor(.028,.041,.049),0);
    Station::Cube(Screen,FVector(0,15.5,94),FVector(.43,.022,.3));
    ScreenMaterial=Station::Material(Screen,FLinearColor(.95,.33,.045),2.5);
    if(Kind==EStationObjectKind::Notice||Kind==EStationObjectKind::AllocationLedger||Kind==EStationObjectKind::WorkAccount||IsServiceRequest())
    {
        Body->SetRelativeLocation(FVector(0,0,116));Body->SetRelativeScale3D(FVector(.92,.08,.96));
        Screen->SetRelativeLocation(FVector(0,5,118));Screen->SetRelativeScale3D(FVector(.84,.02,.84));
        Collision->SetRelativeLocation(FVector(0,0,82));Collision->SetBoxExtent(FVector(46,6,82));
        for(int32 Side : {-1,1})
        {
            auto* Leg=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Leg);Leg->SetupAttachment(Anchor);
            Station::Cube(Leg,FVector(Side*32,0,34),FVector(.045,.045,.68));Leg->RegisterComponent();Station::Material(Leg,FLinearColor(.028,.041,.049),0);
            auto* Foot=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Foot);Foot->SetupAttachment(Anchor);
            Station::Cube(Foot,FVector(Side*32,0,1.25),FVector(.16,.22,.025));Foot->RegisterComponent();Station::Material(Foot,FLinearColor(.028,.041,.049),0);
        }
        if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_StationNotice.M_StationNotice")))Screen->SetMaterial(0,Mat);
    }
    if(Kind==EStationObjectKind::Connector||Kind==EStationObjectKind::ConsignmentCase||Kind==EStationObjectKind::SupplierReturn||Kind==EStationObjectKind::SupplyParcel)
    {
        Body->SetRelativeLocation(FVector(0,0,35.5));Body->SetRelativeScale3D(FVector(.6,.42,.5));
        Screen->SetRelativeLocation(FVector(0,22,51.5));Screen->SetRelativeScale3D(FVector(.3,.02,.1));
        Collision->SetRelativeLocation(FVector(0,0,32));Collision->SetBoxExtent(FVector(30,21,32));
        auto* Stand=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Stand);Stand->SetupAttachment(Anchor);
        Station::Cube(Stand,FVector(0,0,5.25),FVector(.54,.36,.105));Stand->RegisterComponent();Station::Material(Stand,FLinearColor(.028,.041,.049),0);
    }
    if(IsServiceRequest()&&!Indoor)
    {
        auto Add=[this](FVector Location,FVector Scale)
        {
            auto* Mesh=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Mesh);Mesh->SetupAttachment(Anchor);
            Station::Cube(Mesh,Location,Scale);Mesh->RegisterComponent();return Mesh;
        };
        auto* Mast=Add(FVector(0,-9,150),FVector(.07,.07,3));Station::Material(Mast,FLinearColor(.04,.055,.06),0);
        auto* Shade=Add(FVector(0,40,300),FVector(1.8,1.1,.08));Station::Material(Shade,FLinearColor(.04,.055,.06),0);
        auto* Lamp=Add(FVector(0,40,294),FVector(1.65,.95,.035));ServiceLampMaterial=Station::Material(Lamp,FLinearColor(1,.72,.38),0);
    }
    if(Kind==EStationObjectKind::HouseholdArchive)
        if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_HouseholdReceipt.M_HouseholdReceipt")))Screen->SetMaterial(0,Mat);
    if(Kind==EStationObjectKind::WorkAccount)
        if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_WorkAccount.M_WorkAccount")))Screen->SetMaterial(0,Mat);
    if(UseExistingGeometry)
    {
        Body->SetVisibility(false);Screen->SetVisibility(false);Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
FVector AStationObject::GetInteractionPoint() const {return GetActorTransform().TransformPosition(!InteractionOffset.IsNearlyZero()?InteractionOffset:FVector(0,20,(Kind==EStationObjectKind::Notice||Kind==EStationObjectKind::AllocationLedger||Kind==EStationObjectKind::WorkAccount||IsServiceRequest())?118:95));}
FVector AStationObject::GetInteractionStandPoint() const {return GetActorTransform().TransformPositionNoScale(InteractionStandOffset);}
void AStationObject::SetServicePower(bool Powered)
{
    if(ServiceLampMaterial)ServiceLampMaterial->SetScalarParameterValue(TEXT("Glow"),Powered?60.f:0.f);
}
void AStationObject::SetIndicator(const FLinearColor& Color,float Glow)
{
    if(ScreenMaterial){ScreenMaterial->SetVectorParameterValue(TEXT("Tint"),Color);ScreenMaterial->SetScalarParameterValue(TEXT("Glow"),Glow);}
}

AStationEngineer::AStationEngineer()
{
    PrimaryActorTick.bCanEverTick=true;
    GetCapsuleComponent()->InitCapsuleSize(34,90);
    GetMesh()->SetRelativeLocation(FVector(0,0,-90));GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetCharacterMovement()->MaxWalkSpeed=StationMovement::Stroll;GetCharacterMovement()->bRunPhysicsWithNoController=true;
    GetCharacterMovement()->bOrientRotationToMovement=true;GetCharacterMovement()->RotationRate=FRotator(0,240,0);
    bUseControllerRotationYaw=false;
}
void AStationEngineer::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Skin=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")))GetMesh()->SetSkeletalMeshAsset(Skin);
    if(UClass* Anim=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")))GetMesh()->SetAnimInstanceClass(Anim);
    ApplyIdentity(TEXT("YeCen"));
}
void AStationEngineer::Tick(float Delta)
{
    Super::Tick(Delta);
    if(ConversationPaused&&ConversationFaceTarget.IsValid())
    {
        FRotator Facing=(ConversationFaceTarget->GetActorLocation()-GetActorLocation()).Rotation();Facing.Pitch=0;Facing.Roll=0;
        SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(),Facing,Delta,200.f));
    }
    UpdateIdentityMotion();
    TickCommunityMotion(Delta);
}
void AStationEngineer::SetConversationPaused(bool Paused,AActor* FaceActor)
{
    ConversationPaused=Paused;GetCharacterMovement()->StopMovementImmediately();
    if(!Paused)PlayStationAction(TEXT("Idle"));
    ConversationFaceTarget=Paused?FaceActor:nullptr;
}
FVector AStationEngineer::GetInteractionPoint() const{return GetActorLocation()+FVector(0,0,65);}

AStationGameMode::AStationGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> Pawn(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    static ConstructorHelpers::FClassFinder<APlayerController> Controller(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));
    if(Pawn.Succeeded())DefaultPawnClass=Pawn.Class;
    if(Controller.Succeeded())PlayerControllerClass=Controller.Class;
    HUDClass=AStationHUD::StaticClass();
}
void AStationGameMode::BeginPlay()
{
    Super::BeginPlay();GetWorld()->SpawnActor<AStationDirector>();
}
AStationDirector::AStationDirector()
{
    PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StationRuntime")));
}
void AStationDirector::BeginPlay()
{
    Super::BeginPlay();
    const auto* Preferences=StationFrontend::LoadPreferences(this);VoiceEnabled=Preferences->Voice;SubtitleSpeed=Preferences->TextSpeed;
    MusicVolume=Preferences->MusicVolume/10.f;SubtitleScale=Preferences->SubtitleScale;SubtitleBackdrop=Preferences->SubtitleBackdrop;SceneGuides=Preferences->SceneGuides;AmbientStoriesEnabled=Preferences->AmbientStories&&!FParse::Param(FCommandLine::Get(),TEXT("NoCommunityVignettes"));
    VoiceVolume=Preferences->VoiceVolume/10.f;EffectsVolume=Preferences->EffectsVolume/10.f;ReducedMotion=Preferences->ReducedMotion;GuidanceDetail=SceneGuides?Preferences->GuidanceDetail:0;
    StationFrontend::ApplyAudio(GetWorld(),Preferences);
    LoadProgress();CreateNeighborhood();CreateMissionObjects();CreatePublicLighting();CreateAudio();CreateScore();CreateDistrict();ApplyProgress();CreateExplorationObjects();CreateExperienceWorld();CreateSurveyWorld();ApplyRealityMaterials();LoadAchievements();LoadArcStory();
}
void AStationDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(AchievementNeedsSave)StoreAchievements();
    StopSurveyPulse();EndQuietMoment();FinishStoryFilm(false);CancelPhysicalAction(false);RestoreDialogueCamera();StopPresentationVoice();SetInputLock(false);Super::EndPlay(Reason);
}
void AStationDirector::CreateMissionObjects()
{
    // A shallow public forecourt bypasses the existing arcade posts and gives
    // readers a standing area by converting a narrow strip beside the station.
    auto AddPaving=[this](FVector Position,FVector Size,FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* Slab=GetWorld()->SpawnActor<AStaticMeshActor>(Position,Rotation);
        auto* Mesh=Slab->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Slab->SetActorScale3D(Size);Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Materials/M_BZ_paving.M_BZ_paving")))Mesh->SetMaterial(0,Mat);
    };
    AddPaving(FVector(-600,4180,4),FVector(43,3,.08));
    AddPaving(FVector(-2810,4180,3),FVector(1.2,3,.02),FRotator(3.814f,0,0));
    AddPaving(FVector(1610,4180,3),FVector(1.2,3,.02),FRotator(-3.814f,0,0));
    auto Spawn=[this](EStationObjectKind Kind,const TCHAR* Name,FVector Position,bool Existing=false)
    {
        FTransform Transform(Existing||NeighborhoodReady?Position:PlaceOnPavement(Position));
        auto* Object=GetWorld()->SpawnActorDeferred<AStationObject>(AStationObject::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Object->Kind=Kind;Object->DisplayName=Name;Object->UseExistingGeometry=Existing;
        Object->FinishSpawning(Transform);Objects.Add(Object);
    };
    Spawn(EStationObjectKind::Notice,TEXT("旧站抵押告示"),NeighborhoodPoint(TEXT("Notice"),FVector(-2050,3985,7)));
    Spawn(EStationObjectKind::Connector,TEXT("备用接口"),NeighborhoodPoint(TEXT("Connector"),FVector(-1730,3985,10)));
    Spawn(EStationObjectKind::NodeA,TEXT("连接 A · 公共近街"),NeighborhoodPoint(TEXT("NodeA"),FVector(-1100,3985,7)));
    Spawn(EStationObjectKind::NodeB,TEXT("连接 B · 冷却回路"),NeighborhoodPoint(TEXT("NodeB"),FVector(750,3985,8)));
    Spawn(EStationObjectKind::NodeC,TEXT("连接 C · 应急供光"),NeighborhoodPoint(TEXT("NodeC"),FVector(1420,3985,8)));
    Spawn(EStationObjectKind::Terminal,TEXT("公共供光主控制台"),FVector(-580,3765,80),true);
    Spawn(EStationObjectKind::HouseholdArchive,TEXT("家庭旧票与故事档案"),NeighborhoodPoint(TEXT("HouseholdArchive"),FVector(-1570,3985,7)));
    HouseholdArchive=Objects.Last();
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Engineer=GetWorld()->SpawnActor<AStationEngineer>(NeighborhoodPoint(TEXT("EngineerHome"),FVector(-2250,4140,15))+FVector(0,0,90),FRotator(0,90,0),Params);
}
void AStationDirector::CreatePublicLighting()
{
    FVector Positions[]={FVector(-900,4000,640),FVector(-1850,4000,640),FVector(-2800,4000,640),FVector(-3750,4000,640),FVector(-4700,4000,640),FVector(-5650,4000,640)};
    for(auto& Position:Positions)Position.Z=PlaceOnPavement(FVector(Position.X,Position.Y,0)).Z+640;
    for(int32 I=0;I<6;++I)
    {
        auto* Fixture=GetWorld()->SpawnActor<AActor>(Positions[I],FRotator::ZeroRotator);
        auto* Root=NewObject<USceneComponent>(Fixture);Fixture->SetRootComponent(Root);Fixture->AddInstanceComponent(Root);Root->RegisterComponent();Root->SetWorldLocation(Positions[I]);
        auto AddCube=[Fixture,Root](const TCHAR* Name,FVector Position,FVector Scale)
        {
            auto* Mesh=NewObject<UStaticMeshComponent>(Fixture,FName(Name));Fixture->AddInstanceComponent(Mesh);
            Mesh->SetupAttachment(Root);Station::Cube(Mesh,Position,Scale);Mesh->RegisterComponent();return Mesh;
        };
        auto* Post=AddCube(TEXT("PublicLightPost"),FVector(0,0,-320),FVector(.09,.09,6.4));Station::Material(Post,FLinearColor(.05,.063,.067),0);
        auto* Plate=AddCube(TEXT("PublicLightPlate"),FVector(0,0,0),FVector(2.8,.95,.09));FixtureMaterials.Add(Station::Material(Plate,FLinearColor(1,.8,.53),0));
        auto* Louver=AddCube(TEXT("MovableLouver"),FVector(0,0,-10),FVector(2.8,.95,.045));Station::Material(Louver,FLinearColor(.07,.08,.075),0);Shutters.Add(Louver);
        auto* Light=GetWorld()->SpawnActor<ARectLight>(Positions[I]-FVector(0,0,12),FRotator(-90,0,0));
        Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        auto* Rect=CastChecked<URectLightComponent>(Light->GetLightComponent());
        Rect->SetIntensityUnits(ELightUnits::Lumens);Rect->SetSourceWidth(280);Rect->SetSourceHeight(95);
        Rect->SetLightColor(FLinearColor(1,.78,.46));Rect->SetAttenuationRadius(1800);Rect->SetIntensity(0);
        PublicLights.Add(Light);
    }
    struct Shot {FVector From,To;};
    const Shot Shots[]={{{-950,5150,650},{0,2900,600}},{{-2800,5400,620},{-2200,4000,520}},{{-3900,6100,1000},{-2300,4000,230}},{{-1700,4730,190},{-1450,4140,160}}};
    for(const auto& Shot:Shots)
    {
        auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Shot.From,(Shot.To-Shot.From).Rotation());
        Camera->GetCameraComponent()->SetFieldOfView(58);Cameras.Add(Camera);
    }
}
UAudioComponent* AStationDirector::MakeLoop(const TCHAR* Name,FVector Position,float Volume,bool Spatial)
{
    const FString Key(Name);Volume*=Key.StartsWith(TEXT("MU"))?1.f:Key.StartsWith(TEXT("EV_"))||Key.StartsWith(TEXT("VR_"))||Key.StartsWith(TEXT("HR_"))?VoiceVolume:EffectsVolume;
    auto* Audio=NewObject<UAudioComponent>(this);AddInstanceComponent(Audio);Audio->SetupAttachment(RootComponent);
    Audio->bAutoActivate=false;Audio->SetSound(Station::Sound(Name));Audio->bAllowSpatialization=Spatial;
    Audio->bOverrideAttenuation=Spatial;Audio->AttenuationOverrides.bAttenuate=Spatial;
    Audio->AttenuationOverrides.AttenuationShapeExtents=FVector(120);Audio->AttenuationOverrides.FalloffDistance=2000;
    Audio->RegisterComponent();Audio->SetWorldLocation(Position);Audio->SetVolumeMultiplier(Volume);Audio->Play();return Audio;
}
void AStationDirector::CreateAudio()
{
    AirAudio=MakeLoop(TEXT("AM_Air"),FVector::ZeroVector,.10,false);
    IdleAudio=MakeLoop(TEXT("AM_StationIdle"),FVector(0,3600,180),.22,true);
    RunAudio=MakeLoop(TEXT("AM_StationRun"),FVector(0,3600,220),0,true);
    MakeLoop(TEXT("AM_RichGarden"),FVector(4300,2100,150),.15,true);
}
void AStationDirector::PlayCue(const TCHAR* Name,FVector Location,float Volume,bool Spatial)
{
    if(!Spatial){UGameplayStatics::PlaySound2D(this,Station::Sound(Name),Volume*EffectsVolume);return;}
    auto* Audio=MakeLoop(Name,Location,Volume,true);Audio->bAutoDestroy=true;
}
void AStationDirector::LoadProgress()
{
    Progress=Cast<UStationProgress>(UGameplayStatics::LoadGameFromSlot(Station::Slot,0));
    if(!Progress)Progress=CastChecked<UStationProgress>(UGameplayStatics::CreateSaveGameObject(UStationProgress::StaticClass()));
    Progress->Connections&=7;Progress->Reserve=FMath::Clamp(Progress->Reserve,0,100);
    if(Progress->Started&&(Progress->ChosenPlan<0||Progress->ChosenPlan>1)){Progress->Started=false;Progress->ChosenPlan=-1;}
    Progress->AllocationPolicy=FMath::Clamp(Progress->AllocationPolicy,-1,1);Progress->AllocationReviewMask&=7;
    Progress->RequestReadMask&=7;Progress->RationVisitMask&=7;Progress->RationReviewMask&=7;
    Progress->RationMask=FMath::Clamp(Progress->RationMask,-1,7);
    Progress->ConflictWitness=FMath::Clamp(Progress->ConflictWitness,0,3);Progress->ConflictHeardMask&=3;
    Progress->ConflictTraceMask&=7;
    if(Progress->ConflictTraceMask==0&&Progress->ConflictWitness>0)Progress->ConflictTraceMask=(1<<Progress->ConflictWitness)-1;
    Progress->ConflictChoice=FMath::Clamp(Progress->ConflictChoice,-1,1);WitnessSeconds=0;
    Progress->SupplyEvidenceMask&=7;Progress->SupplyMeasuredMask&=7;Progress->SupplyHeardMask&=7;
    Progress->SupplyChoice=FMath::Clamp(Progress->SupplyChoice,-1,1);
    Progress->MandateWitnessMask&=3;Progress->MandateChoice=FMath::Clamp(Progress->MandateChoice,-1,1);
    Progress->CrisisVerifiedMask&=3;
    if(Progress->CrisisAllocation!=-1&&(Progress->CrisisAllocation<12||Progress->CrisisAllocation>24||Progress->CrisisAllocation%2!=0)){Progress->CrisisAllocation=-1;Progress->CrisisVerifiedMask=0;Progress->CrisisEndingHeard=false;}
    if(Progress->MandateCommitted&&Progress->MandateChoice<0){Progress->MandateCommitted=false;Progress->MandateEndingHeard=false;}
    Progress->DistrictEvidence&=7;Progress->DistrictChoice=FMath::Clamp(Progress->DistrictChoice,-1,1);
    Progress->AmbientSeenMask&=31;Progress->ExplorationSeenMask=StationExplorationPolicy::CleanMemories(Progress->ExplorationSeenMask);Progress->NeighborhoodFilmMask=StationExplorationPolicy::CleanFilms(Progress->NeighborhoodFilmMask);
    Progress->DutyWorkMask&=15;Progress->DutyFaultIndex=FMath::Clamp(Progress->DutyFaultIndex,-1,2);
    Progress->DutyEmergencyChoice=FMath::Clamp(Progress->DutyEmergencyChoice,-1,1);DutyRepairClock=0;
    Progress->UrbanReadMask=StationExperience::CleanUrbanMask(Progress->UrbanReadMask);Progress->QuietVisitMask=StationExperience::CleanQuietMask(Progress->QuietVisitMask);
    while(Progress->DialogueHistory.Num()>StationExperience::LogCapacity)Progress->DialogueHistory.RemoveAt(0);
    for(FString& Line:Progress->DialogueHistory)Line=Line.Replace(TEXT("\r"),TEXT(" ")).Replace(TEXT("\n"),TEXT(" ")).Left(160);
}
void AStationDirector::SaveProgress()
{
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))return;
#endif
    if(!UGameplayStatics::SaveGameToSlot(Progress,Station::Slot,0))UE_LOG(LogTemp,Warning,TEXT("Station checkpoint could not be saved"));
}
void AStationDirector::ApplyProgress()
{
    for(auto& Object:Objects)
    {
        if(Object->Kind==EStationObjectKind::Connector)
        {
            Object->Active=!Progress->HasConnector;Object->SetActorHiddenInGame(!Object->Active);Object->SetActorEnableCollision(Object->Active);
        }
        int32 Index=int32(Object->Kind)-int32(EStationObjectKind::NodeA);
        if(Index>=0&&Index<3)Object->SetIndicator((Progress->Connections&(1<<Index))?FLinearColor(.17,.65,.45):FLinearColor(.95,.33,.045),3);
    }
    if(Progress->Started)
    {
        StartElapsed=46;Starting=false;UpdateSupply(0);IdleAudio->SetVolumeMultiplier(.06);RunAudio->SetVolumeMultiplier(.25);
        if(!LinHe)
        {
            FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            LinHe=GetWorld()->SpawnActor<AStationEngineer>(NeighborhoodPoint(TEXT("SisterHome"),FVector(-1450,4140,10))+FVector(0,0,90),FRotator(0,90,0),P);LinHe->PersonName=TEXT("林禾");LinHe->ApplyIdentity(TEXT("LinHe"));LinHe->Patrol=false;
        }
    }
    else
    {
        for(auto& Light:PublicLights)Light->GetLightComponent()->SetIntensity(0);
        for(auto& Mat:FixtureMaterials)if(Mat)Mat->SetScalarParameterValue(TEXT("Glow"),0);
        for(auto& Shutter:Shutters)Shutter->SetRelativeRotation(FRotator::ZeroRotator);
        if(IdleAudio)IdleAudio->SetVolumeMultiplier(.22);if(RunAudio)RunAudio->SetVolumeMultiplier(0);
        if(LinHe){LinHe->Destroy();LinHe=nullptr;}
    }
    UpdateAllocationActors();UpdateRationActors();UpdateConflictActors();UpdateSupplyActors();UpdateDutyActors();UpdateDistrictState();
}
void AStationDirector::BindControls()
{
    if(BoundInput)return;PC=UGameplayStatics::GetPlayerController(this,0);if(!PC)return;
    EnableInput(PC);if(!InputComponent)return;
    FRotator InitialView=PC->GetControlRotation();InitialView.Pitch=-12;InitialView.Roll=0;PC->SetControlRotation(InitialView);
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AStationDirector::PressInteract);
    InputComponent->BindKey(EKeys::E,IE_Released,this,&AStationDirector::ReleaseInteract);
    for(const FKey Key:{EKeys::S,EKeys::Down})InputComponent->BindKey(Key,IE_Pressed,this,&AStationDirector::SelectNext).bConsumeInput=false;
    for(const FKey Key:{EKeys::W,EKeys::Up})InputComponent->BindKey(Key,IE_Pressed,this,&AStationDirector::SelectPrevious).bConsumeInput=false;
    InputComponent->BindKey(EKeys::BackSpace,IE_Pressed,this,&AStationDirector::GoBack);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AStationDirector::RequestHome);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AStationDirector::RequestRestart);BoundInput=true;
    InputComponent->BindKey(EKeys::J,IE_Pressed,this,&AStationDirector::RequestJournal);
    InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&AStationDirector::RequestRelationships);
    InputComponent->BindKey(EKeys::M,IE_Pressed,this,&AStationDirector::RequestNeighborhoodMap);
    InputComponent->BindKey(EKeys::L,IE_Pressed,this,&AStationDirector::RequestDialogueLog);
    InputComponent->BindKey(EKeys::H,IE_Pressed,this,&AStationDirector::RequestAchievements);
    InputComponent->BindKey(EKeys::V,IE_Pressed,this,&AStationDirector::ToggleVoice);
    InputComponent->BindKey(EKeys::K,IE_Pressed,this,&AStationDirector::RequestSurveyBook);
    InputComponent->BindKey(EKeys::F,IE_Pressed,this,&AStationDirector::SurveyHint).bConsumeInput=false;
    for(const FKey Key:{EKeys::A,EKeys::Left})InputComponent->BindKey(Key,IE_Pressed,this,&AStationDirector::SurveyLeft).bConsumeInput=false;
    for(const FKey Key:{EKeys::D,EKeys::Right})InputComponent->BindKey(Key,IE_Pressed,this,&AStationDirector::SurveyRight).bConsumeInput=false;
    InputComponent->BindKey(EKeys::LeftShift,IE_Pressed,this,&AStationDirector::PressLeftSprint);
    InputComponent->BindKey(EKeys::LeftShift,IE_Released,this,&AStationDirector::ReleaseLeftSprint);
    InputComponent->BindKey(EKeys::RightShift,IE_Pressed,this,&AStationDirector::PressRightSprint);
    InputComponent->BindKey(EKeys::RightShift,IE_Released,this,&AStationDirector::ReleaseRightSprint);
}
void AStationDirector::Tick(float Delta)
{
    Super::Tick(Delta);BindControls();if(!PC)return;
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("StationSmokeTest")))
    {
        if(!SmokeWritten&&GetWorld()->GetTimeSeconds()>5&&PC->GetPawn())
        {
            IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Station")),true);
            FFileHelper::SaveStringToFile(GetSnapshotJson(),*(FPaths::ProjectSavedDir()/TEXT("Station/standalone_smoke.json")));
            // Exercise dynamically named resources in the cooked executable,
            // where editor-only registries and loose .uasset files are absent.
            auto Evidence=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Checks;bool AllPresent=true;
            auto Record=[&](const FString& Name,bool Present)
            {
                auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("resource"),Name);Row->SetBoolField(TEXT("passed"),Present);
                Checks.Add(MakeShared<FJsonValueObject>(Row));AllPresent&=Present;
            };
            for(const TCHAR* Asset:{
                TEXT("/Game/BZ/RealismV24/Motion/LinZhao/A24_LinZhao_Jog.A24_LinZhao_Jog"),
                TEXT("/Game/BZ/RealismV24/Motion/LinZhao/A24_LinZhao_Run.A24_LinZhao_Run"),
                TEXT("/Game/BZ/RealismV24/Motion/LinZhao/BS24_LinZhao_Locomotion.BS24_LinZhao_Locomotion"),
                TEXT("/Game/BZ/Characters/ShenYan/SK_ShenYan.SK_ShenYan"),
                TEXT("/Game/BZ/Characters/LuoShen/SK_LuoShen.SK_LuoShen"),
                TEXT("/Game/BZ/Characters/AZhan/SK_AZhan.SK_AZhan"),
                TEXT("/Game/BZ/Audio/EV_Arc_001.EV_Arc_001"),
                TEXT("/Game/BZ/Audio/EV_Arc_088.EV_Arc_088"),
                TEXT("/Game/BZ/Audio/MU14_Title.MU14_Title"),
                TEXT("/Game/BZ/Frontend/T_DaylightCover.T_DaylightCover")})
                Record(Asset,IsValid(LoadObject<UObject>(nullptr,Asset)));
            for(const TCHAR* File:{TEXT("BZ/StoryV27/campaign.json"),TEXT("BZ/RuntimeImages/Portraits/T_N19_LinZhao.png"),TEXT("BZ/RuntimeImages/Portraits/T_N19_LuoShen.png"),TEXT("Movies/BZ_WhiteDay.mp4")})
                Record(File,FPaths::FileExists(FPaths::ProjectContentDir()/File));
            Record(TEXT("Authored story entry and free-roam ending loaded"),ArcNodes.Contains(TEXT("queue"))&&ArcNodes.Contains(TEXT("free")));
            Record(TEXT("Playable pawn and native input binding"),PC->GetPawn()&&BoundInput);
            Evidence->SetArrayField(TEXT("checks"),Checks);Evidence->SetBoolField(TEXT("passed"),AllPresent);Evidence->SetStringField(TEXT("scope"),TEXT("Cooked resource loading and game startup only; not rendered or full-story acceptance"));
            FString EvidenceText;FJsonSerializer::Serialize(Evidence,TJsonWriterFactory<>::Create(&EvidenceText));
            FFileHelper::SaveStringToFile(EvidenceText,*(FPaths::ProjectSavedDir()/TEXT("Station/standalone_assets.json")));
            UE_LOG(LogTemp,Display,TEXT("STATION_STANDALONE_ASSETS_READY %s"),AllPresent?TEXT("true"):TEXT("false"));
            if(!FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Station/packaged_gameplay.png"),true,false);
            UE_LOG(LogTemp,Display,TEXT("STATION_STANDALONE_RUNTIME_READY"));SmokeWritten=true;
        }
        if(SmokeWritten&&GetWorld()->GetTimeSeconds()>8)FPlatformMisc::RequestExit(false);
    }
#endif
    #if WITH_EDITOR
    if(!MandatePreviewEntered&&FParse::Param(FCommandLine::Get(),TEXT("DaylightMandatePreview"))&&PC->GetPawn()&&GetWorld()->GetTimeSeconds()>3)
    {MandatePreviewEntered=true;AutomationMandatePreview(0);}
#endif
    if(!CrisisPreviewEntered&&FParse::Param(FCommandLine::Get(),TEXT("DaylightCrisisPreview"))&&PC->GetPawn()&&GetWorld()->GetTimeSeconds()>3)
    {CrisisPreviewEntered=true;AutomationCrisisPreview(0);}
    if(!ArcPreviewEntered&&FParse::Param(FCommandLine::Get(),TEXT("DaylightArcPreview"))&&PC->GetPawn()&&GetWorld()->GetTimeSeconds()>3)
    {ArcPreviewEntered=true;AutomationArcPreview(0);}
    UpdateCrisisWorld();TickRationLights(Delta);TickDuty(Delta);TickArc(Delta);
    TickExperience(Delta);TickSurvey(Delta);TickScore(Delta);TickAchievements(Delta);
    TickCommunityVignette(Delta);
    TickPresentation(Delta);
    UpdateSprintState();
    UpdateCharacterMotion();
    TickStoryFilm(Delta);
    if(IsStoryFilmActive()){UpdateVoiceMix(Delta);return;}
    if(Starting){UpdateSupply(Delta);UpdateVoiceMix(Delta);return;}
    TickCommunity(Delta);TickNeighborhood(Delta);UpdateArcWorld(Delta);
    if(!IsMenuOpen()&&!IsPhysicalActionActive())UpdateFocus();
    else if(Panel!=EStationPanel::ArcScene&&Panel!=EStationPanel::ArcCredits&&!IsArcMenu()&&!RemoteConversation&&ConversationTarget&&PC->GetPawn()&&FVector::Dist2D(PC->GetPawn()->GetActorLocation(),ConversationTarget->GetActorLocation())>300)ClosePanel();
    UpdateVoiceMix(Delta);
}
FVector AStationDirector::TargetPoint(AActor* Target) const
{
    if(auto* Object=Cast<AStationObject>(Target))return Object->GetInteractionPoint();
    if(auto* Person=Cast<AStationEngineer>(Target))return Person->GetInteractionPoint();return FVector::ZeroVector;
}
bool AStationDirector::CanSeeTarget(AActor* Target) const
{
    if(!PC||!PC->GetPawn()||!IsValid(Target))return false;
    FVector Eyes=PC->GetPawn()->GetActorLocation()+FVector(0,0,55);FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(StationInteraction),false,PC->GetPawn());Query.AddIgnoredActor(Target);Query.AddIgnoredActor(this);
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eyes,TargetPoint(Target),ECC_Visibility,Query);
}
void AStationDirector::UpdateFocus()
{
    AActor* PreviousFocus=Focus.Get();Focus=nullptr;if(!PC||!PC->GetPawn()||!PC->PlayerCameraManager)return;
    const FVector Position=PC->GetPawn()->GetActorLocation(),Camera=PC->PlayerCameraManager->GetCameraLocation();
    const FVector Forward=PC->PlayerCameraManager->GetCameraRotation().Vector();float Best=-100;
    auto Consider=[&](AActor* Target,float Range)
    {
        if(!IsValid(Target))return;
        const FVector Point=TargetPoint(Target);const float Distance=FVector::Dist2D(Position,Point);
        if(Distance>Range||FMath::Abs(Position.Z-Point.Z)>190)return;
        const FVector Facing=PC->GetControlRotation().Vector().GetSafeNormal2D();
        if(FVector::DotProduct(Facing,(Point-Position).GetSafeNormal2D())<-.05f)return;
        const float Aim=FVector::DotProduct(Forward,(Point-Camera).GetSafeNormal());
        if(Aim<.66||!CanSeeTarget(Target))return;
        const float Score=StationExperience::FocusScore(Aim,Distance,Target==PreviousFocus);
        if(Score>Best){Best=Score;Focus=Target;}
    };
    for(auto& Object:Objects)if(Object->Active)Consider(Object,125);
    Consider(Engineer,180);if(Progress->Started){Consider(LinHe,180);Consider(ZhouChi,180);}
    if(IsArcChapter())for(AStationEngineer* Person:ArcPeople)if(IsValid(Person))Consider(Person,180);
}
void AStationDirector::SetInputLock(bool Lock)
{
    if(Lock){EndQuietMoment();ResetSprint();}
    if(InputLocked==Lock||!PC)return;
    PC->SetIgnoreMoveInput(Lock);PC->SetIgnoreLookInput(Lock);InputLocked=Lock;
    if(Lock)if(auto* Character=Cast<ACharacter>(PC->GetPawn()))Character->GetCharacterMovement()->StopMovementImmediately();
}
void AStationDirector::OpenPanel(EStationPanel NewPanel)
{
    CancelCommunityVignette();
    if(Panel==EStationPanel::None&&!RemoteConversation)ConversationTarget=Focus;
    Panel=NewPanel;Selection=0;SetInputLock(true);BeginPanelPresentation();
}
void AStationDirector::ClosePanel()
{
    if(Panel==EStationPanel::SurveyGame)StopSurveyPulse();
    if(ArcPendingNode.StartsWith(TEXT("archive:")))ArcPendingNode.Empty();
    if(Panel==EStationPanel::Conversation||IsArcMenu())
    {for(AStationEngineer* Person:ArcPausedPeople)if(IsValid(Person))Person->SetConversationPaused(false,nullptr);ArcPausedPeople.Empty();}
    if(Panel==EStationPanel::DutyCalibrate){DutyCalibrationOrder=-1;DutyPressureHeld=false;DutyStable=0;}
    RestoreDialogueCamera();
    if(!RemoteConversation)if(auto* Person=Cast<AStationEngineer>(ConversationTarget))Person->SetConversationPaused(false,nullptr);
    ConversationTarget=nullptr;RemoteConversation=false;CivicReviewPhase=-1;Panel=EStationPanel::None;SetInputLock(false);
    ActiveUrban=-1;StopPresentationVoice();PresentedText.Empty();PresentedSpeaker.Empty();VisibleCharacters=0;
}
void AStationDirector::Notify(const FString& Text,bool Rejected)
{
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))UE_LOG(LogTemp,Display,TEXT("STATION_NOTICE %s"),*Text);
    Toast=Text;ToastUntil=GetWorld()->GetTimeSeconds()+4;
    if(Rejected)PlayCue(TEXT("UI_Unavailable"),FVector::ZeroVector,.3,false);
}
TArray<FString> AStationDirector::GetBaseConversation() const
{
    if(IsDistrictChapter())
    {
        if(Progress->DistrictSettled)
        {
            // Revisit a completed disposition without asking the player to choose it again.
            // These lines already have recordings in the established project cast.
            if(ConversationTarget==Engineer)return {TEXT("叶岑：检验合格。灯不是坏了，是原来的地址，付不起下一期。"),TEXT("叶岑：林照，你替家里着急，我明白。可我的检修组，也有人等着拿这份工钱回家。")};
            if(ConversationTarget==ZhouChi)return {TEXT("周驰：凭条我能收，折价也是真的。你可以不做这笔生意，别装作我没从里面挣钱。")};
            if(ConversationTarget==LinHe)return {TEXT("林禾：这次，你没有替他们说不难受。我听见了。")};
        }
        if(ConversationTarget==Engineer)return {TEXT("叶岑：检验合格。灯不是坏了，是原来的地址，付不起下一期。"),TEXT("叶岑：夜班押金里有我组员的钱。别因为我能理解你，就以为他们已经同意。")};
        if(ConversationTarget==ZhouChi)return {TEXT("周驰：这六个额度，先退工人的押金，还是先试亮？别把押金那一栏，藏在公益两个字后面。"),TEXT("周驰：凭条我能收，折价也是真的。你可以不做这笔生意，别装作我没从里面挣钱。")};
        if(ConversationTarget==LinHe)return {TEXT("林禾：他们想拿回押金，我也想。可住在黑里的时候，我又盼着有人别把灯全关掉。"),TEXT("林禾：别只问我站哪边。让我看看，你准备把谁的异议写下来。")};
    }
    if(IsDutyChapter())return GetDutyConversation();
    if(IsSupplyChapter())return GetSupplyConversation();
    if(IsConflictChapter())return GetConflictConversation();
    if(IsRationChapter())return GetRationConversation();
    if(Progress->Started&&(ConversationTarget==ZhouChi||Progress->AllocationPolicy>=0))return GetAllocationConversation();
    if(ConversationTarget==LinHe)return {TEXT("我想再坐一会。你先去看配给名单。"),TEXT("别把我排在别人前面，又说这是规矩。")};
    if(Progress->Started)return Progress->ChosenPlan==0
        ?TArray<FString>{TEXT("叶岑：近街先亮了。远处的人会问，为什么是他们再等一轮。"),TEXT("叶岑：储备够稳，只是说机器的。别拿这句话替所有人回答。")}
        :TArray<FString>{TEXT("叶岑：第一轮覆盖得更广，下一轮他们还会不会把这当成你该给的？"),TEXT("叶岑：临时过载的记录我不会删。下一次出问题，别说你没看过。")};
    if(RiskConversation)return {TEXT("林照：临时过载会损坏旧设备？你在厨房怎么没说？"),TEXT("叶岑：那时候说，你们还来吗？"),TEXT("林照：那时候我们还有机会决定。"),TEXT("叶岑：先看准备送到哪儿，再确认。保守方案只亮近街；加大首轮，库存会掉得更快。")};
    if(Progress->MetEngineer)return {TEXT("叶岑：告示挂在西边。备用接口在工具箱里。三处连接都确认后，再回来谈风险。")};
    return {TEXT("叶岑：这座站没有免费白昼。它只有一笔还没用完的应急额度。"),TEXT("林照：至少今晚不用先交钱。"),TEXT("叶岑：把告示读完。衡昼还握着抵押权。你让谁先见到光，谁就会把这件事记住。"),TEXT("叶岑：拿上备用接口，确认 A、B、C 三处连接。别碰还在运行的常昼线路。")};
}
void AStationDirector::Repair(int32 Index)
{
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))UE_LOG(LogTemp,Display,TEXT("STATION_REPAIR index=%d met=%d connector=%d mask=%d"),Index,Progress->MetEngineer,Progress->HasConnector,Progress->Connections);
    if(!Progress->MetEngineer){Notify(TEXT("先与叶岑核对检修安排。"),true);return;}
    if(!Progress->HasConnector){Notify(TEXT("缺少备用接口：先查看西侧工具箱。"),true);return;}
    const int32 Bit=1<<Index;if(Progress->Connections&Bit){Notify(TEXT("该连接已确认，不会重复消耗或增加计数。"));return;}
    BeginPhysicalAction(Cast<AStationObject>(Focus),TEXT("Repair"),Index);
}
void AStationDirector::PressInteract()
{
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))UE_LOG(LogTemp,Display,TEXT("STATION_PRESS ready=%d panel=%d focus=%s"),EReady,int32(Panel),Focus?*Focus->GetName():TEXT("none"));
    if(QuietIndex>=0){if(EReady){EReady=false;EndQuietMoment();}return;}
    if(IsStoryFilmActive()){StorySkipHeld=true;return;}
    if(Panel==EStationPanel::ArcScene){SetArcSkipHeld(true);return;}
    if(Panel==EStationPanel::DutyCalibrate){DutyPressureHeld=true;return;}
    if(!EReady||!PC)return;EReady=false;
    if(Panel==EStationPanel::SurveyGame||Panel==EStationPanel::SurveyBook||Panel==EStationPanel::SurveyRecord){PressSurvey();return;}
    if(IsArcMenu()){PressArcChoice();return;}
    if(Panel==EStationPanel::ArcCredits){FinishArcCredits();return;}
    if(IsCrisisMenu()){PressCrisisChoice();return;}
    if(IsMandateMenu()){PressMandateChoice();return;}
    if(Panel==EStationPanel::WorldRecord){if(IsTyping())CompletePresentation();else CloseUrbanRecord();return;}
    if(Panel==EStationPanel::DialogueLog||Panel==EStationPanel::Achievements){ClosePanel();return;}
    if(Panel==EStationPanel::Exploration){if(IsTyping())CompletePresentation();else CloseExploration();return;}
    if(IsPhysicalActionActive())return;
    if(Starting){FinishSequence();return;}
    if(Panel==EStationPanel::ReturnHome)
    {
        if(Selection==1){SaveProgress();ClosePanel();UGameplayStatics::OpenLevel(this,TEXT("/Game/BZ/Maps/L_Title"));}
        else ClosePanel();return;
    }
    if(Panel==EStationPanel::Restart)
    {
        if(Selection==1){ClosePanel();if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightReadOnlySave")))UGameplayStatics::DeleteGameInSlot(Station::Slot,0);Progress=NewObject<UStationProgress>(this);ApplyProgress();Notify(TEXT("本段演示已重新开始。"));}
        else ClosePanel();return;
    }
    if((Panel==EStationPanel::Document||Panel==EStationPanel::Conversation)&&IsTyping()){CompletePresentation();return;}
    if(Panel==EStationPanel::Document||Panel==EStationPanel::Journal||Panel==EStationPanel::Relationships||Panel==EStationPanel::NeighborhoodMap)
    {
        auto* Document=Cast<AStationObject>(ConversationTarget);
        if(Panel==EStationPanel::Document&&CompleteConflictTraceDocument(Document))return;
        if(Panel==EStationPanel::Document&&CompleteSupplyDocument(Document))return;
        if(Panel==EStationPanel::Document&&ArcPendingNode.StartsWith(TEXT("archive:"))){ClosePanel();return;}
        if(Panel==EStationPanel::Document&&IsArcDocument(Document)){CompleteArcDocument(Document);return;}
        const bool ShowAftermath=Panel==EStationPanel::Document&&Document&&Document->Kind==EStationObjectKind::Terminal&&Progress->ReadRecall&&!Progress->SeenAftermathFilm&&StoryAutoplayEnabled();
        const bool ShowDuty=Panel==EStationPanel::Document&&Document&&(Document->Kind==EStationObjectKind::Terminal||Document==DutyLoadBank)&&IsDutyChapter()&&Progress->DutySettled&&!Progress->SeenDutyFilm&&StoryAutoplayEnabled();
        const bool ShowDistrict=Panel==EStationPanel::Document&&DistrictObjects.Num()==6&&Document==DistrictObjects[5]&&Progress->DistrictSettled&&!Progress->SeenDistrictFilm&&StoryAutoplayEnabled();
        const bool ShowLocal=NeighborhoodReady&&StationExplorationPolicy::CanShowConsequences(Panel==EStationPanel::Document&&Document&&Document->IsServiceRequest(),Progress->RationMask>=0,Progress->RationVisitMask,Progress->ReadRecall,Progress->NeighborhoodFilmMask,StoryAutoplayEnabled());
        CompleteCrisisDocument(Document);CompleteMandateDocument(Document);ClosePanel();if(ShowLocal&&!IsCrisisChapter())StartStoryFilm(TEXT("C14"));else if(ShowDistrict)StartStoryFilm(TEXT("C12"));else if(ShowDuty)StartStoryFilm(TEXT("C11"));else if(ShowAftermath)StartStoryFilm(TEXT("C09"));return;
    }
    if(Panel==EStationPanel::StoryArchive)
    {
        if(!CanPlayStoryFilm(Selection)){Notify(TEXT("该片段尚未解锁。先沿任务档案继续。"),true);return;}
        if(Selection==9){if(!AStationBackgroundFilm::Open(PC))Notify(TEXT("背景影片暂不可用，原有故事不受影响。"),true);ReleaseInteract();return;}
        const TCHAR* Films[]={TEXT("P00"),TEXT("C08"),TEXT("C09"),TEXT("C10"),TEXT("C11"),TEXT("C12"),TEXT("C13"),TEXT("C14"),TEXT("C15")};StartStoryFilm(Films[Selection]);return;
    }
    if(Panel==EStationPanel::Conversation)
    {
        if(++ConversationLine>=GetConversation().Num())
        {
            if(CompleteArcConversation(ConversationTarget))return;
            if(CompleteCrisisConversation(ConversationTarget))return;
            if(CompleteMandateConversation(ConversationTarget))return;
            if(CompleteAftermathConversation(ConversationTarget))return;
            const int32 CompletedReviewPhase=CivicReviewPhase;
            const bool ReactToRisk=ConversationTarget==Engineer&&RiskConversation;
            if(ConversationTarget==Engineer){Progress->MetEngineer=true;if(RiskConversation)Progress->HeardRisk=true;SaveProgress();}
            if(IsDutyChapter())CompleteDutyConversation(ConversationTarget);
            else if(IsSupplyChapter())CompleteSupplyConversation(ConversationTarget);
            else if(IsConflictChapter())CompleteConflictConversation(ConversationTarget);
            else CompleteAllocationConversation(ConversationTarget);
            ClosePanel();
            if(IsSupplyChapter()&&!Progress->SupplyComplete&&Progress->SupplyChoice<0&&Progress->SupplyEvidenceMask==7&&SupplyMeasurementsComplete()&&Progress->SupplyHeardMask==7){OpenPanel(EStationPanel::SupplyChoice);return;}
            if(IsConflictChapter()&&Progress->ConflictOffer&&Progress->ConflictChoice<0){OpenPanel(EStationPanel::ConflictChoice);return;}
            if(CompletedReviewPhase>=0)BeginCivicReview(CompletedReviewPhase);
            if(ReactToRisk&&Engineer)PlayCharacterReaction(TEXT("VR_YeCen_Sigh"),Engineer->GetActorLocation()+FVector(0,0,60),.32f);
        }
        else BeginPanelPresentation();
        return;
    }
    if(Panel==EStationPanel::DistrictChoice||Panel==EStationPanel::DistrictConfirm){PressDistrictChoice();return;}
    if(Panel==EStationPanel::DutyStart||Panel==EStationPanel::DutyChoice||Panel==EStationPanel::DutyConfirm){PressDutyChoice();return;}
    if(Panel==EStationPanel::SupplyStart||Panel==EStationPanel::SupplyChoice||Panel==EStationPanel::SupplyConfirm){PressSupplyChoice();return;}
    if(Panel==EStationPanel::ConflictStart||Panel==EStationPanel::ConflictChoice||Panel==EStationPanel::ConflictConfirm){PressConflictChoice();return;}
    if(Panel==EStationPanel::Plans){ConfirmSelection=Selection;OpenPanel(EStationPanel::Confirmation);Selection=1;return;}
    if(Panel==EStationPanel::Confirmation)
    {
        if(Selection==0)StartSupply();else{OpenPanel(EStationPanel::Plans);Selection=ConfirmSelection;}return;
    }
    if(Panel==EStationPanel::Allocation){ConfirmSelection=Selection;OpenPanel(EStationPanel::AllocationConfirm);Selection=1;return;}
    if(Panel==EStationPanel::AllocationConfirm)
    {
        if(Selection==0)CommitAllocation();else {OpenPanel(EStationPanel::Allocation);Selection=ConfirmSelection;}return;
    }
    if(Panel==EStationPanel::RationPlan){PressRationChoice();return;}
    if(Panel==EStationPanel::RationConfirm)
    {
        if(Selection==0)CommitRation();else{OpenPanel(EStationPanel::RationPlan);Selection=3;}return;
    }
    UpdateFocus();if(!Focus)return;
    if(auto* Person=Cast<AStationEngineer>(Focus))
    {
        if(HandleArcPerson(Person))return;
        RiskConversation=Person==Engineer&&Progress->Connections==7&&!Progress->HeardRisk;
        Person->SetConversationPaused(true,PC->GetPawn());ConversationLine=0;OpenPanel(EStationPanel::Conversation);return;
    }
    auto* Object=Cast<AStationObject>(Focus);if(!Object)return;
    if(HandleSurveyObject(Object))return;
    if(HandleArcObject(Object))return;
    if(HandleCrisisObject(Object))return;
    if(HandleMandateObject(Object))return;
    if(IsConflictChapter()&&Progress->ConflictStarted&&ConflictTraces.Contains(Object)&&HandleConflictObject(Object))return;
    if(HandleExperienceObject(Object))return;
    if(HandleNeighborhoodObject(Object))return;
    if(Object->Kind==EStationObjectKind::HouseholdArchive){OpenPanel(EStationPanel::StoryArchive);return;}
    if(Object->Kind==EStationObjectKind::Terminal&&Progress->Started&&Progress->HeardAllocation&&Progress->MetBroker&&Progress->ReadLedger&&Progress->AllocationPolicy<0&&!Progress->SeenFamilyFilm&&StoryAutoplayEnabled())
    {StartStoryFilm(TEXT("C08"),true);return;}
    if(HandleDistrictObject(Object))return;
    if(HandleDutyObject(Object))return;
    if(HandleSupplyObject(Object))return;
    if(HandleConflictObject(Object))return;
    if(HandleRationObject(Object))return;
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))UE_LOG(LogTemp,Display,TEXT("STATION_TARGET %s kind=%d"),*Object->GetName(),int32(Object->Kind));
    switch(Object->Kind)
    {
        case EStationObjectKind::Notice:Progress->ReadNotice=true;SaveProgress();OpenPanel(EStationPanel::Document);PlayCue(TEXT("FX_Paper"),TargetPoint(Object),.35);break;
        case EStationObjectKind::Connector:
            if(!Progress->HasConnector)BeginPhysicalAction(Object,TEXT("Reach"));break;
        case EStationObjectKind::NodeA:Repair(0);break;
        case EStationObjectKind::NodeB:Repair(1);break;
        case EStationObjectKind::NodeC:Repair(2);break;
        case EStationObjectKind::AllocationLedger:
            Progress->ReadLedger=true;SaveProgress();OpenPanel(EStationPanel::Document);PlayCue(TEXT("FX_Paper"),TargetPoint(Object),.35);break;
        case EStationObjectKind::Terminal:
            if(Progress->Started)
            {
                if(Progress->HeardAllocation&&Progress->MetBroker&&Progress->ReadLedger&&Progress->AllocationPolicy<0)OpenPanel(EStationPanel::Allocation);
                else OpenPanel(EStationPanel::Document);
                break;
            }
            if(!Progress->ReadNotice){Notify(TEXT("请先阅读西侧抵押告示，查看这座站的服务限制。"),true);break;}
            if(Progress->Connections!=7){Notify(TEXT("主控制台锁定：三处连接尚未全部确认。"),true);break;}
            if(!Progress->HeardRisk){Notify(TEXT("先向叶岑询问临时过载的风险。"),true);break;}
            OpenPanel(EStationPanel::Plans);break;
    }
}
void AStationDirector::ReleaseInteract(){SetArcSkipHeld(false);DutyPressureHeld=false;EReady=true;StorySkipHeld=false;StorySkipHeldSeconds=0;}
void AStationDirector::SelectNext()
{
    if(Panel==EStationPanel::SurveyGame||Panel==EStationPanel::SurveyBook){SurveySelect(1);return;}
    if(Panel==EStationPanel::Relationships){TArray<FString> Entries;GetSocialMemoryBody().ParseIntoArray(Entries,TEXT("\n\n"),true);Selection=(Selection+1)%FMath::Max(1,Entries.Num());PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(IsArcMenu()){Selection=(Selection+1)%FMath::Max(1,GetArcChoiceCount());PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(Panel==EStationPanel::ArcScene||Panel==EStationPanel::ArcCredits)return;
    if(Panel==EStationPanel::CrisisBalance){AdjustCrisis(-1);return;}
    if(IsCrisisMenu()){Selection=(Selection+1)%2;PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(IsMandateMenu()){Selection=(Selection+1)%(Panel==EStationPanel::MandateCompare?3:2);PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(Panel==EStationPanel::Achievements){const int32 Before=Selection;Selection=StationAchievements::ClampRow(Selection+1);if(Before!=Selection)PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(Panel==EStationPanel::DialogueLog){LogPage=FMath::Min(LogPage+1,StationExperience::LogPages(Progress->DialogueHistory.Num())-1);return;}
    if(Panel==EStationPanel::DutyCalibrate){DutyAssist=!DutyAssist;DutyStable=0;return;}
    if(Panel==EStationPanel::DutyStart||Panel==EStationPanel::DutyChoice||Panel==EStationPanel::DutyConfirm){Selection=(Selection+1)%2;return;}
    if(IsStoryFilmActive())return;
    if(Panel==EStationPanel::SupplyStart||Panel==EStationPanel::SupplyChoice||Panel==EStationPanel::SupplyConfirm){Selection=(Selection+1)%2;return;}
    if(Panel==EStationPanel::ConflictStart||Panel==EStationPanel::ConflictChoice||Panel==EStationPanel::ConflictConfirm){Selection=(Selection+1)%2;return;}
    if(Panel==EStationPanel::DistrictChoice||Panel==EStationPanel::DistrictConfirm){Selection=(Selection+1)%2;return;}
    if(Panel==EStationPanel::StoryArchive){Selection=(Selection+1)%10;return;}
    if(Panel==EStationPanel::RationPlan){Selection=(Selection+1)%4;return;}
    if(Panel==EStationPanel::Plans||Panel==EStationPanel::Confirmation||Panel==EStationPanel::Restart||Panel==EStationPanel::ReturnHome||Panel==EStationPanel::Allocation||Panel==EStationPanel::AllocationConfirm||Panel==EStationPanel::RationConfirm)Selection=(Selection+1)%2;
}
void AStationDirector::SelectPrevious()
{
    if(Panel==EStationPanel::SurveyGame||Panel==EStationPanel::SurveyBook){SurveySelect(-1);return;}
    if(Panel==EStationPanel::Relationships){TArray<FString> Entries;GetSocialMemoryBody().ParseIntoArray(Entries,TEXT("\n\n"),true);const int Count=FMath::Max(1,Entries.Num());Selection=(Selection+Count-1)%Count;PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(IsArcMenu()){const int N=FMath::Max(1,GetArcChoiceCount());Selection=(Selection+N-1)%N;PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(Panel==EStationPanel::ArcScene||Panel==EStationPanel::ArcCredits)return;
    if(Panel==EStationPanel::CrisisBalance){AdjustCrisis(1);return;}
    if(IsCrisisMenu()){Selection=(Selection+1)%2;PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(IsMandateMenu()){const int32 N=Panel==EStationPanel::MandateCompare?3:2;Selection=(Selection+N-1)%N;PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(Panel==EStationPanel::Achievements){const int32 Before=Selection;Selection=StationAchievements::ClampRow(Selection-1);if(Before!=Selection)PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.065f,false);return;}
    if(Panel==EStationPanel::DialogueLog){LogPage=FMath::Max(0,LogPage-1);return;}
    if(IsStoryFilmActive())return;
    if(Panel==EStationPanel::StoryArchive){Selection=(Selection+9)%10;return;}
    if(Panel==EStationPanel::RationPlan){Selection=(Selection+3)%4;return;}SelectNext();
}
void AStationDirector::GoBack()
{
    if(Panel==EStationPanel::SurveyGame||Panel==EStationPanel::SurveyBook||Panel==EStationPanel::SurveyRecord){CloseSurvey();return;}
    if(Panel==EStationPanel::ArcScene)return;
    if(Panel==EStationPanel::ArcCredits){FinishArcCredits();return;}
    if(Panel==EStationPanel::ArcConfirm){const int I=ArcPendingChoice;OpenPanel(EStationPanel::ArcChoice);Selection=FMath::Max(0,I);return;}
    if(QuietIndex>=0){EndQuietMoment();return;}
    if(IsStoryFilmActive())return;
    if(IsPhysicalActionActive()){CancelPhysicalAction();return;}
    if(Starting)return;
    if(Panel==EStationPanel::CrisisConfirm){OpenPanel(EStationPanel::CrisisBalance);return;}
    if(Panel==EStationPanel::MandateConfirm){OpenPanel(EStationPanel::MandateChoice);Selection=ConfirmSelection;return;}
    if(Panel==EStationPanel::DistrictConfirm){OpenPanel(EStationPanel::DistrictChoice);Selection=ConfirmSelection;return;}
    if(Panel==EStationPanel::DutyConfirm){OpenPanel(EStationPanel::DutyChoice);Selection=ConfirmSelection;return;}
    if(Panel==EStationPanel::SupplyConfirm){OpenPanel(EStationPanel::SupplyChoice);Selection=ConfirmSelection;return;}
    if(Panel==EStationPanel::ConflictConfirm){OpenPanel(EStationPanel::ConflictChoice);Selection=ConfirmSelection;return;}
    if(Panel==EStationPanel::Confirmation){OpenPanel(EStationPanel::Plans);Selection=ConfirmSelection;return;}
    if(Panel==EStationPanel::AllocationConfirm){OpenPanel(EStationPanel::Allocation);Selection=ConfirmSelection;return;}
    if(Panel==EStationPanel::RationConfirm){OpenPanel(EStationPanel::RationPlan);Selection=3;return;}
    if(IsMenuOpen())ClosePanel();
}
void AStationDirector::RequestRestart(){if(!IsStoryFilmActive()&&!Starting&&!IsMenuOpen()&&!IsPhysicalActionActive())OpenPanel(EStationPanel::Restart);}
void AStationDirector::RequestHome()
{
    if(Panel==EStationPanel::SurveyGame||Panel==EStationPanel::SurveyBook||Panel==EStationPanel::SurveyRecord)CloseSurvey();
    if(QuietIndex>=0){EndQuietMoment();return;}
    if(IsStoryFilmActive()||Starting||IsPhysicalActionActive()||Panel==EStationPanel::ArcScene||Panel==EStationPanel::ArcCredits)return;
    if(Panel==EStationPanel::ReturnHome){ClosePanel();return;}
    ClosePanel();OpenPanel(EStationPanel::ReturnHome);
}
void AStationDirector::StartSupply()
{
    if(Progress->Started||Progress->Connections!=7||!Progress->HeardRisk||!Progress->ReadNotice)return;
    Progress->ChosenPlan=ConfirmSelection;Progress->Reserve=ConfirmSelection==0?72:38;Progress->Started=true;Progress->CutsceneDone=false;
    ClosePanel();SetInputLock(true);Starting=true;StartElapsed=0;LightCommit=false;PlayedMusic=false;CameraIndex=-1;SpeechMask=0;
    PrepareC07Timing();
    PreviousView=PC->GetViewTarget();PlayCue(TEXT("FX_Relay"),FVector(0,3600,180),.65);PlayCue(TEXT("FX_Start"),FVector(0,3600,180),.55);SaveProgress();
}
void AStationDirector::UpdateSupply(float Delta)
{
    if(!Progress->Started)return;
    if(Starting)StartElapsed+=Delta;
    const float Alpha=FMath::Clamp((StartElapsed-10)/8,0.f,1.f);
    const int32 Count=Progress->ChosenPlan==0?3:6;
    for(int32 I=0;I<PublicLights.Num();++I)
    {
        const float Power=I<Count?FMath::Clamp((StartElapsed-10-I*.24f)/7.2f,0.f,1.f):0;
        PublicLights[I]->GetLightComponent()->SetIntensity(150000*Power);
        if(FixtureMaterials[I])FixtureMaterials[I]->SetScalarParameterValue(TEXT("Glow"),160*Power);
        Shutters[I]->SetRelativeRotation(FRotator(0,0,Power*78));
    }
    if(!Starting)return;
    IdleAudio->SetVolumeMultiplier(.22f*(1-Alpha)+.05f*Alpha);RunAudio->SetVolumeMultiplier(.25f*Alpha);
    if(StartElapsed>=18&&!PlayedMusic)
    {
        PlayedMusic=true;MusicAudio=MakeLoop(TEXT("MU14_FirstLight"),FVector::ZeroVector,.23,false);
        if(!LinHe)
        {
            FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            LinHe=GetWorld()->SpawnActor<AStationEngineer>(FVector(-1450,4140,100),FRotator(0,90,0),P);LinHe->PersonName=TEXT("林禾");LinHe->ApplyIdentity(TEXT("LinHe"));LinHe->Patrol=false;
        }
        // The first stable public light motivates the gaze; it is not an idle loop.
        if(LinHe){LinHe->ApplyIdentity(TEXT("LinHe"));LinHe->PlayStationAction(TEXT("LookLight"),false);}
        PlayPlayerAction(TEXT("LookLight"));
    }
    if(StartElapsed>=30&&MusicAudio)MusicAudio->SetVolumeMultiplier(.09);
    for(int32 I=0;I<5;++I)if(StartElapsed>=C07SpeechTimes[I]&&!(SpeechMask&(1<<I)))
    {
        SpeechMask|=1<<I;C07Line=I;
        const FString Cue=FString::Printf(TEXT("EV_C07_%02d"),I+1);
        BeginPresentation(GetSubtitle(),true,Cue);
    }
    const int32 Shot=StartElapsed<8?0:(StartElapsed<18?1:(StartElapsed<30?2:3));
    if(Shot!=CameraIndex&&Cameras.IsValidIndex(Shot)){PC->SetViewTargetWithBlend(Cameras[Shot],ReducedMotion?0.f:1.4f);CameraIndex=Shot;}
    if(StartElapsed>=C07Duration)FinishSequence();
}
void AStationDirector::FinishSequence()
{
    if(!Starting)return;
    Starting=false;StartElapsed=46;Progress->CutsceneDone=true;
    if(MusicAudio)MusicAudio->FadeOut(4,0);
    StopPresentationVoice();PresentedText.Empty();PresentedSpeaker.Empty();VisibleCharacters=0;
    ApplyProgress();PC->SetViewTargetWithBlend(IsValid(PreviousView)?PreviousView.Get():PC->GetPawn(),ReducedMotion?0.f:.8f);SetInputLock(false);SaveProgress();
    Notify(TEXT("旧站已恢复公共供光。额度有限，医疗、公共活动与值守尚待分配。"));
}
FString AStationDirector::GetBaseObjective() const
{
    if(IsArcChapter())return GetArcObjective();
    if(IsCrisisChapter())return GetCrisisObjective();
    if(IsMandateChapter())return GetMandateObjective();
    if(IsAftermathChapter())return GetAftermathObjective();
    if(IsDistrictChapter())return GetDistrictObjective();
    if(Progress->DutySettled&&Progress->SeenDutyFilm)return TEXT("隔街前往回光埠入口，接取夜班回收委托");
    if(IsDutyChapter())return GetDutyObjective();
    if(IsSupplyChapter())return GetSupplyObjective();
    if(IsConflictChapter())return GetConflictObjective();
    if(IsRationChapter())return Progress->ReadRecall?TEXT("首夜已结算 · 主控制台可追查稳压器退件"):GetRationObjective();
    if(Progress->Started)
    {
        if(!Progress->HeardAllocation)return TEXT("与林禾交谈，了解下一轮配给");
        if(!Progress->MetBroker)return TEXT("与周驰核对暂定名单");
        if(!Progress->ReadLedger)return TEXT("阅读公告架上的原始配给名单");
        if(Progress->AllocationPolicy<0)return TEXT("在主控制台决定公示方式");
        if(Progress->AllocationReviewMask!=7)return TEXT("回访林禾、叶岑和周驰，听取公示后的反应");
        return TEXT("本轮公示已完成，下一笔账尚待核对");
    }
    if(!Progress->MetEngineer)return TEXT("与叶岑会合，核对旧站情况");
    if(!Progress->ReadNotice)return TEXT("阅读旧站西侧的抵押告示");
    if(!Progress->HasConnector)return TEXT("从工具箱取得备用接口");
    if(Progress->Connections!=7)
    {
        int32 Count=0;for(int I=0;I<3;++I)if(Progress->Connections&(1<<I))++Count;
        return FString::Printf(TEXT("检修三处连接  %d / 3"),Count);
    }
    if(!Progress->HeardRisk)return TEXT("向叶岑询问启动风险");
    if(!Progress->Started)return TEXT("前往公共供光主控制台，预览并确认方案");
    return TEXT("第一束公共光已到达 · 自由查看街区变化");
}
FString AStationDirector::GetPrompt() const
{
    if(IsPhysicalActionActive())return GetPhysicalActionLabel()+TEXT(" · Backspace 取消");
    return GetContextPrompt();
}
AActor* AStationDirector::GetBaseGuidanceTarget() const
{
    if(IsArcChapter())return GetArcGuidance();
    if(IsCrisisChapter())return GetCrisisGuidance();
    if(IsMandateChapter())return GetMandateGuidance();
    if(IsAftermathChapter())return GetAftermathGuidance();
    if(IsDistrictChapter())return GetDistrictGuidance();
    if(Progress->DutySettled&&Progress->SeenDutyFilm&&DistrictObjects.Num())return DistrictObjects[0];
    if(IsDutyChapter())return GetDutyGuidance();
    if(IsSupplyChapter())return GetSupplyGuidance();
    if(IsConflictChapter())return GetConflictGuidance();
    if(IsRationChapter())return GetRationGuidance();
    if(Progress->Started)
    {
        if(!Progress->HeardAllocation)return LinHe;
        if(!Progress->MetBroker)return ZhouChi;
        if(!Progress->ReadLedger)return AllocationLedger;
        if(Progress->AllocationPolicy>=0)
        {
            if(!(Progress->AllocationReviewMask&1))return LinHe;
            if(!(Progress->AllocationReviewMask&2))return Engineer;
            if(!(Progress->AllocationReviewMask&4))return ZhouChi;
            return nullptr;
        }
    }
    if(!Progress->MetEngineer)return Engineer;
    EStationObjectKind Needed=EStationObjectKind::Terminal;
    if(!Progress->ReadNotice)Needed=EStationObjectKind::Notice;
    else if(!Progress->HasConnector)Needed=EStationObjectKind::Connector;
    else if(Progress->Connections!=7)
    {
        for(int32 I=0;I<3;++I)if(!(Progress->Connections&(1<<I))){Needed=EStationObjectKind(int32(EStationObjectKind::NodeA)+I);break;}
    }
    else if(!Progress->HeardRisk)return Engineer;
    for(const auto& Object:Objects)if(Object->Kind==Needed)return Object;
    return nullptr;
}
FString AStationDirector::GetPanelTitle() const
{
    if(IsArcMenu()||Panel==EStationPanel::ArcScene||Panel==EStationPanel::ArcCredits||Panel==EStationPanel::Document&&(ArcPendingNode.StartsWith(TEXT("archive:"))||IsArcDocument(Cast<AStationObject>(ConversationTarget))))return GetArcTitle();
    if(IsCrisisMenu())return Panel==EStationPanel::CrisisBalance?TEXT("闸门另一边 · 本班余热分流"):TEXT("闸门另一边 · 联锁与复核");
    if(Panel==EStationPanel::Document&&IsCrisisDocument(Cast<AStationObject>(ConversationTarget)))return TEXT("余热旁路 · 实测与末端核验");
    if(IsMandateMenu())return Panel==EStationPanel::MandateCompare?TEXT("原件核对 · 哪一项被扩大？"):TEXT("替你签下的名字 · 申请与本人意见");
    if(Panel==EStationPanel::Document&&IsMandateDocument(Cast<AStationObject>(ConversationTarget)))return TEXT("下一期申请 · 关联担保核对");
    if(Panel==EStationPanel::Achievements)return TEXT("留光簿 · 为经历留下名字");
    if(Panel==EStationPanel::WorldRecord)return UrbanObjects.IsValidIndex(ActiveUrban)?UrbanObjects[ActiveUrban]->DisplayName:TEXT("恒昼访客阅览");
    if(Panel==EStationPanel::DialogueLog)return TEXT("对白回看 · 已经听到的声音");
    if(Panel==EStationPanel::Exploration)return NeighborhoodReady&&ActiveExploration>=0?ExplorationObjects[ActiveExploration]->DisplayName:TEXT("社区记录");
    if(Panel==EStationPanel::NeighborhoodMap)return TEXT("社区地图 · 巷道、生活与检修");
    if(Panel==EStationPanel::Relationships)return TEXT("人与回访 · 留在彼此之间的事");
    if(Panel==EStationPanel::DistrictChoice||Panel==EStationPanel::DistrictConfirm)return TEXT("光退到哪里 · 处置与异议");
    if(Panel==EStationPanel::Document&&DistrictObjects.Contains(Cast<AStationObject>(ConversationTarget)))return Cast<AStationObject>(ConversationTarget)->DisplayName;
    if(Panel==EStationPanel::DutyStart)return TEXT("借来的下班 · 本班派单");
    if(Panel==EStationPanel::DutyChoice)return TEXT("欠压与验收 · 谁替谁等待");
    if(Panel==EStationPanel::DutyConfirm)return TEXT("行动前最后复核");
    if(Panel==EStationPanel::DutyCalibrate)return GetDutyCalibrationTitle();
    if(Panel==EStationPanel::Document&&IsDutyChapter()&&IsDutyDocument(Cast<AStationObject>(ConversationTarget)))return TEXT("本班验收、工时与下班回执");
    if(Panel==EStationPanel::SupplyStart)return TEXT("签在谁的名字下 · 新批要约");
    if(Panel==EStationPanel::SupplyChoice)return TEXT("本批供货 · 风险签给谁");
    if(Panel==EStationPanel::SupplyConfirm)return TEXT("担保签署前复核");
    if(Panel==EStationPanel::Document&&((IsSupplyChapter()&&IsSupplyDocument(Cast<AStationObject>(ConversationTarget)))||ConversationTarget==WorkAccount))return TEXT("新单、工时与旧债原件");
    if(Panel==EStationPanel::ConflictStart)return TEXT("谁的备用 · 退件通知");
    if(Panel==EStationPanel::ConflictChoice)return TEXT("稳压器处置 · 共同承担");
    if(Panel==EStationPanel::ConflictConfirm)return TEXT("处置前最后复核");
    const auto* ConflictDoc=Cast<AStationObject>(ConversationTarget);
    if(Panel==EStationPanel::Document&&ConflictTraces.Contains(const_cast<AStationObject*>(ConflictDoc)))return ConflictDoc->DisplayName;
    if(Panel==EStationPanel::Document&&IsConflictChapter()&&ConflictDoc&&(ConflictDoc->Kind==EStationObjectKind::Terminal||ConflictDoc==ConsignmentCase||ConflictDoc==SupplierReturn))return TEXT("耗材、担保与退件记录");
    switch(Panel)
    {
        case EStationPanel::Conversation:return Cast<AStationEngineer>(ConversationTarget)?Cast<AStationEngineer>(ConversationTarget)->PersonName:TEXT("对话");
        case EStationPanel::Plans:return TEXT("供光方案 · 尚未启动");
        case EStationPanel::Confirmation:return TEXT("确认这一次启动");
        case EStationPanel::StoryArchive:return TEXT("家庭旧票 · 故事档案");
        case EStationPanel::RationPlan:return TEXT("首夜配给 · 勾选服务");
        case EStationPanel::RationConfirm:return TEXT("首夜配给 · 最后复核");
        case EStationPanel::Allocation:return TEXT("公示方式 · 尚未发布");
        case EStationPanel::AllocationConfirm:return TEXT("确认这一次公示");
        case EStationPanel::Restart:return TEXT("重新开始本段演示");
        case EStationPanel::ReturnHome:return TEXT("返回主页");
        case EStationPanel::Journal:return TEXT("任务档案 · ")+GetMissionTitle();
        default:
        {
            const auto* Object=Cast<AStationObject>(ConversationTarget);
            if(IsRationChapter()&&Object&&Object->IsServiceRequest())return Object->DisplayName;
            if(IsRationChapter()&&Object&&Object->Kind==EStationObjectKind::Terminal)return Progress->ReadRecall?TEXT("结算与抵押复核通知"):TEXT("首夜配给总账");
            if(Object&&Object->Kind==EStationObjectKind::AllocationLedger)return Progress->AllocationPolicy<0?TEXT("暂定配给 · 原始名单"):TEXT("第一轮配给 · 已公示");
            return Progress->Started&&Object&&Object->Kind==EStationObjectKind::Terminal?TEXT("公共供光运行记录"):TEXT("资产与服务限制告示");
        }
    }
}
FString AStationDirector::GetPanelBody() const
{
    if(IsArcMenu()||Panel==EStationPanel::ArcScene||Panel==EStationPanel::ArcCredits||Panel==EStationPanel::Document&&(ArcPendingNode.StartsWith(TEXT("archive:"))||IsArcDocument(Cast<AStationObject>(ConversationTarget))))return GetArcBody();
    if(IsCrisisMenu()||Panel==EStationPanel::Document&&IsCrisisDocument(Cast<AStationObject>(ConversationTarget)))return GetCrisisBody();
    if(IsMandateMenu()||Panel==EStationPanel::Document&&IsMandateDocument(Cast<AStationObject>(ConversationTarget))||Panel==EStationPanel::Journal&&IsMandateChapter())return GetMandateBody();
    if(Panel==EStationPanel::Achievements)return TEXT("");
    if(Panel==EStationPanel::WorldRecord)return GetUrbanBody();
    if(Panel==EStationPanel::DialogueLog)return GetDialogueLogBody();
    if(Panel==EStationPanel::Exploration)return GetExplorationBody();
    if(Panel==EStationPanel::NeighborhoodMap)return TEXT("M 收起地图。探索记录只在实际看过后显示，不是主线收集门槛。");
    if(Panel==EStationPanel::Relationships)return GetSocialMemoryBody();
    if(Panel==EStationPanel::DistrictChoice||Panel==EStationPanel::DistrictConfirm||Panel==EStationPanel::Document&&DistrictObjects.Contains(Cast<AStationObject>(ConversationTarget)))return GetDistrictBody();
    const auto* ConflictDoc=Cast<AStationObject>(ConversationTarget);
    if(Panel==EStationPanel::DutyStart||Panel==EStationPanel::DutyChoice||Panel==EStationPanel::DutyConfirm||Panel==EStationPanel::DutyCalibrate||(Panel==EStationPanel::Document&&(IsDutyChapter()&&IsDutyDocument(ConflictDoc)||ConflictDoc&&ConflictDoc==DutyLoadBank)))return GetDutyBody();
    if(Panel==EStationPanel::SupplyStart||Panel==EStationPanel::SupplyChoice||Panel==EStationPanel::SupplyConfirm||(Panel==EStationPanel::Document&&((IsSupplyChapter()&&IsSupplyDocument(ConflictDoc))||ConflictDoc==WorkAccount)))return GetSupplyBody();
    if(Panel==EStationPanel::Document&&ConflictTraces.Contains(const_cast<AStationObject*>(ConflictDoc)))return GetConflictTraceDocument(const_cast<AStationObject*>(ConflictDoc));
    if(Panel==EStationPanel::ConflictStart||Panel==EStationPanel::ConflictChoice||Panel==EStationPanel::ConflictConfirm||(Panel==EStationPanel::Document&&IsConflictChapter()&&ConflictDoc&&(ConflictDoc->Kind==EStationObjectKind::Terminal||ConflictDoc==ConsignmentCase||ConflictDoc==SupplierReturn)))return GetConflictBody();
    if(Panel==EStationPanel::StoryArchive)return TEXT("八年前的家庭旧票，与几轮供光、供货和值守留下的片段。\n\n重播只回看故事，不重新派单或记账；长按 E 可跳过。");
    if(Panel==EStationPanel::RationPlan||Panel==EStationPanel::RationConfirm)return GetRationChoiceBody();
    if(Panel==EStationPanel::Allocation||Panel==EStationPanel::AllocationConfirm)return GetAllocationChoiceBody();
    if(Panel==EStationPanel::Journal)
    {
        if(IsAftermathChapter())return GetAftermathJournal();
        if(IsDutyChapter())return GetDutyObjective();
    if(IsSupplyChapter())return GetSupplyObjective();
    if(IsConflictChapter())return GetConflictObjective();
        if(IsRationChapter())return GetRationObjective();
        if(Progress->Started)return TEXT("M06 / 谁先见光\n\n名单已经排好，规则才准备公开。先听当事人说完，再决定如何公示。\n\n当前：")+GetObjective();
        const TCHAR* Steps[]={TEXT("与叶岑会合"),TEXT("阅读抵押告示"),TEXT("取得备用接口"),TEXT("检修三处连接"),TEXT("了解启动风险"),TEXT("确认供光方案"),TEXT("查看第一束公共光")};
        FString Record=TEXT("M05 / 旧站复燃\n\n应急额度有限。把设备重新接通，只是分配问题的开始。\n\n");
        const int32 Stage=GetQuestStage();
        for(int32 I=0;I<7;++I)Record+=FString::Printf(TEXT("%02d  %s  %s\n"),I+1,I<Stage?TEXT("[已完成]"):I==Stage?TEXT("[当前]"):TEXT("[待处理]"),Steps[I]);
        return Record;
    }
    if(Panel==EStationPanel::Conversation){const auto Lines=GetConversation();return Lines.IsValidIndex(ConversationLine)?Lines[ConversationLine]:TEXT("");}
    if(Panel==EStationPanel::Plans)return TEXT("保守启动：点亮近街三个供光单元。剩余额度 72 / 100。远街继续等待，设备负荷较稳。\n\n大功率启动：点亮六个供光单元。剩余额度 38 / 100。覆盖更广，临时过载会留下故障隐患。\n\n两种方案都恢复公共服务。这里显示的是本轮预估；选择后仍须再次确认。");
    if(Panel==EStationPanel::Confirmation)return ConfirmSelection==0?TEXT("将按保守方案启动。近街先获得供光；远街本轮仍无有效日照。剩余额度 72。抵押权与外部供能限制仍然存在。\n\n确认后设备才会运行。"):TEXT("将按大功率方案启动。六个单元获得首轮供光；剩余额度降至 38，并记录临时过载风险。抵押权与外部供能限制仍然存在。\n\n确认后设备才会运行。");
    if(Panel==EStationPanel::Restart)return TEXT("重新开始将清除这段旧站演示的进度与方案选择，关闭新增公共供光。社区模型和原始 Blender 文件不受影响。");
    if(Panel==EStationPanel::ReturnHome)return TEXT("已完成的任务与选择会保存。再次选择“继续游戏”时，将从最近的任务检查点进入社区。尚未完成的调压与维修动作需要重新操作。");
    const auto* DocumentObject=Cast<AStationObject>(ConversationTarget);
    if(IsRationChapter()&&DocumentObject&&(DocumentObject->IsServiceRequest()||DocumentObject->Kind==EStationObjectKind::Terminal))return GetRationDocument(DocumentObject);
    if(DocumentObject&&DocumentObject->Kind==EStationObjectKind::AllocationLedger)return GetAllocationDocument();
    if(Progress->Started&&DocumentObject&&DocumentObject->Kind==EStationObjectKind::Terminal)return FString::Printf(TEXT("本轮方案：%s\n剩余额度：%d / 100\n供光单元：%d / 6\n维护记录：%s\n\n医疗、公共活动、值守尚未分配。光站只恢复了一次公共供光，没有解除产权与认证限制。"),Progress->ChosenPlan==0?TEXT("保守启动"):TEXT("大功率启动"),Progress->Reserve,Progress->ChosenPlan==0?3:6,Progress->ChosenPlan==0?TEXT("储备较稳"):TEXT("临时过载风险已登记"));
    return TEXT("衡昼资产管理 · 旧公共光站\n\n设备与地址仍在抵押登记中。应急额度只恢复短时公共供光，不构成免费永久服务，不解除外部供能、耗材和维修义务。\n\n当前账面额度：100。各申请区所需总量尚待核定。\n\n林照的旧工作证可以接入检修；正式启动仍需三处连接确认和方案复核。");
}
FString AStationDirector::GetSubtitle() const
{
    if(Panel==EStationPanel::ArcScene||IsStoryFilmActive()||IsAmbientStoryActive())return PresentedSpeaker+TEXT("：")+PresentedText;
    static const TCHAR* Lines[]={TEXT("林禾：以前的人，天天都这样吗？"),TEXT("林照：录像里是。"),TEXT("林禾：他们有时候也嫌太阳太大。"),TEXT("林照：嗯。"),TEXT("林禾：真奢侈。")};
    return Starting&&C07Line>=0&&C07Line<5?Lines[C07Line]:TEXT("");
}
FString AStationDirector::GetSnapshotJson() const
{
    int32 Count=0;
    for(const auto& Light:PublicLights)if(Light&&Light->GetLightComponent()->Intensity>1)++Count;
    FString Text=FString::Printf(TEXT("{\"met\":%s,\"notice\":%s,\"connector\":%s,\"risk\":%s,\"mask\":%d,\"plan\":%d,\"reserve\":%d,\"started\":%s,\"starting\":%s,\"panel\":%d,\"selection\":%d,\"engineer_paused\":%s,\"focus\":\"%s\",\"input_bound\":%s,\"input_locked\":%s,\"public_units\":%d}"),
        Progress->MetEngineer?TEXT("true"):TEXT("false"),Progress->ReadNotice?TEXT("true"):TEXT("false"),Progress->HasConnector?TEXT("true"):TEXT("false"),Progress->HeardRisk?TEXT("true"):TEXT("false"),Progress->Connections,Progress->ChosenPlan,Progress->Reserve,
        Progress->Started?TEXT("true"):TEXT("false"),Starting?TEXT("true"):TEXT("false"),int32(Panel),Selection,Engineer->ConversationPaused?TEXT("true"):TEXT("false"),Focus?*Focus->GetName():TEXT(""),BoundInput?TEXT("true"):TEXT("false"),InputLocked?TEXT("true"):TEXT("false"),Progress->Started?Count:0);
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))
    {
        TSharedPtr<FJsonObject> State;
        if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),State))
        {
            const AActor* Guide=GetGuidanceTarget();State->SetStringField(TEXT("guide"),IsValid(Guide)?Guide->GetName():TEXT(""));
            State->SetStringField(TEXT("mission"),GetMissionTitle());State->SetStringField(TEXT("objective"),GetObjective());
            State->SetNumberField(TEXT("quest_stage"),GetQuestStage());State->SetStringField(TEXT("film"),StoryFilm);
            State->SetStringField(TEXT("cinematic_subtitle"),GetSubtitle());
            FJsonSerializer::Serialize(State,TJsonWriterFactory<>::Create(&Text));
        }
    }
#endif
    return Text;
}
void AStationDirector::AutomationReset(bool DeleteCheckpoint)
{
#if WITH_EDITOR
    if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))return;
    CancelCommunityVignette();FinishStoryFilm(false);StoryAutoAttempted=true;
    AutomationOccluder(false);
    CancelPhysicalAction(false);
    ResetSprint();SprintExertion=0;NextReactionTime=0;ReactionCount=0;ReactionKey.Empty();
    if(Starting)FinishSequence();ClosePanel();if(DeleteCheckpoint)UGameplayStatics::DeleteGameInSlot(Station::Slot,0);
    Progress=NewObject<UStationProgress>(this);DistrictPendingChoice=-1;StartElapsed=0;EReady=true;ApplyProgress();
    for(auto& Light:PublicLights)Light->GetLightComponent()->SetIntensity(0);
    for(auto& Mat:FixtureMaterials)if(Mat)Mat->SetScalarParameterValue(TEXT("Glow"),0);
    for(auto& Shutter:Shutters)Shutter->SetRelativeRotation(FRotator::ZeroRotator);
    IdleAudio->SetVolumeMultiplier(.22);RunAudio->SetVolumeMultiplier(0);
    if(LinHe){LinHe->Destroy();LinHe=nullptr;}
#endif
}
void AStationDirector::AutomationReload()
{
#if WITH_EDITOR
    if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))return;
    CancelCommunityVignette();FinishStoryFilm(false);StoryAutoAttempted=true;
    CancelPhysicalAction(false);if(Starting)FinishSequence();ClosePanel();LoadProgress();ApplyProgress();
#endif
}
void AStationDirector::AutomationOccluder(bool Enabled)
{
#if WITH_EDITOR
    if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))return;
    if(TestOccluder){TestOccluder->Destroy();TestOccluder=nullptr;}
    if(!Enabled)return;
    for(const auto& Object:Objects)if(Object->Kind==EStationObjectKind::NodeA)
    {
        FVector Position=Object->GetActorLocation()+FVector(0,70,100);
        auto* Wall=GetWorld()->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator);
        Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Wall->SetActorScale3D(FVector(1,.1,2.2));Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));TestOccluder=Wall;break;
    }
#endif
}
