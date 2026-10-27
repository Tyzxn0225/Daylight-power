#pragma once
#include "CoreMinimal.h"
class AActor;
struct FStationMapPin
{
 TWeakObjectPtr<AActor> Actor;
 FVector Position=FVector::ZeroVector;
 FString Name;
 int32 Portrait=-1;
 bool Primary=false,Optional=false,Discovered=false;
};
