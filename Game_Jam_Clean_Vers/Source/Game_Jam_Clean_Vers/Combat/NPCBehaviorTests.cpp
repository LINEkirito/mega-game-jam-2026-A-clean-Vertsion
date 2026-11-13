#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PatrolEnemy.h"
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
#include "NPCMotionAnimInstance.h"
#include "Engine/DamageEvents.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FNPCBehaviorCheck,FAutomationTestBase*,Test);
bool FNPCBehaviorCheck::Update()
{
 UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
 auto* PC=World?World->GetFirstPlayerController():nullptr;
 auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 if(!Player){Test->AddError(TEXT("No PIE player"));return true;}
 Test->TestEqual(TEXT("Player hurtbox blocks the NPC bullet trace channel"),Player->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility),ECR_Block);
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
 SafeOfficer->SetActorRotation(FRotator(0,180,0));
 SafeOfficer->WitnessViolence(Wounded,Suppressor,true);
 for(int I=0;I<200;++I){SafeOfficer->Tick(.1f);Suppressor->Tick(.1f);}
 Test->TestEqual(TEXT("Gang retaliates against officer firing at it"),Suppressor->CombatTarget.Get(),static_cast<AActor*>(SafeOfficer));
 Test->TestTrue(TEXT("Police remain in the firefight during the short encounter"),SafeOfficer->Health>0);
 Test->TestTrue(TEXT("Police twenty-second damage budget remains very low"),Suppressor->Health>=126.f);
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
 if(Camera){Camera->bHeld=true;Camera->bPowered=true;Camera->bViewfinderRaised=true;MovePlayer(800);
  Test->TestEqual(TEXT("Aimed active camera expands warning range to ten meters"),CameraBoss->PlayerWarningRange(),1000.f);
  CameraBoss->Tick(.4f);Test->TestEqual(TEXT("Boss warns filming player at eight meters"),CameraBoss->Activity,ENPCActivity::WarnPlayer);
  Camera->bHeld=false;CameraBoss->Tick(.1f);Test->TestEqual(TEXT("Putting camera away does not substitute for retreat"),CameraBoss->Activity,ENPCActivity::WarnPlayer);
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
 AimedGang->Tick(.01f);Test->TestEqual(TEXT("Aiming starts an immediate warning outside proximity range"),AimedGang->Activity,ENPCActivity::WarnPlayer);
 AimedGang->Tick(.5f);Test->TestFalse(TEXT("Half second aiming does not start combat"),AimedGang->bCombat);
 PC->SetControlRotation(FRotator(0,90,0));AimedGang->Tick(.01f);
 Test->TestEqual(TEXT("Looking away clears aim countdown"),AimedGang->WarningRemaining,0.f);
 PC->SetControlRotation(Aim);AimedGang->Tick(.01f);AimedGang->Tick(.8f);
 Test->TestFalse(TEXT("Reaim starts a fresh full countdown"),AimedGang->bCombat);
 AimedGang->Tick(.2f);Test->TestEqual(TEXT("One second continuous aiming targets player"),AimedGang->CombatTarget.Get(),static_cast<AActor*>(Player));
 AimedGang->GetCharacterMovement()->Velocity=AimedGang->GetActorForwardVector()*95;
 AimedGang->Tick(.01f);
 auto* Motion=Cast<UNPCMotionAnimInstance>(AimedGang->GetMesh()->GetAnimInstance());
 if(Test->TestNotNull(TEXT("Combat retains native animation blending"),Motion))
 {
  Test->TestNotNull(TEXT("Moving fire has an upper-body holding pose"),Motion->UpperBodyPose.Get());
  Test->TestTrue(TEXT("Combat gait follows actual slow walking speed"),Motion->PlaybackRate>.7f&&Motion->PlaybackRate<1.f);
  // This fixture has no navigation floor. Test repeated firing at rest separately
  // from the real-path walking check, so a failed MoveTo cannot change the gait.
  MovePlayer(200);AimedGang->GetCharacterMovement()->Velocity=FVector::ZeroVector;AimedGang->Tick(1.f);
  auto* HoldingPose=Motion->Sequence.Get();
  for(int I=0;I<3;++I){AimedGang->Tick(.7f);Test->TestEqual(TEXT("Repeated shots preserve holding sequence"),Motion->Sequence.Get(),HoldingPose);}
 }
 Combat->ActiveWeapon=PreviousWeapon;Combat->bADS=false;Gun->Destroy();
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

