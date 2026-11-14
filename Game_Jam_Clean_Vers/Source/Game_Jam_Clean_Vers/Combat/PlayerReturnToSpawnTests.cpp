#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/RecordingLibrary.h"

class FReturnToSpawnCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
public:
 explicit FReturnToSpawnCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  UWorld* W=nullptr;
  for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  if(!P){Test->AddError(TEXT("Missing player"));return true;}
  auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
  PC->SetPause(false);PC->bShowMouseCursor=false;PC->ResetIgnoreMoveInput();
  if(E)E->SetBackpackOpen(false);
  P->Health=73.f;
  Test->TestTrue(TEXT("Initial return succeeds"),P->ReturnToSpawn());
  const FVector Spawn=P->GetActorLocation();
  const int32 Items=E&&E->Inventory?E->Inventory->Items.Num():0;
  P->SetActorLocation(Spawn+FVector(600,0,500),false,nullptr,ETeleportType::TeleportPhysics);
  P->GetCharacterMovement()->Velocity=FVector(100,100,-1500);
  P->bSprintRequested=true;P->bSprinting=true;
  Test->TestTrue(TEXT("Return from airborne stuck location"),P->ReturnToSpawn());
  Test->TestTrue(TEXT("Returns to original spawn"),P->GetActorLocation().Equals(Spawn,2.f));
  Test->TestTrue(TEXT("Fall velocity cleared"),P->GetVelocity().IsNearlyZero());
  Test->TestTrue(TEXT("Sprint unchanged"),P->bSprintRequested&&P->bSprinting);
  Test->TestEqual(TEXT("Health preserved"),P->Health,73.f);
  if(E&&E->Inventory)Test->TestEqual(TEXT("Inventory preserved"),E->Inventory->Items.Num(),Items);
  PC->bShowMouseCursor=true;Test->TestFalse(TEXT("Menu blocks return"),P->ReturnToSpawn());PC->bShowMouseCursor=false;
  PC->SetIgnoreMoveInput(true);Test->TestFalse(TEXT("Input lock blocks return"),P->ReturnToSpawn());PC->ResetIgnoreMoveInput();
  if(E&&E->RecordingLibrary){E->RecordingLibrary->bRoundEnded=true;Test->TestFalse(TEXT("Settlement blocks return"),P->ReturnToSpawn());E->RecordingLibrary->bRoundEnded=false;}
  P->Health=0;Test->TestFalse(TEXT("Dead player cannot return"),P->ReturnToSpawn());P->Health=73;
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerReturnToSpawn,"Game.Player.ReturnToSpawn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlayerReturnToSpawn::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FReturnToSpawnCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
