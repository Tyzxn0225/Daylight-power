#include "StationVisualLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#endif
bool UStationVisualLibrary::NormalizeRetargetRoot(UAnimSequence* Sequence)
{
#if WITH_EDITOR
    if(!Sequence||!Sequence->GetSkeleton()||!Sequence->GetPathName().StartsWith(TEXT("/Game/BZ/RealismV24/")))return false;
    const auto& Ref=Sequence->GetSkeleton()->GetReferenceSkeleton();if(Ref.GetNum()<2)return false;
    auto* Model=Sequence->GetDataModel();if(!Model||!Model->IsValidBoneTrackName(TEXT("root")))return false;
    TArray<FTransform> Roots;Model->GetBoneTrackTransforms(TEXT("root"),Roots);if(Roots.IsEmpty())return false;
    const FVector Scale=Ref.GetRefBonePose()[0].GetScale3D();
    if(Scale.Equals(Roots[0].GetScale3D(),.001))return true;
    // These existing FBX skeletons store metre-length children below a 100x
    // root. The IK exporter writes a unit root but centimetre pelvis keys.
    // Restore the reference root and change only direct-child translations.
    if(!Scale.Equals(FVector(100),.01)||!Roots[0].GetScale3D().Equals(FVector(1),.01))return false;
    auto& Controller=Sequence->GetController();Controller.OpenBracket(FText::FromString(TEXT("Restore original FBX root units")),false);
    for(int32 Bone=0;Bone<Ref.GetNum();++Bone)
    {
        if(Bone!=0&&Ref.GetParentIndex(Bone)!=0)continue;
        const FName Name=Ref.GetBoneName(Bone);if(!Model->IsValidBoneTrackName(Name))continue;
        TArray<FTransform> Keys;Model->GetBoneTrackTransforms(Name,Keys);TArray<FVector> P,S;TArray<FQuat> R;
        for(const auto& Key:Keys){P.Add(Key.GetTranslation()/Scale);R.Add(Key.GetRotation());S.Add(Bone==0?Scale:Key.GetScale3D());}
        if(!Controller.SetBoneTrackKeys(Name,P,R,S,false)){Controller.CloseBracket(false);return false;}
    }
    Controller.CloseBracket(false);Sequence->MarkPackageDirty();return true;
#else
    return false;
#endif
}
