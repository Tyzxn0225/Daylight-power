#include "StationPrototype.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/World.h"
#include "Engine/RectLight.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimSingleNodeInstance.h"

namespace Story
{
    struct FStoryLine
    {
        const TCHAR* Film;const TCHAR* Text;const TCHAR* Audio;int32 Shot;float Minimum;
        const TCHAR* ArchiveTitle;const TCHAR* ArchiveBody;int32 Bit;bool Funded;int32 Disposition=-1;int32 DistrictDisposition=-1;
    };
    #include "StationStoryData.inl"
    USoundBase* Sound(const TCHAR* Name){if(FString(Name).IsEmpty())return nullptr;return LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/BZ/Audio/%s.%s"),Name,Name));}
    const FStoryLine* Cue(const TArray<int32>& Indices,int32 Index){return Indices.IsValidIndex(Index)?&StoryLines[Indices[Index]]:nullptr;}
    FVector Head(ACharacter* Character)
    {
        if(!Character)return FVector(-1450,4140,151);
        if(Character->GetMesh()->DoesSocketExist(TEXT("head")))return Character->GetMesh()->GetSocketLocation(TEXT("head"))-FVector(0,0,7);
        return Character->GetActorLocation()+FVector(0,0,55);
    }
}
bool AStationDirector::StoryAutoplayEnabled() const {return !FParse::Param(FCommandLine::Get(),TEXT("NoStoryAutoplay"));}
bool AStationDirector::CanPlayStoryFilm(int32 Index) const
{
    if(Index==9)return true;
    if(Index==6)return NeighborhoodReady&&Progress->Started;
    if(Index==7)return NeighborhoodReady&&Progress->RationMask>=0&&Progress->RationVisitMask==7;
    if(Index==8)return NeighborhoodReady&&(Progress->ExplorationSeenMask&16);
    return Index==0||Index==1&&Progress->Started&&Progress->ReadLedger||Index==2&&Progress->ReadRecall||Index==3&&Progress->SupplyStarted||Index==4&&Progress->DutySettled||Index==5&&Progress->DistrictSettled;
}
FString AStationDirector::GetStoryMenuChoice(int32 Index) const
{
    const TCHAR* Titles[]={TEXT("把白昼寄到明天 · 世界与家庭"),TEXT("没有她的签字 · 名单之前"),TEXT("灯亮以后 · 本轮配给的后果"),TEXT("空白收货栏 · 担保签给谁"),TEXT("今晚先不说再等 · 下班之后"),TEXT("被退回的白昼 · 回光埠"),TEXT("空着的座位 · 林家"),TEXT("申请后面的人 · 社区巡回"),TEXT("五个手印 · 共光露台")};
    if(Index==9)return TEXT("白昼曾经免费 · 世界前史 / 03:45");
    if(Index<0||Index>8)return TEXT("");
    if(Index>=6)return CanPlayStoryFilm(Index)?FString(Titles[Index]):FString(Titles[Index])+TEXT("  [随探索解锁]");
    return CanPlayStoryFilm(Index)?FString(Titles[Index]):FString(Titles[Index])+(Index==5?TEXT("  [回光埠处置后]"):Index==4?TEXT("  [完成本班结算后]"):Index==1?TEXT("  [阅读原始名单后]"):Index==3?TEXT("  [接受新批供货后]"):TEXT("  [完成首夜结算后]"));
}
FString AStationDirector::GetStoryTitle() const
{
    return StoryFilm==TEXT("C13")?TEXT("空着的座位"):StoryFilm==TEXT("C14")?TEXT("申请后面的人"):StoryFilm==TEXT("C15")?TEXT("五个手印"):StoryFilm==TEXT("C12")?TEXT("被退回的白昼"):StoryFilm==TEXT("C11")?TEXT("今晚先不说再等"):StoryFilm==TEXT("C10")?TEXT("空白收货栏"):StoryFilm==TEXT("P00")?TEXT("把白昼寄到明天"):StoryFilm==TEXT("C08")?TEXT("没有她的签字"):TEXT("灯亮以后");
}
FString AStationDirector::GetStoryArchiveTitle() const
{
    const auto* Line=Story::Cue(StoryLineIndices,StoryCue);return Line?Line->ArchiveTitle:TEXT("");
}
FString AStationDirector::GetStoryArchiveBody() const
{
    const auto* Line=Story::Cue(StoryLineIndices,StoryCue);return Line?Line->ArchiveBody:TEXT("");
}
float AStationDirector::GetStoryShotProgress() const
{
    return StoryLineStarts.IsValidIndex(StoryCue)?FMath::Clamp((StoryElapsed-StoryLineStarts[StoryCue])/FMath::Max(.1f,StoryLineEnds[StoryCue]-StoryLineStarts[StoryCue]),0.f,1.f):0.f;
}
float AStationDirector::GetStoryFade() const
{
    return IsStoryFilmActive()?FMath::Max(1.f-FMath::Clamp(StoryElapsed/.7f,0.f,1.f),FMath::Clamp((StoryElapsed-StoryDuration+1.f)/1.f,0.f,1.f)):0.f;
}
bool AStationDirector::StartStoryFilm(const FString& Film,bool OpenPublicationAfter)
{
    if(!PC||!PC->GetPawn()||IsStoryFilmActive()||Starting||IsPhysicalActionActive())return false;
    if(Film!=TEXT("P00")&&Film!=TEXT("C08")&&Film!=TEXT("C09")&&Film!=TEXT("C10")&&Film!=TEXT("C11")&&Film!=TEXT("C12")&&Film!=TEXT("C13")&&Film!=TEXT("C14")&&Film!=TEXT("C15"))return false;
    if((Film==TEXT("C13")||Film==TEXT("C14")||Film==TEXT("C15"))&&!NeighborhoodReady)return false;
    if(Film==TEXT("C13")&&!Progress->Started)return false;
    if(Film==TEXT("C14")&&(Progress->RationMask<0||Progress->RationVisitMask!=7))return false;
    if(Film==TEXT("C15")&&!(Progress->ExplorationSeenMask&16))return false;
    if(Film==TEXT("C08")&&(!Progress->Started||!Progress->ReadLedger))return false;
    if(Film==TEXT("C09")&&!Progress->ReadRecall)return false;
    if(Film==TEXT("C10")&&!Progress->SupplyStarted)return false;
    if(Film==TEXT("C11")&&!Progress->DutySettled)return false;
    if(Film==TEXT("C12")&&!Progress->DistrictSettled)return false;
    CancelCommunityVignette();ClosePanel();StoryFilm=Film;StoryElapsed=0;StoryDuration=1;StoryCue=-1;
    StoryLineIndices.Empty();StoryLineStarts.Empty();StoryLineEnds.Empty();
    StorySkipHeld=false;StorySkipHeldSeconds=0;StoryManualCapture=false;StoryOpenPublicationAfter=OpenPublicationAfter;
    for(int32 I=0;I<UE_ARRAY_COUNT(Story::StoryLines);++I)
    {
        const auto& Line=Story::StoryLines[I];if(Film!=Line.Film)continue;
        if(Line.Bit&&bool(Progress->RationMask&Line.Bit)!=Line.Funded)continue;
        if(Line.Disposition>=0&&Progress->ConflictChoice!=Line.Disposition)continue;
        if(Line.DistrictDisposition>=0&&Progress->DistrictChoice!=Line.DistrictDisposition)continue;
        const auto* Audio=Story::Sound(Line.Audio);
        StoryLineIndices.Add(I);StoryLineStarts.Add(StoryDuration);
        StoryDuration+=FMath::Max(Line.Minimum,Audio?Audio->GetDuration()+1.05f:Line.Minimum);
        StoryLineEnds.Add(StoryDuration);
    }
    StoryDuration+=1.f;
    if(Film==TEXT("P00"))CreateStoryMemory();
    StoryPreviousView=PC->GetViewTarget();if(StoryCameras.Contains(Cast<ACameraActor>(StoryPreviousView)))StoryPreviousView=PC->GetPawn();
    StoryPlayerTransform=PC->GetPawn()->GetActorTransform();StoryControlRotation=PC->GetControlRotation();
    SetInputLock(true);Focus=nullptr;
    if(!LinHe)
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        LinHe=GetWorld()->SpawnActor<AStationEngineer>(FVector(-1420,4140,100),FRotator(0,90,0),Params);
        LinHe->PersonName=TEXT("林禾");LinHe->ApplyIdentity(TEXT("LinHe"));LinHe->Patrol=false;StoryTemporaryLinHe=true;
    }
    if(!ZhouChi&&Film==TEXT("P00"))
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ZhouChi=GetWorld()->SpawnActor<AStationEngineer>(FVector(-1240,4180,100),FRotator(0,180,0),Params);ZhouChi->PersonName=TEXT("周驰");ZhouChi->ApplyIdentity(TEXT("ZhouChi"));ZhouChi->Patrol=false;StoryTemporaryZhouChi=true;
    }
    StoryPeople.Empty();StoryPeopleTransforms.Empty();StoryPeoplePaused.Empty();
    for(AStationEngineer* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(IsValid(Person))
    {
        StoryPeople.Add(Person);StoryPeopleTransforms.Add(Person->GetActorTransform());StoryPeoplePaused.Add(Person->ConversationPaused);
        Person->GetCharacterMovement()->StopMovementImmediately();Person->SetConversationPaused(true,PC->GetPawn());Person->PlayStationAction(TEXT("Idle"));
    }
    auto Stage=[&](ACharacter* Person,FVector Position)
    {
        const float Height=Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();Position.Z=9+Height;
        Person->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);Person->GetCharacterMovement()->StopMovementImmediately();
    };
    Stage(CastChecked<ACharacter>(PC->GetPawn()),FVector(-1575,4190,0));Stage(LinHe,FVector(-1420,4140,0));
    PC->GetPawn()->SetActorRotation(FRotator(0,-18,0));LinHe->SetActorRotation(FRotator(0,162,0));
    if(Film==TEXT("C10"))
    {
        if(Engineer){Stage(Engineer,FVector(-1800,4100,0));Engineer->SetActorRotation(FRotator(0,25,0));}
        if(ZhouChi){Stage(ZhouChi,FVector(-1240,4180,0));ZhouChi->SetActorRotation(FRotator(0,180,0));}
    }
    if(Film==TEXT("P00"))
    {
        if(Engineer){Stage(Engineer,FVector(-1850,4140,0));Engineer->SetActorRotation(FRotator(0,20,0));}
        if(ZhouChi){Stage(ZhouChi,FVector(-1240,4180,0));ZhouChi->SetActorRotation(FRotator(0,180,0));}
    }
    if(Film==TEXT("C12"))
    {
        Stage(CastChecked<ACharacter>(PC->GetPawn()),FVector(1100,10330,0));Stage(LinHe,FVector(1240,10380,0));
        PC->GetPawn()->SetActorRotation(FRotator(0,20,0));LinHe->SetActorRotation(FRotator(0,200,0));
        if(Engineer)Stage(Engineer,FVector(700,10500,0));if(ZhouChi)Stage(ZhouChi,FVector(1500,10500,0));
    }
    StageNeighborhoodStory(Film);
    if(StoryCameras.IsEmpty())for(int32 I=0;I<2;++I)
    {
        auto* Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(52);StoryCameras.Add(Camera);
    }
    if(!StoryFaceLight)
    {
        StoryFaceLight=GetWorld()->SpawnActor<ARectLight>();auto* Light=CastChecked<URectLightComponent>(StoryFaceLight->GetLightComponent());Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetSourceWidth(160);Light->SetSourceHeight(180);Light->SetAttenuationRadius(750);Light->SetLightColor(FLinearColor(1.f,.83f,.65f));Light->SetIntensity(0);
    }
    PC->SetViewTargetWithBlend(StoryCameras[0],0);UpdateStoryCamera();
    if(StoryMusic){StoryMusic->Stop();StoryMusic->DestroyComponent();StoryMusic=nullptr;}
    StoryMusic=MakeLoop(Film==TEXT("P00")?TEXT("MU14_World"):(Film==TEXT("C09")||Film==TEXT("C10")||Film==TEXT("C14"))?TEXT("MU14_Pressure"):Film==TEXT("C12")?TEXT("MU14_Return"):TEXT("MU14_Family"),FVector::ZeroVector,.13f,false);
    if(Film==TEXT("P00"))PlayCue(TEXT("FX_ArchiveReel"),FVector::ZeroVector,.22f,false);
    return true;
}
void AStationDirector::SetStoryCue(int32 Index,bool Silent)
{
    const auto* Line=Story::Cue(StoryLineIndices,Index);if(!Line)return;
    StoryCue=Index;BeginPresentation(Line->Text,true,Line->Audio);
    if(Silent){StopPresentationVoice();CompletePresentation();}
    StoryPlayerMotion=FString(Line->Text).StartsWith(TEXT("林照："))?TEXT("Talk"):TEXT("Idle");
    for(auto& Person:StoryPeople)if(IsValid(Person))Person->PlayStationAction(FString(Line->Text).StartsWith(Person->PersonName+TEXT("："))?TEXT("Talk"):TEXT("Idle"));
    if(StoryFilm==TEXT("C09")&&!NeighborhoodReady)
    {
        if(Line->Shot==13&&Engineer)Engineer->SetActorLocation(FVector(-2445,4150,9+84),false,nullptr,ETeleportType::TeleportPhysics);
        if(Line->Shot==14&&ZhouChi)ZhouChi->SetActorLocation(FVector(-110,4150,9+84),false,nullptr,ETeleportType::TeleportPhysics);
        if(Line->Shot==15&&LinHe)LinHe->SetActorLocation(FVector(1140,4140,9+80),false,nullptr,ETeleportType::TeleportPhysics);
        if(Line->Shot==10||Line->Shot==11||Line->Shot==12)LinHe->SetActorLocation(FVector(-1420,4140,9+80),false,nullptr,ETeleportType::TeleportPhysics);
    }
    StageNeighborhoodStoryShot(Line->Shot);
    if(!Silent)
    {
        if(Line->Shot==4||Line->Shot==19)PlayCue(TEXT("FX_Paper"),FVector::ZeroVector,.22f,false);
        if(Line->Shot==3||Line->Shot==16)PlayCue(TEXT("FX_StoryStamp"),FVector::ZeroVector,.24f,false);
    }
    UpdateStoryCamera();PC->SetViewTargetWithBlend(StoryCameras[FMath::Max(0,StoryCue)%2],0.f);
    if(PC->PlayerCameraManager)PC->PlayerCameraManager->SetGameCameraCutThisFrame();
}
void AStationDirector::UpdateStoryCamera()
{
    if(StoryCameras.Num()!=2||!PC)return;
    UpdateStoryMemory();
    const auto* Line=Story::Cue(StoryLineIndices,StoryCue);const int32 Shot=Line?Line->Shot:StoryFilm==TEXT("P00")?17:0;
    const float T=ReducedMotion?0.f:FMath::InterpEaseInOut(0.f,1.f,GetStoryShotProgress(),2.f);
    FVector A,B,Aim;float Fov=52;
    switch(Shot)
    {
        case 23:case 24:case 25:
        {
            ACharacter* Subject=Shot==23?Cast<ACharacter>(PC->GetPawn()):Shot==24?Cast<ACharacter>(Engineer.Get()):Cast<ACharacter>(ZhouChi.Get());
            Aim=Story::Head(Subject);const FVector Front=Subject?Subject->GetActorForwardVector():FVector(1,0,0);const FVector Side=Subject?Subject->GetActorRightVector():FVector(0,1,0);
            A=Aim+Front*235+Side*115+FVector(0,0,10);B=A-Front*25+FVector(0,0,-3);Fov=43;break;
        }
        case 27:A=FVector(-300,13000,1450);B=FVector(-600,12600,1150);Aim=FVector(-2200,10900,280);Fov=57;break;
        case 19:A=FVector(-1420,4350,190);B=FVector(-1370,4280,170);Aim=FVector(-1280,3995,128);Fov=43;break;
        case 20:case 21:
        {
            auto* Speaker=Shot==20?ZhouChi.Get():Engineer.Get();Aim=Story::Head(Speaker);
            A=Aim+FVector(Shot==20?-140:140,245,18);B=A+FVector(0,-24,-4);Fov=43;break;
        }
        case 22:A=FVector(-1760,4790,345);B=FVector(-1700,4630,300);Aim=FVector(-1515,4170,145);Fov=53;break;
        case 17:A=FVector(-12180,10410,205);B=FVector(-12135,10325,175);Aim=FVector(-12030,10045,87);Fov=45;break;
        case 18:A=FVector(-11870,10390,185);B=FVector(-11900,10330,170);Aim=FVector(-12000,9880,145);Fov=50;break;
        case 0:A=FVector(-3400,6500,2100);B=FVector(-3150,6000,1750);Aim=FVector(-2300,2900,620);Fov=62;break;
        case 1:A=FVector(-2800,5000,1000);B=FVector(-2360,4700,850);Aim=FVector(-2200,3100,460);Fov=54;break;
        case 2:A=FVector(-300,7300,2850);B=FVector(150,6750,2500);Aim=FVector(2000,1700,1100);Fov=65;break;
        case 3:case 16:A=FVector(3100,6500,1800);B=FVector(3600,5900,1600);Aim=FVector(4200,1900,650);Fov=55;break;
        case 4:A=FVector(-1770,4230,158);B=FVector(-1720,4170,145);Aim=FVector(-1570,4005,95);Fov=44;break;
        case 5:A=FVector(-3000,4600,470);B=FVector(-2940,4380,435);Aim=FVector(-2930,3850,380);Fov=47;break;
        case 10:case 11:
        {
            auto* Speaker=Shot==10?Cast<ACharacter>(LinHe.Get()):Cast<ACharacter>(PC->GetPawn());
            const FVector Axis=(LinHe->GetActorLocation()-PC->GetPawn()->GetActorLocation()).GetSafeNormal2D();
            const FVector Side=FVector::CrossProduct(FVector::UpVector,Axis);
            Aim=Story::Head(Speaker);A=Aim+Axis*(Shot==10?-240.f:240.f)+Side*115.f+FVector(0,0,8);B=A+Axis*(Shot==10?26.f:-26.f);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(StoryPortrait),false,PC->GetPawn());Query.AddIgnoredActor(LinHe);Query.AddIgnoredActor(this);
            FHitResult Hit;if(GetWorld()->LineTraceSingleByChannel(Hit,Aim,A,ECC_Visibility,Query)){A=Hit.Location+(Aim-Hit.Location).GetSafeNormal()*18;B=A;}
            Fov=43;break;
        }
        case 13:case 14:case 15:
        {
            const float X=Shot==13?-2550:Shot==14?-250:1070;
            A=FVector(X-100,4650,240);B=FVector(X-55,4510,215);Aim=FVector(X+10,4040,150);Fov=53;break;
        }
        default:A=FVector(-1770,4650,220);B=FVector(-1700,4520,205);Aim=FVector(-1500,4160,146);Fov=52;break;
    }
    GetNeighborhoodStoryCamera(Shot,A,B,Aim,Fov);
    auto* Camera=StoryCameras[FMath::Max(0,StoryCue)%2].Get();FVector Position=FMath::Lerp(A,B,T);
    if(Shot>=30&&Shot<=40)
    {
        // Interior shots trace towards the eye point, not through a wall.
        FCollisionQueryParams Query(SCENE_QUERY_STAT(NeighborhoodFilmCamera),false,PC->GetPawn());Query.AddIgnoredActor(this);
        for(const auto& Person:StoryPeople)if(IsValid(Person))Query.AddIgnoredActor(Person);
        FHitResult Hit;if(GetWorld()->LineTraceSingleByChannel(Hit,Aim,Position,ECC_Visibility,Query))Position=Hit.Location+(Aim-Hit.Location).GetSafeNormal()*18.f;
    }
    Camera->SetActorLocationAndRotation(Position,(Aim-Position).Rotation());Camera->GetCameraComponent()->SetFieldOfView(Fov-1.8f*T);
    if(StoryFaceLight)
    {
        const FVector Key=Position+FVector(0,0,65);StoryFaceLight->SetActorLocationAndRotation(Key,(Aim-Key).Rotation());
        StoryFaceLight->GetLightComponent()->SetIntensity(Shot==10||Shot==11||Shot==23||Shot==24||Shot==25||Shot==31||Shot==32||Shot==36||Shot==37||Shot==39||Shot==40?(StoryFilm==TEXT("C12")?10000.f:3000.f):0.f);
    }
    auto& Post=Camera->GetCameraComponent()->PostProcessSettings;
    Post.bOverride_DepthOfFieldFocalDistance=true;Post.DepthOfFieldFocalDistance=FVector::Dist(Position,Aim);
    Post.bOverride_DepthOfFieldFstop=true;Post.DepthOfFieldFstop=(Shot==10||Shot==11)?4.f:8.f;
    Post.bOverride_MotionBlurAmount=true;Post.MotionBlurAmount=0.f;
    Post.bOverride_AutoExposureBias=Shot==10||Shot==11;Post.AutoExposureBias=-.35f;
}
void AStationDirector::TickStoryFilm(float Delta)
{
    if(!IsStoryFilmActive())
    {
        if(!StoryAutoAttempted&&StoryAutoplayEnabled()&&!Progress->SeenWorldFilm&&!Progress->Started&&!Progress->MetEngineer&&PC&&PC->GetPawn()&&!IsMenuOpen()&&!Starting&&GetWorld()->GetTimeSeconds()>2.f)
        {StoryAutoAttempted=true;StartStoryFilm(TEXT("P00"));}
        return;
    }
    if(StoryManualCapture){UpdateStoryCamera();return;}
    StoryElapsed+=Delta;
    StorySkipHeldSeconds=StorySkipHeld?StorySkipHeldSeconds+Delta:0.f;
    if(StorySkipHeldSeconds>=.9f||StoryElapsed>=StoryDuration){FinishStoryFilm();return;}
    int32 Next=-1;for(int32 I=0;I<StoryLineStarts.Num();++I)if(StoryElapsed>=StoryLineStarts[I])Next=I;
    if(Next!=StoryCue&&Next>=0)SetStoryCue(Next);
    if(!IsVoicePlaying()&&!IsTyping())
    {
        StoryPlayerMotion=TEXT("Idle");for(auto& Person:StoryPeople)if(IsValid(Person))Person->PlayStationAction(TEXT("Idle"));
    }
    UpdateStoryCamera();
    if(StoryMusic)
    {
        // Hold the score back while the sister asks for the right to refuse.
        const bool Silence=(StoryFilm==TEXT("C08")&&StoryCue==7)||(StoryFilm==TEXT("C10")&&StoryCue==2)||(StoryFilm==TEXT("C11")&&StoryCue==1)||((StoryFilm==TEXT("C13")||StoryFilm==TEXT("C15"))&&StoryCue==3)||(StoryFilm==TEXT("C14")&&StoryCue==2);
        StoryMusic->SetVolumeMultiplier(Silence?0.f:.15f*VoiceMix*MusicVolume);
    }
}
void AStationDirector::FinishStoryFilm(bool MarkSeen)
{
    if(!IsStoryFilmActive())return;
    const FString Finished=StoryFilm;const bool ContinuePublication=MarkSeen&&StoryOpenPublicationAfter;
    StoryFilm.Empty();StoryManualCapture=false;StorySkipHeld=false;StorySkipHeldSeconds=0;StoryPlayerMotion.Empty();
    StopPresentationVoice();PresentedText.Empty();PresentedSpeaker.Empty();VisibleCharacters=0;
    if(StoryMusic)StoryMusic->FadeOut(.7f,0.f);
    if(StoryMemorySun){StoryMemorySun->Destroy();StoryMemorySun=nullptr;}
    if(StoryFaceLight){StoryFaceLight->Destroy();StoryFaceLight=nullptr;}
    if(StoryMemorySet){StoryMemorySet->Destroy();StoryMemorySet=nullptr;}
    if(PC&&PC->GetPawn())
    {
        PC->GetPawn()->SetActorTransform(StoryPlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);
        if(auto* Player=Cast<ACharacter>(PC->GetPawn()))Player->GetCharacterMovement()->StopMovementImmediately();
        PC->SetControlRotation(StoryControlRotation);
        PC->SetViewTargetWithBlend(IsValid(StoryPreviousView)?StoryPreviousView.Get():PC->GetPawn(),ReducedMotion?0.f:.35f);
    }
    for(int32 I=0;I<StoryPeople.Num();++I)if(IsValid(StoryPeople[I]))
    {
        auto* Person=StoryPeople[I].Get();Person->SetActorTransform(StoryPeopleTransforms[I],false,nullptr,ETeleportType::TeleportPhysics);
        Person->PlayStationAction(TEXT("Idle"));Person->SetConversationPaused(StoryPeoplePaused[I],nullptr);
    }
    if(StoryTemporaryLinHe&&LinHe){LinHe->Destroy();LinHe=nullptr;StoryTemporaryLinHe=false;}
    if(StoryTemporaryZhouChi&&ZhouChi){ZhouChi->Destroy();ZhouChi=nullptr;StoryTemporaryZhouChi=false;}
    StoryPeople.Empty();StoryPeopleTransforms.Empty();StoryPeoplePaused.Empty();StoryPreviousView=nullptr;
    SetInputLock(false);EReady=true;ObjectiveSpoken=false;ObjectiveElapsed=0;
    if(MarkSeen)
    {
        if(Finished==TEXT("P00"))Progress->SeenWorldFilm=true;
        if(Finished==TEXT("C08"))Progress->SeenFamilyFilm=true;
        if(Finished==TEXT("C09"))Progress->SeenAftermathFilm=true;
        if(Finished==TEXT("C10"))Progress->SeenSupplyFilm=true;
        if(Finished==TEXT("C11"))Progress->SeenDutyFilm=true;
        if(Finished==TEXT("C12"))Progress->SeenDistrictFilm=true;
        if(Finished==TEXT("C13"))Progress->NeighborhoodFilmMask|=1;
        if(Finished==TEXT("C14"))Progress->NeighborhoodFilmMask|=2;
        if(Finished==TEXT("C15"))Progress->NeighborhoodFilmMask|=4;
        SaveProgress();
    }
    if(ContinuePublication&&Progress->AllocationPolicy<0)
    {
        for(auto& Object:Objects)if(Object->Kind==EStationObjectKind::Terminal){Focus=Object;break;}
        OpenPanel(EStationPanel::Allocation);
    }
}
bool AStationDirector::AutomationStoryFilm(const FString& Film)
{
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))return StartStoryFilm(Film);
#endif
    return false;
}
void AStationDirector::AutomationSeekStoryFilm(float Seconds)
{
#if WITH_EDITOR
    if(!FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))||!IsStoryFilmActive())return;
    StoryManualCapture=true;StoryElapsed=FMath::Clamp(Seconds,0.f,StoryDuration-.001f);
    if(StoryMusic)StoryMusic->Stop();
    int32 Next=-1;for(int32 I=0;I<StoryLineStarts.Num();++I)if(StoryElapsed>=StoryLineStarts[I])Next=I;
    if(Next>=0&&Next!=StoryCue)SetStoryCue(Next,true);StopPresentationVoice();UpdateStoryCamera();
    const float Phase=StoryLineStarts.IsValidIndex(StoryCue)?StoryElapsed-StoryLineStarts[StoryCue]:StoryElapsed;
    VisibleCharacters=0;float RevealAt=.05f;
    while(VisibleCharacters<PresentedText.Len()&&Phase>=RevealAt)
    {
        const TCHAR C=PresentedText[VisibleCharacters++];RevealAt+=GlyphPeriod;
        if(C==TCHAR(0xff0c)||C==TCHAR(0xff1b))RevealAt+=.14f;
        if(C==TCHAR(0x3002)||C==TCHAR(0xff1f)||C==TCHAR(0xff01))RevealAt+=.24f;
    }
    for(auto& Person:StoryPeople)if(IsValid(Person))if(auto* Instance=Person->GetMesh()->GetSingleNodeInstance())Instance->SetPosition(FMath::Fmod(Phase,2.8f),false);
    if(auto* Player=Cast<ACharacter>(PC->GetPawn()))if(auto* Instance=Player->GetMesh()->GetSingleNodeInstance())Instance->SetPosition(FMath::Fmod(Phase,2.8f),false);
#endif
}
void AStationDirector::AutomationCancelStoryFilm()
{
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))FinishStoryFilm(false);
#endif
}
FString AStationDirector::GetStoryStateJson() const
{
    FString Timeline;for(int32 I=0;I<StoryLineIndices.Num();++I)
    {
        const auto& Line=Story::StoryLines[StoryLineIndices[I]];if(!Timeline.IsEmpty())Timeline+=TEXT(",");
        Timeline+=FString::Printf(TEXT("{\"cue\":%d,\"start\":%.3f,\"end\":%.3f,\"shot\":%d,\"voice\":\"%s\"}"),I,StoryLineStarts[I],StoryLineEnds[I],Line.Shot,Line.Audio);
    }
    return FString::Printf(TEXT("{\"active\":%s,\"film\":\"%s\",\"elapsed\":%.3f,\"duration\":%.3f,\"cue\":%d,\"manual\":%s,\"seen_world\":%s,\"seen_family\":%s,\"seen_aftermath\":%s,\"skip\":%.3f,\"timeline\":[%s]}"),IsStoryFilmActive()?TEXT("true"):TEXT("false"),*StoryFilm,StoryElapsed,StoryDuration,StoryCue,StoryManualCapture?TEXT("true"):TEXT("false"),Progress->SeenWorldFilm?TEXT("true"):TEXT("false"),Progress->SeenFamilyFilm?TEXT("true"):TEXT("false"),Progress->SeenAftermathFilm?TEXT("true"):TEXT("false"),GetStorySkipProgress(),*Timeline);
}
