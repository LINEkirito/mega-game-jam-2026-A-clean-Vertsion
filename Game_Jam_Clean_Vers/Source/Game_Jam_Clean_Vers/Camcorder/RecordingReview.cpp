#include "RecordingReview.h"
#include "Audio/AudioWidgets.h"
#include "RecordingLibrary.h"
#include "Localization/GameLanguage.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSlider.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace
{
 const FLinearColor Paper(.92f,.89f,.8f),Gold(1.f,.65f,.12f),Red(.55f,.012f,.018f),Ink(.012f,.01f,.012f);
 TSharedRef<SLocalizedTextBlock> Label(const FString& Text,int32 Size=22,FLinearColor Color=Paper,float Wrap=0)
 {
  return SNew(SLocalizedTextBlock).Text(FText::FromString(Text)).Font(GameLanguage::Font(Size))
   .ColorAndOpacity(Color).Justification(ETextJustify::Center).AutoWrapText(false).WrapTextAt(Wrap);
 }
 TSharedRef<SWidget> Fill(FLinearColor Color)
 {
  return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Color).Padding(0);
 }
 void Put(TSharedRef<SCanvas> Canvas,TSharedRef<SWidget> Widget,float X,float Y,float W,float H)
 {
  Canvas->AddSlot().Position(FVector2D(X,Y)).Size(FVector2D(W,H))[Widget];
 }
 TSharedRef<SWidget> Center(TSharedRef<SWidget> Widget)
 {
  return SNew(SBox).HAlign(HAlign_Fill).VAlign(VAlign_Center)[Widget];
 }
 TSharedRef<SWidget> TextButton(const FString& Text,TFunction<void()> Action)
 {
  return SNew(SAudioButton).ButtonStyle(FCoreStyle::Get(),"Button").ContentPadding(FMargin(12,3)).NormalPaddingOverride(FMargin(0)).PressedPaddingOverride(FMargin(0,1,0,0))
   .ButtonColorAndOpacity(Ink).HAlign(HAlign_Center).VAlign(VAlign_Center)
   .OnClicked_Lambda([Action]{Action();return FReply::Handled();})[Label(Text,18)];
 }
 // Decoration uses the engine's white brush; all supplied artwork remains unmodified.
 class SFilmPerforations final : public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SFilmPerforations){}SLATE_END_ARGS()
  void Construct(const FArguments&){SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(1280,720);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool)const override
  {
   for(float Y=18;Y<710;Y+=32)for(float X:{15.f,1257.f})
    FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(8,12),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Paper*Style.GetColorAndOpacityTint());
   return Layer;
  }
 };
 FString Keyword(const FString& Category)
 {
  if(Category==TEXT("NPC首次入镜"))return TEXT("新面孔");
  if(Category==TEXT("NPC累计时长"))return TEXT("人物");
  if(Category==TEXT("NPC不同行为"))return TEXT("行动");
  if(Category==TEXT("事件连续入镜"))return TEXT("现场");
  if(Category==TEXT("死亡瞬间"))return TEXT("致命镜头");
  if(Category==TEXT("不同事件种类"))return TEXT("新事件");
  if(Category==TEXT("同类事件累计时长"))return TEXT("追踪");
  if(Category==TEXT("不同区域"))return TEXT("新地点");
  if(Category==TEXT("行人入镜扣分"))return TEXT("路人");
  if(Category==TEXT("警察入镜扣分"))return TEXT("警察");
  if(Category==TEXT("Logo入镜扣分"))return TEXT("商标");
  return Category;
 }
}

void SRecordingReview::Construct(const FArguments& A)
{
 Library=A._Library;
 if(auto* L=Library.Get())for(int32 I=0;I<L->Clips.Num();++I)Thumbnails.Add(MakeShared<FThumbnail>());
 LoadArt();Build();
}
void SRecordingReview::LoadArt()
{
 const TCHAR* Files[]={TEXT("Pause"),TEXT("Play"),TEXT("Speed1"),TEXT("Speed2"),TEXT("Speed4"),TEXT("Theater"),TEXT("GoldShit"),TEXT("SilverBullet"),TEXT("OuchScar"),TEXT("Palme")};
 for(const auto* File:Files)
 {
  auto Item=MakeShared<FThumbnail>();
  const FString Path=FPaths::ProjectContentDir()/TEXT("UI/Ending")/(FString(File)+TEXT(".png"));
  Item->Texture.Reset(FImageUtils::ImportFileAsTexture2D(Path));
  if(Item->Texture.IsValid())
  {
   Item->Brush.SetResourceObject(Item->Texture.Get());Item->Brush.DrawAs=ESlateBrushDrawType::Image;
   Item->Brush.ImageSize=FVector2D(Item->Texture->GetSizeX(),Item->Texture->GetSizeY());Item->bLoaded=true;
  }
  else UE_LOG(LogTemp,Error,TEXT("Ending artwork missing: %s"),*Path);
  Art.Add(Item);
 }
 CurtainBrush=Art[5]->Brush;CurtainBrush.SetUVRegion(FBox2f(FVector2f(0,0),FVector2f(1,.73f)));
 AudienceBrush=Art[5]->Brush;AudienceBrush.SetUVRegion(FBox2f(FVector2f(0,.73f),FVector2f(1,1)));
 // UV framing removes unused transparent margins without altering the supplied PNGs.
 const FBox2f Frames[]={{{.40f,.13f},{.61f,.89f}},{{.40f,.13f},{.60f,.89f}},{{.42f,.06f},{.63f,.89f}},{{.29f,.04f},{.73f,.95f}}};
 for(int32 I=0;I<4;++I)
 {
  Art[I+6]->Brush.SetUVRegion(Frames[I]);
  Art[I+6]->Brush.ImageSize=FVector2D(Frames[I].GetSize())*FVector2D(1920,1080);
 }
}
void SRecordingReview::LoadThumbnail(int32 Index)
{
 auto* L=Library.Get();if(!L||!Thumbnails.IsValidIndex(Index)||Thumbnails[Index]->bLoaded)return;
 auto& T=*Thumbnails[Index];T.bLoaded=true;FCamcorderPlayback Reader;TArray<uint8> Pixels;
 if(Reader.Open(L->Clips[Index].Path)&&Reader.ReadFrame(0,Pixels))
 {
  TArray64<uint8> Data;Data.Append(Pixels.GetData(),Pixels.Num());T.Texture.Reset(UTexture2D::CreateTransient(Reader.GetWidth(),Reader.GetHeight(),PF_B8G8R8A8,NAME_None,Data));
  if(T.Texture.IsValid()){T.Texture->SRGB=true;T.Texture->UpdateResource();T.Brush.SetResourceObject(T.Texture.Get());T.Brush.ImageSize=FVector2D(Reader.GetWidth(),Reader.GetHeight());T.Brush.DrawAs=ESlateBrushDrawType::Image;++LoadedThumbnails;}
 }
}
void SRecordingReview::SetPhase(ERecordingReviewPhase NewPhase){Phase=NewPhase;PhaseSeconds=0;Build();}
void SRecordingReview::PlayClip(int32 Index)
{
 auto* L=Library.Get();if(!L)return;
 if(!L->Clips.IsValidIndex(Index)){BeginScoreReveal();return;}
 SelectedClip=Index;PlaybackSeconds=0;LastFrame=INDEX_NONE;PlaybackError.Reset();bHoldFast=false;
 bPlaying=Playback.Open(L->Clips[Index].Path);Duration=bPlaying?Playback.NumFrames()/15.f:0;
 if(!bPlaying)PlaybackError=TEXT("录像无法读取，评分已保留。请选择下一段。");
 const int32 Width=bPlaying?Playback.GetWidth():640,Height=bPlaying?Playback.GetHeight():360;
 if(!VideoTexture.IsValid()||VideoTexture->GetSizeX()!=Width||VideoTexture->GetSizeY()!=Height)
 {
  TArray64<uint8> Black;Black.SetNumZeroed(Width*Height*4);for(int32 I=3;I<Black.Num();I+=4)Black[I]=255;
  VideoTexture.Reset(UTexture2D::CreateTransient(Width,Height,PF_B8G8R8A8,NAME_None,Black));VideoTexture->SRGB=true;VideoTexture->UpdateResource();VideoBrush.SetResourceObject(VideoTexture.Get());VideoBrush.ImageSize=FVector2D(Width,Height);VideoBrush.DrawAs=ESlateBrushDrawType::Image;
 }
 PreviousScore=GetPresentedScore();PulseSeconds=10;UpdateFrame();SetPhase(ERecordingReviewPhase::Review);
}
void SRecordingReview::BeginScoreReveal()
{
 bPlaying=false;bHoldFast=false;Playback.Close();SetPhase(ERecordingReviewPhase::ScoreReveal);
}
void SRecordingReview::ShowSummary(){bPlaying=false;bHoldFast=false;Playback.Close();SetPhase(ERecordingReviewPhase::Summary);}
void SRecordingReview::Seek(float Fraction)
{
 if(Duration<=0)return;PlaybackSeconds=FMath::Clamp(Fraction,0.f,1.f)*FMath::Max(0.f,Duration-1.f/15);UpdateFrame();PreviousScore=GetPresentedScore();PulseSeconds=10;
}
void SRecordingReview::UpdateFrame()
{
 if(!VideoTexture.IsValid()||Playback.NumFrames()==0)return;
 const int32 Frame=FMath::Clamp(FMath::FloorToInt(PlaybackSeconds*15),0,Playback.NumFrames()-1);if(Frame==LastFrame)return;
 TArray<uint8> Pixels;if(!Playback.ReadFrame(Frame,Pixels)){bPlaying=false;PlaybackError=TEXT("录像帧读取失败，评分记录仍保留。");return;}
 auto* Bytes=new uint8[Pixels.Num()];FMemory::Memcpy(Bytes,Pixels.GetData(),Pixels.Num());
 auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Playback.GetWidth(),Playback.GetHeight());VideoTexture->UpdateTextureRegions(0,1,Region,Playback.GetWidth()*4,4,Bytes,[](uint8* Data,const FUpdateTextureRegion2D* R){delete[] Data;delete R;});LastFrame=Frame;
}
int32 SRecordingReview::GetPresentedScore()const
{
 auto* L=Library.Get();if(!L)return 0;int32 Score=0;
 for(int32 I=0;I<L->Clips.Num();++I)
 {
  if(I<SelectedClip)Score+=L->Clips[I].Score;
  else if(I==SelectedClip)for(const auto& M:L->Clips[I].ScoreMoments)if(M.Seconds<=PlaybackSeconds)Score+=M.Points;
 }
 return Score;
}
int32 SRecordingReview::GetAwardTier()const
{
 const auto* L=Library.Get();const float Score=L?L->RoundScore:0;
 // A display badge, not a reward, score mutation, or persistent unlock. Negative scores use the no-award presentation.
 return Score>=300?3:Score>=200?2:Score>=100?1:0;
}
float SRecordingReview::PopScale(int32 Index)const
{
 const float T=FMath::Clamp((OverviewSeconds-PopTimes[Index])/.32f,0.f,1.f)-1;
 return 1+2.70158f*T*T*T+1.70158f*T*T;
}
void SRecordingReview::Tick(const FGeometry& G,double Now,float DT)
{
 SCompoundWidget::Tick(G,Now,DT);auto* L=Library.Get();if(!L)return;
 PhaseSeconds+=DT;PulseSeconds+=DT;
 if(Phase==ERecordingReviewPhase::Overview)
 {
  OverviewSeconds+=DT;
  if(Revealed<L->Clips.Num()&&OverviewSeconds>=NextPop)
  {
   LoadThumbnail(Revealed++);PopTimes.Add(OverviewSeconds);LastPop=OverviewSeconds;
   NextPop=OverviewSeconds+FMath::Max(.055f,.2f/(1+Revealed*.07f));Build();
  }
  if(Revealed==L->Clips.Num()&&OverviewSeconds>LastPop+1.5f)SetPhase(ERecordingReviewPhase::Intertitle);
 }
 else if(Phase==ERecordingReviewPhase::Intertitle&&PhaseSeconds>=3)
 {
  if(L->Clips.IsEmpty())BeginScoreReveal();else PlayClip(0);
 }
 else if(Phase==ERecordingReviewPhase::Review&&bPlaying)
 {
  PlaybackSeconds+=DT*GetPlaybackRate();
  if(PlaybackSeconds>=Duration){PlayClip(SelectedClip+1);return;}
  UpdateFrame();const int32 Score=GetPresentedScore();if(Score!=PreviousScore){PulseSeconds=0;PreviousScore=Score;}
 }
 else if(Phase==ERecordingReviewPhase::ScoreReveal&&PhaseSeconds>=(L->bEndedByDeath?3.2f:1.8f))ShowSummary();
}
FReply SRecordingReview::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
 if(E.IsRepeat())return FReply::Handled();const FKey K=E.GetKey();
 if(Phase==ERecordingReviewPhase::Review)
 {
  if(K==EKeys::SpaceBar)TogglePause();
  if(K==EKeys::One||K==EKeys::NumPadOne)SetPlaybackRate(1);
  if(K==EKeys::Two||K==EKeys::NumPadTwo)SetPlaybackRate(2);
  if(K==EKeys::Four||K==EKeys::NumPadFour)SetPlaybackRate(4);
  if(K==EKeys::Left)PlayClip(FMath::Max(0,SelectedClip-1));
  if(K==EKeys::Right)PlayClip(SelectedClip+1);
  if(K==EKeys::SpaceBar||K==EKeys::One||K==EKeys::NumPadOne||K==EKeys::Two||K==EKeys::NumPadTwo||K==EKeys::Four||K==EKeys::NumPadFour||K==EKeys::Left||K==EKeys::Right)return FReply::Handled();
 }
 // Leave Tab/Enter/navigation to Slate so every final-panel action is keyboard accessible.
 return FReply::Unhandled();
}
FReply SRecordingReview::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
 const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition()),Size=G.GetLocalSize();
 const float Scale=FMath::Min(Size.X/1280.f,Size.Y/720.f);if(Scale<=0)return FReply::Unhandled();
 const FVector2D Local=(P-(Size-FVector2D(1280,720)*Scale)*.5f)/Scale;
 if(Phase==ERecordingReviewPhase::Review&&E.GetEffectingButton()==EKeys::LeftMouseButton&&Local.X>=230&&Local.X<=1050&&Local.Y>=86&&Local.Y<=547)
 {bHoldFast=true;return FReply::Handled().CaptureMouse(SharedThis(this));}
 return FReply::Unhandled();
}
FReply SRecordingReview::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E)
{
 if(E.GetEffectingButton()==EKeys::LeftMouseButton&&bHoldFast){bHoldFast=false;PlaybackRate=1;return FReply::Handled().ReleaseMouseCapture();}return FReply::Unhandled();
}
TSharedRef<SWidget> SRecordingReview::ArtButton(int32 Index,const FString& Tooltip,TFunction<void()> Action,TAttribute<bool> Selected)
{
 return SNew(SAudioButton).IsFocusable(false).HAlign(HAlign_Fill).VAlign(VAlign_Fill).ContentPadding(4).NormalPaddingOverride(FMargin(0)).PressedPaddingOverride(FMargin(0,1,0,0)).ButtonColorAndOpacity_Lambda([Selected]{return Selected.Get()?Gold:FLinearColor(.08f,.06f,.055f);})
  .ToolTipText_Lambda([Tooltip]{return GameLanguage::Text(Tooltip);}).OnClicked_Lambda([Action]{Action();return FReply::Handled();})[SNew(SImage).Image(&Art[Index]->Brush)];
}
TSharedRef<SWidget> SRecordingReview::MakeOverview()
{
 auto* L=Library.Get();auto C=SNew(SCanvas);
 Put(C,Fill(FLinearColor(.027f,.025f,.025f)),0,0,1280,720);
 Put(C,SNew(SFilmPerforations),0,0,1280,720);
 Put(C,Center(Label(TEXT("素材回顾"),38)),160,20,960,62);
 auto Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(14,12));
 for(int32 I=0;I<Revealed;++I)
 {
  const auto& Clip=L->Clips[I];auto Card=SNew(SVerticalBox);
  Card->AddSlot().AutoHeight()[SNew(SBox).WidthOverride(242).HeightOverride(136.125f)
   [SNew(SOverlay)+SOverlay::Slot()[Fill(Ink)]
    +SOverlay::Slot()[SNew(SImage).Image(&Thumbnails[I]->Brush)]
    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(6)[Label(FString::Printf(TEXT("%04d"),I+1),16)]
    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Label(Thumbnails[I]->Texture.IsValid()?TEXT(""):TEXT("预览不可用"),16)]]];
  Card->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(26)[Center(Label(FString::Printf(TEXT("%02d:%02d"),int32(Clip.Seconds)/60,int32(Clip.Seconds)%60),18,Ink))]];
  Grid->AddSlot(I%4,I/4)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(5)
   .BorderBackgroundColor_Lambda([this,I]{auto Color=Paper;Color.A=FMath::Clamp((OverviewSeconds-PopTimes[I])/.12f,0.f,1.f);return Color;})
   .ColorAndOpacity_Lambda([this,I]{return FLinearColor(1,1,1,FMath::Clamp((OverviewSeconds-PopTimes[I])/.12f,0.f,1.f));})
   .RenderTransformPivot(FVector2D(.5f,.5f)).RenderTransform_Lambda([this,I]{return FSlateRenderTransform(FScale2D(PopScale(I)));})[Card]];
 }
 auto Scroll=SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed).ConsumeMouseWheel(EConsumeMouseWheel::Always)
  +SScrollBox::Slot()[Grid];
 if(Revealed>8)Scroll->ScrollToEnd();
 Put(C,Scroll,86,98,1108,516);
 Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(28)).Justification(ETextJustify::Center).ColorAndOpacity(Paper)
  .Text_Lambda([this]{auto* Lib=Library.Get();return FText::FromString(Lib&&Revealed==Lib->Clips.Num()?FString::Printf(TEXT("共 %d 段录像"),Lib->Clips.Num()):FString::Printf(TEXT("正在整理素材  %d / %d"),Revealed,Lib?Lib->Clips.Num():0));})),150,634,980,52);
 return C;
}
TSharedRef<SWidget> SRecordingReview::MakeIntertitle()
{
 auto* L=Library.Get();auto C=SNew(SCanvas);Put(C,Fill(Ink),0,0,1280,720);
 const FString Text=L->bEndedByDeath?TEXT("你的朋友帮你投递了 Gunnes。"):TEXT("你拍到了史上最伟大的影片，投递了 Gunnes。");
 Put(C,SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0).VAlign(VAlign_Center)
  .ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,FMath::Min(FMath::Clamp(PhaseSeconds/.5f,0.f,1.f),FMath::Clamp((3-PhaseSeconds)/.5f,0.f,1.f)));})[Label(Text,36,Paper,980)],150,245,980,230);
 return C;
}
FString SRecordingReview::ScoreCaption(bool bNegative)const
{
 auto* L=Library.Get();if(!L||!L->Clips.IsValidIndex(SelectedClip))return FString();
 FString Result;
 for(const auto& M:L->Clips[SelectedClip].ScoreMoments)
  if((M.Points<0)==bNegative&&M.Seconds<=PlaybackSeconds&&PlaybackSeconds-M.Seconds<1.8f*GetPlaybackRate())
  {
   Result=FString::Printf(TEXT("%+d  %s"),M.Points,*Keyword(M.Category));
  }
 return Result;
}
TSharedRef<SWidget> SRecordingReview::MakeTheater()
{
 auto* L=Library.Get();auto C=SNew(SCanvas);Put(C,Fill(FLinearColor::Black),0,0,1280,720);
 Put(C,Fill(Paper),222,78,836,477);
 Put(C,SNew(SImage).Image(&VideoBrush),230,86,820,461);
 // Split only the brush UVs: the lower part can squash while the curtain stays still.
 Put(C,SNew(SImage).Image(&CurtainBrush).Visibility(EVisibility::HitTestInvisible),0,0,1280,525.6f);
 Put(C,SNew(SImage).Image(&AudienceBrush).Visibility(EVisibility::HitTestInvisible)
  .RenderTransformPivot(FVector2D(.5f,1)).RenderTransform_Lambda([this]
  {
   const float T=FMath::Clamp(PulseSeconds/.38f,0.f,1.f);
   const float Squash=1-.07f*FMath::Sin(T*PI)*FMath::Cos(T*PI*.5f);
   return FSlateRenderTransform(FScale2D(1+(1-Squash)*.2f,Squash));
  }),0,525.6f,1280,194.4f);
 Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(34)).ColorAndOpacity(Paper).Justification(ETextJustify::Center)
  .RenderTransformPivot(FVector2D(.5f,.5f)).RenderTransform_Lambda([this]{return FSlateRenderTransform(FScale2D(1+.055f*FMath::Max(0.f,1-PulseSeconds/.3f)));})
  .Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("当前得分  %d"),GetPresentedScore()));})),230,14,820,58);
 Put(C,SNew(SAudioSlider).IsFocusable(false).Value_Lambda([this]{return Duration>0?PlaybackSeconds/Duration:0.f;})
  .OnValueChanged_Lambda([this](float V){Seek(V);}),236,558,510,22);
 Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(16)).ColorAndOpacity(Paper)
  .Text_Lambda([this]{auto* Lib=Library.Get();return FText::FromString(FString::Printf(TEXT("录像 %02d / %02d    %02d:%02d / %02d:%02d"),SelectedClip+1,Lib?Lib->Clips.Num():0,int32(PlaybackSeconds)/60,int32(PlaybackSeconds)%60,int32(Duration)/60,int32(Duration)%60));})),236,581,510,26);
 // The pause/play artwork switches live without rebuilding the widget or losing slider capture.
 Put(C,SNew(SAudioButton).IsFocusable(false).HAlign(HAlign_Fill).VAlign(VAlign_Fill).ContentPadding(2).NormalPaddingOverride(FMargin(0)).PressedPaddingOverride(FMargin(0,1,0,0)).ToolTipText_Lambda([]{return GameLanguage::Text(TEXT("播放 / 暂停"));})
  .OnClicked_Lambda([this]{TogglePause();return FReply::Handled();})[SNew(SImage).Image_Lambda([this]{return &Art[bPlaying?0:1]->Brush;})],788,558,54,54);
 for(int32 I=0;I<3;++I)
 {
  const int32 Rate=I==0?1:I==1?2:4;
  Put(C,ArtButton(I+2,FString::Printf(TEXT("%d×"),Rate),[this,Rate]{SetPlaybackRate(Rate);},TAttribute<bool>::CreateLambda([this,Rate]{return GetPlaybackRate()==Rate;})),858+I*62,558,54,54);
 }
 for(bool Negative:{false,true})Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(24)).Justification(ETextJustify::Center)
  .ColorAndOpacity(Negative?FLinearColor(1,.23f,.18f):Gold)
  .Text_Lambda([this,Negative]{return FText::FromString(ScoreCaption(Negative));})),Negative?666:264,614,400,42);
 if(!PlaybackError.IsEmpty())Put(C,Center(Label(PlaybackError,22,Gold,740)),270,255,740,100);
 auto Buttons=SNew(SHorizontalBox);
 Buttons->AddSlot().FillWidth(1).Padding(4,0)[TextButton(TEXT("上一段"),[this]{PlayClip(FMath::Max(0,SelectedClip-1));})];
 Buttons->AddSlot().FillWidth(1).Padding(4,0)[TextButton(TEXT("下一段"),[this]{PlayClip(SelectedClip+1);})];
 Buttons->AddSlot().FillWidth(1).Padding(4,0)[TextButton(TEXT("跳过回看"),[this]{BeginScoreReveal();})];
 Put(C,Buttons,778,661,458,42);
 Put(C,Center(Label(TEXT("空格 暂停 · 1/2/4 倍速 · ←/→ 切换 · 按住影片 2×"),14)),68,680,700,28);
 return C;
}
TSharedRef<SWidget> SRecordingReview::MakeSummary()
{
 auto* L=Library.Get();auto C=SNew(SCanvas);Put(C,Fill(Red),0,0,1280,720);
 Put(C,Fill(Gold),48,42,5,54);auto Logo=Label(TEXT("Gunnes"),38);Logo->SetJustification(ETextJustify::Left);Put(C,Logo,72,40,192,60);
 Put(C,Label(TEXT("电影节 · 本届评选"),12),48,108,360,32);
 if(Phase==ERecordingReviewPhase::ScoreReveal)
 {
  int32 Raw=0;for(const auto& Clip:L->Clips)Raw+=Clip.Score;
  Put(C,Center(Label(L->bEndedByDeath?TEXT("导演缺席，电影节表示遗憾。最终得分减半。"):TEXT("影片已送达，评审结果即将揭晓。"),28,Paper,1000)),140,190,1000,100);
  Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(66)).ColorAndOpacity(Paper).Justification(ETextJustify::Center)
   .Text_Lambda([this,Raw]{auto* Lib=Library.Get();if(!Lib)return FText::GetEmpty();const float T=FMath::Clamp((PhaseSeconds-.6f)/1.4f,0.f,1.f);return FText::FromString(FString::Printf(TEXT("%.1f"),FMath::Lerp(float(Raw),Lib->RoundScore,T)));})),200,325,880,110);
  if(L->bEndedByDeath)Put(C,Center(Label(FString::Printf(TEXT("%d × 0.5"),Raw),26,Gold)),200,458,880,55);
  return C;
 }
 const int32 Tier=GetAwardTier();
 const TCHAR* Names[]={TEXT("金粪奖"),TEXT("银子弹奖"),TEXT("痛斯卡奖"),TEXT("屠戮金棕榈奖")};
 const TCHAR* Comments[]={TEXT("厉害！史上最烂的电影！"),TEXT("比那些冲奖片强多了。"),TEXT("真是一部直击脑门的电影！"),TEXT("黑帮巨作！")};
 Put(C,Label(TEXT("最终得分"),20),72,192,430,38);
 Put(C,Center(Label(FString::Printf(TEXT("%.1f"),L->RoundScore),68)),72,234,430,100);
 Put(C,Center(Label(Names[Tier],28,Gold,414)),72,344,430,110);
 Put(C,Center(Label(Comments[Tier],24,Paper,426)),74,476,426,112);
 Put(C,SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0)
  .ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,FMath::Clamp(PhaseSeconds/.45f,0.f,1.f));})
  .RenderTransformPivot(FVector2D(.5f,.8f)).RenderTransform_Lambda([this]{const float T=FMath::Clamp(PhaseSeconds/.65f,0.f,1.f)-1;return FSlateRenderTransform(FScale2D(1+2.70158f*T*T*T+1.70158f*T*T));})
  [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&Art[6+Tier]->Brush)]],565,130,340,446);
 // Four thresholds rise from the no-award state. Fractional scores are not rounded for badge selection.
 Put(C,Fill(FLinearColor(.21f,.025f,.025f)),1194,148,6,382);
 const float Progress=FMath::Clamp(L->RoundScore/300.f,0.f,1.f)*382;
 Put(C,Fill(Gold),1194,530-Progress,6,Progress);
 for(int32 I=0;I<4;++I)
 {
  const float Y=519-I*(382.f/3);
  Put(C,Fill(I<=Tier?Gold:Paper*.25f),1186,Y,22,22);
  auto Marker=Label(FString::Printf(TEXT("%s · %d"),Names[I],I*100),14,I==Tier?Gold:Paper);Marker->SetWrapTextAt(228);Marker->SetAutoWrapText(false);
  Put(C,Center(Marker),934,Y-15,232,54);
  if(I==Tier)Put(C,Center(Label(TEXT("◆"),20,Gold)),1212,Y-7,36,36);
 }
 auto Buttons=SNew(SHorizontalBox);
 Buttons->AddSlot().FillWidth(1).Padding(6,0)[TextButton(TEXT("重新开始"),[this]{Navigate(false);})];
 Buttons->AddSlot().FillWidth(1).Padding(6,0)[TextButton(TEXT("返回主菜单"),[this]{Navigate(true);})];
 Buttons->AddSlot().FillWidth(1).Padding(6,0)[TextButton(TEXT("退出游戏"),[this]{if(auto* Lib=Library.Get())if(auto* PC=Cast<APlayerController>(Lib->GetOwner()))UKismetSystemLibrary::QuitGame(PC,PC,EQuitPreference::Quit,false);})];
 Put(C,Buttons,270,630,740,56);
 return C;
}
void SRecordingReview::Navigate(bool bTitle)
{
 auto* L=Library.Get();if(!L)return;
 if(bTitle){L->ReturnToTitle();return;}
 if(auto* PC=Cast<APlayerController>(L->GetOwner()))
 {
  PC->SetPause(false);PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();
  UGameplayStatics::OpenLevel(L,FName(*UGameplayStatics::GetCurrentLevelName(L,true)),true,TEXT("StartRun"));
 }
}
void SRecordingReview::Build()
{
 if(!Library.IsValid())return;
 const bool bRestoreFocus=HasKeyboardFocus()||HasFocusedDescendants();
 TSharedRef<SWidget> Body=Phase==ERecordingReviewPhase::Overview?MakeOverview():Phase==ERecordingReviewPhase::Intertitle?MakeIntertitle():Phase==ERecordingReviewPhase::Review?MakeTheater():MakeSummary();
 ChildSlot[SNew(SOverlay)+SOverlay::Slot()[Fill(FLinearColor::Black)]
  +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1280).HeightOverride(720)
   [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0)
    .ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,FMath::Clamp(PhaseSeconds/.3f,0.f,1.f));})[Body]]]]];
 if(bRestoreFocus)FSlateApplication::Get().SetKeyboardFocus(SharedThis(this),EFocusCause::SetDirectly);
}
