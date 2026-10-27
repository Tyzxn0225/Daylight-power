#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "StationAchievements.generated.h"

// A local profile archive survives starting a new story. It never grants light,
// money, dialogue shortcuts or a preferred moral outcome.
UCLASS()
class DAYLIGHTRIGHTS_API UStationAchievementArchive : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY() int32 SchemaVersion=1;
 UPROPERTY() TMap<FName,FDateTime> UnlockedAt;
 UPROPERTY() TMap<FName,FString> WitnessNotes;
 // First-unlock image choice; never reconstructed from a later playthrough.
 UPROPERTY() TMap<FName,FName> PhotoVariants;
};
