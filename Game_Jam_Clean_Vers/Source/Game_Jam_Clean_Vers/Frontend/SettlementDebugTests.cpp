#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "SettlementDebugPanel.h"
#include "Camcorder/RecordingReview.h"
#include "Camcorder/RecordingLibrary.h"
#include "Localization/GameLanguage.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Widgets/Input/SButton.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

namespace
{
 bool DebugText(TSharedRef<SWidget> W,const FString& Text)
 {
  if(W->GetTypeAsString().Contains(TEXT("TextBlock"))&&StaticCastSharedRef<STextBlock>(W)->GetText().ToString()==Text)return true;
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(DebugText(C->GetChildAt(I),Text))return true;return false;
 }
 TSharedPtr<SButton> DebugButton(TSharedRef<SWidget> W,const FString& Text)
 {
  if(W->GetTypeAsString().Contains(TEXT("Button"))&&DebugText(W,GameLanguage::Display(Text)))return StaticCastSharedRef<SButton>(W);
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto B=DebugButton(C->GetChildAt(I),Text))return B;return nullptr;
 }
 TSharedPtr<SRecordingReview> DebugReview(TSharedRef<SWidget> W)
 {
  if(W->GetTypeAsString()==TEXT("SRecordingReview"))return StaticCastSharedRef<SRecordingReview>(W);
  auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto R=DebugReview(C->GetChildAt(I)))return R;return nullptr;
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlementDebugTest,"Game.Recording.SettlementDebug",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSettlementDebugTest::RunTest(const FString&)
{
 const bool English=GameLanguage::IsEnglish();bool Closed=false;
 auto Panel=SettlementDebug::Make([&]{Closed=true;});
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("Automation/SettlementDebugQA");
 IFileManager::Get().MakeDirectory(*Folder,true);
 FWidgetRenderer Renderer(false);
 auto Shot=[&](const FString& Name)
 {
  TStrongObjectPtr<UTextureRenderTarget2D> Target(Renderer.DrawWidget(Panel,FVector2D(1280,720)));
  Renderer.DrawWidget(Target.Get(),Panel,FVector2D(1280,720),0);
  Renderer.DrawWidget(Target.Get(),Panel,FVector2D(1280,720),0);
  TArray<FColor> Pixels;FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);
  Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,Pixels,PNG);
  TestTrue(TEXT("Save rendered UI"),FFileHelper::SaveArrayToFile(PNG,*(Folder/Name)));
 };
 auto Click=[&](const TCHAR* Text){auto B=DebugButton(Panel,Text);if(!TestTrue(Text,B.IsValid()))return false;B->SimulateClick();return true;};
 const TCHAR* Labels[]={TEXT("金粪奖 · 0–99"),TEXT("银子弹奖 · 100–199"),TEXT("痛斯卡奖 · 200–299"),TEXT("屠戮金棕榈奖 · 300–399")};
 for(bool En:{false,true})
 {
  GameLanguage::SetEnglish(En);Shot(En?TEXT("menu_en.png"):TEXT("menu_zh.png"));
  for(int32 Tier=0;Tier<4;++Tier)
  {
   if(!Click(Labels[Tier]))continue;
   TSet<int32> Scores;
   for(int32 I=0;I<12;++I)
   {
    auto Review=DebugReview(Panel);if(!TestTrue(TEXT("Actual final panel is mounted"),Review.IsValid()))break;
    auto* L=Review->GetLibrary();const int32 Score=int32(L->RoundScore);Scores.Add(Score);
    TestTrue(TEXT("Score remains within selected tier"),Score>=Tier*100&&Score<=Tier*100+99);
    TestEqual(TEXT("Correct actual award"),Review->GetAwardTier(),Tier);
    TestEqual(TEXT("Skips recording flow"),Review->GetPhase(),ERecordingReviewPhase::Summary);
    TestTrue(TEXT("Fixture has no world, player owner, registration or save"),!L->GetWorld()&&!L->GetOwner()&&!L->IsRegistered()&&L->RoundRecordPath.IsEmpty()&&L->Clips.IsEmpty()&&!L->bRoundEnded);
    TestFalse(TEXT("No gameplay restart action in preview"),DebugButton(Panel,TEXT("重新开始")).IsValid());
    if(I==0){Review->Tick(FGeometry(),0,3.5f);Shot(FString::Printf(TEXT("award_%d_%s.png"),Tier,En?TEXT("en"):TEXT("zh")));}
    Click(TEXT("重新随机"));
   }
   TestTrue(TEXT("Reroll produces varied scores"),Scores.Num()>1);Click(TEXT("选择奖项"));
  }
 }
 Click(TEXT("关闭预览"));TestTrue(TEXT("Close returns to owner"),Closed);
 GameLanguage::SetEnglish(English);return true;
}

class FSettlementDebugInputCheck final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Step=0;double Started=0;bool English=false;
 void Key(FKey K){auto& A=FSlateApplication::Get();A.ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));A.ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));}
public:
 explicit FSettlementDebugInputCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started){Started=FPlatformTime::Seconds();English=GameLanguage::IsEnglish();}
  if(FPlatformTime::Seconds()-Started>30){Test->AddError(TEXT("Preview input timed out"));GameLanguage::SetEnglish(English);return true;}
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;auto V=W?W->GetGameViewport():nullptr;auto Window=V?V->GetWindow():nullptr;
  auto E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
  if(!PC||!Window||!E||!E->RecordingLibrary)return false;
  auto Root=Window.ToSharedRef();
  if(Step==0)
  {
   GameLanguage::SetEnglish(false);Window->BringToFront(true);
   FSlateApplication::Get().ProcessWindowActivatedEvent(FWindowActivateEvent(FWindowActivateEvent::EA_Activate,Root));
   FSlateApplication::Get().SetAllUserFocusToGameViewport();Key(EKeys::F3);++Step;return false;
  }
  if(Step==1)
  {
   auto B=DebugButton(Root,TEXT("银子弹奖 · 100–199"));if(!B)return false;
   B->SimulateClick();++Step;return false;
  }
  if(Step==2)
  {
   auto R=DebugReview(Root);if(!R)return false;
   Test->TestTrue(TEXT("Preview uses a separate library"),R->GetLibrary()!=E->RecordingLibrary);
   Test->TestFalse(TEXT("Live run is not ended"),E->RecordingLibrary->bRoundEnded);
   Test->TestTrue(TEXT("Preview captures mouse and movement"),PC->bShowMouseCursor&&PC->IsMoveInputIgnored());
   GameLanguage::SetEnglish(true);
   Test->TestTrue(TEXT("Mounted preview translates immediately"),DebugButton(Root,TEXT("重新随机")).IsValid());
   if(auto B=DebugButton(Root,TEXT("选择奖项")))B->SimulateClick();
   ++Step;return false;
  }
  if(Step==3)
  {
   Test->TestTrue(TEXT("Back restores award menu"),DebugButton(Root,TEXT("银子弹奖 · 100–199")).IsValid());
   Key(EKeys::Escape);++Step;return false;
  }
  Test->TestFalse(TEXT("Escape removes preview"),DebugButton(Root,TEXT("关闭预览")).IsValid());
  Test->TestFalse(TEXT("Gameplay input restored"),PC->IsMoveInputIgnored()||PC->IsLookInputIgnored()||PC->bShowMouseCursor);
  GameLanguage::SetEnglish(English);return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlementDebugInputTest,"Game.Recording.SettlementDebugInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSettlementDebugInputTest::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FSettlementDebugInputCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
