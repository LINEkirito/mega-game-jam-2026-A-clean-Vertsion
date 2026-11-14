#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FPoliceGangNonlethalCheck,FAutomationTestBase*,Test);
bool FPoliceGangNonlethalCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 auto* PC=W?W->GetFirstPlayerController():nullptr;
 if(!PC||!PC->GetPawn()){Test->AddError(TEXT("Missing PIE player"));return true;}
 TArray<APatrolEnemy*> Actors;
 auto Spawn=[&](ENPCRole Role)
 {
  const FTransform T(FRotator::ZeroRotator,FVector(45000+Actors.Num()*400,45000,5000));
  auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  N->NPCRole=Role;N->PatrolRadius=0;N->FinishSpawning(T);N->GetCharacterMovement()->DisableMovement();Actors.Add(N);return N;
 };
 auto* Bus=W->GetSubsystem<UDataTransmitter>();
 auto Hit=[&](APatrolEnemy* Victim,float Damage,AController* Controller,AActor* Source,bool Head=false)
 {
  FHitResult H(Victim,Victim->GetMesh(),Victim->GetActorLocation(),FVector::UpVector);
  H.ImpactPoint=Victim->GetActorLocation();if(Head)H.BoneName=TEXT("head");
  return Bus->TransmitHit(H,Damage,FVector::ForwardVector,Controller,Source);
 };
 const float Injury=GetDefault<UGlobalData>()->NPCInjuryHealth;
 const float Floor=FMath::Max(0.f,Injury)+1.f;
 for(const auto GangRole:{ENPCRole::GangMember,ENPCRole::GangBoss})
 {
  for(bool PoliceVictim:{true,false})
  {
   auto* Victim=Spawn(PoliceVictim?ENPCRole::Police:GangRole);
   auto* Attacker=Spawn(PoliceVictim?GangRole:ENPCRole::Police);
   const auto OriginalRole=Victim->NPCRole;
   Victim->Health=Floor+100;
   Test->TestEqual(TEXT("Ordinary crossfire still damages"),Hit(Victim,1,Attacker->GetController(),Attacker),1.f);
   Hit(Victim,10000,Attacker->GetController(),Attacker,true);
   Test->TestEqual(TEXT("Lethal head hit stops above injury threshold"),Victim->Health,Floor);
   for(int32 I=0;I<16;++I)Test->TestEqual(TEXT("Repeated pellets cannot lower health further"),Hit(Victim,10000,nullptr,Attacker),0.f);
   Test->TestEqual(TEXT("Crossfire preserves combat role"),Victim->NPCRole,OriginalRole);
   Test->TestTrue(TEXT("Crossfire never enters injured or dead activity"),Victim->Activity!=ENPCActivity::Injured&&Victim->Activity!=ENPCActivity::Dead);
   Hit(Victim,Victim->Health-Injury,PC,PC->GetPawn());
   Test->TestEqual(TEXT("Player can still cause injury"),Victim->NPCRole,ENPCRole::Injured);
   const float DownedHealth=Victim->Health;
   Test->TestEqual(TEXT("Opposing faction cannot finish a downed NPC"),Hit(Victim,10000,Attacker->GetController(),Attacker,true),0.f);
   Test->TestEqual(TEXT("Protection never heals downed NPC"),Victim->Health,DownedHealth);
   Hit(Victim,10000,PC,PC->GetPawn());
   Test->TestEqual(TEXT("Player can still kill"),Victim->Health,0.f);
  }
 }
 for(auto* N:Actors)N->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoliceGangNonlethalTest,"Game.NPC.PoliceGangNonlethal",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPoliceGangNonlethalTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FPoliceGangNonlethalCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
