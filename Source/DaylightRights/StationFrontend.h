#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "GameFramework/SaveGame.h"
#include "StationFrontend.generated.h"

class UTexture2D;
class UFont;
class UAudioComponent;

UCLASS()
class DAYLIGHTRIGHTS_API UStationPreferences : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 VoiceVolume=10;
    UPROPERTY() int32 EffectsVolume=10;
    UPROPERTY() int32 GuidanceDetail=1;
    UPROPERTY() bool ReducedMotion=false;
    UPROPERTY() int32 Volume=8;
    UPROPERTY() bool Voice=true;
    UPROPERTY() int32 TextSpeed=1;
    UPROPERTY() int32 Quality=2;
    UPROPERTY() int32 WindowMode=2;
    UPROPERTY() bool VSync=true;
    UPROPERTY() int32 MusicVolume=7;
    UPROPERTY() int32 SubtitleScale=0;
    UPROPERTY() bool SubtitleBackdrop=true;
    UPROPERTY() bool SceneGuides=true;
    UPROPERTY() bool AmbientStories=true;
};

namespace StationFrontend
{
    DAYLIGHTRIGHTS_API UStationPreferences* LoadPreferences(UObject* Owner);
    DAYLIGHTRIGHTS_API void ApplyAudio(UWorld* World,const UStationPreferences* Preferences);
    DAYLIGHTRIGHTS_API bool StorePreferences(UStationPreferences* Preferences);
    inline const TCHAR* CheckpointSlot=TEXT("DaylightRights_Station_v1");
    inline const TCHAR* PreferencesSlot=TEXT("DaylightRights_Preferences_v1");
}

UENUM(BlueprintType)
enum class EStationFrontendScreen : uint8 { Home, Settings, Credits, ConfirmNew, ConfirmQuit, Loading };

UCLASS()
class DAYLIGHTRIGHTS_API AStationFrontendController : public APlayerController
{
    GENERATED_BODY()
public:
    AStationFrontendController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) void MenuNext();
    UFUNCTION(BlueprintCallable) void MenuPrevious();
    UFUNCTION(BlueprintCallable) void MenuActivate();
    UFUNCTION(BlueprintCallable) void MenuBack();
    UFUNCTION(BlueprintCallable) void MenuLeft();
    UFUNCTION(BlueprintCallable) void MenuRight();
    UFUNCTION(BlueprintPure) FString GetFrontendStateJson() const;
    UFUNCTION(BlueprintCallable,meta=(DevelopmentOnly)) void AutomationMenuKey(FName KeyName,bool Pressed);
    UFUNCTION(BlueprintCallable,meta=(DevelopmentOnly)) void AutomationPointerClick(float X,float Y);
    UFUNCTION(BlueprintCallable,meta=(DevelopmentOnly)) void AutomationRefreshCheckpoint();
    UPROPERTY(BlueprintReadOnly) EStationFrontendScreen Screen=EStationFrontendScreen::Home;
    UPROPERTY(BlueprintReadOnly) int32 Selected=0;
    UPROPERTY(BlueprintReadOnly) bool CanContinue=false;
    UPROPERTY(BlueprintReadOnly) FString CheckpointTitle;
    UPROPERTY(BlueprintReadOnly) FString CheckpointTime;
    UPROPERTY(BlueprintReadOnly) FString Message;
    UPROPERTY(BlueprintReadOnly) FString LastBackup;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UStationPreferences> Draft;
    void SetBackgroundFilmAudioPaused(bool Paused);
    FString RowLabel(int32 Row) const;
    FString RowValue(int32 Row) const;
    int32 RowCount() const;
    int32 HitRow(float X,float Y) const;
    float RowY(int32 Row) const;
    void SetScreen(EStationFrontendScreen NewScreen);
private:
    UPROPERTY() TObjectPtr<UStationPreferences> SavedPreferences;
    UPROPERTY() TObjectPtr<UAudioComponent> MenuAmbience;
    UPROPERTY() TObjectPtr<UAudioComponent> MenuMusic;
    FVector2D LastPointer=FVector2D(-1,-1);
    float LoadClock=0;
    bool TravelRequested=false;
    void PreviewMenuAudio(const UStationPreferences* Preferences);
    void RefreshCheckpoint();
    void ChangeSetting(int32 Direction);
    void StartGame(bool NewGame);
    void MouseClick();
    void ClickAt(float X,float Y);
};

UCLASS()
class DAYLIGHTRIGHTS_API AStationFrontendHUD : public AHUD
{
    GENERATED_BODY()
public:
    AStationFrontendHUD();
    virtual void DrawHUD() override;
private:
    UPROPERTY() TObjectPtr<UTexture2D> Cover;
    UPROPERTY() TObjectPtr<UFont> Font;
    float Unit=1,OX=0,OY=0;
    void Text(const FString& Value,float X,float Y,float Size,FLinearColor Color);
    void Box(float X,float Y,float W,float H,FLinearColor Color);
};

UCLASS()
class DAYLIGHTRIGHTS_API AStationFrontendGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AStationFrontendGameMode();
};
