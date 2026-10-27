#include "StationPrototype.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void AStationDirector::UpdateDialogueCamera()
{
    auto* Person=Cast<AStationEngineer>(ConversationTarget);
    auto* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    if(!Person||!Player)return;
    if(!DialoguePreviousView)
    {
        AActor* View=PC->GetViewTarget();
        if(DialogueCameras.Contains(Cast<ACameraActor>(View)))View=PC->GetPawn();
        DialoguePreviousView=View;
    }
    const FVector Axis=(Person->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D();
    const FVector Side=FVector::CrossProduct(FVector::UpVector,Axis);
    const bool PlayerSpeaking=PresentedSpeaker==TEXT("林照");
    ACharacter* Speaker=PlayerSpeaking?Player:Person;
    FVector Aim=Speaker->GetActorLocation()+FVector(0,0,53);
    if(Speaker->GetMesh()->DoesSocketExist(TEXT("head")))Aim=Speaker->GetMesh()->GetSocketLocation(TEXT("head"))-FVector(0,0,11);
    FVector From=Aim+Axis*(PlayerSpeaking?235.f:-235.f)+Side*110.f+FVector(0,0,12);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DialogueCamera),false,Player);
    Query.AddIgnoredActor(Person);Query.AddIgnoredActor(this);
    FHitResult Hit;
    if(GetWorld()->LineTraceSingleByChannel(Hit,Aim,From,ECC_Visibility,Query))From=Hit.Location+(Aim-Hit.Location).GetSafeNormal()*18.f;
    if(FVector::Dist(From,Aim)<100.f)
    {
        From=(Player->GetActorLocation()+Person->GetActorLocation())*.5f+Side*275.f+FVector(0,0,70);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Aim,From,ECC_Visibility,Query))
        { RestoreDialogueCamera();return; }
    }
    if(DialogueCameras.Num()<2)
    {
        for(int32 I=DialogueCameras.Num();I<2;++I)
        {
            auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
            auto* Fill=NewObject<UPointLightComponent>(Camera,TEXT("ConversationFill"));Camera->AddInstanceComponent(Fill);Fill->SetupAttachment(Camera->GetRootComponent());
            Fill->SetMobility(EComponentMobility::Movable);Fill->SetRelativeLocation(FVector(20,45,35));Fill->SetIntensityUnits(ELightUnits::Lumens);
            Fill->SetSourceRadius(35.f);Fill->SetAttenuationRadius(450.f);Fill->SetLightColor(FLinearColor(.78f,.86f,1.f));Fill->SetCastShadows(false);Fill->SetIntensity(0);Fill->RegisterComponent();
            Camera->GetCameraComponent()->SetFieldOfView(48.f);DialogueCameras.Add(Camera);
        }
    }
    DialogueCameraSlot=(DialogueCameraSlot+1)%2;
    auto* Camera=DialogueCameras[DialogueCameraSlot].Get();
    for(auto& View:DialogueCameras)if(View)if(auto* Fill=View->FindComponentByClass<UPointLightComponent>())Fill->SetIntensity(View.Get()==Camera&&IsAftermathChapter()?750.f:0.f);
    Camera->SetActorLocationAndRotation(From,(Aim-From).Rotation());
    PC->SetViewTargetWithBlend(Camera,ReducedMotion?0.f:.5f);
    FRotator Facing=Axis.Rotation();Player->SetActorRotation(FRotator(0,Facing.Yaw,0));
}
void AStationDirector::RestoreDialogueCamera()
{
    for(auto& Camera:DialogueCameras)if(Camera)if(auto* Fill=Camera->FindComponentByClass<UPointLightComponent>())Fill->SetIntensity(0);
    if(IsValid(PC)&&DialoguePreviousView)PC->SetViewTargetWithBlend(IsValid(DialoguePreviousView)?DialoguePreviousView.Get():PC->GetPawn(),ReducedMotion?0.f:.35f);
    DialoguePreviousView=nullptr;
}
