#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/DamageEvents.h"
#include "Combat/ImpactBloodEffect.h"
#include "Combat/PatrolEnemy.h"
#include "Data/DataTransmitter.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBloodFeedbackCheck,FAutomationTestBase*,Test);
bool FBloodFeedbackCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 if(!W){Test->AddError(TEXT("Run in PIE"));return true;}
 auto* Bus=W->GetSubsystem<UDataTransmitter>();auto* PC=W->GetFirstPlayerController();
 auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;if(!Bus||!Player)return true;
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Enemy=W->SpawnActor<APatrolEnemy>(FVector(0,0,1000),FRotator::ZeroRotator,P);if(!Enemy)return true;Enemy->SetActorTickEnabled(false);
 FHitResult Hit(Enemy,Enemy->GetCapsuleComponent(),Enemy->GetActorLocation(),FVector(-1,0,0));
 auto Count=[W](){int32 N=0;for(TActorIterator<AImpactBloodEffect> It(W);It;++It)++N;return N;};const int32 Before=Count();
 Test->TestEqual(TEXT("Valid enemy torso damage"),Bus->TransmitHit(Hit,10,FVector(1,0,0),PC,Player),10.f);
 Test->TestEqual(TEXT("Enemy damage spawns blood at impact"),Count(),Before+1);
 Bus->TransmitHit(Hit,10,FVector(1,0,0),PC,Player);Test->TestEqual(TEXT("Same-frame pellets share one burst"),Count(),Before+1);
 Bus->TransmitHit(Hit,0,FVector(1,0,0),PC,Player);Test->TestEqual(TEXT("Invalid damage produces no blood"),Count(),Before+1);
 for(TActorIterator<AImpactBloodEffect> It(W);It;++It)if(FVector::DistSquared(It->GetActorLocation(),Enemy->GetActorLocation())<100)It->Destroy();
 Enemy->Destroy();
 auto* Dummy=W->SpawnActor<AGame_Jam_Clean_VersCharacter>(FVector(0,0,1500),FRotator::ZeroRotator,P);
 if(Dummy){const int32 BeforePlayer=Count();FDamageEvent Damage;
 Test->TestEqual(TEXT("Player takes valid damage"),Dummy->TakeDamage(20,Damage,nullptr,nullptr),20.f);
 Test->TestTrue(TEXT("Player damage drives screen feedback"),Dummy->DamageFlash>0.f);
 Test->TestEqual(TEXT("Player does not spawn body blood"),Count(),BeforePlayer);
 Test->TestEqual(TEXT("Protected player rejects repeated damage"),Dummy->TakeDamage(20,Damage,nullptr,nullptr),0.f);
 Dummy->Destroy();}
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBloodFeedback,"Game.Combat.BloodFeedback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBloodFeedback::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FBloodFeedbackCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
