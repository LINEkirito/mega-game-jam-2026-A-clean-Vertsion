#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PatrolEnemy.h"
#include "NPCMotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/GlobalData.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FNPCLocomotionCheck,FAutomationTestBase*,Test);
bool FNPCLocomotionCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 if(!W){Test->AddError(TEXT("No PIE world"));return true;}
 Test->TestEqual(TEXT("Patrol uses normal walking speed"),GetDefault<UGlobalData>()->NPCWalkSpeed,180.f);
 Test->TestEqual(TEXT("Combat advance does not force slow walking"),GetDefault<UGlobalData>()->NPCCombatAdvanceSpeed,180.f);
 for(auto Kind:{ETacticalWeaponKind::Pistol,ETacticalWeaponKind::Rifle,ETacticalWeaponKind::Shotgun})
 {
  const FTransform T(FVector(35000,35000,1200));
  auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  N->bRandomGangWeapon=false;N->WeaponKind=Kind;N->FinishSpawning(T);N->GetCharacterMovement()->DisableMovement();
  auto* M=Cast<UNPCMotionAnimInstance>(N->GetMesh()->GetAnimInstance());
  if(!Test->TestNotNull(TEXT("NPC motion"),M)){N->Destroy();continue;}
  auto Speed=[&](float S){N->GetCharacterMovement()->Velocity=FVector(S,0,0);N->SetMotion();};
  N->WeaponCarry=ENPCWeaponCarry::Holstered;N->Activity=ENPCActivity::PursueMelee;
  Speed(470);Test->TestTrue(TEXT("Full-speed chase selects running"),M->Sequence==N->ActivityAnimations.FindRef(ENPCActivity::Flee));
  Test->TestEqual(TEXT("Default chase uses Manny-compatible running cycle"),M->Sequence->GetName(),FString(TEXT("MF_Unarmed_Jog_Fwd")));
  Speed(110);Test->TestTrue(TEXT("Slowed chase selects walk instead of slow-motion sprint"),M->Sequence==N->UnarmedWalk);
  Test->TestTrue(TEXT("Walking cadence has a lower bound"),M->PlaybackRate>=.75f);
  Test->TestTrue(TEXT("Shorter stride matches slow displacement"),FMath::IsNearlyEqual(M->PlaybackRate*M->StrideWeight,110.f/300.f,.001f));
  Speed(8);Test->TestTrue(TEXT("Blocked chase returns to idle"),M->Sequence==N->UnarmedIdle&&!M->bLocomotion);
  Speed(20);Test->TestFalse(TEXT("Near-stop jitter does not restart walking"),M->bLocomotion);
  Speed(30);Test->TestTrue(TEXT("Actual movement restarts walking"),M->bLocomotion);
  Speed(20);Test->TestTrue(TEXT("Hysteresis keeps walking until stopped"),M->bLocomotion);
  N->WeaponCarry=ENPCWeaponCarry::Drawn;N->Activity=ENPCActivity::Combat;
  Speed(200);Test->TestTrue(TEXT("Moderate armed speed does not select sprint"),M->Sequence!=N->Run);
  Speed(470);Test->TestTrue(TEXT("Fast armed movement uses sprint"),M->Sequence==N->Run);
  Speed(400);Test->TestTrue(TEXT("Sprint hysteresis avoids repeated transition"),M->Sequence==N->Run);
  Speed(300);Test->TestTrue(TEXT("Sprint deceleration returns to walk"),M->Sequence!=N->Run);
  for(bool Armed:{false,true})for(float S:{30.f,110.f,180.f,300.f,470.f})
  {
   N->WeaponCarry=Armed?ENPCWeaponCarry::Drawn:ENPCWeaponCarry::Holstered;
   N->Activity=Armed?ENPCActivity::Combat:ENPCActivity::PursueMelee;Speed(S);
   bool FeetPreserved=true;
   for(int Frame=0;Frame<60;++Frame)
   {
    N->GetMesh()->TickAnimation(1.f/30.f,false);N->GetMesh()->RefreshBoneTransforms();N->UpdateWeaponPose(1.f/30.f);
    for(FName Bone:{FName(TEXT("calf_l")),FName(TEXT("calf_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
    {
     const FTransform Source=N->GetMesh()->GetSocketTransform(Bone);
     const FTransform Render=N->NPCBody->GetBoneTransformByName(Bone,EBoneSpaces::WorldSpace);
     FeetPreserved&=!Render.ContainsNaN()&&Source.Equals(Render,.02f);
    }
   }
   Test->TestTrue(FString::Printf(TEXT("Rendered knees/feet preserve animation at speed %.0f, armed %d"),S,Armed),FeetPreserved);
  }
  Speed(0);Test->TestEqual(TEXT("Idle restores normal rate"),M->PlaybackRate,1.f);
  Test->TestNull(TEXT("Idle clears stride blend"),M->StrideIdle);
  N->Destroy();
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCLocomotion,"Game.NPC.Locomotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNPCLocomotion::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FNPCLocomotionCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
