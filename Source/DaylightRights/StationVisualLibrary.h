#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StationVisualLibrary.generated.h"
class UAnimSequence;
UCLASS()
class DAYLIGHTRIGHTS_API UStationVisualLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Daylight|Authoring")
    static bool NormalizeRetargetRoot(UAnimSequence* Sequence);
};
