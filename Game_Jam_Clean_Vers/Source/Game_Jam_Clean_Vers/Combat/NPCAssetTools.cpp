#include "NPCAssetTools.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Builders/CubeBuilder.h"
#include "Engine/World.h"
#include "Model.h"
#include "Components/BrushComponent.h"
#endif
bool UNPCAssetTools::BuildNPCAnimations()
{
#if WITH_EDITOR
 auto* Pose=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"));if(!Pose||!Pose->GetSkeleton())return false;
 const auto& Ref=Pose->GetSkeleton()->GetReferenceSkeleton();const int32 Bones=Ref.GetNum();
 TArray<FTransform> Base=Ref.GetRefBonePose();TArray<FName> TrackNames;Pose->GetDataModel()->GetBoneTrackNames(TrackNames);
 for(int32 B=0;B<Bones;++B){if(!TrackNames.Contains(Ref.GetBoneName(B)))continue;TArray<FTransform> Values;Pose->GetDataModel()->GetBoneTrackTransforms(Ref.GetBoneName(B),Values);if(Values.Num())Base[B]=Values[0];}
 for(const FString Kind:{TEXT("Threaten"),TEXT("Beat"),TEXT("Trade"),TEXT("Command"),TEXT("Injured"),TEXT("Cower")}){
  const FString Path=TEXT("/Game/Combat/NPC/Animations/A_NPC_")+Kind;
  if(LoadObject<UAnimSequence>(nullptr,*Path))continue;
  auto* Package=CreatePackage(*Path);auto* Anim=DuplicateObject<UAnimSequence>(Pose,Package,*FPackageName::GetLongPackageAssetName(Path));if(!Anim)return false;
  Anim->SetFlags(RF_Public|RF_Standalone);auto& Controller=Anim->GetController();Controller.OpenBracket(FText::FromString(TEXT("NPC gesture")),false);Controller.SetFrameRate(FFrameRate(30,1),false);Controller.SetNumberOfFrames(FFrameNumber(60),false);
  TArray<TArray<FTransform>> Frames;Frames.SetNum(Bones);
  for(int32 F=0;F<=60;++F){
   const float T=F/60.f,Wave=FMath::Sin(T*2*PI),Pulse=FMath::Square(FMath::Sin(T*PI));
   TArray<FTransform> Local=Base,Component;Component.SetNum(Bones);
   auto Rebuild=[&](){for(int32 B=0;B<Bones;++B){const int32 Parent=Ref.GetParentIndex(B);Component[B]=Parent==INDEX_NONE?Local[B]:Local[B]*Component[Parent];}};
   auto Rotate=[&](const TCHAR* Bone,FRotator Rotation){const int32 B=Ref.FindBoneIndex(Bone);if(B!=INDEX_NONE)Local[B].SetRotation(Local[B].GetRotation()*Rotation.Quaternion());};
   auto AimBone=[&](const TCHAR* Bone,const TCHAR* Child,FVector Target){Rebuild();const int32 B=Ref.FindBoneIndex(Bone),C=Ref.FindBoneIndex(Child);if(B==INDEX_NONE||C==INDEX_NONE)return;
    FQuat Delta=FQuat::FindBetweenNormals((Component[C].GetLocation()-Component[B].GetLocation()).GetSafeNormal(),(Target-Component[B].GetLocation()).GetSafeNormal());
    const int32 Parent=Ref.GetParentIndex(B);const FQuat New=Delta*Component[B].GetRotation();Local[B].SetRotation(Parent==INDEX_NONE?New:Component[Parent].GetRotation().Inverse()*New);
   };
   // Targets are in Manny mesh space: X lateral, Y forward, Z up.
   if(Kind==TEXT("Threaten")||Kind==TEXT("Command")){
    AimBone(TEXT("upperarm_r"),TEXT("lowerarm_r"),FVector(25,50,135));AimBone(TEXT("lowerarm_r"),TEXT("hand_r"),FVector(15,80,145+Wave*10));
    Rotate(TEXT("head"),FRotator(0,Wave*6,0));
   }else if(Kind==TEXT("Trade")){
    AimBone(TEXT("upperarm_r"),TEXT("lowerarm_r"),FVector(25,35,105));AimBone(TEXT("lowerarm_r"),TEXT("hand_r"),FVector(0,55+Pulse*18,110));
    AimBone(TEXT("upperarm_l"),TEXT("lowerarm_l"),FVector(-25,25,105));AimBone(TEXT("lowerarm_l"),TEXT("hand_l"),FVector(-10,45,115));
   }else if(Kind==TEXT("Beat")){
    const float Punch=FMath::Max(0.f,FMath::Sin(T*2*PI));
    AimBone(TEXT("upperarm_r"),TEXT("lowerarm_r"),FVector(25,20+Punch*55,135));AimBone(TEXT("lowerarm_r"),TEXT("hand_r"),FVector(5,30+Punch*85,140));
    AimBone(TEXT("upperarm_l"),TEXT("lowerarm_l"),FVector(-25,20,125));AimBone(TEXT("lowerarm_l"),TEXT("hand_l"),FVector(-15,25,145));
   }else if(Kind==TEXT("Cower")){
    AimBone(TEXT("upperarm_r"),TEXT("lowerarm_r"),FVector(35,25,150));AimBone(TEXT("lowerarm_r"),TEXT("hand_r"),FVector(15,15,175));
    AimBone(TEXT("upperarm_l"),TEXT("lowerarm_l"),FVector(-35,25,150));AimBone(TEXT("lowerarm_l"),TEXT("hand_l"),FVector(-15,15,175));
    Rotate(TEXT("spine_03"),FRotator(0,0,8+Wave*3));
   }else if(Kind==TEXT("Injured")){
    // Side-lying pose, with head/arms moving independently to show a living casualty.
    AimBone(TEXT("upperarm_r"),TEXT("lowerarm_r"),FVector(30,30,110));AimBone(TEXT("lowerarm_r"),TEXT("hand_r"),FVector(5,40+Wave*8,125));
    AimBone(TEXT("upperarm_l"),TEXT("lowerarm_l"),FVector(-30,45,140));AimBone(TEXT("lowerarm_l"),TEXT("hand_l"),FVector(-20,65,150+Wave*10));
    Rotate(TEXT("head"),FRotator(Wave*5,0,0));
    Rebuild();const int32 Pelvis=Ref.FindBoneIndex(TEXT("pelvis"));if(Pelvis!=INDEX_NONE){FTransform Down=Component[Pelvis];Down.SetRotation(FQuat(FVector::YAxisVector,PI/2)*Down.GetRotation());Down.SetLocation(FVector(0,0,25+Wave));const int32 Parent=Ref.GetParentIndex(Pelvis);Local[Pelvis]=Parent==INDEX_NONE?Down:Down.GetRelativeTransform(Component[Parent]);}
   }
   for(int32 B=0;B<Bones;++B)Frames[B].Add(Local[B]);
  }
  for(int32 B=0;B<Bones;++B){const FName Name=Ref.GetBoneName(B);if(!TrackNames.Contains(Name))Controller.AddBoneCurve(Name,false);TArray<FVector3f> P,S;TArray<FQuat4f> R;
   for(const auto& Transform:Frames[B]){P.Add(FVector3f(Transform.GetLocation()));S.Add(FVector3f(Transform.GetScale3D()));R.Add(FQuat4f(Transform.GetRotation()));}Controller.SetBoneTrackKeys(Name,P,R,S,false);
  }
  Controller.CloseBracket(false);Anim->PostEditChange();Anim->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Anim);
 }
 return true;
#else
 return false;
#endif
}

AActor* UNPCAssetTools::AddNavigationBounds(UObject* WorldContext,FVector Center,FVector Size)
{
#if WITH_EDITOR
 UWorld* World=WorldContext?WorldContext->GetWorld():nullptr;if(!World||World->IsGameWorld()||Size.GetMin()<=0)return nullptr;
 auto* Bounds=World->SpawnActor<ANavMeshBoundsVolume>(Center,FRotator::ZeroRotator);if(!Bounds)return nullptr;
 Bounds->SetActorLabel(TEXT("NPC Navigation Bounds"));Bounds->Tags.Add(TEXT("NPCBehavior"));
 Bounds->Brush=NewObject<UModel>(Bounds,NAME_None,RF_Transactional);Bounds->Brush->Initialize(Bounds,false);
 auto* Builder=NewObject<UCubeBuilder>(Bounds,NAME_None,RF_Transactional);Builder->X=Size.X;Builder->Y=Size.Y;Builder->Z=Size.Z;Bounds->BrushBuilder=Builder;
 if(!Builder->Build(World,Bounds)){Bounds->Destroy();return nullptr;}
 Bounds->SetActorLocation(Center);
 Bounds->GetBrushComponent()->Brush=Bounds->Brush;Bounds->GetBrushComponent()->BuildSimpleBrushCollision();Bounds->PostEditChange();Bounds->MarkPackageDirty();
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)){Nav->OnNavigationBoundsUpdated(Bounds);Nav->Build();}
 return Bounds;
#else
 return nullptr;
#endif
}



