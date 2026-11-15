#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "IntroPresentation.h"
#include "Audio/BackgroundMusic.h"
#include "Localization/GameLanguage.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/RecordingLibrary.h"
#include "Camcorder/RecordingTabletComponent.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Widgets/Input/SButton.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

namespace
{
 TSharedPtr<SWidget> OpeningFindType(TSharedRef<SWidget> Widget,const TCHAR* Type)
 {
  if(!Widget->GetVisibility().IsVisible())return nullptr;
  if(Widget->GetTypeAsString()==Type)return Widget;
  auto* Children=Widget->GetChildren();
  for(int32 I=0;I<Children->Num();++I)if(auto Found=OpeningFindType(Children->GetChildAt(I),Type))return Found;
  return nullptr;
 }
 bool OpeningHasText(TSharedRef<SWidget> Widget,const FString& Text)
 {
  if(!Widget->GetVisibility().IsVisible())return false;
  if(Widget->GetTypeAsString().Contains(TEXT("TextBlock"))&&StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString()==Text)return true;
  auto* Children=Widget->GetChildren();
  for(int32 I=0;I<Children->Num();++I)if(OpeningHasText(Children->GetChildAt(I),Text))return true;
  return false;
 }
 TSharedPtr<SButton> OpeningButton(TSharedRef<SWidget> Widget,const FString& Text)
 {
  if(!Widget->GetVisibility().IsVisible())return nullptr;
  if(Widget->GetTypeAsString().Contains(TEXT("Button"))&&OpeningHasText(Widget,Text))return StaticCastSharedRef<SButton>(Widget);
  auto* Children=Widget->GetChildren();
  for(int32 I=0;I<Children->Num();++I)if(auto Found=OpeningButton(Children->GetChildAt(I),Text))return Found;
  return nullptr;
 }
 void OpeningKey(FKey Key,bool Down)
 {
  auto& App=FSlateApplication::Get();const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
  if(Down)App.ProcessKeyDownEvent(Event);else App.ProcessKeyUpEvent(Event);
 }
 void OpeningPointer(TSharedRef<SWindow> Window,FKey Button,bool Down)
 {
  auto& App=FSlateApplication::Get();const FVector2D Pos=App.GetCursorPos();TSet<FKey> Buttons;if(Down)Buttons.Add(Button);
  const FPointerEvent Event(0,Pos,Pos,Buttons,Button,0,FModifierKeysState());
  if(Down)App.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Event);else App.ProcessMouseButtonUpEvent(Event);
 }
 void OpeningClick(TSharedRef<SWindow> Window)
 {
  OpeningPointer(Window,EKeys::LeftMouseButton,true);OpeningPointer(Window,EKeys::LeftMouseButton,false);
 }
 class FOpeningFlowCheck final:public IAutomationLatentCommand
 {
  FAutomationTestBase* Test;
  bool OldEnglish,OldMessages,OldMovable=false;
  int32 Step=0,Page=-1,CapturedPage=0,Frame=0,IntroFrame=0,Shots=0,Magazine=0;
  double Began=0,Next=0,CountdownStarted=0,NextFrame=0;
  float RestartMusicTime=0;
  float FrozenTime=0,MaxFrozenTimeDrift=0,LastPageElapsed=0;
  FVector FrozenPosition=FVector::ZeroVector;
  FRotator BeforeLook=FRotator::ZeroRotator;
  TWeakObjectPtr<APawn> OldPawn;
  TWeakObjectPtr<ATacticalWeapon> Weapon;
  TSharedPtr<SIntroPresentation> Intro;
  TSharedPtr<SWidget> Capture;
  FString Folder;
  void Shot(const FString& Name)
  {
   TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
   if(Capture&&FSlateApplication::Get().TakeScreenshot(Capture.ToSharedRef(),Pixels,Size))
   {
    TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
    Test->TestTrue(TEXT("Save actual opening viewport"),FFileHelper::SaveArrayToFile(PNG,*(Folder/(Name+TEXT(".png")))));
   }
   else Test->AddError(TEXT("Cannot capture opening viewport"));
  }
  void ReleaseKeys(TSharedRef<SWindow> Window)
  {
   for(FKey Key:{EKeys::W,EKeys::SpaceBar,EKeys::I,EKeys::F,EKeys::One,EKeys::R})OpeningKey(Key,false);
   OpeningPointer(Window,EKeys::LeftMouseButton,false);OpeningPointer(Window,EKeys::RightMouseButton,false);
  }
 public:
  explicit FOpeningFlowCheck(FAutomationTestBase* InTest):Test(InTest),OldEnglish(GameLanguage::IsEnglish()),OldMessages(GAreScreenMessagesEnabled)
  {
   Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/OpeningQA"));
   IFileManager::Get().MakeDirectory(*Folder,true);GAreScreenMessagesEnabled=false;
  }
  ~FOpeningFlowCheck(){GameLanguage::SetEnglish(OldEnglish);GAreScreenMessagesEnabled=OldMessages;}
  virtual bool Update()override
  {
   const double Now=FPlatformTime::Seconds();if(!Began)Began=Now;
   if(Now-Began>150){Test->AddError(FString::Printf(TEXT("Opening flow timed out at stage %d, page %d"),Step,Page));return true;}
   if(Now<Next)return false;
   UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
   auto* PC=World?World->GetFirstPlayerController():nullptr;
   auto* Equipment=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
   auto Window=World&&World->GetGameViewport()?World->GetGameViewport()->GetWindow():nullptr;
   if(!PC||!PC->GetPawn()||!Equipment||!Window)return false;
   Capture=World->GetGameViewport()->GetGameViewportWidget();
   auto Button=[&](const TCHAR* Source){return OpeningButton(Window.ToSharedRef(),GameLanguage::Display(Source));};
   auto Wait=[&](double Seconds=.1){++Step;Next=Now+Seconds;return false;};
   auto FreezeCheck=[&]
   {
    MaxFrozenTimeDrift=FMath::Max(MaxFrozenTimeDrift,FMath::Abs(World->GetTimeSeconds()-FrozenTime));
    Test->TestTrue(TEXT("Opening keeps the world paused"),World->IsPaused());
   };
   switch(Step)
   {
   case 0:
    GameLanguage::SetEnglish(true);OldPawn=PC->GetPawn();
    UGameplayStatics::OpenLevel(World,FName(*UGameplayStatics::GetCurrentLevelName(World,true)),true,TEXT("ShowTitle"));return Wait(.1);
   case 1:
    if(PC->GetPawn()==OldPawn.Get()||!Button(TEXT("开始游戏")))return false;
    FSlateApplication::Get().ProcessWindowActivatedEvent(FWindowActivateEvent(FWindowActivateEvent::EA_Activate,Window.ToSharedRef()));
    Button(TEXT("开始游戏"))->SimulateClick();FrozenTime=World->GetTimeSeconds();FrozenPosition=PC->GetPawn()->GetActorLocation();return Wait(.08);
   case 2:
   {
    if(!Intro)
    {
     auto Found=OpeningFindType(Window.ToSharedRef(),TEXT("SIntroPresentation"));if(!Found)return false;
     Intro=StaticCastSharedPtr<SIntroPresentation>(Found);
     Test->TestTrue(TEXT("Main Start shows Intro before tutorial"),!Button(TEXT("准备好了")));
    }
    if(Intro->IsFinished()){Intro.Reset();return Wait(.1);}
    const int32 Current=Intro->GetPageIndex();
    if(Current!=Page)
    {
     if(Page==0||Page==1||Page==3)
     {
      const float Hold=Page==0?8.f:Page==1?2.f:3.f;
      Test->TestTrue(*FString::Printf(TEXT("Page %d waits for its complete readable hold"),Page),LastPageElapsed>=Hold+.4f);
     }
     Test->TestEqual(TEXT("Intro preserves the supplied order"),Current,Page+1);
     Page=Current;CapturedPage=0;LastPageElapsed=0;
    }
    FreezeCheck();LastPageElapsed=FMath::Max(LastPageElapsed,Intro->GetPageElapsed());
    if(Page==2&&Intro->GetPageElapsed()<.6f)Shot(FString::Printf(TEXT("PaperImpact%02d"),IntroFrame++));
    if(CapturedPage==0&&Intro->GetPageElapsed()>.65f&&!Intro->IsTransitioning())
    {
     Shot(FString::Printf(TEXT("Page%d-en"),Page));GameLanguage::SetEnglish(false);CapturedPage=1;Next=Now+.12;return false;
    }
    if(CapturedPage==1)
    {
     Shot(FString::Printf(TEXT("Page%d-zh"),Page));GameLanguage::SetEnglish(true);CapturedPage=2;Next=Now+.12;return false;
    }
    // Leave the long narration, transition line and one image on their automatic clocks.
    // Advance the others through real Slate mouse events to check the alternate route.
    if(CapturedPage==2&&(Page==2||Page>=4)&&Intro->GetPageElapsed()>1.1f&&!Intro->IsTransitioning())
    {
     FSlateApplication::Get().SetKeyboardFocus(Intro);
     const auto& Geometry=Intro->GetCachedGeometry();
     FSlateApplication::Get().SetCursorPos(Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f));
     OpeningClick(Window.ToSharedRef());CapturedPage=3;
    }
    return false;
   }
   case 3:
    if(!Button(TEXT("准备好了")))return false;
    Test->TestTrue(TEXT("Opening tutorial retains menu BGM"),UBackgroundMusic::For(PC)->GetStage()==EBackgroundMusicStage::Menu);
    Test->TestEqual(TEXT("All five supplied images were presented"),Page,6);FreezeCheck();
    Test->TestTrue(TEXT("Intro does not consume world time"),MaxFrozenTimeDrift<.01f);
    Shot(TEXT("Tutorial-en"));GameLanguage::SetEnglish(false);return Wait(.12);
   case 4:
    Shot(TEXT("Tutorial-zh"));GameLanguage::SetEnglish(true);return Wait(.1);
   case 5:
    if(!Button(TEXT("准备好了")))return false;
    OldMovable=World->bIsCameraMoveableWhenPaused;FrozenTime=World->GetTimeSeconds();FrozenPosition=PC->GetPawn()->GetActorLocation();BeforeLook=PC->GetControlRotation();
    Shots=Equipment->Combat?Equipment->Combat->ShotsFired:0;
    Weapon=Equipment->Combat?Equipment->Combat->ActiveWeapon.Get():nullptr;Magazine=Weapon.IsValid()?Weapon->Magazine:0;
    Button(TEXT("准备好了"))->SimulateClick();CountdownStarted=Now;Frame=0;NextFrame=0;return Wait(.08);
   case 6:
   {
    Test->TestTrue(TEXT("Countdown uses main BGM"),UBackgroundMusic::For(PC)->GetStage()==EBackgroundMusicStage::Gameplay);
    if(Now>=NextFrame){Shot(World->IsPaused()?FString::Printf(TEXT("Countdown%02d"),Frame++):TEXT("Action-en"));NextFrame=Now+.09;}
    const auto Countdown=OpeningFindType(Window.ToSharedRef(),TEXT("SOpeningCountdown"));
    if(!World->IsPaused())
    {
     Test->TestTrue(TEXT("Three countdown seconds elapse before input is restored"),Now-CountdownStarted>=2.85);
     Test->TestTrue(TEXT("Screenshot stalls do not stretch the countdown"),Now-CountdownStarted<4.25);
     Test->TestTrue(TEXT("ACTION is visible when the world unlocks"),Countdown.IsValid());
     Test->TestFalse(TEXT("ACTION restores move input and hidden cursor"),PC->IsMoveInputIgnored()||PC->bShowMouseCursor);
     Test->TestTrue(TEXT("Countdown keeps the view locked"),PC->GetControlRotation().Equals(BeforeLook,.1f));
     Test->TestTrue(TEXT("Blocked action inputs did not move the player"),FVector::Dist(PC->GetPawn()->GetActorLocation(),FrozenPosition)<10.f);
     Test->TestTrue(TEXT("Paused-camera render flag restores its previous value"),bool(World->bIsCameraMoveableWhenPaused)==OldMovable);
     ReleaseKeys(Window.ToSharedRef());return Wait(.16);
    }
    FreezeCheck();
    if(Frame==1)
    {
     Test->TestTrue(TEXT("Countdown is mounted"),Countdown.IsValid());
     for(FKey Key:{EKeys::W,EKeys::SpaceBar,EKeys::I,EKeys::F,EKeys::One,EKeys::R})OpeningKey(Key,true);
     OpeningPointer(Window.ToSharedRef(),EKeys::LeftMouseButton,true);OpeningPointer(Window.ToSharedRef(),EKeys::RightMouseButton,true);
     const FVector2D Pos=FSlateApplication::Get().GetCursorPos();
     FSlateApplication::Get().ProcessMouseMoveEvent(FPointerEvent(0,Pos+FVector2D(80,-30),Pos,TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState()));
     // Do not inject repeatedly if the next frame lands before the screenshot interval.
     ++Frame;
    }
    return false;
   }
   case 7:
    Test->TestFalse(TEXT("Countdown never opens inventory or tablet"),Equipment->bBackpackOpen||(Equipment->Tablet&&Equipment->Tablet->bOpen));
    Test->TestEqual(TEXT("Countdown never fires or buffers a shot"),Equipment->Combat?Equipment->Combat->ShotsFired:0,Shots);
    Test->TestTrue(TEXT("Countdown preserves equipped item"),!Equipment->Combat||Equipment->Combat->ActiveWeapon.Get()==Weapon.Get());
    if(Weapon.IsValid())Test->TestEqual(TEXT("Countdown preserves loaded ammunition"),Weapon->Magazine,Magazine);
    Test->TestFalse(TEXT("Blocked held movement is not replayed at ACTION"),PC->IsInputKeyDown(EKeys::W));
    Test->TestTrue(TEXT("Normal world time starts at ACTION"),World->GetTimeSeconds()>FrozenTime);
    return Wait(.6);
   case 8:
    Test->TestFalse(TEXT("Opening overlay removes itself after ACTION"),OpeningFindType(Window.ToSharedRef(),TEXT("SOpeningCountdown")).IsValid());
    Test->TestFalse(TEXT("Gameplay input fully restored"),PC->IsMoveInputIgnored()||PC->IsLookInputIgnored()||PC->bShowMouseCursor);
    if(!Equipment->RecordingLibrary)return false;
    RestartMusicTime=UBackgroundMusic::For(PC)->GetGameplayPlaybackSeconds();
    Equipment->RecordingLibrary->EndRound(false);return Wait(.4);
   case 9:
    if(!Button(TEXT("重新开始")))return false;
    // Settlement fades the world for one second before switching music.
    Test->TestTrue(TEXT("Main music continues until settlement is shown"),UBackgroundMusic::For(PC)->GetGameplayPlaybackSeconds()>=RestartMusicTime);
    RestartMusicTime=UBackgroundMusic::For(PC)->GetGameplayPlaybackSeconds();
    OldPawn=PC->GetPawn();Button(TEXT("重新开始"))->SimulateClick();return Wait(.2);
   case 10:
    if(PC->GetPawn()==OldPawn.Get()||!Button(TEXT("准备好了")))return false;
    Test->TestTrue(TEXT("Restart tutorial resumes main BGM"),UBackgroundMusic::For(PC)->GetStage()==EBackgroundMusicStage::Gameplay);
    Test->TestTrue(TEXT("Restart carries the previous main music position"),FMath::Abs(FCString::Atof(*UGameplayStatics::ParseOption(World->GetAuthGameMode()->OptionsString,TEXT("MainMusicTime")))-RestartMusicTime)<.1f);
    Test->TestTrue(TEXT("Restart resumes beyond the previous cursor"),UBackgroundMusic::For(PC)->GetGameplayPlaybackSeconds()>=RestartMusicTime-.1f);
    Test->TestTrue(TEXT("Ending restart retains the existing StartRun route"),UGameplayStatics::HasOption(World->GetAuthGameMode()->OptionsString,TEXT("StartRun")));
    Test->TestFalse(TEXT("Restart skips Intro"),OpeningFindType(Window.ToSharedRef(),TEXT("SIntroPresentation")).IsValid());
    Test->TestTrue(TEXT("Restart waits at tutorial"),World->IsPaused());Shot(TEXT("Restart-tutorial"));
    GameLanguage::SetEnglish(false);return Wait(.12);
   case 11:
    Button(TEXT("准备好了"))->SimulateClick();CountdownStarted=Now;return Wait(.15);
   case 12:
    if(World->IsPaused())return false;
    Test->TestTrue(TEXT("Restart also uses a three-second countdown"),Now-CountdownStarted>=2.85);
    Shot(TEXT("Action-zh"));return Wait(.12);
   case 13:
    return Wait(.6);
   default:
    Test->TestFalse(TEXT("Restart leaves no opening overlay"),OpeningFindType(Window.ToSharedRef(),TEXT("SOpeningCountdown")).IsValid());
    Test->TestFalse(TEXT("Restart restores normal input"),World->IsPaused()||PC->IsMoveInputIgnored()||PC->IsLookInputIgnored()||PC->bShowMouseCursor);
    return true;
   }
  }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpeningFlow,"Game.Frontend.OpeningFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpeningFlow::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FOpeningFlowCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
