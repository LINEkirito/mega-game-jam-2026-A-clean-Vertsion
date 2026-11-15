#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "AmbientTrafficCar.h"
#include "Extraction/ExtractionPoint.h"
#include "Combat/PatrolEnemy.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAmbientTrafficCheck,FAutomationTestBase*,Test);
bool FAmbientTrafficCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 if(!W){Test->AddError(TEXT("No PIE world"));return true;}
 int32 Types[3]={};int32 Lots[8]={};
 for(TActorIterator<AAmbientTrafficCar> It(W);It;++It)++Types[static_cast<int32>(It->VehicleKind)];
 Test->TestTrue(TEXT("Civilian, police and gang traffic are all placed"),Types[0]>=1&&Types[1]>=1&&Types[2]>=1);
 for(TActorIterator<AExtractionPoint> It(W);It;++It){const FVector P=It->GetActorLocation();const int32 X=P.X<-18000?0:P.X<-6000?1:P.X<6000?2:3;++Lots[X*2+(P.Y>0?1:0)];}
 for(int32 I=0;I<8;++I)Test->TestTrue(FString::Printf(TEXT("Parking area %d has two extraction vehicles"),I+1),Lots[I]>=2);
 const FVector Origin(60000,60000,5000);
 auto* Car=W->SpawnActorDeferred<AAmbientTrafficCar>(AAmbientTrafficCar::StaticClass(),FTransform(Origin),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 for(int32 I=0;I<120;++I){const float A=2.f*PI*I/120.f;Car->RoutePoints.Add(Origin+FVector(3000*FMath::Cos(A),3000*FMath::Sin(A),0));}
 Car->FinishSpawning(FTransform(Origin));Car->SetActorTickEnabled(false);
 const float Length=Car->Route->GetSplineLength();
 for(int32 I=0;I<2200;++I)Car->Tick(.025f);
 Test->TestTrue(TEXT("Car completes a continuous loop without teleporting"),Car->CompletedLaps>=1&&Car->DistanceTravelled>Length);
 const float Before=Car->DistanceTravelled;
 auto* Person=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),FTransform(Car->GetActorLocation()+Car->GetActorForwardVector()*600),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 Person->NPCRole=ENPCRole::Civilian;Person->FinishSpawning(FTransform(Car->GetActorLocation()+Car->GetActorForwardVector()*600));Person->SetActorTickEnabled(false);Person->GetCharacterMovement()->DisableMovement();
 const float Health=Person->Health;
 for(int32 I=0;I<160;++I)Car->Tick(.025f);
 Test->TestTrue(TEXT("Traffic stops before a pedestrian"),Car->Speed<1&&Car->bWaitingForClearance&&Car->DistanceTravelled-Before<400);
 Test->TestEqual(TEXT("Traffic does no collision damage"),Person->Health,Health);
 Person->Destroy();
 const float Stopped=Car->DistanceTravelled;
 for(int32 I=0;I<120;++I)Car->Tick(.025f);
 Test->TestTrue(TEXT("Traffic resumes when the pedestrian clears"),Car->DistanceTravelled>Stopped+200);
 // A stationary vehicle uses the same obstacle detection as other dynamic blockers.
 auto* Blocker=W->SpawnActor<AAmbientTrafficCar>(Car->GetActorLocation()+Car->GetActorForwardVector()*800,Car->GetActorRotation());Blocker->SetActorTickEnabled(false);
 for(int32 I=0;I<160;++I)Car->Tick(.025f);
 Test->TestTrue(TEXT("Traffic yields to another vehicle"),Car->Speed<1&&Car->bWaitingForClearance);
 Blocker->Destroy();Car->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientTrafficTest,"Game.Traffic.LoopYieldAndParking",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientTrafficTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FAmbientTrafficCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
