#include "StationPrototype.h"
#include "StationNeighborhoodData.inl"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool AStationEngineer::FindNeighborhoodPath(const FVector& Goal)
{
    const FVector Here=GetActorLocation();FVector Destination=Goal;Destination.Z=Here.Z;CommunityPath.Empty();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(NeighborhoodRoute),false,this);
    // Keep long-distance routes independent of transient pedestrians, but plan
    // around nearby bodies that are actually holding their position. A tasked
    // witness may stand still at a gate just as the player can.
    TArray<FVector> StandingPeople;
    for(TActorIterator<ACharacter> It(GetWorld());It;++It)
    {
        if(*It==this)continue;
        const bool Nearby=FVector::DistSquared2D(Here,It->GetActorLocation())<=420.f*420.f;
        const bool HoldingPosition=It->IsPlayerControlled()||It->GetVelocity().SizeSquared2D()<100.f;
        if(Nearby&&HoldingPosition)StandingPeople.Add(It->GetActorLocation());else Q.AddIgnoredActor(*It);
    }
    // Keep the head envelope while allowing the ordinary movement component to
    // resolve small doorstep risers. No actor is teleported or made non-colliding.
    const float Half=70.f,Offset=GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Half;
    const auto Shape=FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius()-.5f,Half);
    auto Clear=[&](const FVector& A,const FVector& B){FHitResult Hit;return !GetWorld()->SweepSingleByChannel(Hit,A+FVector(0,0,Offset),B+FVector(0,0,Offset),FQuat::Identity,ECC_Pawn,Shape,Q);};
    if(Clear(Here,Destination)){CommunityPath.Add(Destination);return true;}
    TArray<FVector> Nodes={Here,Destination};for(const FVector& V:Neighborhood16::RouteNodes)Nodes.Add(FVector(V.X,V.Y,Here.Z));
    // Resolve the last few metres around a kiosk, table or doorpost. Authored
    // street points are too sparse to describe a tight turn out of an alcove.
    // These are candidate standing positions only; no link bypasses collision.
    for(const FVector Origin:{Here,Destination})for(float Radius:{120.f,240.f,480.f})for(int32 Spoke=0;Spoke<8;++Spoke)
    {
        const float Angle=Spoke*PI/4.f;
        Nodes.Add(Origin+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0));
    }
    // A body between two sparse street vertices needs a local arc, rather than
    // deleting the entire corridor from the graph. All links still use sweeps.
    for(FVector Origin:StandingPeople)
    {
        Origin.Z=Here.Z;
        for(float Radius:{95.f,165.f,250.f})for(int32 Spoke=0;Spoke<12;++Spoke)
        {
            const float Angle=Spoke*PI/6.f;
            Nodes.Add(Origin+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0));
        }
    }
    TArray<FIntPoint> Edges;for(const FIntPoint& E:Neighborhood16::RouteEdges)Edges.Add(FIntPoint(E.X+2,E.Y+2));
    for(int32 Endpoint=0;Endpoint<2;++Endpoint)
    {
        TArray<int32> Candidates;for(int32 I=2;I<Nodes.Num();++I)if(FVector::DistSquared2D(Nodes[Endpoint],Nodes[I])<1250.f*1250.f)Candidates.Add(I);
        Candidates.Sort([&](int32 A,int32 B){return FVector::DistSquared2D(Nodes[Endpoint],Nodes[A])<FVector::DistSquared2D(Nodes[Endpoint],Nodes[B]);});
        int32 Connected=0;for(int32 I=0;I<Candidates.Num();++I)if(Clear(Nodes[Endpoint],Nodes[Candidates[I]])){Edges.Add(FIntPoint(Endpoint,Candidates[I]));++Connected;}
        if(!Connected)return false;
    }
    TArray<float> Costs;Costs.Init(BIG_NUMBER,Nodes.Num());Costs[0]=0;TArray<int32> Parent;Parent.Init(-1,Nodes.Num());TArray<bool> Closed;Closed.Init(false,Nodes.Num());TMap<uint64,bool> CollisionCache;
    for(int32 Pass=0;Pass<Nodes.Num();++Pass)
    {
        int32 I=-1;float Best=BIG_NUMBER;for(int32 J=0;J<Nodes.Num();++J)if(!Closed[J]&&Costs[J]<Best){I=J;Best=Costs[J];}
        if(I<0)return false;
        if(I==1)
        {
            TArray<FVector> Reverse;for(int32 K=1;K>0;K=Parent[K]){if(K<0)return false;Reverse.Add(Nodes[K]);}
            for(int32 K=Reverse.Num()-1;K>=0;--K)CommunityPath.Add(Reverse[K]);return !CommunityPath.IsEmpty();
        }
        Closed[I]=true;
        for(int32 J=1;J<Nodes.Num();++J)
        {
            if(J==I||Closed[J])continue;
            // Nearby authored points form a visibility graph. Sparse hand-made
            // edges alone can strand an otherwise open courtyard after a prop
            // is moved. Every extra link is swept with the actual body capsule.
            const bool Local=I>=2&&J>=2&&FVector::DistSquared2D(Nodes[I],Nodes[J])<=1250.f*1250.f;
            if(!Local&&!Edges.ContainsByPredicate([&](const FIntPoint& E){return (E.X==I&&E.Y==J)||(E.Y==I&&E.X==J);}))continue;
            const float Cost=Costs[I]+float(FVector::Dist2D(Nodes[I],Nodes[J]));if(Cost>=Costs[J])continue;
            const uint64 Key=(uint64(FMath::Min(I,J))<<32)|uint32(FMath::Max(I,J));
            bool* Cached=CollisionCache.Find(Key);const bool Open=Cached?*Cached:Clear(Nodes[I],Nodes[J]);if(!Cached)CollisionCache.Add(Key,Open);
            if(!Open)continue;Costs[J]=Cost;Parent[J]=I;
        }
    }
    return false;
}
