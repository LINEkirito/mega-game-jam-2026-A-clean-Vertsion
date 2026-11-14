#include "GameFrontend.h"
#include "Audio/AudioWidgets.h"
#include "Frontend/LocalUserSettings.h"
#include "Localization/GameLanguage.h"
#include "Performance/GamePerformanceSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"

namespace
{
 // Resolution-independent pictograms: walking, camera, and extraction vehicle.
 class SGuideIcon final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SGuideIcon):_Kind(0){} SLATE_ARGUMENT(int32,Kind) SLATE_END_ARGS()
  void Construct(const FArguments& Args){Kind=Args._Kind;}
  virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(70,60);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
  {
   auto Line=[&](std::initializer_list<FVector2D> Points){TArray<FVector2D> P;for(auto V:Points)P.Add(V);FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,FLinearColor(.35f,.9f,.78f),true,3.f);};
   if(Kind==0){Line({{30,6},{38,6},{38,14},{30,14},{30,6}});Line({{34,17},{29,34},{20,52}});Line({{29,34},{43,42},{49,53}});Line({{32,23},{18,29},{12,22}});Line({{32,23},{43,30},{54,29}});}
   else if(Kind==1){Line({{8,18},{44,18},{44,46},{8,46},{8,18}});Line({{44,27},{62,18},{62,46},{44,37}});Line({{16,18},{20,10},{34,10},{38,18}});Line({{19,31},{32,31}});}
   else {Line({{7,32},{16,32},{24,17},{47,17},{55,32},{63,32},{63,47},{7,47},{7,32}});Line({{20,31},{50,31}});Line({{16,47},{16,53},{23,53},{23,47}});Line({{48,47},{48,53},{55,53},{55,47}});}
   return Layer;
  }
 private:int32 Kind=0;
 };
 const TCHAR* Section=TEXT("GameFrontend");
 float Read(const TCHAR* Key,float Default,float Min,float Max)
 {
  float V=Default;GConfig->GetFloat(Section,Key,V,GameLocalSettings::Filename());
  return FMath::IsFinite(V)?FMath::Clamp(V,Min,Max):Default;
 }
 TSharedRef<SWidget> Label(const TCHAR* Text,int Size=20,float Wrap=620)
 { return SNew(SLocalizedTextBlock).Font(GameLanguage::Font(Size)).AutoWrapText(false).WrapTextAt(Wrap).Text(FText::FromString(Text)); }
 TSharedRef<SWidget> Button(const TCHAR* Text,TFunction<void()> Action,int Size=20)
 {
  return SNew(SAudioButton).ContentPadding(FMargin(22,13)).HAlign(HAlign_Center)
   .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})[Label(Text,Size,0)];
 }
 class SFrontendSettings final:public SCompoundWidget
 {
 public:
  SLATE_BEGIN_ARGS(SFrontendSettings){} SLATE_END_ARGS()
  virtual bool SupportsKeyboardFocus() const override{return true;}
  void Construct(const FArguments&,APlayerController* PC,TFunction<void()> Close,TFunction<void()> Language)
  {
   Player=PC;Sensitivity=GameFrontend::Sensitivity();Effects=Read(TEXT("Effects"),1,0,1);Music=Read(TEXT("Music"),1,0,1);
   auto General=SNew(SVerticalBox);
   General->AddSlot().AutoHeight().Padding(0,18)[Label(TEXT("控制与音频"),28)];
   General->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("语言 / Language"),Language)];
   AddSlider(General,TEXT("鼠标灵敏度"),&Sensitivity,.1f,3.f,false);
   AddSlider(General,TEXT("音效音量"),&Effects,0,1,true);
   AddSlider(General,TEXT("音乐音量"),&Music,0,1,true);
   General->AddSlot().AutoHeight().Padding(0,16)[Label(TEXT("音乐音量用于背景音乐；当前关卡尚未配置背景音乐。"),16)];
   General->AddSlot().AutoHeight().Padding(0,16)[SNew(SLocalizedTextBlock).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(bSaved?TEXT("设置已应用并保存。"):TEXT("调整后点击应用；关闭将放弃未应用的修改。"));})];
   General->AddSlot().AutoHeight()[Button(TEXT("应用并保存"),[this]
   {
    GConfig->SetFloat(Section,TEXT("Sensitivity"),Sensitivity,GameLocalSettings::Filename());
    GConfig->SetFloat(Section,TEXT("Effects"),Effects,GameLocalSettings::Filename());
    GConfig->SetFloat(Section,TEXT("Music"),Music,GameLocalSettings::Filename());
    GConfig->Flush(false,GameLocalSettings::Filename());GameFrontend::ApplyAudio(Player.Get());bSaved=true;
   })];
   General->AddSlot().AutoHeight().Padding(0,12)[Button(TEXT("返回"),Close)];
   GeneralPanel=SNew(SScrollBox)+SScrollBox::Slot()[General];
   PerformancePanel=GamePerformance::MakePanel(Close,Language);
   ChildSlot[SNew(SBox).WidthOverride(700).HeightOverride(740)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[Label(TEXT("设置"),30)]
     +SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,6,0)[Button(TEXT("控制与音频"),[this]{Host->SetContent(GeneralPanel.ToSharedRef());},18)]
      +SHorizontalBox::Slot().FillWidth(1).Padding(6,0,0,0)[Button(TEXT("性能与画质"),[this]{Host->SetContent(PerformancePanel.ToSharedRef());},18)]]
     +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Host,SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))[GeneralPanel.ToSharedRef()]]]];
  }
 private:
  TWeakObjectPtr<APlayerController> Player;
  TSharedPtr<SWidget> GeneralPanel,PerformancePanel;
  TSharedPtr<SBorder> Host;
  float Sensitivity=1,Effects=1,Music=1;bool bSaved=false;
  void AddSlider(TSharedRef<SVerticalBox> Rows,const TCHAR* Text,float* Value,float Min,float Max,bool Percent)
  {
   Rows->AddSlot().AutoHeight().Padding(0,16)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Label(Text)]
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SAudioSlider).Value_Lambda([Value,Min,Max]{return (*Value-Min)/(Max-Min);})
      .OnValueChanged_Lambda([this,Value,Min,Max](float V){*Value=Min+V*(Max-Min);bSaved=false;})]
     +SHorizontalBox::Slot().AutoWidth().Padding(16,0)[SNew(SBox).WidthOverride(80)[SNew(SLocalizedTextBlock).Text_Lambda([Value,Percent]{return FText::FromString(Percent?FString::Printf(TEXT("%.0f%%"),*Value*100):FString::Printf(TEXT("%.2f x"),*Value));})]]]];
  }
 };
}
float GameFrontend::Sensitivity(){return Read(TEXT("Sensitivity"),1,.1f,3.f);}
void GameFrontend::ApplyAudio(APlayerController* PC)
{
 if(!PC)return;
 // Per-controller mix belongs to its world. Root classes avoid multiplying the slider at every child.
 auto* Mix=FindObject<USoundMix>(PC,TEXT("FrontendVolumeMix"));
 if(!Mix){Mix=NewObject<USoundMix>(PC,TEXT("FrontendVolumeMix"));Mix->Duration=-1;Mix->FadeInTime=0;UGameplayStatics::PushSoundMixModifier(PC,Mix);}
 for(TObjectIterator<USoundClass> It;It;++It)
 {
  auto* Class=*It;if(Class->HasAnyFlags(RF_ClassDefaultObject)||Class->ParentClass)continue;
  UGameplayStatics::SetSoundMixClassOverride(PC,Mix,Class,Read(Class->Properties.bIsMusic?TEXT("Music"):TEXT("Effects"),1,0,1),1,0,true);
 }
}
void GameFrontend::ReleaseAudio(APlayerController* PC)
{
 if(PC)if(auto* Mix=FindObject<USoundMix>(PC,TEXT("FrontendVolumeMix")))UGameplayStatics::PopSoundMixModifier(PC,Mix);
}
TSharedRef<SWidget> GameFrontend::Settings(APlayerController* PC,TFunction<void()> Close,TFunction<void()> Language)
{return SNew(SFrontendSettings,PC,MoveTemp(Close),MoveTemp(Language));}
TSharedRef<SWidget> GameFrontend::Main(TFunction<void()> Start,TFunction<void()> Settings,TFunction<void()> Tutorial,TFunction<void()> History,TFunction<void()> Quit)
{
 return SNew(SBox).WidthOverride(520)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("DIRECTOR'S SHOT"),42)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,38)[Label(TEXT("观察 · 拍摄 · 带回你的故事"),20)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("开始游戏"),MoveTemp(Start))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("设置"),MoveTemp(Settings))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("使用教程"),MoveTemp(Tutorial))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("游玩记录"),MoveTemp(History))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("退出游戏"),MoveTemp(Quit))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,28,0,0)[Label(TEXT("在设置中选择语言"),16)]];
}
TSharedRef<SWidget> GameFrontend::Pause(TFunction<void()> Resume,TFunction<void()> Settings,TFunction<void()> Unstuck,TFunction<void()> Title,bool bUnstuckFailed)
{
 return SNew(SBox).WidthOverride(520)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("游戏菜单"),32)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[Label(TEXT("菜单打开时游戏继续运行。"),16,500)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("继续游戏"),MoveTemp(Resume))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("设置"),MoveTemp(Settings))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("脱离卡死"),MoveTemp(Unstuck))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,16)[Label(TEXT("返回本局出生点，保留当前生命值和物品。"),16,500)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Button(TEXT("返回标题界面"),MoveTemp(Title))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Label(TEXT("返回标题将结束本局，不进行撤离结算。"),16,500)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,12)[Label(bUnstuckFailed?TEXT("出生点被阻挡，暂时无法返回。请稍后重试。"):TEXT(""),16,500)]];
}
TSharedRef<SWidget> GameFrontend::Tutorial(TFunction<void()> Close)
{
 auto Rows=SNew(SVerticalBox);
 Rows->AddSlot().AutoHeight().Padding(0,0,0,20)[Label(TEXT("使用教程"),30)];
 const TCHAR* Keys[]={TEXT("W A S D"),TEXT("SHIFT / CTRL"),TEXT("SPACE"),TEXT("F / I"),TEXT("1  2  3  4"),TEXT("RMB → LMB"),TEXT("SCROLL / V"),TEXT("R / P")};
 const TCHAR* Texts[]={TEXT("移动"),TEXT("切换冲刺 / 蹲下"),TEXT("跳跃"),TEXT("拾取互动 / 背包"),TEXT("切换装备；滚轮也可切换"),TEXT("摄像机：举起取景器 → 开始或停止录像"),TEXT("取景时变焦 / 切换自拍"),TEXT("枪械装填 / 摄像机开关机")};
 for(int32 I=0;I<8;++I)
  Rows->AddSlot().AutoHeight().Padding(0,5)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(210)[SNew(SBorder).Padding(12).BorderBackgroundColor(FLinearColor(.08,.18,.22))[Label(Keys[I],18)]]]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(20,0)[Label(Texts[I],18,480)]];
 auto Steps=SNew(SHorizontalBox);
 const TCHAR* StepsText[]={TEXT("① 探索街区"),TEXT("② 拍摄事件"),TEXT("③ 车辆撤离")};
 for(int32 I=0;I<3;++I)Steps->AddSlot().FillWidth(1).Padding(8)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SGuideIcon).Kind(I)]
  +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,8)[Label(StepsText[I],20,230)]];
 Rows->AddSlot().AutoHeight().Padding(0,16)[Steps];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,18)[Label(TEXT("15 米内对准撤离车辆，按 F 启动撤离；留在范围内 60 秒，可转身还击。死亡得分减半，撤离保留全分。"),16)];
 Rows->AddSlot().AutoHeight()[Button(TEXT("返回"),MoveTemp(Close))];
 return SNew(SBox).WidthOverride(760).HeightOverride(700)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]];
}
