#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Game_Jam_Clean_VersCharacter.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "PatrolEnemy.h"
#include "WorldEvents.h"
#include "Camcorder/RecordingLibrary.h"
#include "Camcorder/RecordingReview.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Camcorder/BackpackInventory.h"
#include "Data/DataTransmitter.h"
#include "Localization/GameLanguage.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

class FDeathPresentationCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Slot,Stage=0;double Started=0,StageTime=0;bool OldEnglish=false;
 TWeakObjectPtr<AGame_Jam_Clean_VersCharacter> OldPlayer;
 TWeakObjectPtr<ATacticalWeapon> DroppedGun;
 FVector EyeStart;bool bMidShot=false,bMessageShot=false;
 static bool HasText(TSharedRef<SWidget> Widget,const FString& Text){
  if(Widget->GetTypeAsString().Contains(TEXT("TextBlock"))&&StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString()==Text)return true;
  auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)if(HasText(Children->GetChildAt(I),Text))return true;return false;
 }
 static TSharedPtr<SButton> FindButton(TSharedRef<SWidget> Widget,const FString& Text){
  if(!Widget->GetVisibility().IsVisible())return nullptr;
  if(Widget->GetTypeAsString().Contains(TEXT("Button"))&&HasText(Widget,Text))return StaticCastSharedRef<SButton>(Widget);
  auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)if(auto Found=FindButton(Children->GetChildAt(I),Text))return Found;return nullptr;
 }
 void Shot(const TCHAR* Name){
  const FString Folder=TEXT("G:/TheTechfiles/ThefinalGameJam/DeathFlow/Runtime");IFileManager::Get().MakeDirectory(*Folder,true);
  FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("Slot%d_%s.png"),Slot,Name),true,false);
 }
 bool Fail(const TCHAR* Message){Test->AddError(Message);GameLanguage::SetEnglish(OldEnglish);return true;}
public:
 FDeathPresentationCheck(FAutomationTestBase* T,int32 S):Test(T),Slot(S){}
 bool Update() override {
  const double Now=FPlatformTime::Seconds();if(!Started){Started=Now;StageTime=Now;OldEnglish=GameLanguage::IsEnglish();}
  if(Now-Started>65)return Fail(TEXT("Death presentation / title flow timeout"));
  UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){W=Context.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  auto* E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
  if(!P||!E||!E->Combat||!E->RecordingLibrary||!E->Inventory||!E->Camcorder)return false;
  auto* L=E->RecordingLibrary.Get();
  if(Stage==0){
   for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
   W->GetSubsystem<UWorldEvents>()->bAutomatic=false;
   GameLanguage::SetEnglish(Slot%2==1);PC->SetControlRotation(FRotator(0,0,0));
   if(Slot==0)E->EquipCamera();else E->Combat->EquipWeapon(E->Inventory->Slots[Slot]);
   Stage=1;StageTime=Now;return false;
  }
  if(Stage==1){
   if(Now-StageTime<2.5)return false;
   if(Slot>0){DroppedGun=E->Combat->ActiveWeapon;if(!DroppedGun.IsValid())return Fail(TEXT("Requested weapon failed to equip"));}
   OldPlayer=P;EyeStart=P->GetFirstPersonCameraComponent()->GetComponentLocation();Shot(TEXT("Before"));
   Stage=2;StageTime=Now;return false;
  }
  if(Stage==2){
   if(Now-StageTime<.2)return false;
   FHitResult Hit(P,P->GetCapsuleComponent(),P->GetActorLocation(),FVector::UpVector);
   W->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,1000,FVector::ForwardVector,PC,PC);
   Stage=3;StageTime=Now;return false;
  }
  if(Stage==3){
   if(!L->bRoundEnded)return false;
   Test->TestNull(TEXT("Hands empty on death"),E->Combat->GetPresentationWeapon());
   Test->TestFalse(TEXT("Camera immediately leaves hands"),E->Camcorder->IsPresenting());
   if(Slot==0){Test->TestTrue(TEXT("Dropped camera simulates physics"),E->Camcorder->Body->IsSimulatingPhysics());Test->TestEqual(TEXT("Dropped camera removed from bag"),E->Inventory->Count(UBackpackInventory::CameraId),0);}
   else if(auto* Gun=DroppedGun.Get()){Test->TestTrue(TEXT("Dropped weapon simulates physics"),Gun->Body->IsSimulatingPhysics());Test->TestFalse(TEXT("Dropped weapon is visible"),Gun->IsHidden());Test->TestEqual(TEXT("Dropped weapon removed from bag"),E->Inventory->Count(Gun->InventoryId),0);}
   if(!P->IsDeathFallComplete()){
    Test->TestFalse(TEXT("No message or settlement before fall finishes"),L->DeathUI.IsValid()||L->SettlementUI.IsValid());
    if(!bMidShot&&Now-StageTime>.45){Shot(TEXT("Falling"));bMidShot=true;}
    return false;
   }
   Test->TestTrue(TEXT("Body physically collapses"),P->GetMesh()->IsSimulatingPhysics());
   Test->TestTrue(TEXT("No owner-visible shoulders or cut body"),P->GetFirstPersonMesh()->bOwnerNoSee);
   Test->TestTrue(TEXT("Eye descends during fall"),P->GetFirstPersonCameraComponent()->GetComponentLocation().Z<EyeStart.Z-30);
   if(L->DeathUI&&!bMessageShot&&L->DeathMessageTime>.55){Shot(TEXT("Message"));bMessageShot=true;}
   if(!L->SettlementUI)return false;
   Test->TestTrue(TEXT("Death message was visible before review"),bMessageShot);
   Test->TestTrue(TEXT("World pauses only after death presentation"),W->IsPaused());
   StaticCastSharedPtr<SRecordingReview>(L->SettlementUI)->ShowSummary();
   Stage=4;StageTime=Now;return false;
  }
  if(Stage==4){
   if(Now-StageTime<.4)return false;
   auto Button=FindButton(L->SettlementUI.ToSharedRef(),GameLanguage::Display(TEXT("返回标题界面")));
   if(!Button)return Fail(TEXT("Localized Return to Title button missing"));
   Test->TestEqual(TEXT("Return to Title exact English translation"),GameLanguage::Translate(TEXT("返回标题界面"),true),FString(TEXT("Return to Title")));
   Test->TestEqual(TEXT("Death message exact English translation"),GameLanguage::Translate(TEXT("您已死亡"),true),FString(TEXT("You died")));
   Shot(TEXT("Summary"));
   GameLanguage::SetEnglish(!GameLanguage::IsEnglish());Stage=5;StageTime=Now;return false;
  }
  if(Stage==5){
   if(Now-StageTime<.5)return false;
   auto Button=FindButton(L->SettlementUI.ToSharedRef(),GameLanguage::Display(TEXT("返回标题界面")));
   if(!Button)return Fail(TEXT("Open summary does not update its button language"));
   Shot(TEXT("SummaryOtherLanguage"));Stage=6;StageTime=Now;return false;
  }
  if(Stage==6){
   if(Now-StageTime<.3)return false;
   auto Button=FindButton(L->SettlementUI.ToSharedRef(),GameLanguage::Display(TEXT("返回标题界面")));
   if(!Button)return Fail(TEXT("Return button disappeared"));
   Button->SimulateClick();Stage=7;StageTime=Now;return false;
  }
  if(Stage==7){
   if(P==OldPlayer.Get()||Now-StageTime<1)return false;
   auto View=W->GetGameViewport()->GetWindow();if(!View)return false;
   auto Button=FindButton(View.ToSharedRef(),GameLanguage::Display(TEXT("开始游戏")));
   if(!Button)return false;
   Test->TestEqual(TEXT("Title returns to fresh living player"),P->Health,100.f);
   Test->TestFalse(TEXT("New run has no previous settlement"),L->bRoundEnded);
   Test->TestEqual(TEXT("New run has no old clips"),L->Clips.Num(),0);
   Shot(TEXT("Title"));Stage=8;StageTime=Now;return false;
  }
  if(Stage==8){
   if(Now-StageTime<.3)return false;
   auto View=W->GetGameViewport()->GetWindow();auto Button=FindButton(View.ToSharedRef(),GameLanguage::Display(TEXT("开始游戏")));
   if(!Button)return Fail(TEXT("Start button missing after return"));Button->SimulateClick();Stage=9;StageTime=Now;return false;
  }
  if(Stage==9){
   if(Now-StageTime<.4)return false;
   Test->TestFalse(TEXT("Start restores unpaused gameplay"),W->IsPaused());
   Test->TestFalse(TEXT("Start restores movement input"),PC->IsMoveInputIgnored());
   Test->TestFalse(TEXT("Start restores look input"),PC->IsLookInputIgnored());
   Test->TestFalse(TEXT("Start restores gameplay cursor"),PC->bShowMouseCursor);
   GameLanguage::SetEnglish(OldEnglish);return true;
  }
  return false;
 }
};
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPlayerDeathPresentation,"Game.Player.DeathPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FPlayerDeathPresentation::GetTests(TArray<FString>& Names,TArray<FString>& Commands)const{for(int32 Slot=0;Slot<4;++Slot){Names.Add(FString::Printf(TEXT("Slot%d"),Slot));Commands.Add(FString::FromInt(Slot));}}
bool FPlayerDeathPresentation::RunTest(const FString& Parameter){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FDeathPresentationCheck(this,FCString::Atoi(*Parameter)));AddCommand(new FEndPlayMapCommand());return true;}
#endif
