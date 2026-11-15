#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "MenuPresentation.h"
#include "GameFrontend.h"
#include "RunScoreHistoryPanel.h"
#include "Performance/GamePerformanceSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "Input/HittestGrid.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Layout/SScaleBox.h"

class FMenuVisualCapture final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;bool PreviousEnglish;int32 Page=0,Frame=0;
 TSharedPtr<SWidget> View;TSharedPtr<SVirtualWindow> Window;
 TUniquePtr<FWidgetRenderer> Renderer;TUniquePtr<FHittestGrid> HitTest;
 TStrongObjectPtr<UTextureRenderTarget2D> Target;
public:
 explicit FMenuVisualCapture(FAutomationTestBase* T):Test(T),PreviousEnglish(GameLanguage::IsEnglish())
 {
  Renderer=MakeUnique<FWidgetRenderer>(false);HitTest=MakeUnique<FHittestGrid>();
  Window=SNew(SVirtualWindow).Size(FVector2D(1280,900));Window->SetAllowFastUpdate(false);
  Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(1280,900),TF_Bilinear,false));
 }
 ~FMenuVisualCapture(){GameLanguage::SetEnglish(PreviousEnglish);}
 bool Update()override
 {
  if(Page==12)return true;
  if(Frame==0)
  {
   GameLanguage::SetEnglish(Page%2==0);const auto Nothing=[]{};
   TSharedRef<SWidget> Panel=Page<2?GameFrontend::Settings(nullptr,Nothing,Nothing):Page<4?GamePerformance::MakePanel(Nothing,Nothing):Page<6?GameFrontend::Pause(Nothing,Nothing,Nothing,Nothing,false):Page<8?GameFrontend::RunHistory(Nothing):GameFrontend::Tutorial(Nothing,Page>=10);
   View=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.006f,.005f,.005f)).Padding(40)
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
     [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(MenuPresentation::Surface()).Padding(26)[SNew(SMenuEntrance)[Panel]]]];
   Window->SetContent(View.ToSharedRef());
  }
  Renderer->DrawWindow(Target.Get(),*HitTest,Window.ToSharedRef(),1.f,FVector2D(1280,900),.04f);FlushRenderingCommands();
  if(++Frame<14)return false;
  TArray<FColor> Pixels;FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,900,Pixels,PNG);
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("Automation/MenuStyleQA");IFileManager::Get().MakeDirectory(*Folder,true);
  const TCHAR* Names[]={TEXT("Settings"),TEXT("Graphics"),TEXT("Pause"),TEXT("History"),TEXT("Tutorial"),TEXT("ReadyTutorial")};
  Test->TestTrue(TEXT("Save panel preview"),FFileHelper::SaveArrayToFile(PNG,*(Folder/FString::Printf(TEXT("%s-%s.png"),Names[Page/2],Page%2?TEXT("zh"):TEXT("en")))));
  ++Page;Frame=0;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuVisualTest,"Game.Frontend.MenuStyleVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMenuVisualTest::RunTest(const FString&){AddCommand(new FMenuVisualCapture(this));return true;}
#endif
