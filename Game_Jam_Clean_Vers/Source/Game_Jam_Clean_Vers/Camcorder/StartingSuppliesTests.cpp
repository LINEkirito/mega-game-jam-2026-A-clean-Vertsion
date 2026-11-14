#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "CamcorderEquipmentComponent.h"
#include "BackpackInventory.h"
#include "ShoulderCamcorder.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Combat/PatrolEnemy.h"
#include "Components/BoxComponent.h"
#include "Localization/GameLanguage.h"
#include "UnrealClient.h"

class FStartingSuppliesCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Stage=0,Pickup=0;double Next=0;
 bool OldEnglish=false;TArray<TWeakObjectPtr<ATacticalWeapon>> Guns;
public:
 explicit FStartingSuppliesCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override {
  if(FPlatformTime::Seconds()<Next)return false;
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
  auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  if(!E||!E->Combat||!P){Test->AddError(TEXT("Missing equipment"));return true;}
  auto Wait=[&](double S){Next=FPlatformTime::Seconds()+S;return false;};
  if(Stage==0){
   OldEnglish=GameLanguage::IsEnglish();GameLanguage::SetEnglish(false);
   Test->TestEqual(TEXT("No guns owned at spawn"),E->Combat->Weapons.Num(),0);
   Test->TestEqual(TEXT("No spare batteries at spawn"),E->GetBatteryCount(),0);
   for(int32 I=1;I<4;++I)Test->TestTrue(TEXT("Weapon slots initially empty"),E->Inventory->Slots[I].IsNone());
   for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
   int32 Batteries=0;for(TActorIterator<ACamcorderBattery> It(W);It;++It)++Batteries;
   Test->TestEqual(TEXT("No battery pickups in play"),Batteries,0);
   Guns.SetNum(3);for(TActorIterator<ATacticalWeapon> It(W);It;++It)if(It->ActorHasTag(TEXT("StartingTableWeapon")))Guns[int32(It->Kind)]=*It;
   for(auto G:Guns)if(!G.IsValid()){Test->AddError(TEXT("Missing starting table weapon"));return true;}
   for(auto G:Guns){Test->TestTrue(TEXT("Table gun visible and unowned"),!G->IsHidden()&&!G->bEquipped&&!G->bNPCEquipped);Test->TestTrue(TEXT("Gun remains supported above table"),G->GetActorLocation().Z>85);}
   P->SetActorLocation(FVector(-22200,-650,100));PC->SetControlRotation(FRotator(-15,-90,0));
   Stage=1;return Wait(2);
  }
  if(Stage==1){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/StartingSupplies/table_zh.png"),true,false);Stage=2;return Wait(1);}
  if(Stage==2){GameLanguage::SetEnglish(true);Stage=3;return Wait(1);}
  if(Stage==3){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/StartingSupplies/table_en.png"),true,false);Stage=4;return Wait(1);}
  if(Stage==4){
   if(Pickup==3){Stage=6;return Wait(2);}
   auto* G=Guns[Pickup].Get();const FVector Pos=G->GetActorLocation();
   P->SetActorLocation(FVector(Pos.X,Pos.Y-125,100));PC->SetControlRotation(FRotator(-25,90,0));
   Stage=5;return Wait(.5);
  }
  if(Stage==5){
   auto* G=Guns[Pickup].Get();Test->TestTrue(TEXT("Table gun offers pickup prompt"),E->Combat->InteractionText().Contains(TEXT("[F]")));
   Test->TestTrue(TEXT("Pickup interaction succeeds"),E->Combat->TryInteract());
   Test->TestTrue(TEXT("Picked gun enters owned inventory"),E->Combat->Weapons.Contains(G->InventoryId));
   Test->TestEqual(TEXT("Gun binds matching fixed slot"),E->Inventory->Slots[Pickup+1],G->InventoryId);
   ++Pickup;Stage=4;return Wait(3);
  }
  if(Stage==6){
   E->EquipCamera();Stage=7;return Wait(3);
  }
  if(Stage==7){
   E->Camcorder->SetViewfinderRaised(true);Test->TestTrue(TEXT("Camera records before depletion"),E->Camcorder->StartRecording());Stage=8;return Wait(.5);
  }
  if(Stage==8){
   E->Camcorder->ConsumeBattery(10000);Test->TestFalse(TEXT("Depletion stops recording"),E->Camcorder->bRecording);
   E->ReplaceBattery();Test->TestFalse(TEXT("Direct refill is disabled"),E->Camcorder->InstallFreshBattery());E->Camcorder->TogglePower();
   Test->TestEqual(TEXT("No recovery after depletion"),E->Camcorder->BatteryPercent,0.f);Test->TestFalse(TEXT("Empty camera cannot record"),E->Camcorder->StartRecording());
   GameLanguage::SetEnglish(false);Stage=9;return Wait(1);
  }
  if(Stage==9){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/StartingSupplies/empty_zh.png"),true,false);Stage=10;return Wait(.5);}
  if(Stage==10){GameLanguage::SetEnglish(true);Stage=11;return Wait(.5);}
  if(Stage==11){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/StartingSupplies/empty_en.png"),true,false);Stage=12;return Wait(.5);}
  GameLanguage::SetEnglish(OldEnglish);return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartingSuppliesTest,"Game.Equipment.StartingSupplies",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStartingSuppliesTest::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(3));AddCommand(new FStartingSuppliesCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
