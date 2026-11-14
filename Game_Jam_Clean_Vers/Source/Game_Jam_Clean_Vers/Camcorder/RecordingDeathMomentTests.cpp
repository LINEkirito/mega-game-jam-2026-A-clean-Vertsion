#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "RecordingScoreRules.h"
#include "Misc/AutomationTest.h"
#include "Localization/GameLanguage.h"
namespace {
FFilmObservation Person(EFilmSubject Kind=EFilmSubject::Gang,const TCHAR* Key=TEXT("NPC"))
{FFilmObservation O;O.Key=Key;O.Kind=Kind;O.bHasLifeState=true;O.bAlive=true;O.Multiplier=2;return O;}
int32 DeathPoints(const FRecordingScoreRules& R){int32 N=0;for(const auto& M:R.Moments)if(M.Category==TEXT("死亡瞬间"))N+=M.Points;return N;}
void Samples(FRecordingScoreRules& R,FFilmObservation O,int32 Start,int32 End)
{for(int32 I=Start;I<=End;++I)R.Sample(I*.25,{O});}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingDeathMomentRules,"Game.Recording.DeathMomentRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingDeathMomentRules::RunTest(const FString&)
{
 for(auto Kind:{EFilmSubject::Gang,EFilmSubject::Civilian,EFilmSubject::Police})
 {
  FRecordingScoreRules R;auto O=Person(Kind);R.Begin();Samples(R,O,0,4);O.bAlive=false;Samples(R,O,5,7);
  TestEqual(TEXT("1.75 seconds cannot qualify"),DeathPoints(R),0);
  R.Sample(2,{O,O});TestEqual(TEXT("Exactly two seconds across death; multiplier once"),DeathPoints(R),20);
  Samples(R,O,9,16);R.Sample(4.25,{});Samples(R,O,18,30);
  TestEqual(TEXT("Dead body and re-entry never repeat award"),DeathPoints(R),20);
  R.Commit(7.5);Samples(R,O,0,12);TestEqual(TEXT("Next clip cannot borrow alive evidence"),DeathPoints(R),0);
 }
 auto Alive=Person(),Dead=Alive;Dead.bAlive=false;
 FRecordingScoreRules Corpse;Corpse.Begin();Samples(Corpse,Dead,0,20);TestEqual(TEXT("Corpse only"),DeathPoints(Corpse),0);
 FRecordingScoreRules Living;Living.Begin();Samples(Living,Alive,0,20);TestEqual(TEXT("Living only"),DeathPoints(Living),0);
 FRecordingScoreRules Reverse;Reverse.Begin();Samples(Reverse,Dead,0,4);Samples(Reverse,Alive,5,12);TestEqual(TEXT("Dead to living is not a death transition"),DeathPoints(Reverse),0);
 FRecordingScoreRules Hidden;Hidden.Begin();Samples(Hidden,Alive,0,8);Hidden.Sample(2.25,{});Samples(Hidden,Dead,10,24);TestEqual(TEXT("Off-screen death breaks continuity"),DeathPoints(Hidden),0);
 FRecordingScoreRules Gap;Gap.Begin();Samples(Gap,Alive,0,8);Samples(Gap,Dead,12,24);TestEqual(TEXT("Sampling gap cannot bridge death"),DeathPoints(Gap),0);
 FRecordingScoreRules Split;Split.Begin();Samples(Split,Alive,0,8);Split.Commit(2);Samples(Split,Dead,0,12);TestEqual(TEXT("Separate clips cannot combine"),DeathPoints(Split),0);
 FRecordingScoreRules Two;Two.Begin();auto Other=Dead;Other.Key=TEXT("Other");for(int I=0;I<=12;++I)Two.Sample(I*.25,{Alive,Other});TestEqual(TEXT("Different NPCs cannot combine states"),DeathPoints(Two),0);
 FRecordingScoreRules Late;Late.Begin();Samples(Late,Alive,0,12);Late.Sample(3.25,{Dead});TestEqual(TEXT("Death after two seconds awards immediately"),DeathPoints(Late),20);
 FRecordingScoreRules Early;Early.Begin();Early.Sample(0,{Alive});Samples(Early,Dead,1,8);TestEqual(TEXT("Early death followed continuously through two seconds"),DeathPoints(Early),20);
 FRecordingScoreRules NonNPC;NonNPC.Begin();auto Event=Person(EFilmSubject::Event);Samples(NonNPC,Event,0,4);Event.bAlive=false;Samples(NonNPC,Event,5,12);TestEqual(TEXT("Only NPC observations qualify"),DeathPoints(NonNPC),0);
 FRecordingScoreRules Retry;Retry.Begin();Samples(Retry,Alive,0,4);Samples(Retry,Dead,5,8);Retry.Begin();TestEqual(TEXT("Discard clears pending score"),DeathPoints(Retry),0);Samples(Retry,Alive,0,4);Samples(Retry,Dead,5,8);TestEqual(TEXT("Failed save does not consume later valid reward"),DeathPoints(Retry),20);
 TestEqual(TEXT("Bilingual category"),GameLanguage::Translate(TEXT("死亡瞬间"),true),FString(TEXT("Death moment")));
 const FString Formula=GameLanguage::Translate(TEXT("连续入镜2秒并记录由生到死：10 × 2 = +20 分"),true);
 TestTrue(TEXT("Translated explanation retains arithmetic"),Formula.Contains(TEXT("10"))&&Formula.Contains(TEXT("20"))&&!Formula.Contains(TEXT("连续")));
 return true;
}

#include "RecordingLibrary.h"
#include "RecordingReview.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Actor.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
class FDeathMomentDisplayCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int Stage=0;double Since=0;bool PreviousLanguage=false;
 TWeakObjectPtr<AActor> Owner;TSharedPtr<SRecordingReview> Review;
public:
 explicit FDeathMomentDisplayCheck(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  if(!W||!W->GetGameViewport()){Test->AddError(TEXT("Missing preview world"));return true;}
  if(Stage==0)
  {
   PreviousLanguage=GameLanguage::IsEnglish();GameLanguage::SetEnglish(false);
   auto* Actor=W->SpawnActor<AActor>();Owner=Actor;auto* L=NewObject<URecordingLibrary>(Actor);L->RegisterComponent();L->SetComponentTickEnabled(false);
   FRecordingScoreRules R;R.Begin();auto O=Person();Samples(R,O,0,4);O.bAlive=false;Samples(R,O,5,8);
   FRecordedClip Clip;Clip.Seconds=2;Clip.Score=20;
   for(const auto& M:R.Moments){FRecordedScoreMoment V;V.Subject=TEXT("黑帮成员 #1");V.Category=M.Category;V.Explanation=M.Explanation;V.Points=M.Points;V.Seconds=M.Time;Clip.ScoreMoments.Add(V);}
   L->Clips.Add(Clip);L->RoundScore=20;L->bRoundEnded=true;
   Review=SNew(SRecordingReview).Library(L);Review->ShowSummary();W->GetGameViewport()->AddViewportWidgetContent(Review.ToSharedRef(),1000);
   Stage=1;Since=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Since<1.5)return false;
  if(Stage==1){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/DeathMoment/summary_zh.png"),true,false);Stage=2;Since=FPlatformTime::Seconds();return false;}
  if(Stage==2){GameLanguage::SetEnglish(true);Stage=3;Since=FPlatformTime::Seconds();return false;}
  if(Stage==3){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/DeathMoment/summary_en.png"),true,false);Stage=4;Since=FPlatformTime::Seconds();return false;}
  W->GetGameViewport()->RemoveViewportWidgetContent(Review.ToSharedRef());Review.Reset();if(Owner.IsValid())Owner->Destroy();GameLanguage::SetEnglish(PreviousLanguage);return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathMomentDisplay,"Game.Recording.DeathMomentDisplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeathMomentDisplay::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FDeathMomentDisplayCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
