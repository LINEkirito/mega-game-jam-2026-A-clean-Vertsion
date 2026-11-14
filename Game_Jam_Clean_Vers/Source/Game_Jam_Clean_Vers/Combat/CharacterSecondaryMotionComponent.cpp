#include "CharacterSecondaryMotionComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

UCharacterSecondaryMotionComponent::UCharacterSecondaryMotionComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 // Finish attachments after the animated body but before PostUpdateWork scene
 // captures; otherwise selfies can combine this frame's head with last frame's hair.
 PrimaryComponentTick.TickGroup=TG_PostPhysics;
}

void UCharacterSecondaryMotionComponent::Initialize(USkeletalMeshComponent* Source)
{
 if(!Source||Body)return;
 Body=Source;AddTickPrerequisiteComponent(Source);AddTickPrerequisiteActor(GetOwner());
 auto Create=[&](const TCHAR* Name,const TCHAR* Path,bool HideFromOwner)
 {
  auto* Asset=LoadObject<USkeletalMesh>(nullptr,Path);if(!Asset)return static_cast<UPoseableMeshComponent*>(nullptr);
  auto* Part=NewObject<UPoseableMeshComponent>(GetOwner(),Name);
  Part->SetupAttachment(Source);Part->SetSkinnedAssetAndUpdate(Asset);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);
  Part->SetOwnerNoSee(HideFromOwner);Part->SetCastShadow(true);Part->bCastHiddenShadow=HideFromOwner;
  Part->SetBoundsScale(1.3f);GetOwner()->AddInstanceComponent(Part);Part->RegisterComponent();
  return Part;
 };
 Hair=Create(TEXT("CharacterPhysicsHair"),TEXT("/Game/Characters/MechanicGirl/SK_MechanicHair_Secondary"),true);
 Cloth=Create(TEXT("CharacterPhysicsCloth"),TEXT("/Game/Characters/MechanicGirl/SK_MechanicCloth_Secondary"),false);
 FinalPoseHandle=Source->RegisterOnBoneTransformsFinalizedDelegate(
  FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this,&UCharacterSecondaryMotionComponent::RefreshAttachmentsAfterAnimation));
 RefreshAttachmentsAfterAnimation();
}

void UCharacterSecondaryMotionComponent::RefreshAttachmentsAfterAnimation()
{
 if(!Body)return;
 // Camera/weapon late IK may finalize the body again after our simulation tick.
 // Rebase the solved local accessory rotations on that pose, without integrating
 // springs a second time or leaving the scalp on the previous head orientation.
 auto Refresh=[&](UPoseableMeshComponent* Part,TArray<int32>& SourceBones)
 {
  if(!Part)return;
  const TArray<FTransform> Previous=Part->BoneSpaceTransforms;
  CopyAnimatedPose(Part,SourceBones);
  for(int32 B=0;B<Part->BoneSpaceTransforms.Num();++B)
   if(SourceBones.IsValidIndex(B)&&SourceBones[B]==INDEX_NONE&&Previous.IsValidIndex(B))
    Part->BoneSpaceTransforms[B].SetRotation(Previous[B].GetRotation());
  Part->MarkRefreshTransformDirty();Part->RefreshBoneTransforms();
 };
 Refresh(Hair,HairSourceBones);Refresh(Cloth,ClothSourceBones);
}

void UCharacterSecondaryMotionComponent::CopyAnimatedPose(UPoseableMeshComponent* Target,TArray<int32>& SourceBones)
{
 auto* Asset=Target?Cast<USkeletalMesh>(Target->GetSkinnedAsset()):nullptr;
 if(!Asset)return;
 const auto& Ref=Asset->GetRefSkeleton();
 if(SourceBones.Num()!=Ref.GetNum())
 {
  SourceBones.SetNum(Ref.GetNum());
  for(int32 B=0;B<Ref.GetNum();++B)SourceBones[B]=Body->GetBoneIndex(Ref.GetBoneName(B));
 }
 Target->BoneSpaceTransforms=Ref.GetRefBonePose();
 const auto& BodyCS=Body->GetComponentSpaceTransforms();
 TArray<FTransform> CS;CS.SetNum(Ref.GetNum());
 for(int32 B=0;B<Ref.GetNum();++B)
 {
  const int32 Parent=Ref.GetParentIndex(B),Source=SourceBones[B];
  if(BodyCS.IsValidIndex(Source))
  {
   CS[B]=BodyCS[Source];
   Target->BoneSpaceTransforms[B]=Parent<0?CS[B]:CS[B].GetRelativeTransform(CS[Parent]);
  }
  else CS[B]=Parent<0?Target->BoneSpaceTransforms[B]:Target->BoneSpaceTransforms[B]*CS[Parent];
 }
}

void UCharacterSecondaryMotionComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(DT,Type,Tick);
 if(!Body||!Body->GetSkeletalMeshAsset())return;
 CopyAnimatedPose(Hair,HairSourceBones);CopyAnimatedPose(Cloth,ClothSourceBones);
 // The final animated body and accessories share a component-space origin.
 // Hand capsules also keep nearby hanging strands off the held equipment area.
 TArray<FCharacterSecondaryMotion::FCapsule> Guards;
 for(const TCHAR* Side:{TEXT("l"),TEXT("r")})
 {
  const FName Hand(*(FString(TEXT("hand_"))+Side));
  const FName Finger(*(FString(TEXT("middle_01_"))+Side));
  if(Body->GetBoneIndex(Hand)!=INDEX_NONE&&Body->GetBoneIndex(Finger)!=INDEX_NONE)
   Guards.Add({Body->GetSocketLocation(Hand),Body->GetSocketLocation(Finger),5.f});
 }
 HairMotion.Apply(Hair,DT,1.f,Guards);ClothMotion.Apply(Cloth,DT);
}

void UCharacterSecondaryMotionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(Body)Body->UnregisterOnBoneTransformsFinalizedDelegate(FinalPoseHandle);
 FinalPoseHandle.Reset();
 if(Hair)Hair->DestroyComponent();if(Cloth)Cloth->DestroyComponent();
 Hair=nullptr;Cloth=nullptr;Body=nullptr;HairMotion.Reset();ClothMotion.Reset();
 Super::EndPlay(Reason);
}
