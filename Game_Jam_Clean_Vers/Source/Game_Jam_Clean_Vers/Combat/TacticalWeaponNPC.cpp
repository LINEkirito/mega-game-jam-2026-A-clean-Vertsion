#include "TacticalWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"

void ATacticalWeapon::EquipForNPC(USkeletalMeshComponent* CharacterMesh)
{
 if(!CharacterMesh)return;
 Stow();bNPCEquipped=true;SetOwner(CharacterMesh->GetOwner());SetActorHiddenInGame(false);
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
void ATacticalWeapon::UpdateNPCGripPose(USkeletalMeshComponent* Source,UPoseableMeshComponent* BodyPose,float ReadyAlpha)
{
 if(!Source||!BodyPose)return;BodyPose->CopyPoseFromSkeletalComponent(Source);
 const FTransform Frame=BodyPose->GetComponentTransform();
 // Lower the complete right-arm chain for patrol; raise it smoothly for a warning/firefight.
 const FQuat Lowering(FVector::XAxisVector,FMath::DegreesToRadians(-35.f*(1.f-ReadyAlpha)));
 const FVector Pivot=Source->GetSocketTransform(TEXT("upperarm_r"),RTS_Component).GetLocation();
 for(const FName Bone:{FName(TEXT("upperarm_r")),FName(TEXT("lowerarm_r")),FName(TEXT("hand_r"))}){
  FTransform T=Source->GetSocketTransform(Bone,RTS_Component);T.SetLocation(Pivot+Lowering.RotateVector(T.GetLocation()-Pivot));T.SetRotation(Lowering*T.GetRotation());BodyPose->SetBoneTransformByName(Bone,T,EBoneSpaces::ComponentSpace);
 }
 const FTransform Grip=Source->GetSocketTransform(TEXT("HandGrip_R"),RTS_Component);
 FQuat GunRotation=Frame.GetRotation()*Lowering*Grip.GetRotation()*FRotator(0,90,0).Quaternion();
 FVector RightPalm=(Source->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation()+Source->GetSocketTransform(TEXT("middle_01_r"),RTS_Component).GetLocation())*.5f;
 RightPalm=Frame.TransformPosition(Pivot+Lowering.RotateVector(RightPalm-Pivot));
 FVector RightContact(-3,1.7f,Kind==ETacticalWeaponKind::Pistol?-6.5f:Kind==ETacticalWeaponKind::Rifle?-8.5f:-7.f);
 const FTransform AuthoredGun=bPackPresentation?Hands->GetSocketTransform(TEXT("ik_hand_gun"),RTS_Component):FTransform::Identity;
 if(bPackPresentation){
  const FTransform AuthoredHand=Hands->GetSocketTransform(TEXT("hand_r"),RTS_Component);
  const FVector AuthoredPalm=(AuthoredHand.GetLocation()+Hands->GetSocketTransform(TEXT("middle_01_r"),RTS_Component).GetLocation())*.5f;
  RightContact=PackWeaponMesh->GetRelativeTransform().TransformPosition(AuthoredGun.InverseTransformPosition(AuthoredPalm));
  GunRotation=Frame.GetRotation()*Lowering*Source->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetRotation()*AuthoredHand.GetRotation().Inverse()*AuthoredGun.GetRotation()*FRotator(0,90,0).Quaternion();
 }
 SetActorTransform(FTransform(GunRotation,RightPalm-GunRotation.RotateVector(RightContact)));
 const FTransform Upper=Source->GetSocketTransform(TEXT("upperarm_l"),RTS_Component);
 const FTransform Lower=Source->GetSocketTransform(TEXT("lowerarm_l"),RTS_Component);
 const FTransform Hand=Source->GetSocketTransform(TEXT("hand_l"),RTS_Component);
 const FVector Palm=(Hand.GetLocation()+Source->GetSocketTransform(TEXT("middle_01_l"),RTS_Component).GetLocation())*.5f;
 FVector Contact=Kind==ETacticalWeaponKind::Pistol?FVector(-3,-2.5f,-7):Kind==ETacticalWeaponKind::Rifle?FVector(24,-.5f,-4):FVector(27,0,-4);
 if(bPackPresentation){
  const FVector AuthoredPalm=(Hands->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation()+Hands->GetSocketTransform(TEXT("middle_01_l"),RTS_Component).GetLocation())*.5f;
  Contact=PackWeaponMesh->GetRelativeTransform().TransformPosition(AuthoredGun.InverseTransformPosition(AuthoredPalm));
 }
 const FVector TargetPalm=Frame.InverseTransformPosition(WeaponVisual->GetComponentTransform().TransformPosition(Contact));
 const FQuat HandRotation=bPackPresentation?Frame.GetRotation().Inverse()*PackWeaponMesh->GetComponentQuat()*AuthoredGun.GetRotation().Inverse()*Hands->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetRotation():Lowering*Hand.GetRotation();
 const FVector Wrist=TargetPalm-HandRotation.RotateVector(Hand.InverseTransformPosition(Palm));
 const FVector Shoulder=Upper.GetLocation(),Axis=(Wrist-Shoulder).GetSafeNormal();
 const float L1=FVector::Distance(Shoulder,Lower.GetLocation()),L2=FVector::Distance(Lower.GetLocation(),Hand.GetLocation());
 if(L1<1||L2<1)return;
 const float Distance=FMath::Clamp(FVector::Distance(Shoulder,Wrist),FMath::Abs(L1-L2)+.1f,L1+L2-.1f);
 const FVector End=Shoulder+Axis*Distance;
 FVector Pole=(Lower.GetLocation()-Shoulder)-Axis*FVector::DotProduct(Lower.GetLocation()-Shoulder,Axis);
 if(!Pole.Normalize())Pole=FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal();
 const float Along=(L1*L1-L2*L2+Distance*Distance)/(2*Distance);
 const FVector Elbow=Shoulder+Axis*Along+Pole*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
 const FQuat UpperRotation=FQuat::FindBetweenNormals((Lower.GetLocation()-Shoulder).GetSafeNormal(),(Elbow-Shoulder).GetSafeNormal())*Upper.GetRotation();
 const FQuat LowerRotation=FQuat::FindBetweenNormals((Hand.GetLocation()-Lower.GetLocation()).GetSafeNormal(),(End-Elbow).GetSafeNormal())*Lower.GetRotation();
 BodyPose->SetBoneTransformByName(TEXT("upperarm_l"),FTransform(UpperRotation,Shoulder),EBoneSpaces::ComponentSpace);
 BodyPose->SetBoneTransformByName(TEXT("lowerarm_l"),FTransform(LowerRotation,Elbow),EBoneSpaces::ComponentSpace);
 BodyPose->SetBoneTransformByName(TEXT("hand_l"),FTransform(HandRotation,End),EBoneSpaces::ComponentSpace);
 BodyPose->RefreshBoneTransforms();
}
