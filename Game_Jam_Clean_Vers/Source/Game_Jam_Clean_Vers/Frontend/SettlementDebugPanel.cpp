#if WITH_EDITOR
#include "SettlementDebugPanel.h"
#include "Audio/AudioWidgets.h"
#include "Camcorder/RecordingLibrary.h"
#include "Camcorder/RecordingReview.h"
#include "Localization/GameLanguage.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
 class SSettlementDebugPanel final : public SCompoundWidget
 {
 public:
  SLATE_BEGIN_ARGS(SSettlementDebugPanel){} SLATE_END_ARGS()
  void Construct(const FArguments&,TFunction<void()> Close)
  {
   OnClose=MoveTemp(Close);
   // Unregistered transient fixture: no player owner, world, recording, or save history.
   Preview.Reset(NewObject<URecordingLibrary>(GetTransientPackage(),NAME_None,RF_Transient));
   ChildSlot[SAssignNew(Host,SBox).WidthOverride(1280).HeightOverride(720)];
   ShowChoices();
  }
 private:
  TStrongObjectPtr<URecordingLibrary> Preview;
  TSharedPtr<SBox> Host;
  TSharedPtr<SRecordingReview> Review;
  TFunction<void()> OnClose;
  int32 Tier=0;
  TSharedRef<SWidget> Button(const FString& Text,TFunction<void()> Action)
  {
   return SNew(SAudioButton).ContentPadding(FMargin(18,12)).OnClicked_Lambda([Action]{Action();return FReply::Handled();})
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
     [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(24)).Justification(ETextJustify::Center).Text(FText::FromString(Text))]];
  }
  void ShowChoices()
  {
   Review.Reset();
   auto Rows=SNew(SVerticalBox);
   Rows->AddSlot().AutoHeight().Padding(0,12)[SNew(SLocalizedTextBlock).Font(GameLanguage::Font(36)).Text(FText::FromString(TEXT("结算调试 · F3")))];
   Rows->AddSlot().AutoHeight().Padding(0,12)[SNew(SLocalizedTextBlock).Font(GameLanguage::Font(22)).AutoWrapText(true)
    .Text(FText::FromString(TEXT("仅编辑器预览，不记录成绩。选择奖项，在对应区间内随机生成分数。")))];
   const TCHAR* Labels[]={TEXT("金粪奖 · 0–99"),TEXT("银子弹奖 · 100–199"),TEXT("痛斯卡奖 · 200–299"),TEXT("屠戮金棕榈奖 · 300–399")};
   TSharedPtr<SWidget> First;
   for(int32 I=0;I<4;++I)
   {
    auto Choice=Button(Labels[I],[this,I]{Tier=I;ShowPreview();});if(I==0)First=Choice;
    Rows->AddSlot().AutoHeight().Padding(0,6)[Choice];
   }
   Rows->AddSlot().AutoHeight().Padding(0,18)[Button(TEXT("关闭预览"),[this]{OnClose();})];
   Host->SetContent(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(800)[Rows]]);
   FSlateApplication::Get().SetKeyboardFocus(First,EFocusCause::SetDirectly);
  }
  void ShowPreview()
  {
   Preview->RoundScore=FMath::RandRange(Tier*100,Tier*100+99);
   if(Review){Review->ShowSummary();return;}
   auto Actions=SNew(SHorizontalBox);
   Actions->AddSlot().FillWidth(1).Padding(5,0)[Button(TEXT("重新随机"),[this]{ShowPreview();})];
   Actions->AddSlot().FillWidth(1).Padding(5,0)[Button(TEXT("选择奖项"),[this]{ShowChoices();})];
   Actions->AddSlot().FillWidth(1).Padding(5,0)[Button(TEXT("关闭预览"),[this]{OnClose();})];
   SAssignNew(Review,SRecordingReview).Library(Preview.Get()).PreviewControls(Actions);
   Review->ShowSummary();Host->SetContent(Review.ToSharedRef());
   FSlateApplication::Get().SetKeyboardFocus(Review,EFocusCause::SetDirectly);
  }
 };
}
TSharedRef<SWidget> SettlementDebug::Make(TFunction<void()> Close){return SNew(SSettlementDebugPanel,MoveTemp(Close));}
#endif
