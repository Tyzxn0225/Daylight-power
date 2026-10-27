#include "StationPrototype.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "StationMovementTuning.h"

void AStationDirector::PressLeftSprint()
{
    if(PC&&!InputLocked&&!Starting&&!IsMenuOpen()&&!IsPhysicalActionActive()&&!PC->IsMoveInputIgnored())LeftSprintHeld=true;
}
void AStationDirector::PressRightSprint()
{
    if(PC&&!InputLocked&&!Starting&&!IsMenuOpen()&&!IsPhysicalActionActive()&&!PC->IsMoveInputIgnored())RightSprintHeld=true;
}
void AStationDirector::ReleaseLeftSprint() { LeftSprintHeld=false; }
void AStationDirector::ReleaseRightSprint() { RightSprintHeld=false; }

void AStationDirector::ResetSprint()
{
    LeftSprintHeld=false;RightSprintHeld=false;SprintActive=false;
    if(auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr)
        Character->GetCharacterMovement()->MaxWalkSpeed=StationMovement::Travel;
}
void AStationDirector::UpdateSprintState()
{
    if(!PC)return;
    // The input settings flush pressed keys when the viewport loses focus. Checking
    // the controller state also clears a missed release event, preventing stuck sprint.
    if(!PC->IsInputKeyDown(EKeys::LeftShift))LeftSprintHeld=false;
    if(!PC->IsInputKeyDown(EKeys::RightShift))RightSprintHeld=false;
    if(InputLocked||Starting||IsMenuOpen()||IsPhysicalActionActive()||PC->IsMoveInputIgnored())ResetSprint();
    SprintActive=LeftSprintHeld||RightSprintHeld;
    if(auto* Character=Cast<ACharacter>(PC->GetPawn()))
    {
        Character->GetCharacterMovement()->MaxWalkSpeed=SprintActive?StationMovement::Sprint:StationMovement::Travel;
        if(SprintActive&&Character->GetVelocity().Size2D()>StationMovement::Travel+40&&!Character->GetCharacterMovement()->IsFalling())
            SprintExertion+=GetWorld()->GetDeltaSeconds();
        else
        {
            if(SprintExertion>=3.f)PlayCharacterReaction(TEXT("VR_LinZhao_Recovery"),Character->GetActorLocation()+FVector(0,0,60),.28f);
            SprintExertion=0;
        }
    }
}

void AStationDirector::AutomationMovementKey(FName KeyName,bool Pressed)
{
#if !UE_BUILD_SHIPPING
    const FKey Key(KeyName);
    if(PC&&(Key==EKeys::W||Key==EKeys::A||Key==EKeys::S||Key==EKeys::D||Key==EKeys::LeftShift||Key==EKeys::RightShift))
        PC->InputKey(FInputKeyParams(Key,Pressed?IE_Pressed:IE_Released,Pressed?1.0:0.0));
#endif
}
void AStationDirector::AutomationFlushMovementKeys()
{
#if !UE_BUILD_SHIPPING
    if(PC)PC->FlushPressedKeys();
#endif
}
