#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "GameLanguage.h"
#include "Frontend/GameFrontend.h"
#include "Camcorder/RecordingReview.h"
#include "Camcorder/RecordingLibrary.h"
#include "Camcorder/CamcorderVideoWriter.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontCache.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "Input/HittestGrid.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"

// One view per engine frame allows the font atlas and text layout to finish normally.
class FTypographyVisualCheck final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TStrongObjectPtr<URecordingLibrary> Library;
 TSharedPtr<SRecordingReview> Review;
 TSharedPtr<SWidget> View;
 TUniquePtr<FWidgetRenderer> Renderer;
 TSharedPtr<SVirtualWindow> Window;
 TUniquePtr<FHittestGrid> HitTest;
 TStrongObjectPtr<UTextureRenderTarget2D> Target;
 FString Folder;
 int32 Step=0,Pass=0;
 bool PreviousEnglish;
 void InvalidateTree(TSharedRef<SWidget> W)
 {
  W->Invalidate(EInvalidateWidgetReason::Layout|EInvalidateWidgetReason::Paint);
  auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)InvalidateTree(Children->GetChildAt(I));
 }
 const TCHAR* Names[16]={TEXT("Menu_en"),TEXT("Menu_zh"),TEXT("Settings_en"),TEXT("Settings_zh"),TEXT("Tutorial_en"),TEXT("Tutorial_zh"),TEXT("Overview_en"),TEXT("Overview_zh"),TEXT("Theater_en"),TEXT("Theater_zh"),TEXT("Final300_en"),TEXT("Final300_zh"),TEXT("Final0_en"),TEXT("Final100_en"),TEXT("Final200_en"),TEXT("Final300_wide_en")};
 void Advance(float Seconds){for(float T=0;T<Seconds;T+=.025f)Review->Tick(FGeometry(),0,.025f);}
 void CheckFilm(TSharedRef<SWidget> W,int32& FullLayers,bool& Film)
 {
  if(W->GetTypeAsString()==TEXT("SImage"))
  {
   auto Image=StaticCastSharedRef<SImage>(W);const auto* Brush=Image->GetImage();
   if(Brush)
   {
    if(Brush->ImageSize==FVector2D(1920,1080))
    {
     ++FullLayers;Test->TestFalse(TEXT("Curtains and audience retain their full UV region"),FBox2f(Brush->GetUVRegion()).bIsValid);
     const FVector2D Size=W->GetPaintSpaceGeometry().GetLocalSize();
     Test->TestTrue(TEXT("Whole source canvas is fitted at 16:9"),Size.Equals(FVector2D(1280,720),1));
    }
    if(const auto* Texture=Cast<UTexture2D>(Brush->GetResourceObject()))if(Texture->GetSizeX()==320&&Texture->GetSizeY()==180)
    {
     Film=true;const FVector2D Size=W->GetPaintSpaceGeometry().GetLocalSize();
     Test->TestTrue(TEXT("Film width is reduced by one third"),FMath::IsNearlyEqual(Size.X,820.f*2/3,1.f));
     Test->TestTrue(TEXT("Film retains a 16:9 aspect ratio"),FMath::IsNearlyEqual(Size.X/Size.Y,16.f/9,.01f));
    }
   }
  }
  auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)CheckFilm(Children->GetChildAt(I),FullLayers,Film);
 }
 void Prepare()
 {
  const bool English=Step!=1&&Step!=3&&Step!=5&&Step!=7&&Step!=9&&Step!=11;
  GameLanguage::SetEnglish(English);
  const auto Nothing=[]{};
  if(Step<6)
  {
   TSharedRef<SWidget> Panel=Step<2?GameFrontend::Main(Nothing,Nothing,Nothing,Nothing,Nothing):Step<4?GameFrontend::Settings(nullptr,Nothing,Nothing):GameFrontend::Tutorial(Nothing);
   View=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.025f,.035f)).Padding(24)
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[Panel]];
  }
  else
  {
   if(Step==6)Advance(1);
   if(Step==7)Advance(1.2f);
   if(Step==8){Review->PlayClip(0);Advance(1.15f);Review->TogglePause();}
   if(Step>=10&&Step!=11)
   {
    Library->RoundScore=Step==12?0:Step==13?100:Step==14?200:300;
    Review->ShowSummary();Advance(.8f);
   }
   View=Review;
  }
  Target.Reset(FWidgetRenderer::CreateTargetFor(Step==15?FVector2D(1720,720):FVector2D(1280,720),TF_Bilinear,false));
  Window->SetContent(View.ToSharedRef());Window->Resize(Step==15?FVector2D(1720,720):FVector2D(1280,720));
  InvalidateTree(View.ToSharedRef());
 }
public:
 explicit FTypographyVisualCheck(FAutomationTestBase* T):Test(T),Library(NewObject<URecordingLibrary>()),PreviousEnglish(GameLanguage::IsEnglish())
 {
  Folder=FPaths::ProjectSavedDir()/TEXT("Automation/TypographyQA");IFileManager::Get().MakeDirectory(*Folder,true);
  const FString Video=Folder/TEXT("Fixture.avi");FCamcorderVideoWriter Writer;
  if(Writer.Open(Video,320,180,15))
  {
   TArray<FColor> Pixels;Pixels.SetNum(320*180);
   for(int32 Y=0;Y<180;++Y)for(int32 X=0;X<320;++X)Pixels[Y*320+X]=FColor(uint8(30+X*.4),uint8(45+Y*.5),uint8(85+(X/40+Y/40)%2*40),255);
   Writer.Enqueue(MoveTemp(Pixels),0);Writer.Finish(150);
  }
  else Test->AddError(TEXT("Cannot create typography QA video"));
  FRecordedClip Clip;Clip.Path=Video;Clip.Seconds=10;Clip.Score=9;
  FRecordedScoreMoment M;M.Seconds=1;M.Points=15;M.Category=TEXT("NPC首次入镜");Clip.ScoreMoments.Add(M);
  M.Seconds=1.1f;M.Points=-6;M.Category=TEXT("行人入镜扣分");Clip.ScoreMoments.Add(M);
  for(int32 I=0;I<13;++I)Library->Clips.Add(Clip);
  Review=SNew(SRecordingReview).Library(Library.Get());Renderer=MakeUnique<FWidgetRenderer>(false);
  Window=SNew(SVirtualWindow).Size(FVector2D(1280,720));HitTest=MakeUnique<FHittestGrid>();
  // Legacy DrawWindow needs a full repaint; cached window batches require DrawInvalidationRoot.
  Window->SetAllowFastUpdate(false);
 }
 virtual ~FTypographyVisualCheck(){GameLanguage::SetEnglish(PreviousEnglish);}
 virtual bool Update()override
 {
  if(Step==16){GameLanguage::SetEnglish(PreviousEnglish);return true;}
  if(Pass==0)Prepare();
  const auto Size=Step==15?FVector2D(1720,720):FVector2D(1280,720);
  Renderer->DrawWindow(Target.Get(),*HitTest,Window.ToSharedRef(),1.f,Size,0);
  FSlateApplication::Get().GetRenderer()->GetFontCache()->UpdateCache();FlushRenderingCommands();
  if(++Pass<4)return false;
  if(Step==8)
  {
   int32 Layers=0;bool Film=false;CheckFilm(Review.ToSharedRef(),Layers,Film);
   Test->TestEqual(TEXT("Theater contains two full-size independent art layers"),Layers,2);
   Test->TestTrue(TEXT("Resized film is present"),Film);
  }
  TArray<FColor> Pixels;FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);
  Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(int32(Size.X),int32(Size.Y),Pixels,PNG);
  Test->TestTrue(TEXT("Save typography preview"),FFileHelper::SaveArrayToFile(PNG,*(Folder/(FString(Names[Step])+TEXT(".png")))));
  ++Step;Pass=0;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameTypographyVisual,"Game.Localization.TypographyVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameTypographyVisual::RunTest(const FString&){AddCommand(new FTypographyVisualCheck(this));return true;}
#endif
