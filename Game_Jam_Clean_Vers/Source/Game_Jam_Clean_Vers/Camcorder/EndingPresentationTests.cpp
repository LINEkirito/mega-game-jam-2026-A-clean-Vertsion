#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "RecordingReview.h"
#include "RecordingLibrary.h"
#include "CamcorderVideoWriter.h"
#include "Localization/GameLanguage.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
 bool ContainsText(TSharedRef<SWidget> W,const FString& Text)
 {
  if(W->GetTypeAsString().Contains(TEXT("TextBlock"))&&StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(Text))return true;
  auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)if(ContainsText(Children->GetChildAt(I),Text))return true;
  return false;
 }
 void Advance(TSharedRef<SRecordingReview> UI,float Seconds)
 {
  for(float T=0;T<Seconds;T+=.025f)UI->Tick(FGeometry(),0,.025f);
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingPresentation,"Game.Recording.EndingPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEndingPresentation::RunTest(const FString&)
{
 const bool OldEnglish=GameLanguage::IsEnglish();
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("Automation/EndingQA");
 IFileManager::Get().MakeDirectory(*Folder,true);
 const FString Video=Folder/TEXT("Fixture.avi");
 FCamcorderVideoWriter Writer;
 if(!TestTrue(TEXT("Create isolated video fixture"),Writer.Open(Video,320,180,15)))return false;
 TArray<FColor> Pixels;Pixels.SetNum(320*180);
 for(int32 Y=0;Y<180;++Y)for(int32 X=0;X<320;++X)Pixels[Y*320+X]=FColor(uint8(X*.7),uint8(Y),uint8(60+((X/40+Y/40)%2)*70),255);
 Writer.Enqueue(MoveTemp(Pixels),0);Writer.Finish(150); // Ten seconds, held still for layout inspection.
 TStrongObjectPtr<URecordingLibrary> L(NewObject<URecordingLibrary>());
 FRecordedClip Clip;Clip.Path=Video;Clip.Seconds=10;Clip.Score=24;
 FRecordedScoreMoment Add;Add.Seconds=1;Add.Points=15;Add.Category=TEXT("NPC首次入镜");Clip.ScoreMoments.Add(Add);
 FRecordedScoreMoment Deduct;Deduct.Seconds=2;Deduct.Points=-6;Deduct.Category=TEXT("行人入镜扣分");Clip.ScoreMoments.Add(Deduct);
 Add.Seconds=5;Clip.ScoreMoments.Add(Add);
 for(int32 I=0;I<13;++I)L->Clips.Add(Clip);
 L->bEndedByDeath=true;L->RoundScore=156.f;
 auto UI=SNew(SRecordingReview).Library(L.Get());
 FWidgetRenderer Renderer(false);
 auto Shot=[&](const TCHAR* Name,FVector2D Size=FVector2D(1280,720))
 {
  TStrongObjectPtr<UTextureRenderTarget2D> Target(Renderer.DrawWidget(UI,Size));
  if(!TestNotNull(TEXT("Slate renders to texture"),Target.Get()))return;
  // Auto-wrapped translated text settles after its first layout pass, as in a live viewport.
  Renderer.DrawWidget(Target.Get(),UI,Size,0);
  Renderer.DrawWidget(Target.Get(),UI,Size,0);
  TArray<FColor> Output;FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);
  Target->GameThread_GetRenderTargetResource()->ReadPixels(Output,Flags);
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(int32(Size.X),int32(Size.Y),Output,PNG);
  TestTrue(TEXT("Save actual Slate screenshot"),FFileHelper::SaveArrayToFile(PNG,*(Folder/(FString(Name)+TEXT(".png")))));
 };
 GameLanguage::SetEnglish(false);Advance(UI,1.0f);Shot(TEXT("OverviewPop_zh"));
 Advance(UI,1.2f);Shot(TEXT("OverviewEnd_zh"));
 TestEqual(TEXT("13 real first-frame thumbnails decode"),UI->GetThumbnailCount(),13);
 TestEqual(TEXT("Overview stays until every pop and end hold complete"),UI->GetPhase(),ERecordingReviewPhase::Overview);
 Advance(UI,1.3f);TestEqual(TEXT("Overview no longer leaves after the old 1.5 second hold"),UI->GetPhase(),ERecordingReviewPhase::Overview);
 Advance(UI,1.8f);TestEqual(TEXT("Overview enters intertitle after the extended hold"),UI->GetPhase(),ERecordingReviewPhase::Intertitle);
 Advance(UI,.5f);Shot(TEXT("Intertitle_zh"));
 GameLanguage::SetEnglish(true);Shot(TEXT("Intertitle_en"));
 Advance(UI,3);TestEqual(TEXT("Intertitle enters review"),UI->GetPhase(),ERecordingReviewPhase::Review);
 UI->Seek(.22f);UI->TogglePause();Shot(TEXT("Theater_en"));
 TestEqual(TEXT("Current clip adds and subtracts existing moments"),UI->GetPresentedScore(),9);
 GameLanguage::SetEnglish(false);Shot(TEXT("Theater_zh"));
 UI->PlayClip(2);UI->Seek(.22f);TestEqual(TEXT("Earlier clips contribute once to running display"),UI->GetPresentedScore(),57);
 const float PersistedScore=L->RoundScore;
 UI->BeginScoreReveal();Advance(UI,1.3f);Shot(TEXT("DeathReduction_zh"));
 Advance(UI,2);TestEqual(TEXT("Reduction automatically reaches final panel"),UI->GetPhase(),ERecordingReviewPhase::Summary);
 TestEqual(TEXT("Reduction does not deduct score twice"),L->RoundScore,PersistedScore);
 const float Scores[]={-7.5f,0,99.5f,100,199.5f,200,299.5f,300};
 const int32 Tiers[]={0,0,0,1,1,2,2,3};
 for(int32 I=0;I<8;++I){L->RoundScore=Scores[I];TestEqual(TEXT("Badge uses exact final-score threshold"),UI->GetAwardTier(),Tiers[I]);}
 const TCHAR* EN[]={TEXT("Gold Shit"),TEXT("Silver Bullet"),TEXT("The Ouch-scar"),TEXT("Palme d'Or du Massacre")};
 for(int32 I=0;I<4;++I)
 {
  L->RoundScore=I*100.f;UI->ShowSummary();Advance(UI,.8f);
  GameLanguage::SetEnglish(true);TestTrue(TEXT("Correct English badge visible"),ContainsText(UI,EN[I]));Shot(*FString::Printf(TEXT("Award%d_en"),I));
  GameLanguage::SetEnglish(false);TestFalse(TEXT("Open panel immediately updates translated award"),ContainsText(UI,EN[I]));Shot(*FString::Printf(TEXT("Award%d_zh"),I));
 }
 GameLanguage::SetEnglish(true);Shot(TEXT("Award3_4x3_en"),FVector2D(1024,768));Shot(TEXT("Award3_ultrawide_en"),FVector2D(1720,720));
 TestTrue(TEXT("Restart action visible"),ContainsText(UI,TEXT("RESTART")));
 TestTrue(TEXT("Main menu action visible"),ContainsText(UI,TEXT("BACK TO MENU")));
 TestTrue(TEXT("Quit action visible"),ContainsText(UI,GameLanguage::Display(TEXT("退出游戏"))));
 TestEqual(TEXT("Playback did not mutate raw clip score"),L->Clips[0].Score,24);
 TStrongObjectPtr<URecordingLibrary> Empty(NewObject<URecordingLibrary>());
 auto EmptyUI=SNew(SRecordingReview).Library(Empty.Get());Advance(EmptyUI,9);
 TestEqual(TEXT("Empty round automatically reaches final panel"),EmptyUI->GetPhase(),ERecordingReviewPhase::Summary);
 GameLanguage::SetEnglish(OldEnglish);
 return true;
}
#endif
