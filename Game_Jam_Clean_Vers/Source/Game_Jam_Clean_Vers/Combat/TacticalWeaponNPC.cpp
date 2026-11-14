#include "TacticalWeapon.h"
#include "Components/BoxComponent.h"
#include "PatrolEnemy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"

void ATacticalWeapon::EquipForNPC(USkeletalMeshComponent* CharacterMesh)
{
 if(!CharacterMesh)return;
 Stow();bNPCEquipped=true;SetOwner(CharacterMesh->GetOwner());SetActorHiddenInGame(false);
 if(const auto* NPC=Cast<APatrolEnemy>(CharacterMesh->GetOwner()))ApplyAppearanceSkin(NPC->IdentityRole==ENPCRole::Police?TEXT("Police"):TEXT("Gang"));
 Body->SetSimulatePhysics(false);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetCanEverAffectNavigation(false);
 Label->SetVisibility(false);Hands->SetVisibility(false);RenderArms->SetVisibility(false);ScopeLens->SetVisibility(false);
 if(bPackPresentation){
  Hands->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/KINEMATION/TacticalShooterPack/Character/Operator/UE5/SKM_Operator_Mono")));
  SamplePackAnimation(Hands,PackAnimation(TEXT("Character"),TEXT("Idle")),0);
 }
 CharacterMesh->TickAnimation(0,false);CharacterMesh->RefreshBoneTransforms();
 const FTransform Grip=CharacterMesh->GetSocketTransform(TEXT("HandGrip_R"));
 FQuat Rotation=Grip.GetRotation()*FRotator(0,90,0).Quaternion();
 const FVector Palm=(CharacterMesh->GetSocketLocation(TEXT("hand_r"))+CharacterMesh->GetSocketLocation(TEXT("middle_01_r")))*.5f;
 FVector Contact(-3,1.7f,Kind==ETacticalWeaponKind::Pistol?-6.5f:Kind==ETacticalWeaponKind::Rifle?-8.5f:-7.f);
 if(bPackPresentation){
  const FTransform AuthoredGun=Hands->GetSocketTransform(TEXT("ik_hand_gun"),RTS_Component);
  const FTransform AuthoredHand=Hands->GetSocketTransform(TEXT("hand_r"),RTS_Component);
  const FVector AuthoredPalm=(AuthoredHand.GetLocation()+Hands->GetSocketTransform(TEXT("middle_01_r"),RTS_Component).GetLocation())*.5f;
  Rotation=CharacterMesh->GetSocketQuaternion(TEXT("hand_r"))*AuthoredHand.GetRotation().Inverse()*AuthoredGun.GetRotation()*FRotator(0,90,0).Quaternion();
  Contact=PackWeaponMesh->GetRelativeTransform().TransformPosition(AuthoredGun.InverseTransformPosition(AuthoredPalm));
 }
 SetActorTransform(FTransform(Rotation,Palm-Rotation.RotateVector(Contact)));
 AttachToComponent(CharacterMesh,FAttachmentTransformRules::KeepWorldTransform,TEXT("HandGrip_R"));
}
void ATacticalWeapon::UpdateNPCGripPose(USkeletalMeshComponent* Source,UPoseableMeshComponent* BodyPose,float ReadyAlpha,bool bRightHandOnly)
{
 if(!Source||!BodyPose)return;BodyPose->CopyPoseFromSkeletalComponent(Source);
 const bool Shell=Kind==ETacticalWeaponKind::Shotgun;
 if(bPackPresentation){
  const float Progress=NPCReloadDuration>0?HandlingTime/NPCReloadDuration:0;
  const bool MagInHand=Handling==EWeaponHandling::MagazineReload&&Progress>=.15f&&Progress<.68f;
  if(MagInHand){
   if(PackMagazineMesh->GetAttachParent()!=Source)PackMagazineMesh->AttachToComponent(Source,FAttachmentTransformRules::KeepWorldTransform,TEXT("hand_l"));
   // Place the prop's centre in the authored palm, rather than preserving the
   // distance from the magazine well when the hand has already moved away.
   const FVector Palm=(Source->GetSocketLocation(TEXT("hand_l"))+Source->GetSocketLocation(TEXT("middle_01_l")))*.5f;
   const FQuat Rotation=Source->GetSocketQuaternion(TEXT("hand_l"));
   const FVector Centre=PackMagazineMesh->GetSkeletalMeshAsset()?PackMagazineMesh->GetSkeletalMeshAsset()->GetBounds().Origin:FVector::ZeroVector;
   PackMagazineMesh->SetWorldLocationAndRotation(Palm-Rotation.RotateVector(Centre*PackMagazineMesh->GetComponentScale()),Rotation);
   PackMagazineMesh->SetVisibility(Progress<.28f||Progress>=.42f);
  }else if(!Shell){
   PackMagazineMesh->AttachToComponent(PackWeaponMesh,FAttachmentTransformRules::SnapToTargetNotIncludingScale,Kind==ETacticalWeaponKind::Rifle?TEXT("Magazine"):TEXT("MagazineSocket"));
   PackMagazineMesh->SetRelativeRotation(Kind==ETacticalWeaponKind::Rifle?FRotator(0,0,90):FRotator::ZeroRotator);
   PackMagazineMesh->SetVisibility(bMagazineSeated);
  }
  ShellInHand->SetVisibility(Handling==EWeaponHandling::ShellInsert&&Progress>=.25f&&Progress<.65f);
  if(ShellInHand->IsVisible())ShellInHand->SetWorldTransform(Source->GetSocketTransform(TEXT("hand_l")));
 }
 // The source reload owns both hands. Keep the prop's existing hand attachment;
 // fitting it between the hands during a magazine reach would twist the gun.
 if(IsReloading())return;

 const FTransform Frame=BodyPose->GetComponentTransform();
 const FTransform Grip=Source->GetSocketTransform(TEXT("HandGrip_R"),RTS_Component);
 FQuat GunRotation=Frame.GetRotation()*Grip.GetRotation()*FRotator(0,90,0).Quaternion();
 FVector RightPalm=(Source->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation()+Source->GetSocketTransform(TEXT("middle_01_r"),RTS_Component).GetLocation())*.5f;
 RightPalm=Frame.TransformPosition(RightPalm);
 FVector RightContact(-3,1.7f,Kind==ETacticalWeaponKind::Pistol?-6.5f:Kind==ETacticalWeaponKind::Rifle?-8.5f:-7.f);
 const FTransform AuthoredGun=bPackPresentation?Hands->GetSocketTransform(TEXT("ik_hand_gun"),RTS_Component):FTransform::Identity;
 if(bPackPresentation){
  const FTransform AuthoredHand=Hands->GetSocketTransform(TEXT("hand_r"),RTS_Component);
  const FVector AuthoredPalm=(AuthoredHand.GetLocation()+Hands->GetSocketTransform(TEXT("middle_01_r"),RTS_Component).GetLocation())*.5f;
  RightContact=PackWeaponMesh->GetRelativeTransform().TransformPosition(AuthoredGun.InverseTransformPosition(AuthoredPalm));
  GunRotation=Frame.GetRotation()*Source->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetRotation()*AuthoredHand.GetRotation().Inverse()*AuthoredGun.GetRotation()*FRotator(0,90,0).Quaternion();
  if(Kind!=ETacticalWeaponKind::Pistol&&!bRightHandOnly){
   // Fit the prop to the authored two-hand pose; never move a character bone.
   const FVector AuthoredLeftPalm=(Hands->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation()+Hands->GetSocketTransform(TEXT("middle_01_l"),RTS_Component).GetLocation())*.5f;
   const FVector LeftContact=PackWeaponMesh->GetRelativeTransform().TransformPosition(AuthoredGun.InverseTransformPosition(AuthoredLeftPalm));
   const FVector LeftPalm=Frame.TransformPosition((Source->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation()+Source->GetSocketTransform(TEXT("middle_01_l"),RTS_Component).GetLocation())*.5f);
   const FVector AuthoredSpan=GunRotation.RotateVector(LeftContact-RightContact);
   const FVector PoseSpan=LeftPalm-RightPalm;
   if(!AuthoredSpan.IsNearlyZero()&&!PoseSpan.IsNearlyZero())GunRotation=FQuat::FindBetweenNormals(AuthoredSpan.GetSafeNormal(),PoseSpan.GetSafeNormal())*GunRotation;
  }
 }
 SetActorTransform(FTransform(GunRotation,RightPalm-GunRotation.RotateVector(RightContact)));
 // The full-body clip owns all bones. Only the weapon follows the authored hand.
}
