#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StationBackgroundFilm.generated.h"

class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class AHUD;
class APlayerController;
class UCanvas;

// An optional viewer, deliberately independent of story progression and saves.
UCLASS()
class DAYLIGHTRIGHTS_API AStationBackgroundFilm : public AActor
{
    GENERATED_BODY()
public:
    AStationBackgroundFilm();
    UFUNCTION(BlueprintCallable) static AStationBackgroundFilm* Open(APlayerController* Controller);
    UFUNCTION(BlueprintPure) static AStationBackgroundFilm* GetActiveFilm(UObject* Context);
    static bool DrawFilm(AHUD* HUD,UCanvas* Canvas);
    UFUNCTION(BlueprintCallable) void CloseFilm();
    UFUNCTION(BlueprintPure) FString GetFilmStateJson() const;
    UFUNCTION(BlueprintCallable,meta=(DevelopmentOnly)) bool AutomationSeek(float Seconds);
    UFUNCTION(BlueprintCallable,meta=(DevelopmentOnly)) void AutomationHoldSkip(float Seconds);
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<APlayerController> Viewer;
    UPROPERTY() TObjectPtr<UMediaPlayer> Player;
    UPROPERTY() TObjectPtr<UMediaTexture> Texture;
    UPROPERTY() TObjectPtr<UMediaSoundComponent> Sound;
    bool Initialize(APlayerController* Controller);
    UFUNCTION() void MediaOpened(FString URL);
    UFUNCTION() void MediaFailed(FString URL);
    UFUNCTION() void MediaEnded();
    void SkipPressed();
    void SkipReleased();
    bool SavedPause=false,SavedCursor=false,Closing=false,Ready=false,SkipHeld=false;
    double OpenedAt=0,LastRealTime=0;
    float SkipSeconds=0;
    FString Failure;
};
