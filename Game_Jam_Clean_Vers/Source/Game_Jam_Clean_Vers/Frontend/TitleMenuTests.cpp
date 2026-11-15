#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "GameFrontend.h"
#include "Localization/GameLanguage.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/RecordingTabletComponent.h"
#include "Combat/PatrolEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
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
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
 bool HasChinese(const FString& Text){for(TCHAR C:Text)if((C>=0x3400&&C<=0x9fff)||(C>=0xf900&&C<=0xfaff))return true;return false;}
 TSharedPtr<SWidget> FindTitle(TSharedRef<SWidget> W)
 {
  if(W->GetTypeAsString()==TEXT("SClapperTitle"))return W;
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto Found=FindTitle(C->GetChildAt(I)))return Found;return nullptr;
 }
 bool HasText(TSharedRef<SWidget> W,const FString& Text)
 {
  if(!W->GetVisibility().IsVisible())return false;
  if(W->GetTypeAsString().Contains(TEXT("TextBlock"))&&StaticCastSharedRef<STextBlock>(W)->GetText().ToString()==Text)return true;
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(HasText(C->GetChildAt(I),Text))return true;return false;
 }
 TSharedPtr<SButton> FindButton(TSharedRef<SWidget> W,const FString& Text)
 {
  if(!W->GetVisibility().IsVisible())return nullptr;
  if(W->GetTypeAsString().Contains(TEXT("Button"))&&HasText(W,Text))return StaticCastSharedRef<SButton>(W);
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto Found=FindButton(C->GetChildAt(I),Text))return Found;return nullptr;
 }
 void AuditEnglish(TSharedRef<SWidget> W,FAutomationTestBase* Test,const FString& Page)
 {
  if(!W->GetVisibility().IsVisible())return;
  // Audit the game's text widgets. The surrounding Unreal editor uses its own language.
  if((W->GetTypeAsString()==TEXT("SLocalizedTextBlock")||W->GetTypeAsString()==TEXT("SMenuTextBlock")))
  {const FString Text=StaticCastSharedRef<STextBlock>(W)->GetText().ToString();Test->TestFalse(*(Page+TEXT(" contains no Chinese: ")+Text),HasChinese(Text));}
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)AuditEnglish(C->GetChildAt(I),Test,Page);
 }
 void Key(FKey K)
 {
  auto& App=FSlateApplication::Get();App.ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));App.ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));
 }
 class FTitleFlowCheck final:public IAutomationLatentCommand
 {
  FAutomationTestBase* Test;int32 Step=0,Frame=0;double Began=0,Next=0;
  bool OldEnglish=false,OldScreenMessages=true;TWeakObjectPtr<APawn> Original;TSharedPtr<SWidget> Title,Capture;FString Folder;
  void Shot(const FString& Name)
  {
   TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
   if(Capture&&FSlateApplication::Get().TakeScreenshot(Capture.ToSharedRef(),Pixels,Size))
   {TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);Test->TestTrue(TEXT("Save actual game viewport preview"),FFileHelper::SaveArrayToFile(PNG,*(Folder/(Name+TEXT(".png")))));}
   else Test->AddError(TEXT("Cannot capture game viewport"));
  }
 public:
  explicit FTitleFlowCheck(FAutomationTestBase* T):Test(T)
  {OldEnglish=GameLanguage::IsEnglish();OldScreenMessages=GAreScreenMessagesEnabled;GAreScreenMessagesEnabled=false;Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/TitleQA"));IFileManager::Get().MakeDirectory(*Folder,true);}
  ~FTitleFlowCheck(){GameLanguage::SetEnglish(OldEnglish);GAreScreenMessagesEnabled=OldScreenMessages;}
  virtual bool Update()override
  {
   const double Now=FPlatformTime::Seconds();if(!Began)Began=Now;
   if(Now-Began>100){Test->AddError(FString::Printf(TEXT("Title UI test timed out at stage %d"),Step));return true;}if(Now<Next)return false;
   UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
   auto* PC=W?W->GetFirstPlayerController():nullptr;auto* E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
   auto Window=W&&W->GetGameViewport()?W->GetGameViewport()->GetWindow():nullptr;
   if(!PC||!PC->GetPawn()||!E||!E->Tablet||!Window)return false;
   Capture=W->GetGameViewport()->GetGameViewportWidget();
   auto Click=[&](const TCHAR* Source){auto Button=FindButton(Window.ToSharedRef(),GameLanguage::Display(Source));if(!Button){Test->AddError(FString(TEXT("Title flow button missing: "))+Source);return false;}Button->SimulateClick();return true;};
   auto Wait=[&](float Seconds=.35f){++Step;Next=Now+Seconds;return false;};
   auto Audit=[&](const TCHAR* Page){AuditEnglish(Window.ToSharedRef(),Test,Page);};
   switch(Step)
   {
   case 0:GameLanguage::SetEnglish(true);Original=PC->GetPawn();UGameplayStatics::OpenLevel(W,FName(*UGameplayStatics::GetCurrentLevelName(W,true)),true,TEXT("ShowTitle"));return Wait(.05f);
   case 1:
    if(PC->GetPawn()==Original.Get())return false;
    Title=FindTitle(Window.ToSharedRef());if(!Title)return false;
    Shot(TEXT("Intro00"));return Wait(.05f);
   case 2:
    Shot(FString::Printf(TEXT("Intro%02d"),++Frame));
    if(Frame<20){Next=Now+.05;return false;}
    Audit(TEXT("Title"));Shot(TEXT("Title-en"));
    {const FVector2D Size=Title->GetPaintSpaceGeometry().GetLocalSize();Test->TestTrue(TEXT("Full board keeps its original aspect ratio"),FMath::IsNearlyEqual(Size.X/Size.Y,1043.f/745,.01f));}
    Test->TestFalse(TEXT("Removed subtitle stays absent"),HasText(Title.ToSharedRef(),TEXT("Observe. Record. Bring back your story.")));
    if(!Click(TEXT("设置")))return true;return Wait();
   case 3:Audit(TEXT("Settings"));Shot(TEXT("Settings-en"));if(!Click(TEXT("性能与画质")))return true;return Wait();
   case 4:Audit(TEXT("Graphics"));Shot(TEXT("Graphics-en"));if(!Click(TEXT("语言 / Language")))return true;return Wait();
   case 5:
    Audit(TEXT("Language"));Test->TestTrue(TEXT("Chinese language choice is translated in English"),HasText(Window.ToSharedRef(),TEXT("    Chinese")));Shot(TEXT("Language-en"));
    {auto Button=FindButton(Window.ToSharedRef(),TEXT("    Chinese"));if(!Button)return true;Button->SimulateClick();}return Wait();
   case 6:
    Test->TestFalse(TEXT("Language selection changes preference"),GameLanguage::IsEnglish());Shot(TEXT("Language-zh"));Key(EKeys::Escape);return Wait();
   case 7:Key(EKeys::Escape);return Wait();
   case 8:
    Test->TestTrue(TEXT("Returning from settings retains the same title and animation instance"),FindTitle(Window.ToSharedRef())==Title);
    Test->TestTrue(TEXT("Existing menu updates to Chinese"),HasText(Title.ToSharedRef(),TEXT("开始游戏")));Shot(TEXT("Title-zh"));GameLanguage::SetEnglish(true);return Wait();
   case 9:if(!Click(TEXT("使用教程")))return true;return Wait();
   case 10:Audit(TEXT("Tutorial"));Shot(TEXT("Tutorial-en"));if(!Click(TEXT("返回")))return true;return Wait();
   case 11:
    Test->TestTrue(TEXT("Returning from tutorial retains title instance"),FindTitle(Window.ToSharedRef())==Title);
    if(!Click(TEXT("游玩记录")))return true;return Wait();
   case 12:Audit(TEXT("Run history"));Shot(TEXT("History-en"));if(!Click(TEXT("返回")))return true;return Wait();
   case 13:
    Test->TestTrue(TEXT("Returning from history retains title instance"),FindTitle(Window.ToSharedRef())==Title);
    {auto Start=FindButton(Title.ToSharedRef(),GameLanguage::Display(TEXT("开始游戏")));Test->TestTrue(TEXT("Keyboard focus returns to Start"),Start&&Start->HasKeyboardFocus());}
    Key(EKeys::Enter);return Wait();
   case 14:
    Test->TestTrue(TEXT("Start keeps world paused for the approved intro"),W->IsPaused()&&PC->IsMoveInputIgnored());
    Test->TestTrue(TEXT("Start shows intro continuation affordance"),HasText(Window.ToSharedRef(),GameLanguage::Display(TEXT("单击继续"))));
    Audit(TEXT("Intro"));Shot(TEXT("Intro-start-en"));return true;
   default:return true;
   }
  }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTitleFlow,"Game.Frontend.TitlePresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTitleFlow::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FTitleFlowCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
