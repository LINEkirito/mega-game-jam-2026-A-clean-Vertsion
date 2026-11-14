#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ExtractionPoint.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Combat/PatrolEnemy.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Camcorder/RecordingLibrary.h"
#include "Localization/GameLanguage.h"
#include "Data/GlobalData.h"

class FExtractionFlowCheck : public IAutomationLatentCommand
{
public:
 explicit FExtractionFlowCheck(FAutomationTestBase* InTest):Test(InTest){}
 virtual bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  if(!W){Test->AddError(TEXT("Missing PIE world"));return true;}
  auto* PC=W->GetFirstPlayerController();auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  auto* Library=PC?PC->FindComponentByClass<URecordingLibrary>():nullptr;
  if(!Player||!Library){Test->AddError(TEXT("Missing player/settlement fixture"));return true;}
  if(Stage==0)
  {
   for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
   for(TActorIterator<AExtractionPoint> It(W);It;++It)It->SetActorTickEnabled(false);
   PC->SetPause(false);PC->bShowMouseCursor=false;Player->Health=100;
   Player->GetCharacterMovement()->DisableMovement();Player->SetActorLocation(FVector(35000,35000,1000),false,nullptr,ETeleportType::TeleportPhysics);
   FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Point=W->SpawnActor<AExtractionPoint>(FVector(35480,35000,925),FRotator::ZeroRotator,Params);
   Point->SetActorTickEnabled(false);
   PC->SetControlRotation(FRotator::ZeroRotator);
   ++Stage;return false; // Allow the camera manager to update the real view.
  }
  if(Stage==1)
  {
   auto* P=Point.Get();if(!P){Test->AddError(TEXT("Point lost"));return true;}
   Test->TestEqual(TEXT("Default radius is 3 metres"),GetDefault<UGlobalData>()->ExtractionRadius,300.f);
   Test->TestEqual(TEXT("Default extraction lasts 15 seconds"),GetDefault<UGlobalData>()->ExtractionSeconds,15.f);
   Test->TestTrue(TEXT("Distance uses closest body surface"),FMath::IsNearlyEqual(P->DistanceToVehicle(Player->GetActorLocation()),250.f));
   const FVector Here=Player->GetActorLocation();
   Player->SetActorLocation(Here-FVector(51,0,0));Test->TestFalse(TEXT("301cm is out of range"),P->IsPlayerInRange(Player));
   Player->SetActorLocation(Here-FVector(50,0,0));Test->TestTrue(TEXT("300cm is in range"),P->IsPlayerInRange(Player));Player->SetActorLocation(Here);
   Test->TestTrue(TEXT("Real view ray hits vehicle"),P->IsLookedAtBy(PC));
   auto* Blocker=W->SpawnActor<AExtractionPoint>(FVector(35200,35000,925),FRotator::ZeroRotator);
   Blocker->VehicleBody->SetBoxExtent(FVector(10,100,100));Blocker->SetActorTickEnabled(false);
   Test->TestFalse(TEXT("Occluder prevents activation through walls"),P->TryStart(PC));Blocker->Destroy();
   const bool WasEnglish=GameLanguage::IsEnglish();
   GameLanguage::SetEnglish(false);Test->TestEqual(TEXT("Chinese F hint"),GameLanguage::Display(P->StatusText()),FString(TEXT("[F] 开始撤离（车身 3 米内）")));
   GameLanguage::SetEnglish(true);Test->TestEqual(TEXT("English F hint"),GameLanguage::Display(P->StatusText()),FString(TEXT("[F] Start extraction (within 3 m)")));
   GameLanguage::SetEnglish(WasEnglish);
   P->Tick(.016f);Test->TestTrue(TEXT("Looking at vehicle without F stays idle"),P->State==EExtractionState::Idle);
   auto* Interaction=PC->FindComponentByClass<UCombatEquipmentComponent>();
   if(!Test->TestNotNull(TEXT("Shared F interaction component exists"),Interaction))return true;
   Test->TestTrue(TEXT("Shared F action handles the vehicle"),Interaction->TryInteract());
   if(!Test->TestTrue(TEXT("Explicit interaction inside range starts extraction"),P->State==EExtractionState::Extracting))return true;
   Test->TestFalse(TEXT("Repeated activation cannot restart countdown"),P->TryStart(PC));
   Test->TestFalse(TEXT("World permits only one extraction"),W->GetSubsystem<UExtractionSubsystem>()->Claim(P));
   P->Advance(5);Test->TestEqual(TEXT("Countdown after five seconds"),P->RemainingSeconds,10.f);
   Player->SetActorLocation(Here-FVector(51,0,0));P->Advance(.01f);
   Test->TestTrue(TEXT("Leaving radius cancels"),P->State==EExtractionState::Idle);
   Test->TestNull(TEXT("Cancellation clears world alarm"),W->GetSubsystem<UExtractionSubsystem>()->GetExtractingPlayer());
   Player->SetActorLocation(Here);Test->TestTrue(TEXT("Re-entry restarts full timer"),P->TryStart(PC));
   Test->TestEqual(TEXT("Fresh timer"),P->RemainingSeconds,15.f);
   Player->Health=0;P->Advance(.01f);Test->TestTrue(TEXT("Death cancels and cannot succeed"),P->State==EExtractionState::Idle&&!Library->bRoundEnded);Player->Health=100;
   Test->TestTrue(TEXT("Restart for AI test"),P->TryStart(PC));
   const FTransform Spawn(FRotator(0,180,0),Here+FVector(0,1500,0));
   auto* Enemy=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Enemy->PatrolRadius=0;Enemy->FinishSpawning(Spawn);Enemy->SetActorTickEnabled(false);
   Enemy->SightRange=500;Enemy->TickExtractionResponse(.016f);
   Test->TestTrue(TEXT("Distant enemy investigates player position"),Enemy->Activity==ENPCActivity::Investigate&&Enemy->InvestigationLocation.Equals(Here,1));
   Test->TestNull(TEXT("Unseen player is not a fire target"),Enemy->CombatTarget.Get());
   Enemy->SightRange=2200;Enemy->SetActorRotation((Here-Enemy->GetActorLocation()).Rotation());Enemy->TickExtractionResponse(.016f);
   Test->TestTrue(TEXT("Sight acquires player without warning"),Enemy->Activity==ENPCActivity::Combat&&Enemy->CombatTarget==Player&&Enemy->WarningRemaining==0);
   Enemy->NPCRole=ENPCRole::Civilian;Test->TestFalse(TEXT("Civilian excluded from extraction assault"),Enemy->TickExtractionResponse(.016f));Enemy->Destroy();
   const bool English=GameLanguage::IsEnglish();
   auto Label=SNew(SLocalizedTextBlock).Text_Lambda([P]{return FText::FromString(P->StatusText());});
   GameLanguage::SetEnglish(false);Test->TestEqual(TEXT("Chinese countdown"),Label->GetText().ToString(),FString(TEXT("撤离倒计时：15.0 秒")));
   GameLanguage::SetEnglish(true);Test->TestEqual(TEXT("Live English countdown"),Label->GetText().ToString(),FString(TEXT("Extracting: 15.0 s remaining")));
   GameLanguage::SetEnglish(English);
   P->Advance(14.9f);Test->TestFalse(TEXT("Cannot settle before 15 seconds"),Library->bRoundEnded);
   PC->SetControlRotation(FRotator(0,180,0));
   Player->GetFirstPersonCameraComponent()->SetWorldRotation(FRotator(0,180,0));
   PC->PlayerCameraManager->UpdateCamera(.016f);++Stage;return false;
  }
  auto* P=Point.Get();
  Test->TestFalse(TEXT("Player can look away during extraction"),P->IsLookedAtBy(PC));
  P->Advance(.11f);
  Test->TestTrue(TEXT("15 seconds reaches successful existing settlement"),P->State==EExtractionState::Completed&&Library->bRoundEnded&&!Library->bEndedByDeath);
  Test->TestNull(TEXT("Completion releases world alarm"),W->GetSubsystem<UExtractionSubsystem>()->GetActivePoint());
  return true;
 }
private:
 FAutomationTestBase* Test;int32 Stage=0;TWeakObjectPtr<AExtractionPoint> Point;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExtractionFlowTest,"Game.Extraction.Flow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExtractionFlowTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FExtractionFlowCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExtractionAssetsTest,"Game.Extraction.VehicleAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExtractionAssetsTest::RunTest(const FString&)
{
 for(const TCHAR* Type:{TEXT("Sedan"),TEXT("SUV"),TEXT("BoxTruck"),TEXT("Campervan")})
 {
  const FString Path=FString::Printf(TEXT("/Game/Extraction/BP_Extraction_%s.BP_Extraction_%s_C"),Type,Type);
  auto* Class=LoadClass<AExtractionPoint>(nullptr,*Path);
  if(!TestNotNull(Path,Class))continue;
  auto* Point=Class->GetDefaultObject<AExtractionPoint>();
  TestNotNull(TEXT("Vehicle model assigned"),Point->VehicleMesh->GetStaticMesh().Get());
  TestTrue(TEXT("Vehicle collision body blocks interaction"),Point->VehicleBody->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Block);
 }
 return true;
}
#endif
