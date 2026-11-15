#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "RecordingLibrary.h"
#include "CamcorderEquipmentComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Combat/PatrolEnemy.h"
#include "Combat/WorldEvents.h"
#include "Localization/GameLanguage.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
namespace
{
 bool EndingText(TSharedRef<SWidget> W,const FString& Text)
 {
  if(W->GetTypeAsString().Contains(TEXT("TextBlock"))&&StaticCastSharedRef<STextBlock>(W)->GetText().ToString()==Text)return true;
  auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)if(EndingText(Children->GetChildAt(I),Text))return true;return false;
 }
 TSharedPtr<SButton> EndingButton(TSharedRef<SWidget> W,const FString& Text)
 {
  if(!W->GetVisibility().IsVisible())return nullptr;
  if(W->GetTypeAsString().Contains(TEXT("Button"))&&EndingText(W,Text))return StaticCastSharedRef<SButton>(W);
  auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)if(auto Button=EndingButton(Children->GetChildAt(I),Text))return Button;return nullptr;
 }
 bool HasEndingFlowWidget(TSharedRef<SWidget> W,const TCHAR* Type)
 {
  if(!W->GetVisibility().IsVisible())return false;
  if(W->GetTypeAsString()==Type)return true;
  auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)if(HasEndingFlowWidget(Children->GetChildAt(I),Type))return true;return false;
 }
 class FEndingNavigationCheck final:public IAutomationLatentCommand
 {
  FAutomationTestBase* Test;int32 Stage=0;double Began=0,Next=0;bool OldEnglish=false;
  TWeakObjectPtr<APawn> OldPawn;
  void Key(const FKey K){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));}
 public:
  explicit FEndingNavigationCheck(FAutomationTestBase* InTest):Test(InTest){}
  bool Update()override
  {
   const double Now=FPlatformTime::Seconds();if(!Began){Began=Now;OldEnglish=GameLanguage::IsEnglish();}
   if(Now-Began>90){Test->AddError(FString::Printf(TEXT("Ending navigation timed out at stage %d"),Stage));GameLanguage::SetEnglish(OldEnglish);return true;}
   if(Now<Next)return false;
   UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
   auto* PC=W?W->GetFirstPlayerController():nullptr;auto* E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
   auto Window=W&&W->GetGameViewport()?W->GetGameViewport()->GetWindow():nullptr;
   if(!PC||!PC->GetPawn()||!E||!E->RecordingLibrary||!Window)return false;
   auto* L=E->RecordingLibrary.Get();
   auto Button=[&](const TCHAR* Source){return EndingButton(Window.ToSharedRef(),GameLanguage::Display(Source));};
   auto Wait=[&](double Seconds=.4){++Stage;Next=Now+Seconds;return false;};
   switch(Stage)
   {
    case 0:
     GameLanguage::SetEnglish(false);
     for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
     W->GetSubsystem<UWorldEvents>()->bAutomatic=false;OldPawn=PC->GetPawn();L->EndRound(false);return Wait();
    case 1:
     if(auto B=Button(TEXT("重新开始"))){B->SimulateClick();return Wait(1);}return false;
    case 2:
     if(PC->GetPawn()==OldPawn.Get())return false;
     if(!Button(TEXT("准备好了")))return false;
     Test->TestTrue(TEXT("Restart passes direct-start menu option"),UGameplayStatics::HasOption(W->GetAuthGameMode()->OptionsString,TEXT("StartRun")));
     Test->TestFalse(TEXT("Restart skips the story intro"),HasEndingFlowWidget(Window.ToSharedRef(),TEXT("SIntroPresentation")));
     Test->TestTrue(TEXT("Restart opens the tutorial before gameplay"),W->IsPaused()&&PC->bShowMouseCursor&&PC->IsMoveInputIgnored());
     Test->TestFalse(TEXT("Restart resets ended state before tutorial"),L->bRoundEnded);Test->TestEqual(TEXT("Restart has no old clips before tutorial"),L->Clips.Num(),0);
     Button(TEXT("准备好了"))->SimulateClick();return Wait(.25);
    case 3:
     Test->TestTrue(TEXT("Ready opens the three-second countdown"),HasEndingFlowWidget(Window.ToSharedRef(),TEXT("SOpeningCountdown")));
     Test->TestTrue(TEXT("Restart countdown holds world and movement"),W->IsPaused()&&PC->IsMoveInputIgnored());
     Test->TestFalse(TEXT("Restart countdown does not replay intro"),HasEndingFlowWidget(Window.ToSharedRef(),TEXT("SIntroPresentation")));
     return Wait(3.8);
    case 4:
     Test->TestFalse(TEXT("Restart restores active gameplay"),W->IsPaused()||PC->bShowMouseCursor||PC->IsMoveInputIgnored()||PC->IsLookInputIgnored());
     Test->TestFalse(TEXT("Countdown clears after ACTION"),HasEndingFlowWidget(Window.ToSharedRef(),TEXT("SOpeningCountdown")));
     Test->TestFalse(TEXT("Restart resets ended state"),L->bRoundEnded);Test->TestEqual(TEXT("Restart has no old clips"),L->Clips.Num(),0);
     for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
     W->GetSubsystem<UWorldEvents>()->bAutomatic=false;L->EndRound(false);return Wait();
    case 5:
     if(!Button(TEXT("返回主菜单")))return false;
     FSlateApplication::Get().ProcessWindowActivatedEvent(FWindowActivateEvent(FWindowActivateEvent::EA_Activate,Window.ToSharedRef()));
     Key(EKeys::F10);Test->TestFalse(TEXT("F10 no longer opens settings"),Button(TEXT("语言 / Language")).IsValid());
     Key(EKeys::Escape);return Wait();
    case 6:
     if(auto B=Button(TEXT("语言 / Language"))){B->SimulateClick();return Wait();}return false;
    case 7:
     if(auto B=EndingButton(Window.ToSharedRef(),TEXT("    English"))){B->SimulateClick();return Wait();}return false;
    case 8:
     Test->TestTrue(TEXT("Existing settings changes language"),GameLanguage::IsEnglish());Key(EKeys::Escape);return Wait();
    case 9:
     Key(EKeys::Escape);return Wait();
    case 10:
     Test->TestTrue(TEXT("Returned final panel immediately shows English"),Button(TEXT("返回主菜单")).IsValid());
     Test->TestTrue(TEXT("Settings returns focus to ending"),PC->bShowMouseCursor&&PC->IsMoveInputIgnored());
     {bool InEnding=false;for(auto Focus=FSlateApplication::Get().GetKeyboardFocusedWidget();Focus;Focus=Focus->GetParentWidget())if(Focus->GetTypeAsString()==TEXT("SRecordingReview"))InEnding=true;
      Test->TestTrue(TEXT("Keyboard focus returns to a mounted ending widget"),InEnding);}
     if(auto B=Button(TEXT("返回主菜单"))){OldPawn=PC->GetPawn();B->SimulateClick();return Wait(1);}return false;
    case 11:
     if(PC->GetPawn()==OldPawn.Get()||!Button(TEXT("开始游戏")))return false;
     Test->TestTrue(TEXT("Back to Menu opens title"),W->IsPaused());
     Button(TEXT("开始游戏"))->SimulateClick();return Wait();
    default:
     Test->TestTrue(TEXT("Starting again from title includes the intro"),HasEndingFlowWidget(Window.ToSharedRef(),TEXT("SIntroPresentation")));
     Test->TestTrue(TEXT("Title intro keeps gameplay held"),W->IsPaused()&&PC->IsMoveInputIgnored());
     GameLanguage::SetEnglish(OldEnglish);return true;
   }
  }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingNavigation,"Game.Recording.EndingNavigation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEndingNavigation::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FEndingNavigationCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
