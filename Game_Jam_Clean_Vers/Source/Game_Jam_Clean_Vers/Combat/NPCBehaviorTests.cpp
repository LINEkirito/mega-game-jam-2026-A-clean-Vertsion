#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "CombatEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "NPCMotionAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Engine/DamageEvents.h"
#include "WorldEvents.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FNPCBehaviorCheck,FAutomationTestBase*,Test);
bool FNPCBehaviorCheck::Update()
{
 UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
 auto* PC=World?World->GetFirstPlayerController():nullptr;
 auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 if(!Player){Test->AddError(TEXT("No PIE player"));return true;}
 Test->TestEqual(TEXT("Player hurtbox blocks the NPC bullet trace channel"),Player->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility),ECR_Block);
 auto* Rules=GetMutableDefault<UGlobalData>();
 TGuardValue<float> SurviveFire(Player->Health,100000.f);
 TGuardValue<float> Counterattack(Rules->CivilianFightBackChance,0.f);
 TGuardValue<bool> LegacySocial(GetMutableDefault<UGlobalData>()->bEnableStreetEvents,false);
 TArray<APatrolEnemy*> Spawned;
 const FVector Origin(15000,15000,1200);
 auto Spawn=[&](ENPCRole Role,FVector Offset){
  const FTransform Transform(FRotator::ZeroRotator,Origin+Offset);
  auto* NPC=World->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  NPC->NPCRole=Role;NPC->PatrolRadius=0;NPC->FinishSpawning(Transform);NPC->GetCharacterMovement()->DisableMovement();Spawned.Add(NPC);return NPC;
 };
 auto* Bus=World->GetSubsystem<UDataTransmitter>();
 auto Hit=[&](APatrolEnemy* Victim,AController* Attacker,AActor* Source,float Damage){
  FHitResult Result(Victim,Victim->GetMesh(),Victim->GetActorLocation(),FVector::UpVector);Result.ImpactPoint=Victim->GetActorLocation();
  return Bus->TransmitHit(Result,Damage,FVector::ForwardVector,Attacker,Source);
 };
 auto* Officer=Spawn(ENPCRole::Police,FVector::ZeroVector);
 auto* Civilian=Spawn(ENPCRole::Civilian,FVector(400,250,0));
 auto* Merchant=Spawn(ENPCRole::Merchant,FVector(500,250,0));
 auto* Gang=Spawn(ENPCRole::GangMember,FVector(600,0,0));
 auto* Boss=Spawn(ENPCRole::GangBoss,FVector(650,-200,0));
 Test->TestEqual(TEXT("Police versus player accuracy"),Officer->ShotHitChance(Player),1.f);
 for(auto* Criminal:{Gang,Boss}){
  Test->TestEqual(TEXT("Gang including boss versus player accuracy"),Criminal->ShotHitChance(Player),.7f);
  Test->TestEqual(TEXT("Gang including boss versus police accuracy"),Criminal->ShotHitChance(Officer),.3f);
  Test->TestEqual(TEXT("Police versus gang including boss accuracy"),Officer->ShotHitChance(Criminal),.3f);
 }
 for(auto Kind:{ETacticalWeaponKind::Pistol,ETacticalWeaponKind::Rifle,ETacticalWeaponKind::Shotgun}){
  const auto Previous=Boss->HeldWeapon->Kind;Boss->HeldWeapon->Kind=Kind;
  const auto Spec=Boss->HeldWeapon->Spec();Boss->HeldWeapon->Kind=Previous;
  const float Expected=Kind==ETacticalWeaponKind::Shotgun?Rules->BuckshotPelletDamage:Kind==ETacticalWeaponKind::Rifle?Rules->RifleBodyDamage:Rules->PistolBodyDamage;
  Test->TestEqual(TEXT("NPC uses shared per-projectile body damage"),Spec.DamageAtDistance(0),Expected);
  Test->TestTrue(TEXT("Shared weapon damage falls to 35 percent at maximum range"),FMath::IsNearlyEqual(Spec.DamageAtDistance(Spec.Range),Expected*.35f));
  Test->TestEqual(TEXT("Shotgun retains eight full-damage pellets"),Spec.Pellets,Kind==ETacticalWeaponKind::Shotgun?8:1);
 }
 Test->TestTrue(TEXT("Gang uses same weapon actor as player"),Gang->HeldWeapon&&Gang->HeldWeapon->bNPCEquipped);
 Test->TestTrue(TEXT("Boss always carries shared pistol"),Boss->HeldWeapon&&Boss->HeldWeapon->Kind==ETacticalWeaponKind::Pistol);
 Test->TestNull(TEXT("Civilian has no weapon actor"),Civilian->HeldWeapon.Get());
 for(auto* NPC:Spawned)Test->TestNotNull(TEXT("Every NPC role uses the blended native animation instance"),Cast<UNPCMotionAnimInstance>(NPC->GetMesh()->GetAnimInstance()));
 Bus->ReportGunshot(Player,Origin+FVector(-500,0,0),1500);
 Test->TestEqual(TEXT("Unidentified gunshot sends police to investigate"),Officer->Activity,ENPCActivity::Investigate);
 Test->TestTrue(TEXT("Investigation stores exact shot location"),Officer->InvestigationLocation.Equals(Origin+FVector(-500,0,0)));
 Test->TestNull(TEXT("Gunshot alone does not criminalize player"),Officer->CombatTarget.Get());
 Test->TestEqual(TEXT("Civilian hears gunshot and flees"),Civilian->Activity,ENPCActivity::Flee);
 Test->TestEqual(TEXT("Merchant hears gunshot and flees"),Merchant->Activity,ENPCActivity::Flee);
 Test->TestTrue(TEXT("Gang never flees from gunfire"),Gang->Activity!=ENPCActivity::Flee&&Boss->Activity!=ENPCActivity::Flee);
 Hit(Civilian,Gang->GetController(),Gang,12);
 Test->TestEqual(TEXT("Gang beating civilian triggers intervention"),Officer->CombatTarget.Get(),static_cast<AActor*>(Gang));
 const float PoliceHealth=Officer->Health;
 Test->TestEqual(TEXT("Gang can damage police through unified damage entry"),Hit(Officer,Gang->GetController(),Gang,3),3.f);
 Test->TestEqual(TEXT("Police lose health when hit by gang"),Officer->Health,PoliceHealth-3);
 Test->TestEqual(TEXT("Boss can damage police"),Hit(Officer,Boss->GetController(),Boss,3),3.f);
 Hit(Merchant,PC,Player,5);
 Test->TestEqual(TEXT("Player attacking protected merchant overrides gang target"),Officer->CombatTarget.Get(),static_cast<AActor*>(Player));
 Bus->ReportGunshot(Boss,Boss->GetActorLocation(),6000);
 Test->TestEqual(TEXT("Fresh gang gunshot cannot override player offender"),Officer->CombatTarget.Get(),static_cast<AActor*>(Player));
 auto* Officer2=Spawn(ENPCRole::Police,FVector(0,-500,0));
 Bus->OnViolence.Broadcast(Player,Gang,false);
 // Bring the reported victim within response range without applying damage to the test player.
 Officer2->SetActorLocation(Player->GetActorLocation()+FVector(200,0,0));
 Bus->OnViolence.Broadcast(Player,Gang,false);
 Test->TestEqual(TEXT("Gang attacking player triggers police intervention"),Officer2->CombatTarget.Get(),static_cast<AActor*>(Gang));
 auto* Wounded=Spawn(ENPCRole::Injured,FVector(250,-300,0));
 Hit(Wounded,PC,Player,4);
 Test->TestEqual(TEXT("Attacking an injured person prioritizes player"),Officer->CombatTarget.Get(),static_cast<AActor*>(Player));
 Test->TestEqual(TEXT("Injured person remains down"),Wounded->Activity,ENPCActivity::Injured);
 auto* Convert=Spawn(ENPCRole::Civilian,FVector(300,-400,0));
 auto* OriginalAppearance=Convert->GetMesh()->GetSkeletalMeshAsset();
 Test->TestNotNull(TEXT("Civilian has an appearance"),OriginalAppearance);
 Test->TestTrue(TEXT("Gang and police have distinct silhouettes"),Gang->GetMesh()->GetSkeletalMeshAsset()!=Officer->GetMesh()->GetSkeletalMeshAsset());
 Hit(Convert,Gang->GetController(),Gang,125);
 Test->TestEqual(TEXT("Living low-health civilian converts to injured"),Convert->NPCRole,ENPCRole::Injured);
 Test->TestEqual(TEXT("Becoming injured preserves the original person's appearance"),Convert->GetMesh()->GetSkeletalMeshAsset(),OriginalAppearance);
 Test->TestTrue(TEXT("Conversion preserves positive instance health"),Convert->Health>0&&Convert->Health<=30);
 Test->TestEqual(TEXT("Other civilian health remains separate"),Civilian->Health,138.f);
 Bus->ReportGunshot(Gang,Convert->GetActorLocation(),6000);
 Test->TestEqual(TEXT("Injured cannot flee after a new gunshot"),Convert->Activity,ENPCActivity::Injured);
 Hit(Convert,PC,Player,1000);
 Test->TestEqual(TEXT("Further damage can kill injured NPC"),Convert->Activity,ENPCActivity::Dead);
 auto* Distant=Spawn(ENPCRole::Police,FVector(12000,0,0));
 Bus->ReportGunshot(Player,Origin,500);
 Test->TestEqual(TEXT("Distant unheard gunshot does not alert police"),Distant->Activity,ENPCActivity::Idle);
 // Exercise actual firing, including retaliation, instead of checking only damage filters.
 Officer->Destroy();Officer2->Destroy();Civilian->Destroy();Merchant->Destroy();Gang->Destroy();Boss->Destroy();
 auto* Suppressor=Spawn(ENPCRole::GangMember,FVector(0,0,0));
 auto* SafeOfficer=Spawn(ENPCRole::Police,FVector(800,0,0));
 // Test retaliation independently of lethal shared weapon damage and random misses.
 SafeOfficer->Health=Suppressor->Health=100000.f;
 SafeOfficer->SetActorRotation(FRotator(0,180,0));
 SafeOfficer->WitnessViolence(Wounded,Suppressor,true);
 for(int I=0;I<30;++I){for(auto* N:{SafeOfficer,Suppressor}){N->GetMesh()->TickAnimation(.1f,false);N->GetMesh()->RefreshBoneTransforms();N->Tick(.1f);}}
 Test->TestEqual(TEXT("Gang retaliates against officer firing at it"),Suppressor->CombatTarget.Get(),static_cast<AActor*>(SafeOfficer));
 Test->TestTrue(TEXT("Police remain in the firefight during the short encounter"),SafeOfficer->Health>0);

 for(auto* NPC:Spawned)if(IsValid(NPC))NPC->Destroy();
 Spawned.Reset();
 auto* TraderA=Spawn(ENPCRole::GangMember,FVector::ZeroVector);
 auto* TraderB=Spawn(ENPCRole::GangMember,FVector(100,0,0));
 for(int I=0;I<140;++I){TraderA->SetActorRotation(FRotator::ZeroRotator);TraderB->SetActorRotation(FRotator(0,180,0));TraderA->Tick(.1f);TraderB->Tick(.1f);}
 Test->TestTrue(TEXT("Nearby gang members complete an actual paired trade"),TraderA->CompletedTrades>0);
 Test->TestEqual(TEXT("Both traders receive exactly the same transaction count"),TraderA->CompletedTrades,TraderB->CompletedTrades);
 TraderA->Destroy();TraderB->Destroy();
 auto* Bully=Spawn(ENPCRole::GangMember,FVector::ZeroVector);
 auto* Victim=Spawn(ENPCRole::Merchant,FVector(100,0,0));bool Threatened=false,RecoveredAfterHit=false;
 for(int I=0;I<140;++I){Bully->SetActorRotation(FRotator::ZeroRotator);Bully->Tick(.1f);Victim->Tick(.1f);Threatened|=Bully->Activity==ENPCActivity::Threaten;RecoveredAfterHit|=Victim->Health<150&&Bully->Activity==ENPCActivity::Beat;}
 Test->TestTrue(TEXT("Bully first threatens merchant"),Threatened);
 Test->TestTrue(TEXT("Punch recovery continues after the victim starts fleeing"),RecoveredAfterHit);
 Test->TestTrue(TEXT("Melee applies real damage through the event bus"),Victim->Health<150);
 Test->TestEqual(TEXT("Beaten merchant breaks interaction and flees"),Victim->Activity,ENPCActivity::Flee);
 Bully->Destroy();Victim->Destroy();
 auto* GestureBoss=Spawn(ENPCRole::GangBoss,FVector::ZeroVector);bool Commanded=false;
 for(int I=0;I<100;++I){GestureBoss->Tick(.1f);Commanded|=GestureBoss->Activity==ENPCActivity::Command;}
 Test->TestTrue(TEXT("Idle boss performs command gesture"),Commanded);
 Test->TestNull(TEXT("Boss gesture does not assign a combat target"),GestureBoss->CombatTarget.Get());
 for(auto* NPC:Spawned)if(IsValid(NPC))NPC->Destroy();
 Spawned.Reset();
 auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();auto* Camera=Equipment?Equipment->Camcorder.Get():nullptr;
 const FVector PlayerLocation=Player->GetActorLocation();const FRotator PlayerView=PC->GetControlRotation();
 const bool Held=Camera&&Camera->bHeld,Raised=Camera&&Camera->bViewfinderRaised,Powered=Camera&&Camera->bPowered;
 if(Camera){Camera->bHeld=false;Camera->bViewfinderRaised=false;}
 auto MovePlayer=[&](float Distance){Player->SetActorLocation(Origin-FVector(Distance,0,0));PC->SetControlRotation(FRotator::ZeroRotator);PC->PlayerCameraManager->UpdateCamera(0);};
 auto* WarningGang=Spawn(ENPCRole::GangMember,FVector::ZeroVector);MovePlayer(400);
 WarningGang->Tick(.3f);Test->TestEqual(TEXT("Player inside five meters gets warning"),WarningGang->Activity,ENPCActivity::WarnPlayer);
 WarningGang->Tick(2.5f);Test->TestFalse(TEXT("No attack before three-second warning expires"),WarningGang->bCombat);
 MovePlayer(600);WarningGang->Tick(.1f);Test->TestFalse(TEXT("Retreat outside warning radius prevents combat"),WarningGang->bCombat);
 MovePlayer(400);WarningGang->Tick(.4f);WarningGang->Tick(3.1f);Test->TestEqual(TEXT("Ignoring a fresh warning causes player combat"),WarningGang->CombatTarget.Get(),static_cast<AActor*>(Player));
 auto* SupportingOfficer=Spawn(ENPCRole::Police,FVector(0,300,0));
 Hit(WarningGang,SupportingOfficer->GetController(),SupportingOfficer,2);WarningGang->Tick(.3f);
 Test->TestEqual(TEXT("Police support does not restart a completed player warning"),WarningGang->Activity,ENPCActivity::Combat);
 Test->TestEqual(TEXT("Visible player remains the gang target after police chip damage"),WarningGang->CombatTarget.Get(),static_cast<AActor*>(Player));
 SupportingOfficer->Destroy();WarningGang->Destroy();
 auto* CameraBoss=Spawn(ENPCRole::GangBoss,FVector::ZeroVector);MovePlayer(800);CameraBoss->Tick(.4f);
 Test->TestTrue(TEXT("Eight meters without camera does not warn"),CameraBoss->Activity!=ENPCActivity::WarnPlayer);
 if(Camera){CameraBoss->SetActorRotation(FRotator(0,180,0));Camera->bHeld=true;Camera->bPowered=true;Camera->bViewfinderRaised=true;MovePlayer(800);
  Test->TestEqual(TEXT("Aimed active camera expands warning range to ten meters"),CameraBoss->PlayerWarningRange(),1000.f);
  CameraBoss->Tick(.4f);Test->TestEqual(TEXT("Boss avoids the camera at eight meters"),CameraBoss->Activity,ENPCActivity::CameraAvoid);
  Camera->bHeld=false;CameraBoss->Tick(.1f);Test->TestEqual(TEXT("Boss finishes leaving after camera is lowered"),CameraBoss->Activity,ENPCActivity::CameraAvoid);
  MovePlayer(1100);CameraBoss->Tick(.1f);Test->TestFalse(TEXT("Retreat past ten meters cancels filming warning"),CameraBoss->bCombat);
  Camera->bHeld=Held;Camera->bViewfinderRaised=Raised;Camera->bPowered=Powered;
 }
 CameraBoss->Destroy();
 // A firearm aimed at a visible enemy warns immediately, independently of proximity.
 auto* Combat=PC->FindComponentByClass<UCombatEquipmentComponent>();
 auto* Gun=World->SpawnActor<ATacticalWeapon>(FVector(0,0,700),FRotator::ZeroRotator);
 auto* PreviousWeapon=Combat->ActiveWeapon.Get();Combat->ActiveWeapon=Gun;Combat->bADS=true;
 auto* AimedGang=Spawn(ENPCRole::GangMember,FVector::ZeroVector);MovePlayer(800);
 AimedGang->GetMesh()->TickAnimation(0,false);AimedGang->GetMesh()->RefreshBoneTransforms();
 const FRotator Aim=(AimedGang->GetActorLocation()+FVector(0,0,25)-Player->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation();
 PC->SetControlRotation(Aim);
 AimedGang->SetActorRotation(FRotator::ZeroRotator);
 AimedGang->Tick(.01f);Test->TestFalse(TEXT("Aiming from behind does not alert an unaware gang member"),AimedGang->bCombat);
 AimedGang->SetActorRotation(FRotator(0,89,0));
 Test->TestFalse(TEXT("Gang cannot see 91 degrees outside forward half-plane"),AimedGang->CanSeeActor(Player));
 AimedGang->SetActorRotation(FRotator(0,91,0));
 Test->TestTrue(TEXT("Gang sees player 89 degrees into peripheral vision"),AimedGang->CanSeeActor(Player));
 AimedGang->Tick(.01f);
 Test->TestEqual(TEXT("Visible aiming immediately targets player without countdown"),AimedGang->CombatTarget.Get(),static_cast<AActor*>(Player));
 Test->TestEqual(TEXT("Gang aimed-threat countdown is skipped"),AimedGang->WarningRemaining,0.f);
 // Acquiring a target is immediate; locomotion/fire readiness follows the draw clip.
 for(int I=0;I<16;++I){AimedGang->Tick(.1f);AimedGang->GetMesh()->TickAnimation(.1f,false);AimedGang->GetMesh()->RefreshBoneTransforms();}
 AimedGang->GetCharacterMovement()->Velocity=AimedGang->GetActorForwardVector()*95;
 AimedGang->Tick(.01f);
 auto* Motion=Cast<UNPCMotionAnimInstance>(AimedGang->GetMesh()->GetAnimInstance());
 if(Test->TestNotNull(TEXT("Combat retains native animation blending"),Motion))
 {
  Test->TestNull(TEXT("Full-body locomotion has no masked holding override"),Motion->UpperBodyPose.Get());
  Test->TestTrue(TEXT("Slow combat movement keeps natural cadence"),Motion->PlaybackRate>=.75f);
  const float GaitReference=Motion->Sequence&&Motion->Sequence->GetName()==TEXT("NPC_A_Pistol_Aim_Walk_Right")?147.66f:183.33f;
  Test->TestTrue(FString::Printf(TEXT("Slow combat stride matches velocity (%s)"),*GetNameSafe(Motion->Sequence)),FMath::IsNearlyEqual(Motion->PlaybackRate*Motion->StrideWeight,95.f/GaitReference,.01f));
  // This fixture has no navigation floor. Test repeated firing at rest separately
  // from the real-path walking check, so a failed MoveTo cannot change the gait.
  MovePlayer(200);AimedGang->GetCharacterMovement()->Velocity=FVector::ZeroVector;AimedGang->Tick(1.f);
  auto* HoldingPose=Motion->Sequence.Get();
  for(int I=0;I<3;++I){AimedGang->Tick(.7f);Test->TestEqual(TEXT("Repeated shots preserve holding sequence"),Motion->Sequence.Get(),HoldingPose);}
 }
 Combat->ActiveWeapon=PreviousWeapon;Combat->bADS=false;Gun->Destroy();
 // A facing actor with a sideways gun must not fire target-seeking hitscan.
 auto* BarrelGuard=Spawn(ENPCRole::GangBoss,FVector(0,5000,0));
 BarrelGuard->CombatTarget=Player;BarrelGuard->Activity=ENPCActivity::Combat;BarrelGuard->bCombat=true;
 Player->SetActorLocation(BarrelGuard->GetActorLocation()+FVector(800,0,0));
 for(int I=0;I<16;++I){BarrelGuard->Tick(.1f);BarrelGuard->GetMesh()->TickAnimation(.1f,false);BarrelGuard->GetMesh()->RefreshBoneTransforms();}
 BarrelGuard->GetMesh()->TickAnimation(.1f,false);BarrelGuard->GetMesh()->RefreshBoneTransforms();
 auto* GuardGun=BarrelGuard->HeldWeapon.Get();
 const FRotator OriginalGunRotation=GuardGun->PackWeaponMesh->GetRelativeRotation();
 GuardGun->PackWeaponMesh->SetRelativeRotation(OriginalGunRotation+FRotator(0,90,0));
 auto CountShots=[&](){int32 Count=0;for(TActorIterator<ACombatShotEffect> It(World);It;++It)++Count;return Count;};
 const int32 BeforeSideways=CountShots();
 BarrelGuard->Tick(0.f);
 Test->TestEqual(TEXT("Facing the target with a sideways barrel cannot fire"),CountShots(),BeforeSideways);
 GuardGun->PackWeaponMesh->SetRelativeRotation(OriginalGunRotation);
 BarrelGuard->Tick(.01f);
 const FVector OnBarrel=GuardGun->MuzzleLocation()+GuardGun->MuzzleDirection()*800-FVector(0,0,25);
 Player->SetActorLocation(OnBarrel);
 const int32 BeforeAligned=CountShots();
 for(int32 I=0;I<100;++I){BarrelGuard->Tick(.1f);BarrelGuard->GetMesh()->TickAnimation(.1f,false);BarrelGuard->GetMesh()->RefreshBoneTransforms();}
 Test->AddInfo(FString::Printf(TEXT("Barrel check: actor yaw %.1f barrel yaw %.1f pitch %.1f target yaw %.1f pitch %.1f activity %d target %s"),BarrelGuard->GetActorRotation().Yaw,GuardGun->MuzzleDirection().Rotation().Yaw,GuardGun->MuzzleDirection().Rotation().Pitch,(Player->GetActorLocation()-BarrelGuard->GetActorLocation()).Rotation().Yaw,(Player->GetActorLocation()+FVector(0,0,25)-GuardGun->MuzzleLocation()).Rotation().Pitch,int32(BarrelGuard->Activity),*GetNameSafe(BarrelGuard->CombatTarget)));
 Test->TestTrue(TEXT("A target in the real barrel direction can be fired on"),CountShots()>BeforeAligned);
 Test->TestTrue(TEXT("NPC pistol uses player cadence during sustained aligned fire"),CountShots()-BeforeAligned>20);
 BarrelGuard->Destroy();
 // Real targets at ordinary ranges/elevations, not artificially placed on the barrel axis.
 for(auto Kind:{ETacticalWeaponKind::Pistol,ETacticalWeaponKind::Rifle,ETacticalWeaponKind::Shotgun}){
  for(const FVector Offset:{FVector(300,0,0),FVector(800,0,0),FVector(600,0,200),FVector(600,0,-180)}){
   const FTransform T(FRotator::ZeroRotator,Origin+FVector(0,7000,0));
   auto* Shooter=World->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Shooter->NPCRole=ENPCRole::Police;Shooter->WeaponKind=Kind;Shooter->PatrolRadius=0;Shooter->FinishSpawning(T);Shooter->GetCharacterMovement()->DisableMovement();
   Shooter->CombatTarget=Player;Shooter->Activity=ENPCActivity::Combat;Shooter->bCombat=true;
   Player->SetActorLocation(Shooter->GetActorLocation()+Offset);
   const int32 Before=CountShots();
   for(int32 I=0;I<50;++I){Shooter->Tick(.1f);Shooter->GetMesh()->TickAnimation(.1f,false);Shooter->GetMesh()->RefreshBoneTransforms();}
   Test->TestTrue(FString::Printf(TEXT("NPC weapon %d fires at real target offset %s"),int32(Kind),*Offset.ToString()),CountShots()>Before);
   Shooter->Destroy();
  }
 }


 // Exercise actual NPC ammunition through the shared weapon state machine.
 for(const auto Kind:{ETacticalWeaponKind::Pistol,ETacticalWeaponKind::Rifle,ETacticalWeaponKind::Shotgun}){
  const FTransform T(FRotator::ZeroRotator,FVector(45000+int32(Kind)*1000,45000,1200));
  auto* Loader=World->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  Loader->NPCRole=ENPCRole::Police;Loader->WeaponKind=Kind;Loader->PatrolRadius=0;Loader->FinishSpawning(T);Loader->GetCharacterMovement()->DisableMovement();
  Loader->Activity=ENPCActivity::Combat;Loader->CombatTarget=Player;Loader->SightRange=0;
  Loader->Tick(0);Loader->Tick(1.3f); // Finish drawing before exercising the reload machinery.
  auto* W=Loader->HeldWeapon.Get();const auto S=W->Spec();const int32 ReserveBefore=Loader->AmmoReserve->Count(S.Ammo);
  int32 Fired=0;while(W->ConsumeShot()){++Fired;W->AdvanceHandling(S.Interval);if(Fired>S.Capacity+1)break;}
  Test->TestEqual(TEXT("NPC consumes magazine plus separately chambered round"),Fired,S.Capacity+1);
  Test->TestFalse(TEXT("Empty NPC cannot fire"),W->CanFire());
  Loader->Tick(.001f);Test->TestTrue(TEXT("Empty NPC starts authored reload"),W->IsReloading());
  const int32 BeforeReloadShots=CountShots();
  for(int32 I=0;I<1000&&W->IsReloading();++I){Test->TestFalse(TEXT("NPC cannot shoot during reload"),W->ConsumeShot());Loader->Tick(.05f);}
  Test->TestFalse(TEXT("NPC reload finishes"),W->IsReloading());
  Test->TestEqual(TEXT("Reload creates no bullet effects"),CountShots(),BeforeReloadShots);
  const int32 Loaded=Kind==ETacticalWeaponKind::Shotgun?S.Capacity+1:S.Capacity;
  Test->TestEqual(TEXT("Reload commits only actual reserve rounds"),Loader->AmmoReserve->Count(S.Ammo),ReserveBefore-Loaded);
  Test->TestEqual(TEXT("Magazine and chamber contain exactly committed ammunition"),W->Magazine+(W->bChambered?1:0),Loaded);
  Test->TestTrue(TEXT("NPC can resume firing after reload"),W->ConsumeShot());W->AdvanceHandling(S.Interval);
  Loader->AmmoReserve->Remove(S.Ammo,Loader->AmmoReserve->Count(S.Ammo));
  for(int32 I=0;I<S.Capacity+1;++I){W->ConsumeShot();W->AdvanceHandling(S.Interval);}
  Loader->Tick(1.f);Test->TestFalse(TEXT("No reserve means no phantom refill"),W->CanFire()||W->IsReloading());
  Loader->Destroy();
 }
 Player->SetActorLocation(PlayerLocation);PC->SetControlRotation(PlayerView);PC->PlayerCameraManager->UpdateCamera(0);
 for(auto* NPC:Spawned)if(IsValid(NPC))NPC->Destroy();
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCBehaviorRules,"Game.NPC.BehaviorRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNPCBehaviorRules::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FNPCBehaviorCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FNPCStreetConflictCheck,FAutomationTestBase*,Test);
bool FNPCStreetConflictCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 auto* PC=W?W->GetFirstPlayerController():nullptr;auto* Player=PC?PC->GetPawn().Get():nullptr;
 if(!Player){Test->AddError(TEXT("No PIE world/player"));return true;}
 auto* Rules=GetMutableDefault<UGlobalData>();auto* Events=W->GetSubsystem<UWorldEvents>();
 TGuardValue<bool> Automatic(Events->bAutomatic,false),StreetEvents(Rules->bEnableStreetEvents,true);
 TGuardValue<float> FightChance(Rules->CivilianFightBackChance,1.f),Early(Rules->LargeEventEarliestSeconds,0.f);
 for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SocialCooldown=999;
 const FVector Origin(25000,25000,8000);TArray<APatrolEnemy*> People;
 auto Spawn=[&](ENPCRole Role,FVector Offset){const FTransform T(FRotator::ZeroRotator,Origin+Offset);auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);N->NPCRole=Role;N->PatrolRadius=0;N->FinishSpawning(T);N->SocialCooldown=0;N->SetActorTickEnabled(false);N->GetCharacterMovement()->DisableMovement();People.Add(N);return N;};
 auto Clear=[&]{for(auto* N:People)if(IsValid(N))N->Destroy();People.Reset();};
 auto Hit=[&](APatrolEnemy* Victim,APawn* Attacker,float Damage){FHitResult H(Victim,Victim->GetMesh(),Victim->GetActorLocation(),FVector::UpVector);H.ImpactPoint=Victim->GetActorLocation();return W->GetSubsystem<UDataTransmitter>()->TransmitHit(H,Damage,FVector::ForwardVector,Attacker->GetController(),Attacker);};
 for(auto Role:{ENPCRole::Civilian,ENPCRole::Merchant}){
  auto* G=Spawn(ENPCRole::GangMember,FVector::ZeroVector);auto* V=Spawn(Role,FVector(100,0,0));auto* P=Spawn(ENPCRole::Police,FVector(0,400,0));
  G->Think();Test->TestEqual(TEXT("Active event director does not disable ordinary assaults"),G->AssaultTarget.Get(),V);
  Test->TestFalse(TEXT("Transaction cannot steal a busy attacker"),Events->TryStartEvent(EStreetEvent::SecretTransaction));
  G->StateTime=2.1f;G->TickAssault(.1f);G->StateTime=.6f;G->TickAssault(.1f);
  Test->TestEqual(TEXT("First attack is a real punch"),V->Health,138.f);
  Test->TestEqual(TEXT("Victim can choose counterattack"),V->Activity,ENPCActivity::Counterattack);
  Test->TestEqual(TEXT("Police target gangster, not defending civilian"),P->CombatTarget.Get(),static_cast<AActor*>(G));
  V->StateTime=.6f;V->TickCounterattack(.1f);
  Test->TestEqual(TEXT("Civilian punch damages attacker"),G->Health,138.f);
  Test->TestEqual(TEXT("Retaliation escalates gang to firearm combat"),G->CombatTarget.Get(),static_cast<AActor*>(V));
  Test->TestTrue(TEXT("Gang abandons melee after escalation"),G->bCombat&&!G->AssaultTarget.IsValid());
  Hit(V,G,110);Test->TestEqual(TEXT("Victim becomes injured"),V->NPCRole,ENPCRole::Injured);
  Test->TestTrue(TEXT("Escalated gun target remains valid through injury"),G->IsCombatTargetAlive(V));
  Hit(V,G,1000);Test->TestFalse(TEXT("Escalated target ends only at death"),G->IsCombatTargetAlive(V));
  Test->TestEqual(TEXT("No resurrection: original corpse lifetime"),V->GetLifeSpan(),30.f);
  Clear();
 }
 {
  TGuardValue<float> NeverFight(Rules->CivilianFightBackChance,0.f);
  auto* G=Spawn(ENPCRole::GangMember,FVector::ZeroVector);auto* V=Spawn(ENPCRole::Civilian,FVector(100,0,0));G->BeginAssault(V);Hit(V,G,12);
  Test->TestEqual(TEXT("Zero retaliation chance flees"),V->Activity,ENPCActivity::Flee);
  V->SetActorLocation(Origin+FVector(700,0,0));G->TickAssault(.1f);
  Test->TestEqual(TEXT("Running victim is pursued"),G->Activity,ENPCActivity::PursueMelee);
  Test->TestEqual(TEXT("Pursuit does not switch to gun for a non-retaliating victim"),G->CombatTarget.Get(),static_cast<AActor*>(nullptr));
  Test->TestTrue(TEXT("Gang pursuit can catch a fleeing civilian"),G->GetCharacterMovement()->MaxWalkSpeed>430.f);
  Clear();
 }
 {
  auto* G=Spawn(ENPCRole::GangMember,FVector::ZeroVector);auto* V=Spawn(ENPCRole::Civilian,FVector(100,0,0));auto* P=Spawn(ENPCRole::Police,FVector(0,400,0));
  Hit(G,Player,125);Test->TestEqual(TEXT("Gang can become injured"),G->NPCRole,ENPCRole::Injured);
  Test->TestFalse(TEXT("Injury does not turn gang into a protected civilian"),G->IsLawProtected());Hit(G,Player,10);
  Test->TestTrue(TEXT("Player hitting injured gang never makes police target player"),P->CombatTarget!=Player);
  Hit(V,Player,5);Test->TestEqual(TEXT("Attacking a civilian can make police target player"),P->CombatTarget.Get(),static_cast<AActor*>(Player));
  Clear();
 }
 // Exercise the actual selector, including the boundary and all three rare types.
 const EStreetEvent Rare[]={EStreetEvent::SecretTransaction,EStreetEvent::GangMassacre,EStreetEvent::MassiveGangWar};
 for(int32 I=0;I<3;++I)Test->TestEqual(TEXT("Rare pool contains the requested three events"),UWorldEvents::SelectScheduledEvent(.2f,.199f,I),Rare[I]);
 Test->TestEqual(TEXT("20 percent boundary belongs to common pool"),UWorldEvents::SelectScheduledEvent(.2f,.2f,0),EStreetEvent::GangMeeting);
 FRandomStream Random(9482);int32 RareCount=0;
 for(int32 I=0;I<10000;++I){const auto K=UWorldEvents::SelectScheduledEvent(.2f,Random.FRand(),Random.RandRange(0,2));RareCount+=K==Rare[0]||K==Rare[1]||K==Rare[2];}
 Test->TestTrue(TEXT("Seeded scheduling sample has roughly 20 percent rare events"),RareCount>1800&&RareCount<2200);
 // Reserve an isolated cluster; disable other map NPCs only in this test world.
 for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SocialCooldown=999;
 for(int32 I=0;I<3;++I){Spawn(ENPCRole::GangMember,FVector(I*250,0,0));Spawn(ENPCRole::Civilian,FVector(I*250,700,0));}
 Test->TestTrue(TEXT("Mass civilian attack starts with actual participants"),Events->TryStartEvent(EStreetEvent::GangMassacre));
 for(auto* N:People)if(N->IsGang()){Test->TestTrue(TEXT("Mass attack gang has a protected target"),Cast<APatrolEnemy>(N->CombatTarget)&&Cast<APatrolEnemy>(N->CombatTarget)->IsLawProtected());}
 if(People[0]->StreetEventId){const auto* E=Events->Find(People[0]->StreetEventId);Test->TestEqual(TEXT("Mass attack retains large-event score tier"),E->Tier,3);Test->TestEqual(TEXT("Mass attack has six participants"),E->Participants.Num(),6);Events->EndEvent(E->Id);}
 Clear();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCStreetConflictRules,"Game.NPC.StreetConflict",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNPCStreetConflictRules::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(3));AddCommand(new FNPCStreetConflictCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif

