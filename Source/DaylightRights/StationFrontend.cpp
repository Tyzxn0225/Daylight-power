#include "StationFrontend.h"
#include "StationBackgroundFilm.h"
#include "StationPrototype.h"
#include "StationExperiencePolicy.h"
#include "AudioDevice.h"
#include "CanvasItem.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace StationFrontend
{
    UStationPreferences* LoadPreferences(UObject* Owner)
    {
        auto* P=Cast<UStationPreferences>(UGameplayStatics::LoadGameFromSlot(PreferencesSlot,0));
        if(!P)P=NewObject<UStationPreferences>(Owner);
        P->MusicVolume=FMath::Clamp(P->MusicVolume,0,10);P->SubtitleScale=FMath::Clamp(P->SubtitleScale,0,2);
        P->Volume=FMath::Clamp(P->Volume,0,10);P->TextSpeed=FMath::Clamp(P->TextSpeed,0,3);
        P->Quality=FMath::Clamp(P->Quality,0,3);P->WindowMode=FMath::Clamp(P->WindowMode,0,2);
        P->VoiceVolume=StationExperience::ClampVolume(P->VoiceVolume);P->EffectsVolume=StationExperience::ClampVolume(P->EffectsVolume);P->GuidanceDetail=P->SceneGuides?FMath::Clamp(P->GuidanceDetail,0,2):0;
        return P;
    }
    void ApplyAudio(UWorld* World,const UStationPreferences* P)
    {
        if(World&&P)if(auto* Audio=World->GetAudioDeviceRaw())Audio->SetTransientPrimaryVolume(P->Volume/10.f);
    }
    bool StorePreferences(UStationPreferences* P){return P&&UGameplayStatics::SaveGameToSlot(P,PreferencesSlot,0);}
    const FLinearColor Paper(.94f,.91f,.83f,1),Gold(.84f,.65f,.37f,1),Muted(.58f,.63f,.65f,1);
}

AStationFrontendGameMode::AStationFrontendGameMode()
{
    DefaultPawnClass=nullptr;PlayerControllerClass=AStationFrontendController::StaticClass();HUDClass=AStationFrontendHUD::StaticClass();
}
AStationFrontendController::AStationFrontendController(){bShowMouseCursor=true;DefaultMouseCursor=EMouseCursor::Default;}
void AStationFrontendController::BeginPlay()
{
    Super::BeginPlay();bShowMouseCursor=true;
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Mode);
    SavedPreferences=StationFrontend::LoadPreferences(this);Draft=DuplicateObject<UStationPreferences>(SavedPreferences,this);
    StationFrontend::ApplyAudio(GetWorld(),SavedPreferences);
    RefreshCheckpoint();Selected=CanContinue?1:0;
    if(auto* Score=LoadObject<USoundBase>(nullptr,TEXT("/Game/BZ/Audio/MU14_Title.MU14_Title")))
    {
        // Configure this persistent UI component before playback. Switching a
        // menu page never stops/restarts it; only travelling into the game fades it.
        MenuMusic=NewObject<UAudioComponent>(this,TEXT("FrontendScore"));AddInstanceComponent(MenuMusic);
        MenuMusic->bAutoActivate=false;MenuMusic->bAutoDestroy=false;MenuMusic->bAllowSpatialization=false;
        MenuMusic->SetUISound(true);MenuMusic->SetSound(Score);MenuMusic->SetVolumeMultiplier(.50f*SavedPreferences->MusicVolume/10.f);MenuMusic->RegisterComponent();
        // Fade envelope and the adjustable component gain multiply in UE.
        // Fade towards unity so later volume previews cannot square the gain.
        MenuMusic->FadeIn(1.4f,1.f);
    }
    else UE_LOG(LogTemp,Warning,TEXT("Frontend soundtrack MU14_Title is missing"));
    if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/BZ/Audio/AM_Air.AM_Air")))
    {
        MenuAmbience=NewObject<UAudioComponent>(this,TEXT("FrontendAir"));AddInstanceComponent(MenuAmbience);
        MenuAmbience->bAutoActivate=false;MenuAmbience->bAutoDestroy=false;MenuAmbience->bAllowSpatialization=false;
        MenuAmbience->SetUISound(true);MenuAmbience->SetSound(Sound);MenuAmbience->SetVolumeMultiplier(.10f*SavedPreferences->EffectsVolume/10.f);MenuAmbience->RegisterComponent();MenuAmbience->FadeIn(2.f,1.f);
    }
}
void AStationFrontendController::SetBackgroundFilmAudioPaused(bool Paused)
{
    if(MenuMusic)MenuMusic->SetPaused(Paused);
    if(MenuAmbience)MenuAmbience->SetPaused(Paused);
}
void AStationFrontendController::EndPlay(const EEndPlayReason::Type Reason)
{
    if(MenuMusic)MenuMusic->Stop();
    if(MenuAmbience)MenuAmbience->Stop();Super::EndPlay(Reason);
}
void AStationFrontendController::SetupInputComponent()
{
    Super::SetupInputComponent();
    for(const FKey Key:{EKeys::Down,EKeys::S})InputComponent->BindKey(Key,IE_Pressed,this,&AStationFrontendController::MenuNext);
    for(const FKey Key:{EKeys::Up,EKeys::W})InputComponent->BindKey(Key,IE_Pressed,this,&AStationFrontendController::MenuPrevious);
    for(const FKey Key:{EKeys::Enter,EKeys::E,EKeys::SpaceBar})InputComponent->BindKey(Key,IE_Pressed,this,&AStationFrontendController::MenuActivate);
    for(const FKey Key:{EKeys::Escape,EKeys::BackSpace})InputComponent->BindKey(Key,IE_Pressed,this,&AStationFrontendController::MenuBack);
    for(const FKey Key:{EKeys::Left,EKeys::A})InputComponent->BindKey(Key,IE_Pressed,this,&AStationFrontendController::MenuLeft);
    for(const FKey Key:{EKeys::Right,EKeys::D})InputComponent->BindKey(Key,IE_Pressed,this,&AStationFrontendController::MenuRight);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AStationFrontendController::MouseClick);
}
void AStationFrontendController::RefreshCheckpoint()
{
    const bool Exists=UGameplayStatics::DoesSaveGameExist(StationFrontend::CheckpointSlot,0);
    auto* P=Exists?Cast<UStationProgress>(UGameplayStatics::LoadGameFromSlot(StationFrontend::CheckpointSlot,0)):nullptr;
    CanContinue=P!=nullptr;CheckpointTime.Empty();
    CheckpointTitle=Exists?TEXT("存档无法读取，请保留文件后重试"):TEXT("暂无存档 · 从新游戏开始");
    if(!P)return;
    if(P->ArcFinished)CheckpointTitle=TEXT("窗后的那个人 · 故事已完成，可继续探索");
    else if(P->ArcStarted)
    {
        CheckpointTitle=TEXT("白昼产权 · 继续上次的故事");FString Text;TSharedPtr<FJsonObject> Story;
        if(FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("BZ/StoryV27/campaign.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Story))
        {
            const TArray<TSharedPtr<FJsonValue>>* Nodes=nullptr;
            if(Story->TryGetArrayField(TEXT("nodes"),Nodes))for(const auto& Value:*Nodes)
            {
                auto Node=Value->AsObject();FString Id,Title;
                if(Node&&Node->TryGetStringField(TEXT("id"),Id)&&Id==P->ArcNode&&Node->TryGetStringField(TEXT("title"),Title)){CheckpointTitle=Title+TEXT(" · 继续上次的故事");break;}
            }
        }
    }
    else if(P->CrisisEndingHeard)CheckpointTitle=TEXT("闸门另一边 · 本班余热已安排");
    else if(P->CrisisStarted)CheckpointTitle=TEXT("闸门另一边 · 实测与分流");
    else if(P->MandateEndingHeard)CheckpointTitle=TEXT("替你签下的名字 · 两份回执已留");
    else if(P->MandateCommitted)CheckpointTitle=TEXT("替你签下的名字 · 与林禾核对回执");
    else if(P->MandateStarted)CheckpointTitle=TEXT("替你签下的名字 · 下一期申请与本人意见");
    else if(P->DistrictSettled)CheckpointTitle=TEXT("光退到哪里 · 回光埠处置已留档");
    else if(P->DistrictStarted)CheckpointTitle=TEXT("光退到哪里 · 检验与押金");
    else if(P->DutySettled)CheckpointTitle=TEXT("借来的下班 · 工作已结算");
    else if(P->DutyStarted)CheckpointTitle=TEXT("借来的下班 · 工单进行中");
    else if(P->SupplyComplete)CheckpointTitle=TEXT("签在谁的名字下 · 收货已完成");
    else if(P->SupplyStarted)CheckpointTitle=TEXT("签在谁的名字下 · 新的担保");
    else if(P->ConflictComplete)CheckpointTitle=TEXT("谁的备用 · 去向已确定");
    else if(P->ConflictStarted)CheckpointTitle=TEXT("谁的备用 · 寻访与取证");
    else if(P->ReadRecall)CheckpointTitle=TEXT("第一夜 · 配额已结算");
    else if(P->Started)CheckpointTitle=TEXT("第一夜 · 公共供光与分配");
    else if(P->MetEngineer)CheckpointTitle=TEXT("旧站重启 · 维修现场");
    else CheckpointTitle=TEXT("旧站重启 · 白昼尚未到来");
    const FDateTime Stamp=IFileManager::Get().GetTimeStamp(*(FPaths::ProjectSavedDir()/TEXT("SaveGames/DaylightRights_Station_v1.sav")));
    if(Stamp!=FDateTime::MinValue())CheckpointTime=(Stamp+(FDateTime::Now()-FDateTime::UtcNow())).ToString(TEXT("%Y.%m.%d  %H:%M"));
}
int32 AStationFrontendController::RowCount() const
{
    return Screen==EStationFrontendScreen::Home?6:Screen==EStationFrontendScreen::Settings?StationExperience::SettingsCount:Screen==EStationFrontendScreen::Credits?1:(Screen==EStationFrontendScreen::ConfirmNew||Screen==EStationFrontendScreen::ConfirmQuit)?2:0;
}
float AStationFrontendController::RowY(int32 Row) const
{
    if(Screen==EStationFrontendScreen::Settings)return StationExperience::RowVisible(Selected,Row)?216+(Row%8)*58:-1000.f;
    return Screen==EStationFrontendScreen::Home?400+Row*56:Screen==EStationFrontendScreen::Credits?728:538+Row*70;
}
int32 AStationFrontendController::HitRow(float X,float Y) const
{
    const float Width=Screen==EStationFrontendScreen::Home?340:Screen==EStationFrontendScreen::Settings?610:Screen==EStationFrontendScreen::Credits?260:570;
    if(X<88||X>88+Width)return INDEX_NONE;
    for(int32 I=0;I<RowCount();++I)if(Y>=RowY(I)-6&&Y<=RowY(I)+(Screen==EStationFrontendScreen::Settings?34:48))return I;
    return INDEX_NONE;
}
FString AStationFrontendController::RowLabel(int32 Row) const
{
    if(Screen==EStationFrontendScreen::Home){const TCHAR* Labels[]={TEXT("新游戏"),TEXT("继续游戏"),TEXT("设置"),TEXT("制作人员"),TEXT("世界前史 · 失昼与供光"),TEXT("退出游戏")};return Row>=0&&Row<6?Labels[Row]:TEXT("");}
    if(Screen==EStationFrontendScreen::Settings){const TCHAR* Labels[]={TEXT("总音量"),TEXT("角色人声"),TEXT("字幕呈现"),TEXT("画面质量"),TEXT("显示模式"),TEXT("垂直同步"),TEXT("音乐音量"),TEXT("字幕大小"),TEXT("字幕背板"),TEXT("导航辅助"),TEXT("社区生活片段"),TEXT("对白音量"),TEXT("环境与效果音量"),TEXT("减少镜头运动"),TEXT("应用并保存"),TEXT("返回")};return Row>=0&&Row<16?Labels[Row]:TEXT("");}
    if(Screen==EStationFrontendScreen::Credits)return TEXT("返回主页");
    if(Screen==EStationFrontendScreen::ConfirmNew)return Row==0?TEXT("备份旧进度，开始新游戏"):TEXT("保留进度，返回");
    if(Screen==EStationFrontendScreen::ConfirmQuit)return Row==0?TEXT("退出游戏"):TEXT("返回主页");
    return TEXT("");
}
FString AStationFrontendController::RowValue(int32 Row) const
{
    if(!Draft||Screen!=EStationFrontendScreen::Settings)return TEXT("");
    switch(Row)
    {
        case 0:return FString::Printf(TEXT("%d %%"),Draft->Volume*10);
        case 1:return Draft->Voice?TEXT("开启"):TEXT("关闭");
        case 2:{const TCHAR* V[]={TEXT("慢速"),TEXT("随对白节奏"),TEXT("快速"),TEXT("直接显示")};return V[Draft->TextSpeed];}
        case 3:{const TCHAR* V[]={TEXT("低"),TEXT("中"),TEXT("高"),TEXT("极高")};return V[Draft->Quality];}
        case 4:{const TCHAR* V[]={TEXT("全屏"),TEXT("无边框"),TEXT("窗口")};return V[Draft->WindowMode];}
        case 5:return Draft->VSync?TEXT("开启"):TEXT("关闭");
        case 6:return FString::Printf(TEXT("%d %%"),Draft->MusicVolume*10);
        case 7:{const TCHAR* V[]={TEXT("标准"),TEXT("较大"),TEXT("大字幕")};return V[Draft->SubtitleScale];}
        case 8:return Draft->SubtitleBackdrop?TEXT("清晰背板"):TEXT("轻量背板");
        case 9:{const TCHAR* V[]={TEXT("沉浸 · 无标记"),TEXT("附近可见目标"),TEXT("详细导航")};return V[Draft->GuidanceDetail];}
        case 10:return Draft->AmbientStories?TEXT("开启"):TEXT("关闭");
        case 11:return FString::Printf(TEXT("%d %%"),Draft->VoiceVolume*10);
        case 12:return FString::Printf(TEXT("%d %%"),Draft->EffectsVolume*10);
        case 13:return Draft->ReducedMotion?TEXT("开启"):TEXT("关闭");
        default:return TEXT("");
    }
}
void AStationFrontendController::SetScreen(EStationFrontendScreen NewScreen)
{
    if(Screen==EStationFrontendScreen::Settings&&NewScreen!=Screen)PreviewMenuAudio(SavedPreferences);
    Screen=NewScreen;Message.Empty();Selected=(Screen==EStationFrontendScreen::ConfirmNew||Screen==EStationFrontendScreen::ConfirmQuit)?1:0;
    if(Screen==EStationFrontendScreen::Home){RefreshCheckpoint();Selected=CanContinue?1:0;}
    if(Screen==EStationFrontendScreen::Settings)Draft=DuplicateObject<UStationPreferences>(SavedPreferences,this);
}
void AStationFrontendController::MenuNext(){if(RowCount()>0)Selected=(Selected+1)%RowCount();}
void AStationFrontendController::MenuPrevious(){if(RowCount()>0)Selected=(Selected+RowCount()-1)%RowCount();}
void AStationFrontendController::MenuLeft(){ChangeSetting(-1);}
void AStationFrontendController::MenuRight(){ChangeSetting(1);}
void AStationFrontendController::ChangeSetting(int32 Direction)
{
    if(Screen!=EStationFrontendScreen::Settings||!Draft)return;
    switch(Selected)
    {
        case 0:Draft->Volume=FMath::Clamp(Draft->Volume+Direction,0,10);break;
        case 1:Draft->Voice=!Draft->Voice;break;
        case 2:Draft->TextSpeed=(Draft->TextSpeed+Direction+4)%4;break;
        case 3:Draft->Quality=(Draft->Quality+Direction+4)%4;break;
        case 4:Draft->WindowMode=(Draft->WindowMode+Direction+3)%3;break;
        case 5:Draft->VSync=!Draft->VSync;break;
        case 6:Draft->MusicVolume=FMath::Clamp(Draft->MusicVolume+Direction,0,10);break;
        case 7:Draft->SubtitleScale=(Draft->SubtitleScale+Direction+3)%3;break;
        case 8:Draft->SubtitleBackdrop=!Draft->SubtitleBackdrop;break;
        case 9:Draft->GuidanceDetail=(Draft->GuidanceDetail+Direction+3)%3;Draft->SceneGuides=Draft->GuidanceDetail>0;break;
        case 10:Draft->AmbientStories=!Draft->AmbientStories;break;
        case 11:Draft->VoiceVolume=StationExperience::ClampVolume(Draft->VoiceVolume+Direction);break;
        case 12:Draft->EffectsVolume=StationExperience::ClampVolume(Draft->EffectsVolume+Direction);break;
        case 13:Draft->ReducedMotion=!Draft->ReducedMotion;break;
        default:return;
    }
    PreviewMenuAudio(Draft);Message=TEXT("声音即时试听 · 选择“应用并保存”保留修改");
}
void AStationFrontendController::PreviewMenuAudio(const UStationPreferences* P)
{
    if(!P)return;StationFrontend::ApplyAudio(GetWorld(),P);
    if(MenuMusic)MenuMusic->SetVolumeMultiplier(.50f*P->MusicVolume/10.f);
    if(MenuAmbience)MenuAmbience->SetVolumeMultiplier(.10f*P->EffectsVolume/10.f);
}
void AStationFrontendController::MenuActivate()
{
    if(Screen==EStationFrontendScreen::Home)
    {
        switch(Selected)
        {
            case 0:SetScreen(EStationFrontendScreen::ConfirmNew);break;
            case 1:StartGame(false);break;
            case 2:SetScreen(EStationFrontendScreen::Settings);break;
            case 3:SetScreen(EStationFrontendScreen::Credits);break;
            case 4:if(!AStationBackgroundFilm::Open(this))Message=TEXT("背景影片尚未就绪，原有游戏可正常继续。");break;
            case 5:SetScreen(EStationFrontendScreen::ConfirmQuit);break;
        }
    }
    else if(Screen==EStationFrontendScreen::Settings)
    {
        if(Selected<14){ChangeSetting(1);return;}
        if(Selected==15){MenuBack();return;}
        if(!StationFrontend::StorePreferences(Draft)){Message=TEXT("设置未能保存，请检查存储空间。");return;}
        SavedPreferences=DuplicateObject<UStationPreferences>(Draft,this);StationFrontend::ApplyAudio(GetWorld(),SavedPreferences);
        if(MenuAmbience)MenuAmbience->SetVolumeMultiplier(.10f*SavedPreferences->EffectsVolume/10.f);
        if(MenuMusic)MenuMusic->SetVolumeMultiplier(.50f*SavedPreferences->MusicVolume/10.f);
        if(auto* Settings=UGameUserSettings::GetGameUserSettings())
        {
            Settings->SetOverallScalabilityLevel(SavedPreferences->Quality);Settings->SetVSyncEnabled(SavedPreferences->VSync);
            Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(SavedPreferences->WindowMode));
            // Use the display's desktop size for borderless/fullscreen; keep the current window size otherwise.
            if(SavedPreferences->WindowMode!=2)Settings->SetScreenResolution(Settings->GetDesktopResolution());
            Settings->ApplySettings(false);
        }
        Message=TEXT("设置已保存 · 下一次进入游戏仍然有效");
    }
    else if(Screen==EStationFrontendScreen::Credits)MenuBack();
    else if(Screen==EStationFrontendScreen::ConfirmNew){if(Selected==0)StartGame(true);else MenuBack();}
    else if(Screen==EStationFrontendScreen::ConfirmQuit){if(Selected==0)UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);else MenuBack();}
}
void AStationFrontendController::MenuBack(){if(Screen!=EStationFrontendScreen::Loading)SetScreen(EStationFrontendScreen::Home);}
void AStationFrontendController::StartGame(bool NewGame)
{
    if(NewGame)
    {
        const FString Old=FPaths::ProjectSavedDir()/TEXT("SaveGames/DaylightRights_Station_v1.sav");
        if(IFileManager::Get().FileExists(*Old))
        {
            LastBackup=FString::Printf(TEXT("DaylightRights_before_new_%s_%s.sav"),*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")),*FGuid::NewGuid().ToString().Left(6));
            const FString Backup=FPaths::ProjectSavedDir()/TEXT("SaveGames")/LastBackup;
            if(IFileManager::Get().Copy(*Backup,*Old,false,true)!=COPY_OK){Message=TEXT("旧进度备份失败，已保留当前存档。");return;}
            if(!UGameplayStatics::DeleteGameInSlot(StationFrontend::CheckpointSlot,0)){Message=TEXT("无法建立新存档，旧进度仍在备份中。");return;}
        }
    }
    else
    {
        RefreshCheckpoint();if(!CanContinue){Message=CheckpointTitle;return;}
    }
    Screen=EStationFrontendScreen::Loading;LoadClock=0;TravelRequested=false;bShowMouseCursor=false;
    Message=NewGame?TEXT("新的白昼，仍需要有人支付。"):TEXT("正在返回上一个已保存的决定……");
    if(MenuMusic)MenuMusic->FadeOut(.6f,0.f);
    if(MenuAmbience)MenuAmbience->FadeOut(.25f,0);
}
void AStationFrontendController::PlayerTick(float Delta)
{
    Super::PlayerTick(Delta);
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("StationFrontendSmokeTest"))&&GetWorld()->GetTimeSeconds()>5)
    {
        auto Evidence=MakeShared<FJsonObject>();bool Passed=Screen==EStationFrontendScreen::Home;
        auto Capture=[&](const TCHAR* Key){TSharedPtr<FJsonObject> State;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GetFrontendStateJson()),State);if(State)Evidence->SetObjectField(Key,State);else Passed=false;};
        Capture(TEXT("home"));auto* OriginalMusic=MenuMusic.Get();
        for(int32 I=0;I<8&&Selected!=2;++I)MenuNext();MenuActivate();
        Passed&=Screen==EStationFrontendScreen::Settings&&MenuMusic.Get()==OriginalMusic;Capture(TEXT("settings"));
        MenuBack();Passed&=Screen==EStationFrontendScreen::Home;
        for(int32 I=0;I<8&&Selected!=3;++I)MenuNext();MenuActivate();
        Passed&=Screen==EStationFrontendScreen::Credits&&MenuMusic.Get()==OriginalMusic;Capture(TEXT("credits"));
        MenuBack();Passed&=Screen==EStationFrontendScreen::Home;Capture(TEXT("returned_home"));
        Evidence->SetBoolField(TEXT("passed"),Passed);Evidence->SetStringField(TEXT("scope"),TEXT("Cooked menu navigation and persistent audio component; no settings or checkpoints written; rendered appearance and audible output require separate review"));
        FString Text;FJsonSerializer::Serialize(Evidence,TJsonWriterFactory<>::Create(&Text));
        IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Station")),true);
        FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("Station/frontend_smoke.json")));
        UE_LOG(LogTemp,Display,TEXT("STATION_FRONTEND_RUNTIME_READY %s"),Passed?TEXT("true"):TEXT("false"));FPlatformMisc::RequestExit(false);return;
    }
#endif
    if(Screen==EStationFrontendScreen::Loading)
    {
        LoadClock+=Delta;if(LoadClock>.35f&&!TravelRequested){TravelRequested=true;SetInputMode(FInputModeGameOnly());UGameplayStatics::OpenLevel(this,TEXT("/Game/BZ/Maps/L_StationPrototype"));}return;
    }
    float X=0,Y=0;int32 W=0,H=0;GetViewportSize(W,H);
    if(GetMousePosition(X,Y)&&W>0&&H>0)
    {
        if(FVector2D::Distance(LastPointer,FVector2D(X,Y))>1.f)
        {
            const float U=FMath::Min(W/1600.f,H/900.f);
            const int32 Row=HitRow((X-(W-1600*U)*.5f)/U,(Y-(H-900*U)*.5f)/U);
            if(Row!=INDEX_NONE)Selected=Row;LastPointer=FVector2D(X,Y);
        }
    }
}
void AStationFrontendController::MouseClick()
{
    float X=0,Y=0;int32 W=0,H=0;GetViewportSize(W,H);
    if(!GetMousePosition(X,Y)||W<=0||H<=0)return;
    const float U=FMath::Min(W/1600.f,H/900.f);ClickAt((X-(W-1600*U)*.5f)/U,(Y-(H-900*U)*.5f)/U);
}
void AStationFrontendController::ClickAt(float X,float Y)
{
    const int32 Row=HitRow(X,Y);if(Row==INDEX_NONE)return;Selected=Row;
    if(Screen==EStationFrontendScreen::Settings&&Row<14){ChangeSetting(X<478?-1:1);return;}
    MenuActivate();
}
void AStationFrontendController::AutomationMenuKey(FName KeyName,bool Pressed)
{
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))InputKey(FInputKeyParams(FKey(KeyName),Pressed?IE_Pressed:IE_Released,Pressed?1.f:0.f));
#endif
}
void AStationFrontendController::AutomationPointerClick(float X,float Y)
{
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))ClickAt(X,Y);
#endif
}
void AStationFrontendController::AutomationRefreshCheckpoint()
{
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))RefreshCheckpoint();
#endif
}
FString AStationFrontendController::GetFrontendStateJson() const
{
    auto Json=MakeShared<FJsonObject>();Json->SetNumberField(TEXT("screen"),int32(Screen));Json->SetNumberField(TEXT("selected"),Selected);
    Json->SetBoolField(TEXT("continue"),CanContinue);Json->SetStringField(TEXT("checkpoint"),CheckpointTitle);Json->SetStringField(TEXT("message"),Message);Json->SetStringField(TEXT("backup"),LastBackup);
    Json->SetStringField(TEXT("music_asset"),MenuMusic&&MenuMusic->Sound?MenuMusic->Sound->GetName():TEXT(""));
    Json->SetStringField(TEXT("music_instance"),MenuMusic?MenuMusic->GetName():TEXT(""));Json->SetBoolField(TEXT("music_playing"),MenuMusic&&MenuMusic->IsPlaying());
    if(Draft){Json->SetNumberField(TEXT("voice_volume"),Draft->VoiceVolume);Json->SetNumberField(TEXT("effects_volume"),Draft->EffectsVolume);Json->SetNumberField(TEXT("guidance_detail"),Draft->GuidanceDetail);Json->SetBoolField(TEXT("reduced_motion"),Draft->ReducedMotion);}
    if(Draft){Json->SetNumberField(TEXT("music_volume"),Draft->MusicVolume);Json->SetNumberField(TEXT("subtitle_scale"),Draft->SubtitleScale);Json->SetBoolField(TEXT("subtitle_backdrop"),Draft->SubtitleBackdrop);Json->SetBoolField(TEXT("scene_guides"),Draft->SceneGuides);Json->SetBoolField(TEXT("ambient_stories"),Draft->AmbientStories);}
    Json->SetStringField(TEXT("ambience_asset"),MenuAmbience&&MenuAmbience->Sound?MenuAmbience->Sound->GetName():TEXT(""));
    if(Draft){Json->SetNumberField(TEXT("volume"),Draft->Volume);Json->SetBoolField(TEXT("voice"),Draft->Voice);Json->SetNumberField(TEXT("text_speed"),Draft->TextSpeed);Json->SetNumberField(TEXT("quality"),Draft->Quality);Json->SetNumberField(TEXT("window_mode"),Draft->WindowMode);Json->SetBoolField(TEXT("vsync"),Draft->VSync);}
    FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Json,Writer);return Out;
}

AStationFrontendHUD::AStationFrontendHUD()
{
    static ConstructorHelpers::FObjectFinder<UFont> Face(TEXT("/Engine/EngineFonts/Roboto.Roboto"));if(Face.Succeeded())Font=Face.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> Art(TEXT("/Game/BZ/Frontend/T_DaylightCover.T_DaylightCover"));if(Art.Succeeded())Cover=Art.Object;
}
void AStationFrontendHUD::Text(const FString& Value,float X,float Y,float Size,FLinearColor Color)
{
    FCanvasTextItem Item(FVector2D(OX+X*Unit,OY+Y*Unit),FText::FromString(Value),FSlateFontInfo(Font,FMath::Max(8.f,Size*Unit),TEXT("Regular")),Color);
    Item.EnableShadow(FLinearColor(0,0,0,.72f));Canvas->DrawItem(Item);
}
void AStationFrontendHUD::Box(float X,float Y,float W,float H,FLinearColor Color){DrawRect(Color,OX+X*Unit,OY+Y*Unit,W*Unit,H*Unit);}
void AStationFrontendHUD::DrawHUD()
{
    Super::DrawHUD();if(AStationBackgroundFilm::DrawFilm(this,Canvas))return;auto* C=Cast<AStationFrontendController>(PlayerOwner);if(!Canvas||!C)return;
    using namespace StationFrontend;
    Unit=FMath::Max(.1f,FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f));OX=(Canvas->SizeX-1600*Unit)*.5f;OY=(Canvas->SizeY-900*Unit)*.5f;
    DrawRect(FLinearColor::Black,0,0,Canvas->SizeX,Canvas->SizeY);
    if(Cover)
    {
        const float Aspect=float(Cover->GetSizeX())/FMath::Max(1,Cover->GetSizeY()),ViewAspect=float(Canvas->SizeX)/Canvas->SizeY;
        const float UW=FMath::Min(1.f,ViewAspect/Aspect),VH=FMath::Min(1.f,Aspect/ViewAspect);
        DrawTexture(Cover,0,0,Canvas->SizeX,Canvas->SizeY,(1-UW)*.5f,(1-VH)*.5f,UW,VH,FLinearColor::White,BLEND_Opaque);
    }
    if(C->Screen==EStationFrontendScreen::Home)
    {
        Box(86,141,2,145,Gold.CopyWithNewOpacity(.8f));
        Text(TEXT("白昼产权"),105,139,67,Paper);Text(TEXT("D A Y L I G H T   R I G H T S"),110,232,15,Gold);
        Text(TEXT("有人买下白昼。"),108,292,20,Muted);Text(TEXT("有人等你回家。"),108,326,20,Muted);
        for(int32 I=0;I<6;++I)
        {
            const float Y=C->RowY(I);const bool On=C->Selected==I,Disabled=I==1&&!C->CanContinue;
            if(On){Box(88,Y-7,338,53,FLinearColor(.028f,.037f,.044f,.74f));Box(88,Y-7,2,53,Disabled?Muted:Gold);Box(105,Y+43,295,.7f,Gold.CopyWithNewOpacity(.35f));}
            Text(C->RowLabel(I),110,Y,25,Disabled?Muted.CopyWithNewOpacity(.4f):On?Paper:Muted);
        }
        Text(C->CheckpointTitle,110,741,16,C->CanContinue?Muted:Muted.CopyWithNewOpacity(.65f));
        if(C->CanContinue)Text(TEXT("上次记录  ")+C->CheckpointTime,110,771,13,Muted.CopyWithNewOpacity(.72f));
        if(C->Screen==EStationFrontendScreen::Settings)Text(FString::Printf(TEXT("%d / 2   ·   W S 翻行与翻页"),StationExperience::SettingsPage(C->Selected)+1),108,742,16,Gold);
        if(!C->Message.IsEmpty())Text(C->Message,110,803,15,Gold);
        Text(TEXT("鼠标选择   /   W S 移动   /   E 或 Enter 确认"),106,853,13,Muted);
        static const FString ProjectVersion=[](){FString Value;return GConfig&&GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),TEXT("ProjectVersion"),Value,GGameIni)?Value:TEXT("preview");}();
        Text(FString::Printf(TEXT("建筑叙事交互作品  ·  v%s"),*ProjectVersion),1266,853,13,Muted);
    }
    else if(C->Screen==EStationFrontendScreen::Loading)
    {
        Box(0,0,1600,900,FLinearColor(0,0,0,.42f));Text(TEXT("白昼产权"),105,139,67,Paper);
        Text(C->Message,109,694,24,Paper);Text(TEXT("正在载入社区"),109,745,16,Gold);
    }
    else
    {
        // A document-like panel leaves the window and empty dinner place visible.
        Box(0,0,804,900,FLinearColor(.009f,.014f,.019f,.96f));Box(803,0,1,900,Gold.CopyWithNewOpacity(.25f));
        Text(TEXT("白昼产权   /   DAYLIGHT RIGHTS"),108,65,15,Gold);
        const FString Title=C->Screen==EStationFrontendScreen::Settings?TEXT("设置"):C->Screen==EStationFrontendScreen::Credits?TEXT("制作人员"):C->Screen==EStationFrontendScreen::ConfirmNew?TEXT("重新开始一段白昼"):TEXT("离开这座社区");
        Text(Title,104,113,40,Paper);Box(108,184,580,1,Gold.CopyWithNewOpacity(.4f));
        if(C->Screen==EStationFrontendScreen::Credits)
        {
            Text(TEXT("暨南大学  ·  建筑学毕业设计"),108,224,22,Paper);
            Text(TEXT("建筑、社区生态与互动叙事实验"),108,265,17,Muted);
            Text(TEXT("建筑与故事方向"),108,335,15,Gold);Text(TEXT("Tyzxn0225"),108,364,22,Paper);
            Text(TEXT("设计、编程与视觉制作辅助"),108,424,15,Gold);Text(TEXT("OpenAI Codex · ImageGen"),108,453,21,Paper);
            Text(TEXT("引擎与制作工具"),108,514,15,Gold);Text(TEXT("Unreal Engine 5.8  /  Blender"),108,543,21,Paper);
            Text(TEXT("角色人声采用原型阶段的合成配音。"),108,587,16,Muted);
            Text(TEXT("真人反应音：Dillon Becker / Ian Lampert / Alex Brodie、Cici Fyre"),108,613,13,Muted);
            Text(TEXT("素材来源与许可随程序附带：THIRD_PARTY_NOTICES.txt"),108,638,13,Muted);
            Text(TEXT("Daylight Rights uses Unreal® Engine. Unreal® is a trademark or registered trademark"),108,662,10,Muted);
            Text(TEXT("of Epic Games, Inc. in the United States of America and elsewhere."),108,675,10,Muted);
            Text(TEXT("Unreal® Engine, Copyright 1998–2026, Epic Games, Inc. All rights reserved."),108,688,10,Muted);
            Text(TEXT("献给每一个想按时回家，却仍被迫等待的人。"),108,701,14,Paper);
        }
        if(C->Screen==EStationFrontendScreen::ConfirmNew)
        {
            Text(TEXT("你做过的选择，曾经决定谁能得到光。"),108,260,23,Paper);
            Text(TEXT("开始新游戏前，会在本机备份现有存档。"),108,325,20,Muted);
            Text(TEXT("当前继续入口随后切换为新进度。"),108,366,20,Muted);
            Text(TEXT("新故事将从林照第一次来到旧站开始。"),108,407,20,Muted);
            Text(C->CanContinue?TEXT("当前记录  ")+C->CheckpointTitle:TEXT("当前没有可继续的进度"),108,468,16,Gold);
        }
        if(C->Screen==EStationFrontendScreen::ConfirmQuit)
        {
            Text(TEXT("今晚还有一盏灯，为某个人留着。"),108,285,23,Paper);
            Text(TEXT("已保存的选择会在下次继续游戏时恢复。"),108,354,20,Muted);
        }
        for(int32 I=0;I<C->RowCount();++I)
        {
            const float Y=C->RowY(I);if(Y<0)continue;const bool On=C->Selected==I;
            if(On){Box(88,Y-7,C->Screen==EStationFrontendScreen::Settings?610:570,C->Screen==EStationFrontendScreen::Settings?36:53,FLinearColor(.09f,.115f,.125f,.65f));Box(88,Y-7,2,C->Screen==EStationFrontendScreen::Settings?36:53,Gold);}
            Text(C->RowLabel(I),110,Y,C->Screen==EStationFrontendScreen::Settings?21:22,On?Paper:Muted);
            if(C->Screen==EStationFrontendScreen::Settings&&I<14)
            {
                Text(TEXT("‹"),422,Y,23,On?Gold:Muted);Text(C->RowValue(I),466,Y+2,20,On?Paper:Muted);Text(TEXT("›"),665,Y,23,On?Gold:Muted);
            }
        }
        if(!C->Message.IsEmpty())Text(C->Message,108,799,16,Gold);
        Text(C->Screen==EStationFrontendScreen::Settings?TEXT("A D 或 ← → 调整   ·   E 确认   ·   Esc 返回"):TEXT("鼠标选择   /   E 确认   /   Esc 返回主页"),108,852,14,Muted);
    }
}
