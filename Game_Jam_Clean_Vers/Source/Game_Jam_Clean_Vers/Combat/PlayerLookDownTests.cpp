#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PatrolEnemy.h"
#include "GameFramework/PlayerController.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/ShoulderCamcorder.h"

class FPlayerLowLookCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 int32 Slot=1,Stage=0,Magazine=0,Shots=0;
 bool Chamber=false;
 float Next=0;
 TWeakObjectPtr<ATacticalWeapon> Gun;
public:
 explicit FPlayerLowLookCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  UWorld* World=nullptr;
  for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* C=PC?PC->FindComponentByClass<UCombatEquipmentComponent>():nullptr;
  if(!C||!C->Equipment||!C->Equipment->Inventory){Test->AddError(TEXT("Missing equipment"));return true;}
  if(World->GetTimeSeconds()<Next)return false;
  auto Wait=[&](float Seconds){Next=World->GetTimeSeconds()+Seconds;return false;};
  switch(Stage++)
  {
  case 0:
   for(TActorIterator<APatrolEnemy> It(World);It;++It)It->SetActorTickEnabled(false);
   PC->SetControlRotation(FRotator::ZeroRotator);C->Equipment->ActivateQuickSlot(Slot);return Wait(4);
  case 1:
   Gun=C->ActiveWeapon;if(!Gun.IsValid()){Test->AddError(TEXT("Gun did not equip"));return true;}
   Magazine=Gun->Magazine;Chamber=Gun->bChambered;Shots=C->ShotsFired;
   PC->SetControlRotation(FRotator(C->MinimumHoldPitch(Gun.Get()),0,0));C->TickLookDownHolster();
   Test->TestTrue(TEXT("Exact boundary retains gun"),C->ActiveWeapon==Gun.Get());
   PC->SetControlRotation(FRotator(C->MinimumHoldPitch(Gun.Get())-1,0,0));C->Fire();
   Test->TestEqual(TEXT("Below threshold blocks fire before tick"),C->ShotsFired,Shots);
   C->TickLookDownHolster();
   Test->TestNull(TEXT("Low look immediately removes active gun"),C->ActiveWeapon.Get());
   Test->TestTrue(TEXT("Authored holster is presented"),C->GetPresentationWeapon()==Gun.Get()&&Gun->Handling==EWeaponHandling::Holster);
   C->Fire();Test->TestFalse(TEXT("Holster blocks reload"),C->Reload());return Wait(3);
  case 2:
   Test->TestNull(TEXT("Holster finishes into empty hands"),C->GetPresentationWeapon());
   Test->TestTrue(TEXT("Same gun remembered"),C->LookDownWeaponId==Gun->InventoryId);
   Test->TestEqual(TEXT("Magazine retained"),Gun->Magazine,Magazine);
   Test->TestEqual(TEXT("Chamber retained"),Gun->bChambered,Chamber);
   PC->SetControlRotation(FRotator(C->MinimumHoldPitch(Gun.Get())+1,0,0));return Wait(.3f);
  case 3:
   Test->TestNull(TEXT("Hysteresis prevents chatter"),C->ActiveWeapon.Get());
   PC->SetControlRotation(FRotator::ZeroRotator);C->TickLookDownHolster();
   Test->TestTrue(TEXT("Raising head restores same gun"),C->ActiveWeapon==Gun.Get());
   C->Fire();Test->TestEqual(TEXT("Drawing cannot fire"),C->ShotsFired,Shots);return Wait(3);
  case 4:
   Test->TestTrue(TEXT("Draw completes"),Gun->Handling==EWeaponHandling::Idle);
   PC->SetControlRotation(FRotator(-85,0,0));return Wait(3);
  case 5:
   C->EquipWeapon(NAME_None);PC->SetControlRotation(FRotator::ZeroRotator);return Wait(.3f);
  default:
   Test->TestNull(TEXT("Manual holster cancels automatic restore"),C->ActiveWeapon.Get());
   Test->TestFalse(TEXT("No stale automatic selection"),C->HasLookDownSelection());
   if(++Slot<=3){Stage=0;return false;}
   return true;
  }
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerLowLook,"Game.Combat.PlayerLowLook",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlayerLowLook::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FPlayerLowLookCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
