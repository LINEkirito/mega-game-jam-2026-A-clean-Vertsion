#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "RecordingReview.h"
#include "RecordingLibrary.h"
#include "CamcorderVideoWriter.h"
#include "Localization/GameLanguage.h"
#include "Combat/PatrolEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

namespace
{
 TSharedPtr<SScrollBox> FindOverviewScroll(TSharedRef<SWidget> W)
 {
  if(W->GetTypeAsString()==TEXT("SScrollBox"))return StaticCastSharedRef<SScrollBox>(W);
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto Found=FindOverviewScroll(C->GetChildAt(I)))return Found;return nullptr;
 }
 class FEndingMotionCheck final:public IAutomationLatentCommand
 {
  FAutomationTestBase* Test;TStrongObjectPtr<URecordingLibrary> Library;TSharedPtr<SRecordingReview> Review;
  TWeakObjectPtr<UGameViewportClient> Viewport;TWeakObjectPtr<APlayerController> Player;
  bool OldEnglish,OldMessages,OldPause=false,OldCursor=false,FullShot=false;
  int32 Step=0,Frame=0,Gallery=0;double Began=0,Next=0,CompletedOverview=0;FString Folder;
  const float Scores[6]={0,100,200,300,-7.5f,450};
  void Shot(const FString& Name)
  {
   TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
   auto Capture=Viewport.IsValid()?Viewport->GetGameViewportWidget():nullptr;
   if(Capture&&FSlateApplication::Get().TakeScreenshot(Capture.ToSharedRef(),Pixels,Size))
   {TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);Test->TestTrue(TEXT("Save live ending viewport"),FFileHelper::SaveArrayToFile(PNG,*(Folder/(Name+TEXT(".png")))));}
   else Test->AddError(TEXT("Cannot capture ending viewport"));
  }
 public:
  explicit FEndingMotionCheck(FAutomationTestBase* T):Test(T),OldEnglish(GameLanguage::IsEnglish()),OldMessages(GAreScreenMessagesEnabled)
  {Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/EndingMotionQA"));IFileManager::Get().MakeDirectory(*Folder,true);}
  ~FEndingMotionCheck()
  {
   if(Viewport.IsValid()&&Review)Viewport->RemoveViewportWidgetContent(Review.ToSharedRef());
   if(Player.IsValid()){Player->SetPause(OldPause);Player->bShowMouseCursor=OldCursor;}
   GameLanguage::SetEnglish(OldEnglish);GAreScreenMessagesEnabled=OldMessages;
  }
  virtual bool Update()override
  {
   const double Now=FPlatformTime::Seconds();if(!Began)Began=Now;
   if(Now-Began>100){Test->AddError(FString::Printf(TEXT("Ending motion QA timed out at step %d"),Step));return true;}if(Now<Next)return false;
   auto Wait=[&](float Seconds=.15f){++Step;Next=Now+Seconds;return false;};
   if(Step==0)
   {
    UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
    auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC||!W->GetGameViewport())return false;
    Player=PC;Viewport=W->GetGameViewport();OldPause=PC->IsPaused();OldCursor=PC->bShowMouseCursor;
    for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
    PC->SetPause(true);PC->bShowMouseCursor=true;GAreScreenMessagesEnabled=false;GameLanguage::SetEnglish(true);
    const FString Video=Folder/TEXT("Fixture.avi");FCamcorderVideoWriter Writer;
    if(!Writer.Open(Video,320,180,15)){Test->AddError(TEXT("Cannot create ending video fixture"));return true;}
    TArray<FColor> Pixels;Pixels.SetNum(320*180);
    for(int32 Y=0;Y<180;++Y)for(int32 X=0;X<320;++X)Pixels[Y*320+X]=FColor(uint8(35+X*.35f),uint8(35+Y*.5f),uint8(80+(X/40+Y/40)%2*40),255);
    Writer.Enqueue(MoveTemp(Pixels),0);Writer.Finish(150);
    Library.Reset(NewObject<URecordingLibrary>(PC));Library->RoundScore=218;
    for(int32 I=0;I<13;++I){FRecordedClip Clip;Clip.Path=Video;Clip.Seconds=10;Clip.Score=I?16:26;Library->Clips.Add(Clip);}
    Review=SNew(SRecordingReview).Library(Library.Get());Viewport->AddViewportWidgetContent(Review.ToSharedRef(),1500);
    return Wait(.02f);
   }
   switch(Step)
   {
   case 1:
    Shot(FString::Printf(TEXT("Overview%02d"),Frame++));
    if(Review->GetThumbnailCount()==13)
    {if(!CompletedOverview)CompletedOverview=Now;if(Now-CompletedOverview>.65){Shot(TEXT("Overview-final-en"));
      if(auto Scroll=FindOverviewScroll(Review.ToSharedRef()))Test->TestTrue(TEXT("Overview reaches final row after layout"),Scroll->GetScrollOffset()>=Scroll->GetScrollOffsetOfEnd()-2);
      Frame=0;return Wait(.02f);}}
    Next=Now+.07;return false;
   case 2:
    if(Review->GetPhase()!=ERecordingReviewPhase::Intertitle)return false;
    if(Review->GetPhaseElapsed()<.9f){Shot(FString::Printf(TEXT("Typing%02d"),Frame++));Next=Now+.07;return false;}
    Shot(TEXT("Intertitle-full-en"));GameLanguage::SetEnglish(false);return Wait();
   case 3:Shot(TEXT("Intertitle-full-zh"));GameLanguage::SetEnglish(true);return Wait();
   case 4:
    if(Review->GetPhase()==ERecordingReviewPhase::Intertitle&&Review->GetPhaseElapsed()<3.7f)return false;
    Shot(TEXT("Intertitle-hold-en"));Review->ShowSummary();Frame=0;return Wait(.02f);
   case 5:
    Shot(FString::Printf(TEXT("Summary%02d"),Frame++));
    if(Review->GetPhaseElapsed()<3.1f){Next=Now+.07;return false;}
    Test->TestEqual(TEXT("Live score ends at the stored result"),Review->GetAnimatedFinalScore(),218.f);
    Test->TestTrue(TEXT("Live rail matches the final result"),FMath::IsNearlyEqual(Review->GetAwardProgress(),218.f/300));
    Shot(TEXT("Final218-en"));GameLanguage::SetEnglish(false);return Wait();
   case 6:Shot(TEXT("Final218-zh"));return Wait();
   case 7:
    if(Gallery==6)return true;
    Library->RoundScore=Scores[Gallery];GameLanguage::SetEnglish(true);Review->ShowSummary();
    for(int32 I=0;I<124;++I)Review->Tick(FGeometry(),0,.025f);
    return Wait();
   case 8:Shot(FString::Printf(TEXT("Gallery%d-en"),Gallery));GameLanguage::SetEnglish(false);return Wait();
   case 9:Shot(FString::Printf(TEXT("Gallery%d-zh"),Gallery++));Step=7;Next=Now+.1;return false;
   default:return true;
   }
  }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingMotionVisual,"Game.Recording.EndingMotionVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEndingMotionVisual::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FEndingMotionCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
