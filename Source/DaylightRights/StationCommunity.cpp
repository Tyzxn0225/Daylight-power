#include "StationPrototype.h"
#include "StationNeighborhoodData.inl"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "StationMovementTuning.h"

void AStationEngineer::SetCommunityRoutine(const FString& Key)
{
    if(RoutineKey==Key)return;
    RoutineKey=Key;ActivityGoals.Empty();ActivityNames.Empty();ActivityActions.Empty();ActivityWaits.Empty();
    auto Add=[&](float X,const TCHAR* Name,const TCHAR* Action,float Wait)
    {ActivityGoals.Add(FVector(X,FString(Action)==TEXT("Repair")?4057:4080,0));ActivityNames.Add(Name);ActivityActions.Add(Action);ActivityWaits.Add(Wait);};
    if(Key.StartsWith(TEXT("N16_")))
    {
        auto At=[&](const TCHAR* Point,const TCHAR* Purpose,const TCHAR* Action,float Wait)
        {ActivityGoals.Add(Neighborhood16::Stand(Point,Neighborhood16::Point(Point,FVector::ZeroVector)));ActivityNames.Add(Purpose);ActivityActions.Add(Action);ActivityWaits.Add(Wait);};
        if(PersonName==TEXT("叶岑"))
        {
            At(TEXT("EngineerHome"),TEXT("在维修铺整理工位与来件"),TEXT("Idle"),10);
            At(TEXT("NodeA"),TEXT("核对院内公共接口"),TEXT("Repair"),7);
            At(TEXT("NodeC"),TEXT("核验后巷计量端"),TEXT("LookLight"),9);
            At(TEXT("NodeB"),TEXT("检查真实冷却服务带"),TEXT("Repair"),9);
        }
        else if(PersonName==TEXT("林禾"))
        {
            At(TEXT("SisterHome"),TEXT("在家整理本人的票据与申请"),TEXT("Idle"),16);
            At(TEXT("KitchenRequest"),TEXT("到厨房核对轮候与停餐记录"),TEXT("Idle"),12);
            At(TEXT("HousingRequest"),TEXT("在院内按户轮候共用灯"),TEXT("LookLight"),14);
        }
        else
        {
            At(TEXT("BrokerHome"),TEXT("在厨房核对垫款与耗材"),TEXT("Idle"),11);
            At(TEXT("AllocationLedger"),TEXT("在公共院落核对担保原单"),TEXT("Idle"),9);
            At(TEXT("ConsignmentCase"),TEXT("到维修铺核对退件封签"),TEXT("Idle"),10);
            At(TEXT("SupplierReturn"),TEXT("到后勤通道办理交接"),TEXT("Idle"),12);
        }
    }
    else if(Key.StartsWith(TEXT("ReturnLight")))
    {
        auto District=[&](float X,float Y,const TCHAR* Purpose,const TCHAR* Action,float Wait)
        {ActivityGoals.Add(FVector(X,Y,0));ActivityNames.Add(Purpose);ActivityActions.Add(Action);ActivityWaits.Add(Wait);};
        if(PersonName==TEXT("叶岑"))
        {
            District(300,8200,TEXT("通过回收片区的清晰通道"),TEXT("Idle"),2);
            District(800,9900,TEXT("核验回收设备与停订编号"),TEXT("Idle"),9);
            District(2200,12300,TEXT("核对厂房检验与估值记录"),TEXT("Idle"),12);
            District(800,11900,TEXT("检查回收热力管线"),TEXT("LookLight"),8);
        }
        else if(PersonName==TEXT("林禾"))
        {
            District(-300,8200,TEXT("从入口步行前往公开检光井"),TEXT("Idle"),3);
            District(-2200,8700,TEXT("等待公开检验"),TEXT("Idle"),7);
            District(-2200,9800,TEXT("查看灯具检验与原住户记录"),TEXT("LookLight"),12);
            District(-2200,8700,TEXT("从池底出口回到入口广场"),TEXT("Idle"),4);
        }
        else
        {
            District(400,8400,TEXT("到回收片区核对退件"),TEXT("Idle"),5);
            District(1500,10000,TEXT("核对夜班押金凭条"),TEXT("Idle"),10);
            District(2000,12200,TEXT("核对本批复售估值"),TEXT("Idle"),11);
            District(1500,10000,TEXT("等候凭条持有人回应折价要约"),TEXT("Idle"),9);
        }
    }
    else if(PersonName==TEXT("叶岑"))
    {
        Add(-1100,TEXT("检查近街接口"),TEXT("Repair"),6);
        Add(750,TEXT("检查冷却回路"),TEXT("Repair"),7);
        Add(1420,TEXT("核查应急回路负荷"),TEXT("LookLight"),5);
        Add(-2550,TEXT("核对诊疗点运行记录"),TEXT("Idle"),8);
    }
    else if(PersonName==TEXT("林禾"))
    {
        Add(1070,Key.Contains(TEXT("Funded"))?TEXT("按户轮候共用灯"):TEXT("查询居民候补登记"),TEXT("LookLight"),11);
        Add(-1280,TEXT("核对本人登记与担保关系"),TEXT("Idle"),7);
        Add(-1570,TEXT("阅读家庭旧票"),TEXT("Idle"),6);
    }
    else
    {
        Add(-250,Key.Contains(TEXT("Funded"))?TEXT("核对厨房领料"):TEXT("核对停餐与退票"),TEXT("Idle"),8);
        Add(-1280,TEXT("核对垫款与担保账册"),TEXT("Idle"),6);
        Add(350,TEXT("核查待退耗材封签"),TEXT("Idle"),8);
    }
    ActivityIndex=0;ActivityRemaining=0;ActivityDwelling=false;
    if(!CommunityTask)BeginCommunityLeg();
}
void AStationEngineer::BeginCommunityLeg()
{
    if(!CommunityTask)
    {
        if(!ActivityGoals.IsValidIndex(ActivityIndex))return;
        ActivityGoal=ActivityGoals[ActivityIndex];ActivityPurpose=ActivityNames[ActivityIndex];ActivityAction=ActivityActions[ActivityIndex];
    }
    ActivityDwelling=false;BlockedSeconds=0;TravelLeg=0;CommunityPath.Empty();RepathCountdown=0;CommunityYielding=false;CommunityYieldWait=0;
    ActivityLane=ActivityGoal.X>GetActorLocation().X?4175.f:4110.f;
    TravelPoint=FVector(GetActorLocation().X,ActivityLane,GetActorLocation().Z);
}
void AStationEngineer::SetCommunityTask(FVector Goal,const FString& Purpose,const FString& Action)
{
    if(CommunityTask&&FVector::DistSquared2D(Goal,ActivityGoal)<1&&ActivityPurpose==Purpose)return;
    CommunityTask=true;ActivityGoal=Goal;ActivityPurpose=Purpose;ActivityAction=Action;BeginCommunityLeg();
}
void AStationEngineer::ClearCommunityTask()
{
    if(!CommunityTask)return;CommunityTask=false;BeginCommunityLeg();
}
bool AStationEngineer::FindCommunityPath(const FVector& Goal)
{
    // A small walkable forecourt graph, rebuilt against real capsule collision.
    // Route around a person or a kiosk instead of repeatedly steering into it.
    constexpr int32 NX=87,NY=5,N=NX*NY;
    const FVector Here=GetActorLocation();
    if(NeighborhoodRoutes&&Here.Y<6000&&Goal.Y<6000)return FindNeighborhoodPath(Goal);
    if(NeighborhoodRoutes&&Here.Y<3900&&Goal.Y>6000)return FindNeighborhoodPath(FVector(-700,4200,Here.Z));
    const FVector RouteGoal=NeighborhoodRoutes&&Here.Y>6000&&Goal.Y<3900?FVector(-700,4200,Here.Z):Goal;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CommunityPath),false,this);
    const auto Shape=FCollisionShape::MakeCapsule(33.5f,55.f);
    auto Clear=[&](const FVector& A,const FVector& B){FHitResult H;return !GetWorld()->SweepSingleByChannel(H,A,B,FQuat::Identity,ECC_Pawn,Shape,Q);};
    CommunityPath.Empty();
    FVector Destination=RouteGoal;Destination.Z=Here.Z;
    if(Clear(Here,Destination)){CommunityPath.Add(Destination);return true;}
    if(Goal.Y>6000||Here.Y>6000)
    {
        // Authored ground-level transit spines, checked against actual capsule collision.
        // This graph does not teleport an actor or disable a building's collision.
        TArray<FVector> Nodes={Here,Destination,
            FVector(Here.X,4800,Here.Z),FVector(Here.X,5600,Here.Z),
            FVector(-400,5650,Here.Z),FVector(400,5650,Here.Z),
            FVector(-400,6500,Here.Z),FVector(400,6500,Here.Z),
            FVector(-400,8200,Here.Z),FVector(400,8200,Here.Z),
            FVector(-2200,8700,Here.Z),FVector(-2200,9800,Here.Z),
            FVector(900,9900,Here.Z),FVector(900,11900,Here.Z),
            FVector(2200,12300,Here.Z),FVector(Destination.X,Destination.Y-180,Here.Z)};
        const int32 Count=Nodes.Num();TArray<float> Distance;Distance.Init(BIG_NUMBER,Count);Distance[0]=0;
        TArray<int32> Previous;Previous.Init(-1,Count);TArray<bool> Closed;Closed.Init(false,Count);
        for(int32 Pass=0;Pass<Count;++Pass)
        {
            int32 I=-1;float Best=BIG_NUMBER;
            for(int32 J=0;J<Count;++J)if(!Closed[J]&&Distance[J]<Best){I=J;Best=Distance[J];}
            if(I<0)break;if(I==1)
            {
                TArray<FVector> Reverse;for(int32 K=1;K>0;K=Previous[K])Reverse.Add(Nodes[K]);
                for(int32 K=Reverse.Num()-1;K>=0;--K)CommunityPath.Add(Reverse[K]);return !CommunityPath.IsEmpty();
            }
            Closed[I]=true;
            for(int32 J=1;J<Count;++J)
            {
                if(Closed[J]||I==J)continue;const float Step=float(FVector::Dist2D(Nodes[I],Nodes[J]));
                if(Step>2200&&I!=0&&J!=1)continue;
                if(Distance[I]+Step>=Distance[J]||!Clear(Nodes[I],Nodes[J]))continue;
                Distance[J]=Distance[I]+Step;Previous[J]=I;
            }
        }
        return false;
    }
    auto Position=[&](int32 I){return FVector(-2700+(I/NY)*50,4070+(I%NY)*55,Here.Z);};
    TArray<float> Cost;Cost.Init(BIG_NUMBER,N);TArray<int32> Parent;Parent.Init(-1,N);
    TArray<bool> Closed;Closed.Init(false,N);TArray<int32> Open;
    for(int32 I=0;I<N;++I)if(FVector::DistSquared2D(Here,Position(I))<10000&&Clear(Here,Position(I)))
    {Cost[I]=float(FVector::Dist2D(Here,Position(I)));Open.Add(I);}
    int32 End=-1;
    while(!Open.IsEmpty())
    {
        int32 Best=0;float Score=BIG_NUMBER;
        for(int32 J=0;J<Open.Num();++J)
        {const float S=Cost[Open[J]]+float(FVector::Dist2D(Position(Open[J]),Destination));if(S<Score){Best=J;Score=S;}}
        const int32 I=Open[Best];Open.RemoveAtSwap(Best);if(Closed[I])continue;Closed[I]=true;
        const FVector P=Position(I);
        if(FVector::DistSquared2D(P,Destination)<8100&&Clear(P,Destination)){End=I;break;}
        const int32 X=I/NY,Y=I%NY;
        for(int32 DX=-1;DX<=1;++DX)for(int32 DY=-1;DY<=1;++DY)
        {
            if((DX==0&&DY==0)||X+DX<0||X+DX>=NX||Y+DY<0||Y+DY>=NY)continue;
            const int32 J=(X+DX)*NY+Y+DY;if(Closed[J])continue;
            const FVector Next=Position(J);const float C=Cost[I]+float(FVector::Dist2D(P,Next));
            if(C>=Cost[J]||!Clear(P,Next))continue;Cost[J]=C;Parent[J]=I;Open.Add(J);
        }
    }
    if(End<0)return false;
    TArray<FVector> Reverse;Reverse.Add(Destination);
    for(int32 I=End;I>=0;I=Parent[I])Reverse.Add(Position(I));
    // Collapse clear segments so grid vertices do not produce mechanical zigzags.
    FVector Anchor=Here;
    for(int32 I=Reverse.Num()-1;I>=0;)
    {
        int32 Furthest=I;for(int32 J=I;J>=0;--J)if(Clear(Anchor,Reverse[J]))Furthest=J;
        CommunityPath.Add(Reverse[Furthest]);Anchor=Reverse[Furthest];I=Furthest-1;
    }
    return !CommunityPath.IsEmpty();
}
bool AStationEngineer::BeginCommunityYield()
{
    if(CommunityTask||CommunityYielding)return false;
    const FVector Here=GetActorLocation();
    for(float DX:{-150.f,150.f,-250.f,250.f})for(float Y:{4290.f,4070.f,4235.f})
    {
        const FVector Bay(NeighborhoodRoutes||Here.Y>6000?Here.X+DX:FMath::Clamp(Here.X+DX,-2660.,1560.),NeighborhoodRoutes||Here.Y>6000?Here.Y+Y-4170.f:Y,Here.Z);
        bool BlocksTask=false;
        for(TActorIterator<AStationEngineer> It(GetWorld());It;++It)
            if(*It!=this&&It->CommunityTask&&FVector::Dist2D(Bay,It->ActivityGoal)<140)BlocksTask=true;
        if(BlocksTask||!FindCommunityPath(Bay))continue;
        ActivityDwelling=false;CommunityYieldPoint=Bay;CommunityYielding=true;RepathCountdown=.8f;BlockedSeconds=0;return true;
    }
    return false;
}
void AStationEngineer::TickCommunityMotion(float Delta)
{
    if(ConversationPaused||(!Patrol&&!CommunityTask)||ActivityGoals.IsEmpty())return;
    // Important actors travel at the player's pace. Background routines retain
    // their walking gait; a distant quest actor never needs a minute to catch up.
    if(CommunityTask)
    {
        const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
        const float ToGoal=FVector::Dist2D(GetActorLocation(),ActivityGoal);
        float Pace=StationMovement::Travel;
        if(Player)
        {
            const float PlayerSpeed=Player->GetVelocity().Size2D();
            const float Gap=FVector::Dist2D(Player->GetActorLocation(),GetActorLocation());
            const bool PlayerAhead=FVector::Dist2D(Player->GetActorLocation(),ActivityGoal)+150<ToGoal;
            Pace=PlayerAhead&&Gap>450?StationMovement::Sprint:FMath::Clamp(PlayerSpeed+30,StationMovement::Travel,StationMovement::Sprint);
            // A guide waits only when the player deliberately leaves him behind.
            // The independently accessible evidence remains available meanwhile.
            if(!PlayerAhead&&Gap>900)Pace=StationMovement::Stroll;
        }
        if(ToGoal<180)Pace=FMath::Min(Pace,FMath::GetMappedRangeValueClamped(FVector2D(18,180),FVector2D(100,340),ToGoal));
        // CharacterMovement supplies acceleration and braking. Some chapter
        // routines reset MaxWalkSpeed each tick, so interpolating that reset
        // value would keep the guide permanently close to 115 cm/s.
        GetCharacterMovement()->MaxWalkSpeed=Pace;
    }
    // Routine workers also yield while dwelling at a shared counter. Waiting
    // until they themselves hit a body can deadlock a witnessed story task at
    // the kitchen, because the routine's timer and return path use that counter.
    if(!CommunityTask&&!CommunityYielding&&CommunityYieldWait<=0)
        for(TActorIterator<AStationEngineer> It(GetWorld());It;++It)
            if(*It!=this&&It->CommunityTask&&FVector::Dist2D(GetActorLocation(),It->ActivityGoal)<140&&FVector::Dist2D(It->GetActorLocation(),It->ActivityGoal)<330)
            {BeginCommunityYield();break;}
    FVector Escape=FVector::ZeroVector;
    for(TActorIterator<ACharacter> It(GetWorld());It;++It)
    {
        if(*It==this||It->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::NoCollision)continue;
        const float Limit=GetCapsuleComponent()->GetScaledCapsuleRadius()+It->GetCapsuleComponent()->GetScaledCapsuleRadius()+1.f;
        if(FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation())>=Limit*Limit||FMath::Abs(GetActorLocation().Z-It->GetActorLocation().Z)>GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())continue;
        FVector Away=(GetActorLocation()-It->GetActorLocation()).GetSafeNormal2D();
        if(Away.IsNearlyZero())Away=GetUniqueID()>It->GetUniqueID()?FVector(0,1,0):FVector(0,-1,0);
        Escape+=Away;
    }
    if(!Escape.IsNearlyZero())
    {
        // An initial overlap cannot produce a valid sweep path. Ask character
        // movement to separate the bodies first, using its normal penetration
        // resolution and collision, then rebuild the route from the clear spot.
        ActivityDwelling=false;CommunityPath.Empty();RepathCountdown=0;AddMovementInput(Escape.GetSafeNormal2D(),1,true);return;
    }
    if(ActivityDwelling)
    {
        GetCharacterMovement()->StopMovementImmediately();
        const FRotator Facing(0,-90,0);SetActorRotation(FMath::RInterpTo(GetActorRotation(),Facing,Delta,4));
        // One tool gesture at arrival; sustained work only at the actual repair points.
        if(FMath::Abs(FRotator::NormalizeAxis(GetActorRotation().Yaw+90.0))>12)PlayStationAction(TEXT("Idle"));
        else if(ActivityAction==TEXT("Repair")||ActivityAction==TEXT("LookLight"))PlayStationAction(ActivityAction);
        if(CommunityTask)return;
        ActivityRemaining-=Delta;
        if(ActivityRemaining<=0){ActivityIndex=(ActivityIndex+1)%ActivityGoals.Num();BeginCommunityLeg();}
        return;
    }
    if(CommunityYieldWait>0)
    {CommunityYieldWait-=Delta;GetCharacterMovement()->StopMovementImmediately();return;}
    if(CommunityYielding&&FVector::Dist2D(GetActorLocation(),CommunityYieldPoint)<18)
    {CommunityYielding=false;CommunityYieldWait=4.f;CommunityPath.Empty();RepathCountdown=0;GetCharacterMovement()->StopMovementImmediately();return;}
    if(!CommunityYielding&&FVector::Dist2D(GetActorLocation(),ActivityGoal)<18)
    {
        ActivityDwelling=true;ActivityRemaining=CommunityTask?0:ActivityWaits[ActivityIndex];
        GetCharacterMovement()->StopMovementImmediately();PlayStationAction(TEXT("Idle"));return;
    }
    RepathCountdown-=Delta;
    if(RepathCountdown<=0)
    {
        if(CommunityPath.IsEmpty())FindCommunityPath(CommunityYielding?CommunityYieldPoint:ActivityGoal);RepathCountdown=.8f;
    }
    while(!CommunityPath.IsEmpty()&&FVector::Dist2D(GetActorLocation(),CommunityPath[0])<12)CommunityPath.RemoveAt(0);
    if(CommunityPath.IsEmpty())
    {
        BlockedSeconds+=Delta;GetCharacterMovement()->StopMovementImmediately();
        // If two people occupy one another's destinations, waiting alone cannot
        // resolve it. Routine pedestrians step into a clear bay, retaining their
        // actual work goal; the witnessed consignment task gets right of way.
        if(BlockedSeconds>1.2f)BeginCommunityYield();return;
    }
    const FVector Direction=(CommunityPath[0]-GetActorLocation()).GetSafeNormal2D();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CommunityWalk),false,this);FHitResult Hit;
    const float Probe=FMath::Min(40.f,float(FVector::Dist2D(GetActorLocation(),CommunityPath[0])));
    if(GetWorld()->SweepSingleByChannel(Hit,GetActorLocation(),GetActorLocation()+Direction*Probe,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(33.5f,55.f),Q))
    {BlockedSeconds+=Delta;CommunityPath.Empty();RepathCountdown=0;GetCharacterMovement()->StopMovementImmediately();if(BlockedSeconds>1.2f)BeginCommunityYield();return;}
    BlockedSeconds=0;
    AddMovementInput(Direction,1,true);
}
FString AStationEngineer::GetActivityStateJson() const
{
    const FVector P=GetActorLocation();
    const float AnimationTime=GetMesh()->GetSingleNodeInstance()?GetMesh()->GetSingleNodeInstance()->GetCurrentTime():0;
    FString Text=FString::Printf(TEXT("{\"name\":\"%s\",\"purpose\":\"%s\",\"routine\":\"%s\",\"task\":%s,\"paused\":%s,\"dwelling\":%s,\"index\":%d,\"wait\":%.2f,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"goal_x\":%.2f,\"goal_y\":%.2f,\"speed\":%.2f,\"motion\":\"%s\",\"animation_time\":%.3f,\"blocked\":%.2f}"),*PersonName,*ActivityPurpose,*RoutineKey,CommunityTask?TEXT("true"):TEXT("false"),ConversationPaused?TEXT("true"):TEXT("false"),ActivityDwelling?TEXT("true"):TEXT("false"),ActivityIndex,ActivityRemaining,P.X,P.Y,P.Z,ActivityGoal.X,ActivityGoal.Y,GetVelocity().Size2D(),*MotionKey,AnimationTime,BlockedSeconds);
#if WITH_EDITOR
    if(CommunityTask&&BlockedSeconds>2&&FParse::Param(FCommandLine::Get(),TEXT("DaylightAutomation")))
    {
        TSharedPtr<FJsonObject> State;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),State);
        const float Half=70.f,Offset=GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Half;
        const auto Shape=FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius()-.5f,Half);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CommunityDiagnostics),false,this);
        for(int Side=0;Side<2;++Side)
        {
            FVector Origin=Side?ActivityGoal:P;Origin.Z=P.Z;TArray<FVector> Nearest;
            for(const FVector& V:Neighborhood16::RouteNodes)if(FVector::DistSquared2D(V,Origin)<1250.f*1250.f)Nearest.Add(FVector(V.X,V.Y,P.Z));
            Nearest.Sort([&](const FVector& A,const FVector& B){return FVector::DistSquared2D(A,Origin)<FVector::DistSquared2D(B,Origin);});
            TArray<TSharedPtr<FJsonValue>> Links;
            for(int I=0;I<FMath::Min(16,Nearest.Num());++I)
            {
                FHitResult Hit;const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Origin+FVector(0,0,Offset),Nearest[I]+FVector(0,0,Offset),FQuat::Identity,ECC_Pawn,Shape,Query);
                auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("x"),Nearest[I].X);Row->SetNumberField(TEXT("y"),Nearest[I].Y);Row->SetBoolField(TEXT("blocked"),Blocked);Row->SetBoolField(TEXT("overlap"),Hit.bStartPenetrating);
                Row->SetStringField(TEXT("actor"),Hit.GetActor()?Hit.GetActor()->GetName():TEXT(""));Row->SetStringField(TEXT("component"),Hit.GetComponent()?Hit.GetComponent()->GetName():TEXT(""));Links.Add(MakeShared<FJsonValueObject>(Row));
            }
            State->SetArrayField(Side?TEXT("goal_links"):TEXT("start_links"),Links);
        }
        TArray<TSharedPtr<FJsonValue>> Bridges;
        for(const FIntPoint& Edge:Neighborhood16::RouteEdges)if(Edge.Y>=152)
        {
            FVector A=Neighborhood16::RouteNodes[Edge.X],B=Neighborhood16::RouteNodes[Edge.Y];A.Z=B.Z=P.Z+Offset;FHitResult Hit;
            const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_Pawn,Shape,Query);auto Row=MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("a"),Edge.X);Row->SetNumberField(TEXT("b"),Edge.Y);Row->SetBoolField(TEXT("blocked"),Blocked);
            Row->SetStringField(TEXT("actor"),Hit.GetActor()?Hit.GetActor()->GetName():TEXT(""));Row->SetStringField(TEXT("point"),Hit.ImpactPoint.ToString());
            const auto* Static=Cast<UStaticMeshComponent>(Hit.GetComponent());Row->SetStringField(TEXT("mesh"),Static&&Static->GetStaticMesh()?Static->GetStaticMesh()->GetName():TEXT(""));Bridges.Add(MakeShared<FJsonValueObject>(Row));
        }
        State->SetArrayField(TEXT("bridges"),Bridges);FJsonSerializer::Serialize(State,TJsonWriterFactory<>::Create(&Text));
    }
#endif
    return Text;
}
FString AStationDirector::GetCommunityStateJson() const
{
    return FString::Printf(TEXT("{\"people\":[%s%s%s]}"),Engineer?*Engineer->GetActivityStateJson():TEXT("null"),LinHe?*(TEXT(",")+LinHe->GetActivityStateJson()):TEXT(""),ZhouChi?*(TEXT(",")+ZhouChi->GetActivityStateJson()):TEXT(""));
}
void AStationDirector::UpdateWorkYield()
{
    if(!IsPhysicalActionActive())
    {
        for(auto& Person:WorkYieldPeople)if(IsValid(Person))Person->ClearCommunityTask();
        WorkYieldPeople.Empty();return;
    }
    int32 I=0;
    TArray<AStationEngineer*> Workers={Engineer.Get(),LinHe.Get(),ZhouChi.Get()};
    if(IsArcChapter())for(AStationEngineer* Person:ArcPeople)Workers.Add(Person);
    for(AStationEngineer* Person:Workers)
    {
        const float Offsets[]={-220,180,340};
        FVector Bay(NeighborhoodReady||PendingStandPoint.Y>6000?PendingStandPoint.X+Offsets[I%3]:FMath::Clamp(PendingStandPoint.X+Offsets[I%3],-2660.,1560.),NeighborhoodReady||PendingStandPoint.Y>6000?PendingStandPoint.Y+190.f:I==2?4180:4290,PendingStandPoint.Z);++I;
        if(!IsValid(Person)||Person->ConversationPaused||WorkYieldPeople.Contains(Person)||FVector::Dist2D(Person->GetActorLocation(),PendingStandPoint)>180)continue;
        if(NeighborhoodReady&&PendingStandPoint.Y<6000)
        {
            bool Found=false;
            for(const FVector Offset:{FVector(-210,160,0),FVector(210,160,0),FVector(-210,-160,0),FVector(210,-160,0),FVector(0,260,0),FVector(0,-260,0)})
                if(Person->FindNeighborhoodPath(PendingStandPoint+Offset)){Bay=PendingStandPoint+Offset;Found=true;break;}
            if(!Found)Bay=Person->GetActorLocation();
        }
        WorkYieldPeople.Add(Person);Person->SetCommunityTask(Bay,TEXT("让出操作位 · 等待检修完成"));
    }
}
void AStationDirector::TickCommunity(float Delta)
{
    UpdateWorkYield();
    if(IsArcChapter()){TickArcCommunity();return;}
    if(IsCrisisChapter()){TickCrisisCommunity();return;}
    if(IsMandateChapter()){TickMandateCommunity(Delta);return;}
    if(IsDistrictChapter())
    {
        for(auto* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(Person)
        {
            Person->Patrol=true;Person->GetCharacterMovement()->MaxWalkSpeed=115.f;
            if(!WorkYieldPeople.Contains(Person))Person->ClearCommunityTask();
            Person->SetCommunityRoutine(TEXT("ReturnLightV14"));
        }
        return;
    }
    if(Engineer)Engineer->SetCommunityRoutine(NeighborhoodReady?TEXT("N16_Maintenance"):TEXT("Maintenance"));
    if(LinHe){LinHe->Patrol=true;LinHe->SetCommunityRoutine(NeighborhoodReady?TEXT("N16_Home"):Progress->RationMask>=0&&(Progress->RationMask&4)?TEXT("HousingFunded"):TEXT("HousingWaiting"));}
    if(ZhouChi)ZhouChi->SetCommunityRoutine(NeighborhoodReady?TEXT("N16_Exchange"):Progress->RationMask>=0&&(Progress->RationMask&2)?TEXT("KitchenFunded"):TEXT("KitchenWaiting"));
    UpdateConflictActors();UpdateSupplyActors();UpdateDutyActors();
    if(IsDutyChapter()){WasDutyCommunity=true;TickDutyCommunity();return;}
    if(WasDutyCommunity)
    {
        for(auto* P:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(P&&!WorkYieldPeople.Contains(P)){P->GetCharacterMovement()->MaxWalkSpeed=115.f;P->ClearCommunityTask();}
        WasDutyCommunity=false;
    }
    if(IsSupplyChapter()){WasSupplyCommunity=true;TickSupplyCommunity();return;}
    if(WasSupplyCommunity)
    {
        for(auto* Person:{Engineer.Get(),LinHe.Get(),ZhouChi.Get()})if(Person&&!WorkYieldPeople.Contains(Person))Person->ClearCommunityTask();
        WasSupplyCommunity=false;
    }
    if(!IsConflictChapter()||!ZhouChi||WorkYieldPeople.Contains(ZhouChi))return;
    // The broker waits beside the crate, leaving its authored player
    // operating position clear for inspection and the later pickup.
    const FVector CaseWaitPoint=NeighborhoodReady?Neighborhood16::Stand(TEXT("ConsignmentCase"),FVector(350,4080,0))+FVector(180,0,0):FVector(500,4080,0);
    if(Progress->ConflictWitness<3)
    {
        const FVector Stops[]={ConflictTraces.Num()==3?ConflictTraces[0]->GetActorLocation()+FVector(110,110,88):FVector(-250,4080,0),ConflictTraces.Num()==3?ConflictTraces[1]->GetActorLocation()+FVector(110,110,88):FVector(-1280,4080,0),CaseWaitPoint};
        static const TCHAR* Purposes[]={TEXT("退回前核对厨房领料"),TEXT("退回前核对垫款账册"),TEXT("交接封存稳压器 · 等待见证")};
        int32 I=0;while(I<2&&(Progress->ConflictTraceMask&(1<<I)))++I;
        ZhouChi->SetCommunityTask(Stops[I],Purposes[I]);
        // Existing checkpoints keep their past witnesses. New progress is earned
        // by examining physical clues, independently of the guide's arrival.
        WitnessSeconds=0;
    }
    else if(!Progress->ConflictComplete)ZhouChi->SetCommunityTask(CaseWaitPoint,TEXT("等待稳压器处置与退件签收"));
    else ZhouChi->ClearCommunityTask();
}
