#include "PatrolEnemy.h"
#include "NPCMotionAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Data/GlobalData.h"

bool APatrolEnemy::WantsWeaponDrawn() const
{
 return Health>0&&HeldWeapon&&!IsProtectedPerson()&&(Activity==ENPCActivity::Combat||Activity==ENPCActivity::WarnPlayer);
}
bool APatrolEnemy::IsWeaponTransitioning() const
{
 return WeaponCarry==ENPCWeaponCarry::Drawing||WeaponCarry==ENPCWeaponCarry::Holstering;
}
bool APatrolEnemy::IsWeaponInHand() const
{
 if(WeaponCarry==ENPCWeaponCarry::Drawing)return WeaponCarryTime>=.2f;
 if(WeaponCarry==ENPCWeaponCarry::Holstering)return WeaponCarryTime<(HolsterWeaponAnimation?HolsterWeaponAnimation->GetPlayLength():1.f);
 return WeaponCarry==ENPCWeaponCarry::Drawn;
}
FName APatrolEnemy::WeaponCarrySocket() const
{
 return HeldWeapon&&HeldWeapon->Kind==ETacticalWeaponKind::Pistol?FName(TEXT("pelvis")):FName(TEXT("spine_03"));
}
void APatrolEnemy::InitializeWeaponCarry()
{
 if(!HeldWeapon)return;
 const bool Pistol=HeldWeapon->Kind==ETacticalWeaponKind::Pistol;
 const auto* Config=GetDefault<UGlobalData>();
 const FString ClipBase=Pistol?TEXT("/Game/Combat/NPC/FullBody/NPC_A_Pistol"):TEXT("/Game/Combat/NPC/FullBody/NPC_A_Rifle");
 DrawWeaponAnimation=LoadObject<UAnimSequence>(nullptr,*(ClipBase+TEXT("_Equip_Weapon")));
 HolsterWeaponAnimation=LoadObject<UAnimSequence>(nullptr,*(ClipBase+TEXT("_Holster_Weapon")));
 // Preserve calibrated palm contact, using an over-shoulder reach for long
 // guns and the source project's waist reach for the pistol.
 if(auto* Motion=Cast<UNPCMotionAnimInstance>(GetMesh()->GetAnimInstance());Motion&&DrawWeaponAnimation)
 {
  Motion->Sequence=DrawWeaponAnimation;Motion->ExplicitTime=0;Motion->bLoop=false;
  GetMesh()->TickAnimation(1.f,false);GetMesh()->RefreshBoneTransforms();
  HeldWeapon->UpdateNPCGripPose(GetMesh(),NPCBody,1.f,true);NPCBody->RefreshBoneTransforms();
  FTransform Back=HeldWeapon->GetActorTransform();
  // Seat the receiver against the middle of the back, below the shoulder.
  const FVector BackBarrel=GetActorQuat().RotateVector(Pistol?FVector(0,0,-1):Config->NPCBackCarryMuzzleDirection.GetSafeNormal());
  Back.SetRotation(FQuat::FindBetweenNormals(HeldWeapon->MuzzleDirection(),BackBarrel)*Back.GetRotation());
  Back.SetLocation(GetActorTransform().TransformPosition(Pistol?Config->NPCPistolCarryOffset:Config->NPCBackCarryOffset));
  // Store the mount against the relaxed spine, not the reaching clip's bent
  // torso, otherwise returning to idle rotates the gun off the back plane.
  WeaponCarry=ENPCWeaponCarry::Holstered;WeaponCarryTime=0;
  SetMotion();GetMesh()->TickAnimation(1.f,false);GetMesh()->RefreshBoneTransforms();
  NPCBody->CopyPoseFromSkeletalComponent(GetMesh());NPCBody->RefreshBoneTransforms();
  BackWeaponRelative=Back.GetRelativeTransform(NPCBody->GetBoneTransformByName(WeaponCarrySocket(),EBoneSpaces::WorldSpace));
 }
 WeaponCarry=ENPCWeaponCarry::Holstered;WeaponCarryTime=0;
 SetMotion();GetMesh()->TickAnimation(1.f,false);GetMesh()->RefreshBoneTransforms();
}
void APatrolEnemy::ApplyBackCarryPose(const FTransform& AuthoredGun)
{
 if(!HeldWeapon||!IsWeaponTransitioning())return;
 const bool Drawing=WeaponCarry==ENPCWeaponCarry::Drawing;
 const float BackWeight=Drawing?1.f-FMath::SmoothStep(.2f,.65f,WeaponCarryTime):FMath::SmoothStep(.55f,1.f,WeaponCarryTime);
 const FTransform Back=BackWeaponRelative*NPCBody->GetBoneTransformByName(WeaponCarrySocket(),EBoneSpaces::WorldSpace);
 FTransform Gun;Gun.Blend(AuthoredGun,Back,BackWeight);
 // RogueBox's AnimateRifleTransfer uses a 10 cm outward arc between mounts.
 // Keep that clearance while adapting its bone-local placement to Manny.
 if(HeldWeapon->Kind!=ETacticalWeaponKind::Pistol)
  Gun.AddToTranslation(GetActorRightVector()*(GetDefault<UGlobalData>()->NPCWeaponTransferArc*4.f*BackWeight*(1.f-BackWeight)));
 if(IsWeaponInHand())HeldWeapon->SetActorTransform(Gun);
 else Gun=HeldWeapon->GetActorTransform();
 // Retarget the authored reaching arm to the lower back grip. The weapon and
 // wrist use the same rigid correction, preserving the original finger contact.
 const float ReachWeight=Drawing?FMath::SmoothStep(0.f,.2f,WeaponCarryTime):1.f-FMath::SmoothStep(1.f,1.15f,WeaponCarryTime);
 if(ReachWeight<=0)return;
 FTransform A=NPCBody->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::WorldSpace);
 FTransform B=NPCBody->GetBoneTransformByName(TEXT("lowerarm_r"),EBoneSpaces::WorldSpace);
 FTransform H=NPCBody->GetBoneTransformByName(TEXT("hand_r"),EBoneSpaces::WorldSpace);
 FTransform Target=H.GetRelativeTransform(AuthoredGun)*Gun;
 FTransform Wrist;Wrist.Blend(H,Target,ReachWeight);
 const FVector Shoulder=A.GetLocation(),OldElbow=B.GetLocation(),OldWrist=H.GetLocation();
 const float L1=FVector::Distance(Shoulder,OldElbow),L2=FVector::Distance(OldElbow,OldWrist);
 if(L1<1||L2<1)return;
 const FVector Direction=(Wrist.GetLocation()-Shoulder).GetSafeNormal();
 const float Distance=FMath::Clamp(FVector::Distance(Shoulder,Wrist.GetLocation()),FMath::Abs(L1-L2)+.1f,L1+L2-.1f);
 FVector Bend=OldElbow-Shoulder;Bend-=Direction*FVector::DotProduct(Bend,Direction);
 if(!Bend.Normalize())Bend=GetActorRightVector();
 const float Along=(L1*L1-L2*L2+Distance*Distance)/(2*Distance);
 const FVector Elbow=Shoulder+Direction*Along+Bend*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
 Wrist.SetLocation(Shoulder+Direction*Distance);
 A.SetRotation(FQuat::FindBetweenNormals((OldElbow-Shoulder).GetSafeNormal(),(Elbow-Shoulder).GetSafeNormal())*A.GetRotation());
 B.SetRotation(FQuat::FindBetweenNormals((OldWrist-OldElbow).GetSafeNormal(),(Wrist.GetLocation()-Elbow).GetSafeNormal())*B.GetRotation());B.SetLocation(Elbow);
 NPCBody->SetBoneTransformByName(TEXT("upperarm_r"),A,EBoneSpaces::WorldSpace);
 NPCBody->SetBoneTransformByName(TEXT("lowerarm_r"),B,EBoneSpaces::WorldSpace);
 NPCBody->SetBoneTransformByName(TEXT("hand_r"),Wrist,EBoneSpaces::WorldSpace);
 NPCBody->RefreshBoneTransforms();
}
void APatrolEnemy::TickWeaponCarry(float DT)
{
 if(!HeldWeapon)return;
 const bool Draw=WantsWeaponDrawn();
 if(!IsWeaponTransitioning()&&Draw!=(WeaponCarry==ENPCWeaponCarry::Drawn))
 {
  HeldWeapon->CancelHandling();WeaponCarry=Draw?ENPCWeaponCarry::Drawing:ENPCWeaponCarry::Holstering;WeaponCarryTime=0;
 }
 else if(IsWeaponTransitioning())
 {
  WeaponCarryTime+=FMath::Max(0.f,DT);
  const bool Drawing=WeaponCarry==ENPCWeaponCarry::Drawing;
  const auto* Clip=Drawing?DrawWeaponAnimation.Get():HolsterWeaponAnimation.Get();
  if(WeaponCarryTime>=(Clip?Clip->GetPlayLength():1.f)+(Drawing?.2f:.15f))
   WeaponCarry=Drawing?ENPCWeaponCarry::Drawn:ENPCWeaponCarry::Holstered;
 }
 if(IsWeaponTransitioning())
 {
  bTurning=false;bMoving=false;
  if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
  GetCharacterMovement()->StopMovementImmediately();
 }
}
void APatrolEnemy::DropDeathAmmo()
{
 if(bDeathAmmoDropped||!IsValid(HeldWeapon))return;
 bDeathAmmoDropped=true;
 const int32 Boxes=FMath::RandRange(1,3);
 for(int32 I=0;I<Boxes;++I)
 {
  const float Angle=2.f*PI*I/Boxes+FMath::FRandRange(-.15f,.15f);
  FVector Position=GetActorLocation()+FVector(FMath::Cos(Angle)*35.f,FMath::Sin(Angle)*35.f,20.f);
  FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCAmmoDrop),false,this);Query.AddIgnoredActor(HeldWeapon);
  if(GetWorld()->LineTraceSingleByChannel(Floor,Position+FVector(0,0,60),Position-FVector(0,0,200),ECC_WorldStatic,Query))Position.Z=Floor.ImpactPoint.Z+15.f;
  const FTransform Spawn(FRotator(0,FMath::FRandRange(0.f,360.f),0),Position);
  auto* Box=GetWorld()->SpawnActorDeferred<ACombatAmmoPickup>(ACombatAmmoPickup::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
  if(!Box)continue;
  Box->Kind=HeldWeapon->Kind;Box->Amount=HeldWeapon->Spec().Capacity;Box->FinishSpawning(Spawn);
  Box->Mesh->AddImpulse(FVector(FMath::Cos(Angle)*30.f,FMath::Sin(Angle)*30.f,20.f),NAME_None,true);
 }
}
