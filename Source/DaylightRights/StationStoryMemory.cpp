#include "StationPrototype.h"
#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/RectLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

void AStationDirector::CreateStoryMemory()
{
    if(StoryMemorySet)return;
    const FVector Origin(-12000,10000,0);
    StoryMemorySet=GetWorld()->SpawnActor<AActor>(Origin,FRotator::ZeroRotator);
    auto* Root=NewObject<USceneComponent>(StoryMemorySet);StoryMemorySet->SetRootComponent(Root);StoryMemorySet->AddInstanceComponent(Root);Root->RegisterComponent();Root->SetWorldLocation(Origin);
    auto Add=[&](FVector Position,FVector Size,FLinearColor Color,const TCHAR* Shape=TEXT("Cube"))
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(StoryMemorySet);StoryMemorySet->AddInstanceComponent(Mesh);Mesh->SetupAttachment(Root);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
        Mesh->SetRelativeLocation(Position);Mesh->SetRelativeScale3D(Size);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->RegisterComponent();
        if(auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/Gameplay/Materials/M_StationDevice.M_StationDevice")))
        {
            auto* Mat=UMaterialInstanceDynamic::Create(Parent,Mesh);Mat->SetVectorParameterValue(TEXT("Tint"),Color);Mat->SetScalarParameterValue(TEXT("Glow"),0);Mesh->SetMaterial(0,Mat);
        }
        return Mesh;
    };
    const FLinearColor Wall(.43f,.38f,.29f),Wood(.15f,.075f,.029f),Enamel(.64f,.68f,.57f),Dark(.016f,.021f,.020f);
    auto* Sky=Add(FVector(0,-490,200),FVector(8,.02,6),FLinearColor(.48f,.61f,.69f));
    if(auto* Mat=Cast<UMaterialInstanceDynamic>(Sky->GetMaterial(0)))Mat->SetScalarParameterValue(TEXT("Glow"),.45f);
    Add(FVector(0,0,-5),FVector(4.2,4.0,.1),FLinearColor(.25f,.22f,.17f));
    Add(FVector(-145,-125,150),FVector(1.3,.12,3),Wall);Add(FVector(145,-125,150),FVector(1.3,.12,3),Wall);
    Add(FVector(0,-125,50),FVector(1.6,.12,1),Wall);Add(FVector(0,-125,275),FVector(1.6,.12,.5),Wall);
    Add(FVector(-207,0,150),FVector(.12,2.5,3),Wall);
    // A real opening and mullions cast a moving shadow onto the table and floor.
    for(float X:{-80.f,0.f,80.f})Add(FVector(X,-120,175),FVector(.045,.06,1.55),Wood);
    for(float Z:{100.f,175.f,250.f})Add(FVector(0,-120,Z),FVector(1.65,.06,.045),Wood);
    Add(FVector(0,-113,98),FVector(1.8,.28,.06),Wood);
    Add(FVector(-30,42,76),FVector(1.18,.70,.045),Wood);
    for(float X:{-81.f,21.f})for(float Y:{15.f,69.f})Add(FVector(X,Y,37),FVector(.05,.05,.74),Wood);
    Add(FVector(66,56,44),FVector(.40,.40,.04),Wood);
    for(float X:{49.f,83.f})for(float Y:{39.f,73.f})Add(FVector(X,Y,22),FVector(.035,.035,.44),Wood);
    Add(FVector(66,76,74),FVector(.4,.035,.12),Wood);
    for(float X:{49.f,83.f})Add(FVector(X,76,64),FVector(.035,.035,.42),Wood);
    Add(FVector(-52,34,83),FVector(.095,.095,.095),Enamel,TEXT("Cylinder"));
    Add(FVector(-52,34,87.9f),FVector(.078,.078,.002),Dark,TEXT("Cylinder"));
    for(int32 I=0;I<=12;++I)
    {
        const float A=-PI*.5f+PI*I/12.f;
        Add(FVector(-47+4*FMath::Cos(A),34,83+4*FMath::Sin(A)),FVector(.018),Enamel,TEXT("Sphere"));
    }
    Add(FVector(-10,35,78.8f),FVector(.20,.20,.012),Enamel,TEXT("Cylinder"));
    // Scratches, a repaired board and the vacant chair carry the memory without an invented family portrait.
    Add(FVector(-30,42,78.5f),FVector(.018,.64,.001),FLinearColor(.09f,.043f,.02f));
    Add(FVector(-85,44,78.4f),FVector(.04,.65,.002),FLinearColor(.22f,.13f,.056f));
    StoryMemorySun=GetWorld()->SpawnActor<ARectLight>(Origin+FVector(-120,-240,360),FRotator::ZeroRotator);
    auto* Light=CastChecked<URectLightComponent>(StoryMemorySun->GetLightComponent());Light->SetMobility(EComponentMobility::Movable);
    Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetSourceWidth(12);Light->SetSourceHeight(12);Light->SetAttenuationRadius(1400);
    Light->SetLightColor(FLinearColor(1,.77f,.47f));Light->SetIntensity(62000);Light->SetCastShadows(true);
    UpdateStoryMemory();
}
void AStationDirector::UpdateStoryMemory()
{
    if(!StoryMemorySun)return;
    const FVector Origin(-12000,10000,0);const float Phase=FMath::Clamp(StoryElapsed/13.f,0.f,1.f);
    const FVector From=Origin+FVector(FMath::Lerp(-130.f,80.f,Phase),-250,355),To=Origin+FVector(-30,70,25);
    StoryMemorySun->SetActorLocationAndRotation(From,(To-From).Rotation());
    const float Fade=1.f-FMath::Clamp((StoryElapsed-11.f)/2.f,0.f,1.f);
    StoryMemorySun->GetLightComponent()->SetIntensity(62000*Fade);
}
