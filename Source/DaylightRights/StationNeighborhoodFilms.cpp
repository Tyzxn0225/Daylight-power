#include "StationPrototype.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace NeighborhoodFilm
{
    FVector Head(ACharacter* Person)
    {
        if(!Person)return FVector::ZeroVector;
        return Person->GetMesh()->DoesSocketExist(TEXT("head"))?Person->GetMesh()->GetSocketLocation(TEXT("head"))-FVector(0,0,7):Person->GetActorLocation()+FVector(0,0,55);
    }
    void Mark(ACharacter* Person,FVector Floor)
    {
        if(!Person)return;Floor.Z+=Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f;
        Person->SetActorLocation(Floor,false,nullptr,ETeleportType::TeleportPhysics);Person->GetCharacterMovement()->StopMovementImmediately();
    }
    void Face(ACharacter* A,ACharacter* B)
    {
        if(!A||!B)return;FRotator R=(B->GetActorLocation()-A->GetActorLocation()).Rotation();R.Pitch=R.Roll=0;A->SetActorRotation(R);
    }
}
void AStationDirector::StageNeighborhoodStory(const FString& Film)
{
    if(!NeighborhoodReady||!PC)return;auto* Player=Cast<ACharacter>(PC->GetPawn());
    if(Film==TEXT("C08")||Film==TEXT("C09")||Film==TEXT("C11")||Film==TEXT("C13"))
    {
        NeighborhoodFilm::Mark(Player,FVector(-3700,2050,19));NeighborhoodFilm::Mark(LinHe,FVector(-3650,1850,19));
    }
    if(Film==TEXT("C10"))
    {
        NeighborhoodFilm::Mark(Player,FVector(-3760,3170,19));NeighborhoodFilm::Mark(LinHe,FVector(-3710,2980,19));
        NeighborhoodFilm::Mark(Engineer,FVector(-3620,2950,19));NeighborhoodFilm::Mark(ZhouChi,FVector(-3850,2780,19));
    }
    if(Film==TEXT("C14"))
    {
        NeighborhoodFilm::Mark(Player,FVector(-4750,380,3));NeighborhoodFilm::Mark(LinHe,FVector(-4740,100,3));
        NeighborhoodFilm::Mark(Engineer,FVector(-5620,-600,19));NeighborhoodFilm::Mark(ZhouChi,FVector(-4770,1720,19));
    }
    if(Film==TEXT("C15"))
    {
        NeighborhoodFilm::Mark(Player,FVector(-2870,-3450,300));NeighborhoodFilm::Mark(LinHe,FVector(-2800,-3330,300));
    }
    if(Film!=TEXT("P00")&&Film!=TEXT("C12")){NeighborhoodFilm::Face(Player,LinHe);NeighborhoodFilm::Face(LinHe,Player);}
}
void AStationDirector::StageNeighborhoodStoryShot(int32 Shot)
{
    if(!NeighborhoodReady||!PC)return;auto* Player=Cast<ACharacter>(PC->GetPawn());
    if(StoryFilm==TEXT("C09"))
    {
        if(Shot==13)NeighborhoodFilm::Mark(Engineer,FVector(-5620,-600,19));
        if(Shot==14)NeighborhoodFilm::Mark(ZhouChi,FVector(-4770,1720,19));
        if(Shot==15)NeighborhoodFilm::Mark(LinHe,FVector(-4740,100,3));
        if(Shot==10||Shot==11||Shot==12)StageNeighborhoodStory(TEXT("C09"));
    }
    if(StoryFilm==TEXT("C14"))
    {
        if(Shot==34){NeighborhoodFilm::Mark(Player,FVector(-5820,-430,19));NeighborhoodFilm::Face(Engineer,Player);}
        if(Shot==35){NeighborhoodFilm::Mark(Player,FVector(-4900,1950,19));NeighborhoodFilm::Face(ZhouChi,Player);}
        if(Shot==36||Shot==37){NeighborhoodFilm::Mark(Player,FVector(-4750,380,3));NeighborhoodFilm::Face(Player,LinHe);NeighborhoodFilm::Face(LinHe,Player);}
    }
    // These are voiced thoughts over objects, not an invented lip performance.
    if(Shot==30||Shot==33||Shot==38||Shot==41)StoryPlayerMotion=TEXT("Idle");
}
bool AStationDirector::GetNeighborhoodStoryCamera(int32 Shot,FVector& A,FVector& B,FVector& Aim,float& Fov) const
{
    if(!NeighborhoodReady||!PC)return false;
    auto* Player=Cast<ACharacter>(PC->GetPawn());
    switch(Shot)
    {
        case 30:A=FVector(-3490,2080,174);B=A+FVector(-20,-25,0);Aim=FVector(-3570,1700,110);Fov=57;break;
        case 31:A=FVector(-3500,2040,181);B=A+FVector(-10,-15,0);Aim=NeighborhoodFilm::Head(LinHe);Fov=44;break;
        case 32:A=FVector(-3480,1990,180);B=A+FVector(-10,-8,0);Aim=NeighborhoodFilm::Head(Player);Fov=45;break;
        case 33:A=FVector(-3570,1910,150);B=A+FVector(0,-12,-3);Aim=FVector(-3560,1680,92);Fov=49;break;
        case 34:A=FVector(-5700,-350,182);B=A+FVector(10,-20,0);Aim=FVector(-5750,-800,120);Fov=65;break;
        case 35:A=FVector(-4750,2090,185);B=A+FVector(-5,-20,0);Aim=FVector(-4800,1550,110);Fov=65;break;
        case 36:A=FVector(-4830,320,178);B=A+FVector(4,-12,0);Aim=NeighborhoodFilm::Head(LinHe);Fov=45;break;
        case 37:A=FVector(-4840,180,178);B=A+FVector(4,12,0);Aim=NeighborhoodFilm::Head(Player);Fov=45;break;
        case 38:A=FVector(-2600,-3340,450);B=A+FVector(0,-12,-3);Aim=FVector(-2600,-3470,380);Fov=45;break;
        case 39:A=FVector(-2950,-3460,480);B=A+FVector(10,4,0);Aim=NeighborhoodFilm::Head(LinHe);Fov=45;break;
        case 40:A=FVector(-2950,-3280,490);B=A+FVector(12,-5,0);Aim=FVector(-2830,-3400,465);Fov=57;break;
        case 41:A=FVector(-2780,-3350,480);B=FVector(-2750,-3310,510);Aim=FVector(3200,-1000,1200);Fov=62;break;
        case 4:A=FVector(-3480,2150,175);B=A+FVector(-5,-10,-3);Aim=NeighborhoodPoint(TEXT("HouseholdArchive"),FVector(-1570,4005,95))+FVector(0,0,100);Fov=43;break;
        case 13:A=FVector(-5700,-350,182);B=A+FVector(10,-20,0);Aim=FVector(-5750,-800,120);Fov=65;break;
        case 14:A=FVector(-4750,2090,185);B=A+FVector(-5,-20,0);Aim=FVector(-4800,1550,110);Fov=65;break;
        case 15:A=FVector(-4350,800,250);B=A+FVector(-20,-25,-4);Aim=FVector(-4750,350,130);Fov=56;break;
        case 19:A=FVector(-4770,480,175);B=A+FVector(-10,-15,0);Aim=NeighborhoodPoint(TEXT("AllocationLedger"),FVector(-1280,3995,128))+FVector(0,0,120);Fov=46;break;
        case 22:A=FVector(-3790,3370,184);B=A+FVector(0,-15,0);Aim=FVector(-3750,2920,140);Fov=62;break;
        case 12:
            if(StoryFilm!=TEXT("C08")&&StoryFilm!=TEXT("C09")&&StoryFilm!=TEXT("C11"))return false;
            A=FVector(-3490,2140,185);B=A+FVector(-12,-20,0);Aim=FVector(-3670,1850,150);Fov=58;break;
        default:return false;
    }
    return true;
}
