#include "StationBackgroundFilm.h"
#include "StationFrontend.h"
#include "CanvasItem.h"
#include "Components/InputComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

AStationBackgroundFilm::AStationBackgroundFilm()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    bAllowTickBeforeBeginPlay=true;
    InputPriority=100000;
    bBlockInput=true;
}
AStationBackgroundFilm* AStationBackgroundFilm::GetActiveFilm(UObject* Context)
{
    UWorld* World=Context?Context->GetWorld():nullptr;
    if(World)for(TActorIterator<AStationBackgroundFilm> It(World);It;++It)if(!It->Closing)return *It;
    return nullptr;
}
AStationBackgroundFilm* AStationBackgroundFilm::Open(APlayerController* Controller)
{
    if(!Controller||!Controller->GetWorld())return nullptr;
    if(auto* Current=GetActiveFilm(Controller))return Current;
    const FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("Movies/BZ_WhiteDay.mp4"));
    if(!IFileManager::Get().FileExists(*Path)){UE_LOG(LogTemp,Error,TEXT("Background film missing: %s"),*Path);return nullptr;}
    FActorSpawnParameters Params;Params.Owner=Controller;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Film=Controller->GetWorld()->SpawnActor<AStationBackgroundFilm>(Params);
    if(!Film||!Film->Initialize(Controller)){if(Film)Film->Destroy();return nullptr;}
    return Film;
}
bool AStationBackgroundFilm::Initialize(APlayerController* Controller)
{
    Viewer=Controller;SavedPause=UGameplayStatics::IsGamePaused(this);SavedCursor=Viewer->bShowMouseCursor;
    Player=NewObject<UMediaPlayer>(this);Player->PlayOnOpen=false;Player->SetLooping(false);
    Player->OnMediaOpened.AddDynamic(this,&AStationBackgroundFilm::MediaOpened);
    Player->OnMediaOpenFailed.AddDynamic(this,&AStationBackgroundFilm::MediaFailed);
    Player->OnEndReached.AddDynamic(this,&AStationBackgroundFilm::MediaEnded);
    Texture=NewObject<UMediaTexture>(this);Texture->NewStyleOutput=true;Texture->AutoClear=true;Texture->SRGB=true;Texture->SetMediaPlayer(Player);Texture->UpdateResource();
    Sound=NewObject<UMediaSoundComponent>(this);AddInstanceComponent(Sound);Sound->bIsUISound=true;Sound->bAllowSpatialization=false;Sound->Channels=EMediaSoundChannels::Stereo;
    Sound->PrimaryComponentTick.bTickEvenWhenPaused=true;Sound->SetMediaPlayer(Player);Sound->RegisterComponent();Sound->Start();
    EnableInput(Viewer);InputComponent->Priority=100000;InputComponent->bBlockInput=true;
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AStationBackgroundFilm::SkipPressed).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::E,IE_Released,this,&AStationBackgroundFilm::SkipReleased).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AStationBackgroundFilm::CloseFilm).bExecuteWhenPaused=true;
    if(auto* Front=Cast<AStationFrontendController>(Viewer))Front->SetBackgroundFilmAudioPaused(true);
    Viewer->bShowMouseCursor=false;UGameplayStatics::SetGamePaused(this,true);
    OpenedAt=LastRealTime=FPlatformTime::Seconds();
    const bool Accepted=Player->OpenFile(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("Movies/BZ_WhiteDay.mp4")));
    if(!Accepted){Failure=TEXT("影片无法打开，请检查本地媒体组件。");CloseFilm();return false;}
    UE_LOG(LogTemp,Display,TEXT("BackgroundFilm opened without changing story state"));return true;
}
void AStationBackgroundFilm::MediaOpened(FString URL)
{
    if(Closing)return;
    const double Duration=Player->GetDuration().GetTotalSeconds();
    if(Duration<=0||Duration>240.05){Failure=TEXT("背景影片时长校验未通过。");CloseFilm();return;}
    Ready=true;Player->Play();UE_LOG(LogTemp,Display,TEXT("BackgroundFilm ready duration=%.3f"),Duration);
}
void AStationBackgroundFilm::MediaFailed(FString URL){Failure=TEXT("背景影片解码失败，已返回原界面。");CloseFilm();}
void AStationBackgroundFilm::MediaEnded(){CloseFilm();}
void AStationBackgroundFilm::SkipPressed(){SkipHeld=true;SkipSeconds=0;}
void AStationBackgroundFilm::SkipReleased(){SkipHeld=false;SkipSeconds=0;}
void AStationBackgroundFilm::Tick(float Delta)
{
    Super::Tick(Delta);if(Closing)return;
    const double Now=FPlatformTime::Seconds();const float Elapsed=FMath::Min(float(Now-LastRealTime),.1f);LastRealTime=Now;
    if(Sound)Sound->UpdatePlayer();
    if(SkipHeld){SkipSeconds+=Elapsed;if(SkipSeconds>=.9f){CloseFilm();return;}}
    if(!Ready&&Now-OpenedAt>15){Failure=TEXT("影片加载超时，已保留原有进度。");CloseFilm();}
}
void AStationBackgroundFilm::CloseFilm()
{
    if(Closing)return;Closing=true;
    if(Player){Player->OnEndReached.RemoveAll(this);Player->OnMediaOpened.RemoveAll(this);Player->OnMediaOpenFailed.RemoveAll(this);Player->Close();}
    if(Sound)Sound->Stop();
    if(Viewer)
    {
        DisableInput(Viewer);if(Viewer->PlayerInput)Viewer->PlayerInput->FlushPressedKeys();Viewer->bShowMouseCursor=SavedCursor;
        if(auto* Front=Cast<AStationFrontendController>(Viewer)){Front->SetBackgroundFilmAudioPaused(false);if(!Failure.IsEmpty())Front->Message=Failure;}
    }
    if(GetWorld())UGameplayStatics::SetGamePaused(this,SavedPause);
    UE_LOG(LogTemp,Display,TEXT("BackgroundFilm closed; previous pause and input restored. %s"),*Failure);Destroy();
}
void AStationBackgroundFilm::EndPlay(const EEndPlayReason::Type Reason)
{
    if(!Closing)CloseFilm();Super::EndPlay(Reason);
}
bool AStationBackgroundFilm::DrawFilm(AHUD* HUD,UCanvas* C)
{
    auto* Film=GetActiveFilm(HUD);if(!Film||!HUD||!C)return false;
    const float W=C->SizeX,H=C->SizeY,U=FMath::Min(W/1920.f,H/1080.f);
    HUD->DrawRect(FLinearColor::Black,0,0,W,H);
    const float VW=1920*U,VH=1080*U;
    if(Film->Ready&&Film->Texture&&Film->Texture->GetSurfaceWidth()>1)HUD->DrawTexture(Film->Texture,(W-VW)/2,(H-VH)/2,VW,VH,0,0,1,1,FLinearColor::White,BLEND_Opaque);
    UFont* Font=GEngine?GEngine->GetMediumFont():nullptr;
    if(!Film->Ready&&Font)
    {
        FCanvasTextItem T(FVector2D((W-VW)/2+96*U,H/2),FText::FromString(TEXT("正在载入《白昼曾经免费》……")),FSlateFontInfo(Font,24*U),FLinearColor(.92f,.9f,.83f));C->DrawItem(T);
    }
    if(Font)
    {
        const int32 Seconds=Film->Player&&Film->Ready?FMath::RoundToInt(Film->Player->GetDuration().GetTotalSeconds()):225;
        const FString Hint=FString::Printf(TEXT("背景前史 · %02d:%02d      长按 E 跳过  /  Esc 返回"),Seconds/60,Seconds%60);
        FCanvasTextItem T(FVector2D((W-VW)/2+96*U,(H+VH)/2-42*U),FText::FromString(Hint),FSlateFontInfo(Font,16*U),FLinearColor(.7f,.72f,.72f));T.EnableShadow(FLinearColor::Black);C->DrawItem(T);
    }
    if(Film->SkipSeconds>0)HUD->DrawRect(FLinearColor(.83f,.65f,.38f),W/2-100*U,H-22*U,200*U*FMath::Clamp(Film->SkipSeconds/.9f,0.f,1.f),3*U);
    return true;
}
FString AStationBackgroundFilm::GetFilmStateJson() const
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("ready"),Ready);J->SetBoolField(TEXT("playing"),Player&&Player->IsPlaying());J->SetBoolField(TEXT("paused_world"),UGameplayStatics::IsGamePaused(this));
    J->SetNumberField(TEXT("time"),Player?Player->GetTime().GetTotalSeconds():0);J->SetNumberField(TEXT("duration"),Player?Player->GetDuration().GetTotalSeconds():0);
    J->SetNumberField(TEXT("width"),Texture?Texture->GetSurfaceWidth():0);J->SetNumberField(TEXT("height"),Texture?Texture->GetSurfaceHeight():0);J->SetStringField(TEXT("error"),Failure);J->SetBoolField(TEXT("input_blocked"),InputComponent&&InputComponent->bBlockInput);
    FString Result;auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(J,Writer);return Result;
}
bool AStationBackgroundFilm::AutomationSeek(float Seconds)
{
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))&&Player&&Ready)return Player->Seek(FTimespan::FromSeconds(Seconds));
#endif
    return false;
}
void AStationBackgroundFilm::AutomationHoldSkip(float Seconds)
{
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation"))){SkipPressed();SkipSeconds=FMath::Max(0.f,Seconds);}
#endif
}
