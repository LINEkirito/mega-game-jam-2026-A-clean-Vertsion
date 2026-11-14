#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FGangRetaliationCheck,FAutomationTestBase*,Test);
bool FGangRetaliationCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 auto* PC=W?W->GetFirstPlayerController():nullptr;
 if(!PC||!PC->GetPawn()){Test->AddError(TEXT("Missing PIE player"));return true;}
 auto* Player=PC->GetPawn().Get();
 Test->TestEqual(TEXT("Nearby observation radius is 20 metres"),GetDefault<UGlobalData>()->GangRetaliationRadius,2000.f);
 Test->TestEqual(TEXT("Nearby observation lasts five seconds"),GetDefault<UGlobalData>()->GangAttackObservationSeconds,5.f);
 TArray<APatrolEnemy*> Actors;
 const FVector Origin(45000,45000,5000);
 auto Spawn=[&](ENPCRole Role,FVector Offset,bool FacingVictim=true)
 {
  const FRotator Facing=FacingVictim?(-Offset).Rotation():Offset.Rotation();
  const FTransform T(FRotator(0,Facing.Yaw,0),Origin+Offset);
  auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  N->NPCRole=Role;N->PatrolRadius=0;N->FinishSpawning(T);N->GetCharacterMovement()->DisableMovement();Actors.Add(N);return N;
 };
 auto* Victim=Spawn(ENPCRole::GangMember,FVector::ZeroVector);
 auto* Visible=Spawn(ENPCRole::GangMember,FVector(1200,0,0));
 auto* FarVisible=Spawn(ENPCRole::GangBoss,FVector(0,2100,0));
 auto* Unseen=Spawn(ENPCRole::GangMember,FVector(-1200,0,0),false);
 auto* Edge=Spawn(ENPCRole::GangMember,FVector(0,-2000,0),false);
 auto* Outside=Spawn(ENPCRole::GangMember,FVector(-2100,-1000,0),false);
 auto* Occluded=Spawn(ENPCRole::GangMember,FVector(1000,1000,0));
 auto* Dead=Spawn(ENPCRole::GangMember,FVector(600,-800,0));Dead->Health=0;
 auto* Injured=Spawn(ENPCRole::GangMember,FVector(700,-900,0));Injured->BecomeInjured();
 auto* Police=Spawn(ENPCRole::Police,FVector(-800,600,0));
 auto* Civilian=Spawn(ENPCRole::Civilian,FVector(-500,-500,0));
 auto* Wall=W->SpawnActor<AActor>();
 auto* Block=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Block);Block->SetBoxExtent(FVector(100,100,200));
 Block->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Block->SetCollisionResponseToAllChannels(ECR_Ignore);Block->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Block->RegisterComponent();
 Wall->SetActorLocation(Origin+FVector(500,500,50));
 Test->TestTrue(TEXT("Front-facing witness sees victim"),Visible->CanSeeActor(Victim));
 Test->TestTrue(TEXT("Witness beyond 20m remains inside visual range"),FarVisible->CanSeeActor(Victim));
 Test->TestFalse(TEXT("Facing away does not witness hit"),Unseen->CanSeeActor(Victim));
 Test->TestFalse(TEXT("Wall blocks witness sight"),Occluded->CanSeeActor(Victim));
 auto* Bus=W->GetSubsystem<UDataTransmitter>();
 auto Hit=[&](float Damage,AController* Instigator,AActor* Source)
 {
  FHitResult H(Victim,Victim->GetMesh(),Victim->GetActorLocation(),FVector::UpVector);
  H.ImpactPoint=Victim->GetActorLocation();return Bus->TransmitHit(H,Damage,FVector::ForwardVector,Instigator,Source);
 };
 Hit(0,PC,Player);Test->TestNull(TEXT("Zero damage does not alert"),Visible->CombatTarget.Get());
 Hit(1,Police->GetController(),Police);Test->TestNull(TEXT("Police damage does not trigger player retaliation"),Visible->CombatTarget.Get());
 Test->TestFalse(TEXT("Police damage does not start watching player"),Unseen->GangAttackLookTarget.IsValid());
 const auto PriorActivity=Unseen->Activity;
 Hit(1,PC,Player);
 for(auto* N:{Victim,Visible,FarVisible})Test->TestEqual(TEXT("Victim defends itself and actual witnesses target attacking player"),N->CombatTarget.Get(),static_cast<AActor*>(Player));
 for(auto* N:{Unseen,Edge,Occluded})
 {
  Test->TestNull(TEXT("Proximity without sight never gives a combat target"),N->CombatTarget.Get());
  Test->TestEqual(TEXT("Nearby non-witness watches player"),N->GangAttackLookTarget.Get(),Player);
  Test->TestTrue(TEXT("Observation deadline is five seconds"),FMath::IsNearlyEqual(N->GangAttackLookUntil-W->GetTimeSeconds(),5.f,.01f));
 }
 for(auto* N:{Outside,Dead,Injured,Police,Civilian})
 {
  Test->TestTrue(TEXT("Ineligible actors do not join player retaliation"),N->CombatTarget!=Player);
  Test->TestFalse(TEXT("Observation is not relayed beyond victim radius"),N->GangAttackLookTarget.IsValid());
 }
 Test->TestTrue(TEXT("Observation consumes normal decision tick"),Unseen->TickGangAttackObservation(.25f));
 Test->TestNull(TEXT("Looking does not become combat without a fresh witnessed attack"),Unseen->CombatTarget.Get());
 Test->TestEqual(TEXT("Original activity preserved while looking"),Unseen->Activity,PriorActivity);
 Unseen->GangAttackLookUntil=W->GetTimeSeconds()-.01f;
 Test->TestTrue(TEXT("Expiry frame does not run automatic hostility"),Unseen->TickGangAttackObservation(.25f));
 Test->TestFalse(TEXT("Observation clears after five seconds"),Unseen->GangAttackLookTarget.IsValid());
 Test->TestFalse(TEXT("Original behavior can resume next tick"),Unseen->TickGangAttackObservation(.25f));
 Test->TestNull(TEXT("Expiry does not target player"),Unseen->CombatTarget.Get());
 Hit(1,PC,Player);Test->TestTrue(TEXT("A later unseen attack restarts observation"),Unseen->GangAttackLookTarget.IsValid());
 Unseen->SetActorRotation((Victim->GetActorLocation()-Unseen->GetActorLocation()).Rotation());
 Hit(1,PC,Player);Test->TestEqual(TEXT("Witnessing an attack during observation enters combat"),Unseen->CombatTarget.Get(),static_cast<AActor*>(Player));
 Test->TestFalse(TEXT("Combat cancels observation"),Unseen->GangAttackLookTarget.IsValid());
 Wall->SetActorEnableCollision(false);Hit(1,PC,Player);
 Test->TestEqual(TEXT("Previously blocked witness retaliates once attack is visible"),Occluded->CombatTarget.Get(),static_cast<AActor*>(Player));
 Visible->CombatTarget=nullptr;
 Hit(Victim->Health-GetDefault<UGlobalData>()->NPCInjuryHealth,PC,Player);
 Test->TestEqual(TEXT("Victim becomes injured"),Victim->NPCRole,ENPCRole::Injured);
 Test->TestEqual(TEXT("Injury hit still alerts visual witnesses of original gang identity"),Visible->CombatTarget.Get(),static_cast<AActor*>(Player));
 Visible->CombatTarget=nullptr;Hit(10000,PC,Player);
 Test->TestEqual(TEXT("Lethal hit still alerts visual witnesses"),Visible->CombatTarget.Get(),static_cast<AActor*>(Player));
 Wall->Destroy();for(auto* N:Actors)N->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGangRetaliationTest,"Game.NPC.GangRetaliation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGangRetaliationTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FGangRetaliationCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
