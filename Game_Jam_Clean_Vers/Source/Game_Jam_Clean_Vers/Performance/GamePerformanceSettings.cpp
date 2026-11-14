#include "GamePerformanceSettings.h"
#include "Audio/AudioWidgets.h"
#include "GameFramework/GameUserSettings.h"
#include "Frontend/LocalUserSettings.h"
#include "Localization/GameLanguage.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"

FGamePerformanceOptions FGamePerformanceOptions::Read(const UGameUserSettings& Settings)
{
 FGamePerformanceOptions Result;Result.Quality=Settings.ScalabilityQuality;
 Result.ResolutionScale=Result.Quality.ResolutionQuality<=0?100.f:Result.Quality.ResolutionQuality;
 Result.FrameLimit=Settings.GetFrameRateLimit();Result.bVSync=Settings.IsVSyncEnabled();return Result;
}
void FGamePerformanceOptions::SetPreset(int32 Level)
{
 Quality.SetFromSingleQualityLevel(FMath::Clamp(Level,0,3));
 // Presets adjust effects without silently changing the player's render scale.
 Quality.ResolutionQuality=ResolutionScale;
}
void FGamePerformanceOptions::Write(UGameUserSettings& Settings) const
{
 Settings.ScalabilityQuality=Quality;
 Settings.SetResolutionScaleValueEx(FMath::Clamp(ResolutionScale,50.f,100.f));
 Settings.SetFrameRateLimit(FMath::Clamp(FrameLimit,0.f,240.f));Settings.SetVSyncEnabled(bVSync);
}

namespace
{
 const TCHAR* QualityName(int32 Level)
 {
  switch(Level){case 0:return TEXT("低");case 1:return TEXT("中");case 2:return TEXT("高");case 3:return TEXT("极高");case 4:return TEXT("电影级");default:return TEXT("自定义");}
 }
 class SGamePerformancePanel final:public SCompoundWidget
 {
 public:
  virtual bool SupportsKeyboardFocus() const override { return true; }
  SLATE_BEGIN_ARGS(SGamePerformancePanel){} SLATE_END_ARGS()
  void Construct(const FArguments&,TFunction<void()> InClose,TFunction<void()> InLanguage)
  {
   Close=MoveTemp(InClose);Language=MoveTemp(InLanguage);
   auto* Settings=UGameUserSettings::GetGameUserSettings();check(Settings);Draft=FGamePerformanceOptions::Read(*Settings);
   auto List=SNew(SVerticalBox);
   List->AddSlot().AutoHeight().Padding(0,0,0,5)[SNew(SLocalizedTextBlock).Font(GameLanguage::Font(28)).Text(FText::FromString(TEXT("性能与画质")))];
   List->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SLocalizedTextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("F10 打开设置 · 应用后保存")))];
   auto Presets=SNew(SHorizontalBox);
   for(int32 Level=0;Level<4;++Level)
    Presets->AddSlot().FillWidth(1).Padding(3,0)[Button(QualityName(Level),[this,Level]{Draft.SetPreset(Level);bDirty=true;})];
   List->AddSlot().AutoHeight().Padding(0,0,0,10)[Presets];
   auto Rows=SNew(SVerticalBox);
   AddQuality(Rows,TEXT("视距"),&Scalability::FQualityLevels::ViewDistanceQuality);
   AddQuality(Rows,TEXT("阴影"),&Scalability::FQualityLevels::ShadowQuality);
   AddQuality(Rows,TEXT("纹理"),&Scalability::FQualityLevels::TextureQuality);
   AddQuality(Rows,TEXT("特效"),&Scalability::FQualityLevels::EffectsQuality);
   AddQuality(Rows,TEXT("后期处理"),&Scalability::FQualityLevels::PostProcessQuality);
   AddQuality(Rows,TEXT("植被"),&Scalability::FQualityLevels::FoliageQuality);
   AddQuality(Rows,TEXT("抗锯齿"),&Scalability::FQualityLevels::AntiAliasingQuality);
   AddQuality(Rows,TEXT("全局光照"),&Scalability::FQualityLevels::GlobalIlluminationQuality);
   AddQuality(Rows,TEXT("反射"),&Scalability::FQualityLevels::ReflectionQuality);
   Rows->AddSlot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("渲染比例")))]
    +SHorizontalBox::Slot().FillWidth(1.2f).VAlign(VAlign_Center)[SNew(SAudioSlider).Value_Lambda([this]{return (Draft.ResolutionScale-50.f)/50.f;}).OnValueChanged_Lambda([this](float Value){Draft.ResolutionScale=FMath::RoundToFloat(50.f+Value*50.f);bDirty=true;})]
    +SHorizontalBox::Slot().AutoWidth().Padding(12,0)[SNew(SLocalizedTextBlock).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%.0f%%"),Draft.ResolutionScale));})]];
   Rows->AddSlot().AutoHeight().Padding(0,3)[SNew(SLocalizedTextBlock).AutoWrapText(true).Font(GameLanguage::Font(14)).Text(FText::FromString(TEXT("降低渲染比例可减轻显卡负担，但画面会变软；不改变显示分辨率。")))];
   AddChoice(Rows,TEXT("帧率上限"),[this]{return Draft.FrameLimit<=0?FString(TEXT("不限")):FString::Printf(TEXT("%.0f 帧/秒"),Draft.FrameLimit);},[this]
   {
    const float Limits[]={30,60,90,120,144,165,240,0};float Next=0;
    for(float Limit:Limits)if(Limit>Draft.FrameLimit){Next=Limit;break;}
    Draft.FrameLimit=Next;bDirty=true;
   });
   AddChoice(Rows,TEXT("垂直同步"),[this]{return FString(Draft.bVSync?TEXT("开启"):TEXT("关闭"));},[this]{Draft.bVSync=!Draft.bVSync;bDirty=true;});
   Rows->AddSlot().AutoHeight().Padding(0,12,0,4)[SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("NVIDIA DLSS / 帧生成：未集成")))];
   Rows->AddSlot().AutoHeight()[SNew(SLocalizedTextBlock).AutoWrapText(true).Font(GameLanguage::Font(14)).Text(FText::FromString(TEXT("当前版本未集成兼容的 NVIDIA 插件，不能启用 DLSS 或帧生成。")))];
   List->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]];
   List->AddSlot().AutoHeight().Padding(0,12,0,8)[SNew(SLocalizedTextBlock).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(bDirty?TEXT("有未应用的修改；关闭将放弃修改。"):bApplied?TEXT("设置已应用并保存。"):TEXT("选择适合设备的画质；部分选项取决于渲染模式。"));})];
   List->AddSlot().AutoHeight()[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,6,0)[Button(TEXT("应用并保存"),[this]{auto* S=UGameUserSettings::GetGameUserSettings();Draft.Write(*S);S->ApplyNonResolutionSettings();GameLocalSettings::SavePerformance();Draft=FGamePerformanceOptions::Read(*S);bDirty=false;bApplied=true;})]
    +SHorizontalBox::Slot().FillWidth(1).Padding(6,0)[Button(TEXT("语言 / Language"),[this]{Language();})]
    +SHorizontalBox::Slot().FillWidth(1).Padding(6,0,0,0)[Button(TEXT("关闭 [F10 / Esc]"),[this]{Close();})]];
   ChildSlot[SNew(SBox).WidthOverride(650).HeightOverride(660)[List]];
  }
 private:
  FGamePerformanceOptions Draft;
  bool bDirty=false,bApplied=false;
  TFunction<void()> Close,Language;
  TSharedRef<SWidget> Button(const TCHAR* Text,TFunction<void()> Action)
  {
   return SNew(SAudioButton).ContentPadding(FMargin(12,8)).HAlign(HAlign_Center).OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})
    [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(16)).Text(FText::FromString(Text))];
  }
  void AddChoice(TSharedRef<SVerticalBox> Rows,const TCHAR* Label,TFunction<FString()> Value,TFunction<void()> Action)
  {
   Rows->AddSlot().AutoHeight().Padding(0,3)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SLocalizedTextBlock).Font(GameLanguage::Font(17)).Text(FText::FromString(Label))]
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SAudioButton).ContentPadding(FMargin(10,5)).HAlign(HAlign_Center).OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})
     [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(17)).Text_Lambda([Value=MoveTemp(Value)]{return FText::FromString(Value());})]]];
  }
  void AddQuality(TSharedRef<SVerticalBox> Rows,const TCHAR* Label,int32 Scalability::FQualityLevels::* Field)
  {
   AddChoice(Rows,Label,[this,Field]{return FString(QualityName(Draft.Quality.*Field));},[this,Field]{Draft.Quality.*Field=(Draft.Quality.*Field+1)%4;bDirty=true;});
  }
 };
}
TSharedRef<SWidget> GamePerformance::MakePanel(TFunction<void()> Close,TFunction<void()> Language)
{
 return SNew(SGamePerformancePanel,MoveTemp(Close),MoveTemp(Language));
}
