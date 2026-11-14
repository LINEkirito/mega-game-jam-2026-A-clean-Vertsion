#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PatrolEnemy.h"
#include "NPCMotionAnimInstance.h"
#include "Data/DataTransmitter.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FNPCWeaponCarryCheck,FAutomationTestBase*,Test);
bool FNPCWeaponCarryCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 if(!W){Test->AddError(TEXT("No PIE world"));return true;}
 for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
 auto CountBoxes=[W](){int32 N=0;for(TActorIterator<ACombatAmmoPickup> It(W);It;++It)++N;return N;};
 for(ENPCRole Role:{ENPCRole::GangMember,ENPCRole::Police,ENPCRole::GangBoss})
 for(ETacticalWeaponKind Kind:{ETacticalWeaponKind::Pistol,ETacticalWeaponKind::Rifle,ETacticalWeaponKind::Shotgun})
 {
  if(Role==ENPCRole::GangBoss&&Kind!=ETacticalWeaponKind::Pistol)continue;
  const FTransform Spawn(FRotator::ZeroRotator,FVector(35000,35000,500));
  auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  N->NPCRole=Role;N->WeaponKind=Kind;N->bRandomGangWeapon=false;N->PatrolRadius=0;N->FinishSpawning(Spawn);N->SetActorTickEnabled(false);N->GetCharacterMovement()->DisableMovement();
  if(!Test->TestNotNull(TEXT("Armed role owns weapon"),N->HeldWeapon.Get())){N->Destroy();continue;}
  Test->TestNotNull(TEXT("Authored draw clip exists"),N->DrawWeaponAnimation.Get());Test->TestNotNull(TEXT("Authored holster clip exists"),N->HolsterWeaponAnimation.Get());
  if(N->DrawWeaponAnimation)Test->TestTrue(TEXT("Pistol uses waist reach; long guns use back reach"),N->DrawWeaponAnimation->GetName().Contains(Kind==ETacticalWeaponKind::Pistol?TEXT("Pistol"):TEXT("Rifle")));
  Test->TestTrue(TEXT("Initial weapon visible on back"),N->WeaponCarry==ENPCWeaponCarry::Holstered&&!N->HeldWeapon->IsHidden()&&N->HeldWeapon->GetRootComponent()->GetAttachParent()==N->NPCBody);
  const FVector BackLocal=N->GetActorTransform().InverseTransformPosition(N->HeldWeapon->GetActorLocation());
  if(Kind==ETacticalWeaponKind::Pistol)
  {
   Test->TestTrue(FString::Printf(TEXT("Pistol sits outside right waist: %s"),*BackLocal.ToString()),BackLocal.Y>20&&FMath::Abs(BackLocal.Z)<25);
   Test->TestEqual(TEXT("Pistol follows waist bone"),N->HeldWeapon->GetRootComponent()->GetAttachSocketName(),FName(TEXT("pelvis")));
  }
  else Test->TestTrue(FString::Printf(TEXT("Receiver mounted behind torso, below shoulders: %s"),*BackLocal.ToString()),BackLocal.X<-15&&BackLocal.Z<55);
  const float BarrelDot=FVector::DotProduct(N->HeldWeapon->MuzzleDirection(),N->GetActorForwardVector());
  Test->TestTrue(FString::Printf(TEXT("Slung barrel runs along back (forward dot %.2f)"),BarrelDot),FMath::Abs(BarrelDot)<.25f);
  Test->TestTrue(TEXT("Stowed muzzle points down"),N->HeldWeapon->MuzzleDirection().Z<-.8f);
  float LargestStep=0;
  auto Advance=[N,&LargestStep,this](float Seconds){for(int I=0;I<FMath::CeilToInt(Seconds/.025f);++I){const FVector Before=N->HeldWeapon->GetActorLocation();N->TickWeaponCarry(.025f);N->SetMotion();N->GetMesh()->TickAnimation(.025f,false);N->GetMesh()->RefreshBoneTransforms();N->UpdateWeaponPose(.025f);const float Step=FVector::Distance(Before,N->HeldWeapon->GetActorLocation());LargestStep=FMath::Max(LargestStep,Step);if(Step>20)Test->AddInfo(FString::Printf(TEXT("Carry step Kind=%d State=%d Time=%.3f Distance=%.1f"),int32(N->WeaponKind),int32(N->WeaponCarry),N->WeaponCarryTime,Step));}};
  N->Activity=ENPCActivity::Combat;Advance(.025f);
  Test->TestTrue(TEXT("Entering combat starts draw before firing is allowed"),N->WeaponCarry==ENPCWeaponCarry::Drawing&&!N->IsWeaponInHand());
  Advance(.4f);Test->TestTrue(TEXT("Draw picks weapon up from back"),N->IsWeaponInHand()&&!N->HeldWeapon->IsHidden());
  Advance(1.f);Test->TestTrue(TEXT("Draw completes in hand"),N->WeaponCarry==ENPCWeaponCarry::Drawn);
  N->Activity=ENPCActivity::Trade;Advance(.025f);Test->TestTrue(TEXT("Leaving combat starts holster"),N->WeaponCarry==ENPCWeaponCarry::Holstering);
  Advance(1.3f);Test->TestTrue(TEXT("Social state retains visible back weapon"),N->WeaponCarry==ENPCWeaponCarry::Holstered&&!N->HeldWeapon->IsHidden()&&N->HeldWeapon->GetRootComponent()->GetAttachParent()==N->NPCBody);
  Test->TestTrue(FString::Printf(TEXT("Back/hand transfers are continuous (largest 25 ms step %.1f cm)"),LargestStep),LargestStep<20.f);
  const int32 Before=CountBoxes();
  TSet<ACombatAmmoPickup*> Existing;for(TActorIterator<ACombatAmmoPickup> It(W);It;++It)Existing.Add(*It);
  FHitResult Hit(N,N->GetMesh(),N->GetActorLocation(),FVector::UpVector);
  W->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,9999.f,FVector::ForwardVector,nullptr,nullptr);
  const int32 Added=CountBoxes()-Before;Test->TestTrue(TEXT("Death drops one to three separate boxes"),Added>=1&&Added<=3);
  TArray<ACombatAmmoPickup*> Drops;
  for(TActorIterator<ACombatAmmoPickup> It(W);It;++It)if(!Existing.Contains(*It))
  {Drops.Add(*It);Test->TestTrue(TEXT("Drop ammo matches actual gun"),It->Kind==N->HeldWeapon->Kind&&It->AmmoId()==N->HeldWeapon->Spec().Ammo);Test->TestEqual(TEXT("One magazine of ammo per box"),It->Amount,N->HeldWeapon->Spec().Capacity);Test->TestNull(TEXT("Drop is not owned by expiring corpse"),It->GetOwner());}
  W->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,9999.f,FVector::ForwardVector,nullptr,nullptr);
  Test->TestEqual(TEXT("Repeated lethal hits never duplicate drops"),CountBoxes()-Before,Added);
  N->Destroy();Test->TestEqual(TEXT("Corpse removal preserves drops"),CountBoxes()-Before,Added);
  for(auto* D:Drops)D->Destroy();
 }
 const int32 Before=CountBoxes();
 auto* Civilian=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),FTransform(FVector(36000,35000,500)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);Civilian->NPCRole=ENPCRole::Civilian;Civilian->FinishSpawning(FTransform(FVector(36000,35000,500)));
 FHitResult Hit(Civilian,Civilian->GetMesh(),Civilian->GetActorLocation(),FVector::UpVector);W->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,9999.f,FVector::ForwardVector,nullptr,nullptr);
 Test->TestEqual(TEXT("Unarmed civilians do not create ammunition"),CountBoxes(),Before);Civilian->Destroy();
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCWeaponCarry,"Game.NPC.WeaponCarryAndAmmo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNPCWeaponCarry::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FNPCWeaponCarryCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
