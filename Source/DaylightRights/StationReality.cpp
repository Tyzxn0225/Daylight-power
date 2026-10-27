#include "StationPrototype.h"
#include "StationRealityData.inl"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "StationRealism.h"
#include "Components/PostProcessComponent.h"
#include "Components/BoxComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/RectLight.h"
#include "Engine/StaticMesh.h"

static TAutoConsoleVariable<int32> CVarDaylightRealism(TEXT("Daylight.Realism"),1,TEXT("Existing-asset realism pass; 0 restores the V23 rendering and gait for comparison."));

void AStationDirector::ApplyRealityMaterials()
{
    TMap<FName,UMaterialInterface*> Materials;
    // Resolve the entire architecture palette before altering any component.
    for(const TCHAR* Name:Reality16::Materials)
    {
        FString Asset=FString(TEXT("MR16_"))+Name;
        FString Path=FString::Printf(TEXT("/Game/BZ/RealityV16/Materials/%s.%s"),*Asset,*Asset);
        auto* Material=LoadObject<UMaterialInterface>(nullptr,*Path);
        if(!Material){UE_LOG(LogTemp,Warning,TEXT("R16 material assets unavailable; original palette retained"));return;}
        if(StationRealism::Enabled()&&FString(Name)!=TEXT("BZ_metal_dark")&&FString(Name)!=TEXT("BZ_stone"))
        {
            Asset=FString(TEXT("MR24_"))+Name;Path=FString::Printf(TEXT("/Game/BZ/RealismV24/Materials/%s.%s"),*Asset,*Asset);
            if(auto* New=LoadObject<UMaterialInterface>(nullptr,*Path))Material=New;
        }
        Materials.Add(FName(*(FString(TEXT("M_"))+Name)),Material);
        if(StationRealism::Enabled())Materials.Add(FName(*(FString(TEXT("MR16_"))+Name)),Material);
    }
    Materials.Add(TEXT("M_BZ14_paper"),Materials.FindChecked(TEXT("M_BZ_fabric_tan")));
    Materials.Add(TEXT("M_BZ14_light"),Materials.FindChecked(TEXT("M_BZ_emissive")));
    int32 Changed=0;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
        for(auto* Component:Components)for(int32 Slot=0;Slot<Component->GetNumMaterials();++Slot)
        {
            auto* Current=Component->GetMaterial(Slot);if(!Current)continue;
            if(auto** Replacement=Materials.Find(Current->GetFName())){Component->SetMaterial(Slot,*Replacement);++Changed;}
            else if(StationRealism::Enabled()&&Current->GetFName()==TEXT("WorldGridMaterial"))
            {
                const UStaticMesh* Mesh=Component->GetStaticMesh();
                if(Mesh&&Mesh->GetStaticMaterials().IsValidIndex(Slot))
                {
                    const FString SlotName=Mesh->GetStaticMaterials()[Slot].ImportedMaterialSlotName.ToString();
                    // Recover only known source material identities. Unknown
                    // slots retain their existing fallback for separate review.
                    if(auto** Known=Materials.Find(FName(*(TEXT("M_")+SlotName)))){Component->SetMaterial(Slot,*Known);++Changed;}
                }
            }
        }
        if(StationRealism::Enabled())for(auto* Component:Components)
            if(Component->GetStaticMesh()&&Component->GetStaticMesh()->GetFName()==TEXT("SM_BZ_0001"))
            {
                // This source mesh is the 600 x 620 m ground slab, entirely
                // below the walking surface. It must receive building/people
                // shadows, but need not cast a world-sized VSM of its own.
                // Architecture, canopies and character shadows stay enabled.
                Component->SetCastShadow(false);
                if(auto* Road=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BZ/RealismV24/Materials/MR24_Asphalt.MR24_Asphalt")))Component->SetMaterial(0,Road);
            }
    }
    UE_LOG(LogTemp,Log,TEXT("Architecture material overrides: %d slots; realism=%d"),Changed,StationRealism::Enabled());
    if(StationRealism::Enabled())ApplyRealismLighting();
}

void AStationDirector::ApplyRealismLighting()
{
    // Preserve the old level and all allocation-driven light states. Lighting
    // and exposure refinements are transient runtime objects with no collision.
    auto* Global=GetWorld()->SpawnActor<APostProcessVolume>();Global->bUnbound=true;Global->Priority=5;
    auto& P=Global->Settings;
    P.bOverride_AutoExposureMinBrightness=true;P.AutoExposureMinBrightness=6.5f;
    P.bOverride_AutoExposureMaxBrightness=true;P.AutoExposureMaxBrightness=7.f;
    P.bOverride_AutoExposureSpeedUp=true;P.AutoExposureSpeedUp=2.f;
    P.bOverride_AutoExposureSpeedDown=true;P.AutoExposureSpeedDown=1.f;
    P.bOverride_BloomIntensity=true;P.BloomIntensity=.16f;
    P.bOverride_BloomThreshold=true;P.BloomThreshold=2.f;
    P.bOverride_ColorSaturation=true;P.ColorSaturation=FVector4(.96f,.96f,.96f,1.f);
    P.bOverride_FilmToe=true;P.FilmToe=.48f;
    P.bOverride_MotionBlurAmount=true;P.MotionBlurAmount=.12f;
    P.bOverride_LumenFinalGatherQuality=true;P.LumenFinalGatherQuality=1.5f;
    P.bOverride_LumenSceneLightingQuality=true;P.LumenSceneLightingQuality=1.f;
    auto Zone=[&](FName Name,FVector Center,FVector Extent,float EV)
    {
        auto* Actor=GetWorld()->SpawnActor<AActor>();Actor->Tags.Add(Name);
        auto* Bounds=NewObject<UBoxComponent>(Actor);Actor->AddInstanceComponent(Bounds);Actor->SetRootComponent(Bounds);
        Bounds->SetBoxExtent(Extent);Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);Bounds->SetGenerateOverlapEvents(false);Bounds->RegisterComponent();Actor->SetActorLocation(Center);
        auto* Post=NewObject<UPostProcessComponent>(Actor);Actor->AddInstanceComponent(Post);Post->SetupAttachment(Bounds);Post->bUnbound=false;Post->Priority=15;Post->BlendRadius=100.f;
        Post->Settings.bOverride_AutoExposureMinBrightness=true;Post->Settings.AutoExposureMinBrightness=EV;
        Post->Settings.bOverride_AutoExposureMaxBrightness=true;Post->Settings.AutoExposureMaxBrightness=EV;
        Post->Settings.bOverride_MotionBlurAmount=true;Post->Settings.MotionBlurAmount=0;Post->RegisterComponent();
    };
    Zone(TEXT("R24_FamilyExposure"),FVector(-3630,1850,150),FVector(300,420,170),3.2f);
    Zone(TEXT("R24_ClinicExposure"),FVector(-5700,-700,160),FVector(300,450,170),3.5f);
    Zone(TEXT("R24_RepairExposure"),FVector(-3800,2950,150),FVector(300,380,170),5.8f);
    for(TActorIterator<ARectLight> It(GetWorld());It;++It)
    {
        auto* Light=Cast<URectLightComponent>(It->GetLightComponent());if(!Light)continue;
        if(FVector::Dist(It->GetActorLocation(),FVector(-3620,1680,258))<2.f)
        {
            Light->SetIntensity(420.f);Light->SetAttenuationRadius(420.f);Light->SetSourceWidth(52.f);Light->SetSourceHeight(23.f);Light->SetCastShadows(true);
        }
    }
    auto* Emergency=GetWorld()->SpawnActor<ARectLight>(FVector(-5700,-750,265),FRotator(-90,0,0));
    auto* E=CastChecked<URectLightComponent>(Emergency->GetLightComponent());E->SetMobility(EComponentMobility::Movable);
    E->SetIntensityUnits(ELightUnits::Lumens);E->SetIntensity(160);E->SetAttenuationRadius(360);E->SetSourceWidth(35);E->SetSourceHeight(16);
    E->SetLightColor(FLinearColor(.72f,.82f,1.f));E->SetCastShadows(true);
    UE_LOG(LogTemp,Display,TEXT("R24 bounded interior exposure and emergency fixture ready; rationed circuits unchanged"));
}
