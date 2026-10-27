#include "StationPrototype.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "StationMovementTuning.h"
#include "StationRealism.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

namespace StationCharacter
{
    USkeletalMesh* Mesh(const FString& Identity)
    {
        return LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/BZ/Characters/%s/SK_%s.SK_%s"),*Identity,*Identity,*Identity));
    }
    UAnimationAsset* Clip(const FString& Identity,const FString& Action)
    {
        if(StationRealism::Enabled()&&(Action==TEXT("Idle")||Action==TEXT("Walk")||Action==TEXT("Jog")||Action==TEXT("Run")))
            if(auto* Asset=LoadObject<UAnimationAsset>(nullptr,*FString::Printf(TEXT("/Game/BZ/RealismV24/Motion/%s/A24_%s_%s.A24_%s_%s"),*Identity,*Identity,*Action,*Identity,*Action)))return Asset;
        return LoadObject<UAnimationAsset>(nullptr,*FString::Printf(TEXT("/Game/BZ/Characters/%s/Animations/A_%s_%s.A_%s_%s"),*Identity,*Identity,*Action,*Identity,*Action));
    }
    UBlendSpace1D* Locomotion(const FString& Identity)
    {
        if(StationRealism::Enabled())
            if(auto* Blend=LoadObject<UBlendSpace1D>(nullptr,*FString::Printf(TEXT("/Game/BZ/RealismV24/Motion/%s/BS24_%s_Locomotion.BS24_%s_Locomotion"),*Identity,*Identity,*Identity)))return Blend;
        return LoadObject<UBlendSpace1D>(nullptr,*FString::Printf(TEXT("/Game/BZ/Characters/%s/Animations/BS_%s_Locomotion.BS_%s_Locomotion"),*Identity,*Identity,*Identity));
    }
    bool IsLocomotion(const FString& Action)
    {
        return Action==TEXT("Idle")||Action==TEXT("Walk")||Action==TEXT("Jog")||Action==TEXT("Run");
    }
    void DriveLocomotion(USkeletalMeshComponent* Skin,float Speed)
    {
        auto* Instance=Skin->GetSingleNodeInstance();
        if(Instance)Instance->SetBlendSpacePosition(FVector(Speed,0,0));
        // Each sample contains one phase-aligned left/right stride. Compensate
        // for the duration-weighted stride between samples, instead of assuming
        // that a linear speed-axis blend itself guarantees a planted foot.
        float Rate=1.f;
        if(auto* Blend=Instance?Cast<UBlendSpace1D>(Instance->GetCurrentAsset()):nullptr)
        {
            const auto& Samples=Blend->GetBlendSamples();
            for(int32 I=1;I<Samples.Num();++I)
            {
                const auto& A=Samples[I-1];const auto& B=Samples[I];
                const float VA=A.SampleValue.X,VB=B.SampleValue.X;
                if(Speed<VA||Speed>VB||!A.Animation||!B.Animation)continue;
                const float Weight=(Speed-VA)/FMath::Max(1.f,VB-VA);
                const float TA=A.Animation->GetPlayLength(),TB=B.Animation->GetPlayLength();
                const float Seconds=FMath::Lerp(TA,TB,Weight),Stride=FMath::Lerp(VA*TA,VB*TB,Weight);
                if(Speed>3&&Stride>1)Rate=FMath::Clamp(Speed*Seconds/Stride,.25f,2.f);
                break;
            }
        }
        Skin->SetPlayRate(Rate);
    }
}
// Import calls this after importing each skeleton's three locomotion clips.
// The resulting saved asset also works in standalone and packaged builds.
bool AStationDirector::BuildLocomotionAsset(const FString& Identity,bool UseRealism)
{
#if WITH_EDITOR
    auto Load=[&](const TCHAR* Action)
    {
        const FString Path=UseRealism?FString::Printf(TEXT("/Game/BZ/RealismV24/Motion/%s/A24_%s_%s.A24_%s_%s"),*Identity,*Identity,Action,*Identity,Action):FString::Printf(TEXT("/Game/BZ/Characters/%s/Animations/A_%s_%s.A_%s_%s"),*Identity,*Identity,Action,*Identity,Action);
        return LoadObject<UAnimSequence>(nullptr,*Path);
    };
    auto* Idle=Load(TEXT("Idle"));auto* Walk=Load(TEXT("Walk"));auto* Jog=Load(TEXT("Jog"));auto* Run=Load(TEXT("Run"));
    if(!Idle||!Walk||!Jog||!Run||Idle->GetSkeleton()!=Walk->GetSkeleton()||Idle->GetSkeleton()!=Jog->GetSkeleton()||Idle->GetSkeleton()!=Run->GetSkeleton())return false;
    const FString Name=UseRealism?FString::Printf(TEXT("BS24_%s_Locomotion"),*Identity):FString::Printf(TEXT("BS_%s_Locomotion"),*Identity);
    const FString PackageName=UseRealism?FString::Printf(TEXT("/Game/BZ/RealismV24/Motion/%s/%s"),*Identity,*Name):FString::Printf(TEXT("/Game/BZ/Characters/%s/Animations/%s"),*Identity,*Name);
    const FString ObjectPath=PackageName+TEXT(".")+Name;
    auto* Blend=LoadObject<UBlendSpace1D>(nullptr,*ObjectPath);
    UPackage* Package=Blend?Blend->GetOutermost():CreatePackage(*PackageName);
    Package->FullyLoad();
    if(!Blend)Blend=NewObject<UBlendSpace1D>(Package,*Name,RF_Public|RF_Standalone);
    Blend->SetSkeleton(Idle->GetSkeleton());
    FBlendParameter& Axis=const_cast<FBlendParameter&>(Blend->GetBlendParameter(0));
    Axis.DisplayName=TEXT("Ground speed cm/s");Axis.Min=0;Axis.Max=600;Axis.GridNum=10;Axis.bSnapToGrid=false;
    // CharacterMovement already smooths acceleration. A second delayed blend
    // used to continue playing the previous gait after the body changed speed.
    Blend->TargetWeightInterpolationSpeedPerSec=UseRealism?12.f:0.f;Blend->bScaleAnimation=false;
    while(Blend->GetNumberOfBlendSamples()>0)Blend->DeleteSample(0);
    Blend->AddSample(Idle,FVector(0,0,0));Blend->AddSample(Walk,FVector(StationMovement::Stroll,0,0));
    Blend->AddSample(Jog,FVector(StationMovement::Travel,0,0));Blend->AddSample(Run,FVector(StationMovement::Sprint,0,0));
    Blend->ValidateSampleData();Blend->ResampleData();Blend->PostEditChange();Blend->MarkPackageDirty();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Package,Blend,*FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension()),Args);
#else
    return false;
#endif
}
void AStationEngineer::ApplyIdentity(const FString& Identity)
{
    if(IdentityKey==Identity)return;
    if(auto* Skin=StationCharacter::Mesh(Identity))
    {
        IdentityKey=Identity;MotionKey.Empty();GetMesh()->SetSkeletalMeshAsset(Skin);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        const float HalfHeight=Identity==TEXT("LinHe")?80.f:84.f;
        GetCapsuleComponent()->SetCapsuleHalfHeight(HalfHeight);GetMesh()->SetRelativeLocation(FVector(0,0,-HalfHeight-StationMovement::FloorClearance));
        GetMesh()->SetRelativeRotation(FRotator(0,-90,0));PlayStationAction(TEXT("Idle"));
    }
}
void AStationEngineer::PlayStationAction(const FString& Action,bool Loop)
{
    auto* Instance=GetMesh()->GetSingleNodeInstance();
    if(IdentityKey.IsEmpty()||(MotionKey==Action&&Loop&&Instance&&Instance->GetCurrentAsset()==MotionAsset&&MotionAsset))return;
    UAnimationAsset* Clip=StationCharacter::IsLocomotion(Action)?StationCharacter::Locomotion(IdentityKey):nullptr;
    if(!Clip)Clip=StationCharacter::Clip(IdentityKey,Action);
    if(Clip)
    {
        if(MotionAsset!=Clip||!Loop||!Instance||Instance->GetCurrentAsset()!=Clip)GetMesh()->PlayAnimation(Clip,Loop);
        MotionAsset=Clip;MotionKey=Action;
        ActionUntil=Loop?0:GetWorld()->GetTimeSeconds()+Clip->GetPlayLength();
    }
}
void AStationEngineer::UpdateIdentityMotion()
{
    const FString BaseIdentity=PersonName==TEXT("林禾")?TEXT("LinHe"):PersonName==TEXT("周驰")||PersonName==TEXT("沈砚")?TEXT("ZhouChi"):PersonName==TEXT("阿湛")?TEXT("LinZhao"):TEXT("YeCen");
    ApplyIdentity(BaseIdentity);
    if(ConversationPaused||GetWorld()->GetTimeSeconds()<ActionUntil)return;
    // Keep a sustained work clip alive. Switching to Idle before the routine
    // restores Repair/LookLight would restart that clip every frame.
    if(ActivityDwelling&&MotionKey==ActivityAction&&(ActivityAction==TEXT("Repair")||ActivityAction==TEXT("LookLight")))return;
    const float Speed=GetVelocity().Size2D();PlayStationAction(Speed>410?TEXT("Run"):Speed>175?TEXT("Jog"):Speed>8?TEXT("Walk"):TEXT("Idle"));
    if(Cast<UBlendSpace1D>(MotionAsset))StationCharacter::DriveLocomotion(GetMesh(),Speed);
    else if(MotionKey==TEXT("Walk"))GetMesh()->SetPlayRate(FMath::Clamp(Speed/StationMovement::Stroll,.1f,1.5f));
}
void AStationDirector::PlayPlayerAction(const FString& Action)
{
    if(auto* Clip=StationCharacter::Clip(TEXT("LinZhao"),Action))
    {
        PlayerAction=Action;PlayerActionUntil=GetWorld()->GetTimeSeconds()+Clip->GetPlayLength();
        PlayerMotionKey.Empty();
    }
}
void AStationDirector::UpdateCharacterMotion()
{
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!Character)return;
    const float Delta=GetWorld()->GetDeltaSeconds();TickPhysicalAction(Delta);
    auto* Skin=Character->GetMesh();
    if(!PlayerSkinConfigured)
    {
        if(auto* Mesh=StationCharacter::Mesh(TEXT("LinZhao")))
        {
            Skin->SetSkeletalMeshAsset(Mesh);Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Skin->SetRelativeLocation(FVector(0,0,-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-StationMovement::FloorClearance));
            Skin->SetRelativeRotation(FRotator(0,-90,0));Skin->SetRelativeScale3D(FVector(1));PlayerSkinConfigured=true;
            Character->GetCharacterMovement()->MaxAcceleration=StationRealism::Enabled()?1350.f:1800.f;
            Character->GetCharacterMovement()->BrakingDecelerationWalking=StationRealism::Enabled()?1550.f:1700.f;
            Character->GetCharacterMovement()->RotationRate=FRotator(0,StationRealism::Enabled()?360.f:480.f,0);
        }
    }
    if(!PlayerSkinConfigured)return;
    const float Speed=Character->GetVelocity().Size2D();
    MotionSpeed=Speed<3?0:Speed;
    FString Next=Speed>StationMovement::Travel+70?TEXT("Run"):Speed>StationMovement::Stroll+60?TEXT("Jog"):Speed>8?TEXT("Walk"):TEXT("Idle");
    if(Character->GetCharacterMovement()->IsFalling())Next=TEXT("Jump");
    else if(Panel==EStationPanel::Conversation)Next=PresentedSpeaker==TEXT("林照")&&(IsVoicePlaying()||IsTyping())?TEXT("Talk"):TEXT("Idle");
    else if(GetWorld()->GetTimeSeconds()<PlayerActionUntil&&Speed<8)Next=PlayerAction;
    if(Starting)Next=GetWorld()->GetTimeSeconds()<PlayerActionUntil?PlayerAction:TEXT("Idle");
    if(Starting&&C07Line>=0&&(IsVoicePlaying()||IsTyping()))Next=PresentedSpeaker==TEXT("林照")?TEXT("Talk"):TEXT("Idle");
    // The instrument holds the tool gesture after setup, including its loop flag.
    // It must not fall back to the expired one-shot action while input is locked.
    if(Panel==EStationPanel::DutyCalibrate)Next=TEXT("Repair");
    if(IsStoryFilmActive())Next=StoryPlayerMotion.IsEmpty()?TEXT("Idle"):StoryPlayerMotion;
    UAnimationAsset* NextAsset=StationCharacter::IsLocomotion(Next)?StationCharacter::Locomotion(TEXT("LinZhao")):nullptr;
    if(!NextAsset)NextAsset=StationCharacter::Clip(TEXT("LinZhao"),Next);
    auto* ActualInstance=Skin->GetSingleNodeInstance();
    if(NextAsset&&(PlayerMotionAsset!=NextAsset||PlayerMotionKey.IsEmpty()||!ActualInstance||ActualInstance->GetCurrentAsset()!=NextAsset))
    {
        Skin->PlayAnimation(NextAsset,StationCharacter::IsLocomotion(Next)||Next==TEXT("Talk")||Panel==EStationPanel::DutyCalibrate);PlayerMotionAsset=NextAsset;
    }
    PlayerMotionKey=Next;
    if(Cast<UBlendSpace1D>(PlayerMotionAsset))StationCharacter::DriveLocomotion(Skin,MotionSpeed);
    else if(Next==TEXT("Walk"))Skin->SetPlayRate(FMath::Clamp(Speed/StationMovement::Stroll,.1f,3.0f));
    else if(Next==TEXT("Jog"))Skin->SetPlayRate(FMath::Clamp(Speed/StationMovement::Travel,.2f,1.4f));
    else if(Next==TEXT("Run"))Skin->SetPlayRate(FMath::Clamp(Speed/StationMovement::Sprint,.2f,1.4f));
    else Skin->SetPlayRate(1);
}
FString AStationDirector::GetCharacterStateJson() const
{
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    auto* Instance=Character?Character->GetMesh()->GetSingleNodeInstance():nullptr;FVector BlendInput=FVector::ZeroVector,FilteredInput=FVector::ZeroVector;
    if(Instance)Instance->GetBlendSpaceState(BlendInput,FilteredInput);
    return FString::Printf(TEXT("{\"player_mesh\":\"%s\",\"player_motion\":\"%s\",\"engineer_identity\":\"%s\",\"engineer_motion\":\"%s\",\"linhe_identity\":\"%s\",\"pending_action\":\"%s\",\"locomotion_blend\":%s,\"ground_speed\":%.2f,\"sprint_held\":%s,\"sprint_active\":%s,\"max_walk_speed\":%.2f,\"runtime_asset\":\"%s\",\"animation_phase\":%.5f,\"blend_speed\":%.2f,\"play_rate\":%.4f}"),
        Character&&Character->GetMesh()->GetSkeletalMeshAsset()?*Character->GetMesh()->GetSkeletalMeshAsset()->GetName():TEXT("none"),*PlayerMotionKey,Engineer?*Engineer->IdentityKey:TEXT("none"),Engineer?*Engineer->MotionKey:TEXT("none"),LinHe?*LinHe->IdentityKey:TEXT("none"),*PendingAction,Cast<UBlendSpace1D>(PlayerMotionAsset)?TEXT("true"):TEXT("false"),Character?Character->GetVelocity().Size2D():0.f,(LeftSprintHeld||RightSprintHeld)?TEXT("true"):TEXT("false"),SprintActive?TEXT("true"):TEXT("false"),Character?Character->GetCharacterMovement()->MaxWalkSpeed:0.f,Instance&&Instance->GetCurrentAsset()?*Instance->GetCurrentAsset()->GetName():TEXT("none"),Instance?Instance->GetCurrentTime():0.f,BlendInput.X,Instance?Instance->GetPlayRate():0.f);
}
