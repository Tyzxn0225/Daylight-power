#include "StationPrototype.h"
#include "Engine/World.h"

FVector AStationDirector::PlaceOnPavement(const FVector& Position) const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(StationStreetFurniture),false);
    Params.AddIgnoredActor(this);
    if(GetWorld()->LineTraceSingleByChannel(Hit,FVector(Position.X,Position.Y,250),FVector(Position.X,Position.Y,-100),ECC_Visibility,Params))
        return FVector(Position.X,Position.Y,Hit.ImpactPoint.Z+.3f);
    return Position;
}
