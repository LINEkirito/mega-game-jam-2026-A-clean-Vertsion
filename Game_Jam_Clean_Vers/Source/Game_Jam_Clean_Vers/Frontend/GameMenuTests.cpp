#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Localization/GameLanguage.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/RecordingLibrary.h"
#include "Combat/PatrolEnemy.h"

namespace
{
 bool HasMenuText(TSharedRef<SWidget> W,const FString& Text)
 {
  if(!W->GetVisibility().IsVisible())return false;
  if(W->GetTypeAsString().Contains(TEXT("TextBlock"))&&StaticCastSharedRef<STextBlock>(W)->GetText().ToString()==Text)return true;
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(HasMenuText(C->GetChildAt(I),Text))return true;return false;
 }
 TSharedPtr<SButton> MenuButton(TSharedRef<SWidget> W,const FString& Text)
 {
  if(!W->GetVisibility().IsVisible())return nullptr;
  if(W->GetTypeAsString().Contains(TEXT("Button"))&&HasMenuText(W,Text))return StaticCastSharedRef<SButton>(W);
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto B=MenuButton(C->GetChildAt(I),Text))return B;return nullptr;
 }
 void MenuKey(FKey Key)
 {
  auto& App=FSlateApplication::Get();
  App.ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
  App.ProcessKeyUpEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
 }
 class FGameMenuCheck : public IAutomationLatentCommand
 {
  FAutomationTestBase* Test;int32 Step=0;double Next=0,Started=0;float WorldTime=0;
  bool OldEnglish=false;FVector Spawn;FRotator Facing;TWeakObjectPtr<AGame_Jam_Clean_VersCharacter> OldPlayer;
  TArray<FBackpackEntry> Items;TArray<FName> Slots;
 public:
  explicit FGameMenuCheck(FAutomationTestBase* T):Test(T){}
  bool Update() override
  {
   const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
   if(Now-Started>80){Test->AddError(TEXT("Menu test timed out"));GameLanguage::SetEnglish(OldEnglish);return true;}
   if(Now<Next)return false;
   UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
   auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
   if(!P||!W->GetGameViewport()||!W->GetGameViewport()->GetWindow())return false;
   auto Window=W->GetGameViewport()->GetWindow();auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
   if(!E||!E->Inventory||!E->RecordingLibrary){Test->AddError(TEXT("Missing inventory/library"));return true;}
   auto Click=[&](const TCHAR* Text){auto B=MenuButton(Window.ToSharedRef(),GameLanguage::Display(Text));if(!B){Test->AddError(FString(TEXT("Missing button: "))+Text);return false;}B->SimulateClick();return true;};
   auto Wait=[&](double Seconds=.3){++Step;Next=Now+Seconds;return false;};
   auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FString(TEXT("G:/TheTechfiles/ThefinalGameJam/GameMenu/"))+Name+TEXT(".png"),true,false);};
   switch(Step)
   {
   case 0:
    OldEnglish=GameLanguage::IsEnglish();GameLanguage::SetEnglish(false);
    for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
    PC->SetPause(false);PC->bShowMouseCursor=false;PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();
    E->SetBackpackOpen(false);PC->SetInputMode(FInputModeGameOnly());FSlateApplication::Get().SetAllUserFocusToGameViewport();
    Test->TestTrue(TEXT("Capture spawn through return"),P->ReturnToSpawn());Spawn=P->GetActorLocation();
    P->Health=73;Items=E->Inventory->Items;Slots=E->Inventory->Slots;
    P->SetActorLocation(Spawn+FVector(600,0,350),false,nullptr,ETeleportType::TeleportPhysics);
    P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);PC->SetControlRotation(FRotator(-10,81,0));Facing=PC->GetControlRotation();
    MenuKey(EKeys::Escape);WorldTime=W->GetTimeSeconds();return Wait(.8);
   case 1:
    Test->TestFalse(TEXT("ESC menu does not pause world"),W->IsPaused());
    Test->TestTrue(TEXT("World time advances in menu"),W->GetTimeSeconds()>WorldTime+.1f);
    Test->TestTrue(TEXT("Menu locks player controls"),PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored()&&PC->bShowMouseCursor);
    Test->TestTrue(TEXT("Chinese menu visible"),HasMenuText(Window.ToSharedRef(),TEXT("游戏菜单")));Shot(TEXT("Menu-zh"));return Wait();
   case 2:
    if(!Click(TEXT("设置")))return true;return Wait();
   case 3:
    if(!Click(TEXT("语言 / Language")))return true;return Wait();
   case 4:
    {auto B=MenuButton(Window.ToSharedRef(),TEXT("    English"));if(!B){Test->AddError(TEXT("English choice missing"));return true;}B->SimulateClick();}return Wait();
   case 5:
    Test->TestTrue(TEXT("Language selection switches immediately"),GameLanguage::IsEnglish());
    Test->TestFalse(TEXT("Language menu does not pause"),W->IsPaused());Shot(TEXT("Language-en"));MenuKey(EKeys::Escape);return Wait();
   case 6:
    Test->TestTrue(TEXT("Language returns to settings"),HasMenuText(Window.ToSharedRef(),TEXT("Settings")));Shot(TEXT("Settings-en"));return Wait();
   case 7:
    MenuKey(EKeys::Escape);return Wait();
   case 8:
    Test->TestTrue(TEXT("Settings returns to menu"),HasMenuText(Window.ToSharedRef(),TEXT("Game Menu")));Shot(TEXT("Menu-en"));return Wait();
   case 9:
    if(!Click(TEXT("脱离卡死")))return true;
    Test->TestTrue(TEXT("Unstuck is immediate"),P->GetActorLocation().Equals(Spawn,3.f));
    Test->TestEqual(TEXT("Unstuck preserves health"),P->Health,73.f);
    Test->TestTrue(TEXT("Unstuck preserves view"),PC->GetControlRotation().Equals(Facing,.01f));
    Test->TestEqual(TEXT("Inventory entry count preserved"),E->Inventory->Items.Num(),Items.Num());
    for(const auto& Item:Items)Test->TestEqual(TEXT("Inventory quantity preserved"),E->Inventory->Count(Item.Id),Item.Count);
    Test->TestTrue(TEXT("Quick slots preserved"),E->Inventory->Slots==Slots);
    Test->TestFalse(TEXT("Controls restored after unstuck"),PC->IsMoveInputIgnored()||PC->IsLookInputIgnored()||PC->bShowMouseCursor||W->IsPaused());return Wait();
   case 10:
    MenuKey(EKeys::Escape);return Wait();
   case 11:
    if(!Click(TEXT("继续游戏")))return true;
    Test->TestFalse(TEXT("Resume restores input"),PC->IsMoveInputIgnored()||PC->IsLookInputIgnored()||PC->bShowMouseCursor);MenuKey(EKeys::Escape);return Wait();
   case 12:
    MenuKey(EKeys::Escape);Test->TestFalse(TEXT("ESC also closes menu"),PC->bShowMouseCursor);MenuKey(EKeys::Escape);return Wait();
   case 13:
    OldPlayer=P;if(!Click(TEXT("返回标题界面")))return true;return Wait(2);
   case 14:
    if(P==OldPlayer.Get())return false;
    if(!MenuButton(Window.ToSharedRef(),GameLanguage::Display(TEXT("开始游戏"))))return false;
    Test->TestTrue(TEXT("Title still pauses fresh run"),W->IsPaused());
    Test->TestEqual(TEXT("Fresh title player"),P->Health,100.f);
    if(!Click(TEXT("设置")))return true;return Wait();
   case 15:
    Test->TestTrue(TEXT("Title settings remain available"),HasMenuText(Window.ToSharedRef(),TEXT("Settings")));
    MenuKey(EKeys::Escape);return Wait();
   case 16:
    if(!Click(TEXT("开始游戏")))return true;return Wait();
   case 17:
    Test->TestFalse(TEXT("Start after title restores gameplay"),W->IsPaused()||PC->bShowMouseCursor||PC->IsMoveInputIgnored());
    MenuKey(EKeys::Escape);P->Health=0;return Wait(.3);
   default:
    Test->TestFalse(TEXT("Death closes menu"),HasMenuText(Window.ToSharedRef(),GameLanguage::Display(TEXT("游戏菜单"))));
    Test->TestFalse(TEXT("Death prevents unstuck"),P->ReturnToSpawn());
    GameLanguage::SetEnglish(OldEnglish);return true;
   }
  }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameMenuTest,"Game.Frontend.GameMenu",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameMenuTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FGameMenuCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
