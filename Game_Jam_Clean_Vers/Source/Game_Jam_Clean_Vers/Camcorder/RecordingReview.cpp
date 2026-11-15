#include "RecordingReview.h"
#include "Audio/BackgroundMusic.h"
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
#include "Fonts/FontMeasure.h"
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
 const FLinearColor Paper(.92f,.89f,.8f),Gold(1.f,.65f,.12f),ReviewRed(.55f,.012f,.018f),Ink(.012f,.01f,.012f);
 constexpr float FilmWidth=820.f*2.f/3.f,FilmHeight=FilmWidth*9.f/16.f,FilmX=(1280-FilmWidth)*.5f,FilmY=96;
 constexpr float CountStart=.36f,CountDuration=1.2f,CountEnd=CountStart+CountDuration;
 constexpr float TypingDuration=.85f,IntertitleHold=3.f,IntertitleFade=.2f;
 float ImpactScale(float Age,float Duration=.25f,float Large=1.55f)
 {
  const float T=FMath::Clamp(Age/Duration,0.f,1.f);
  return T<.7f?FMath::Lerp(Large,.94f,1-FMath::Pow(1-T/.7f,3.f)):FMath::Lerp(.94f,1.f,1-FMath::Pow(1-(T-.7f)/.3f,2.f));
 }
 class SAwardRail final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SAwardRail){}SLATE_ATTRIBUTE(float,Progress)SLATE_END_ARGS()
  void Construct(const FArguments& A){Progress=A._Progress;SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(50,430);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& Style,bool)const override
  {
   auto Box=[&](float X,float Y,float W,float H,FLinearColor Color){if(W>0&&H>0)FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(FVector2D(W,H),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint());};
   const float P=FMath::Clamp(Progress.Get(),0.f,1.f);
   Box(22,24,6,382,FLinearColor(.21f,.025f,.025f));Box(22,406-P*382,6,P*382,Gold);
   for(int32 I=0;I<4;++I)Box(14,395-I*(382.f/3),22,22,P+UE_SMALL_NUMBER>=I/3.f?Gold:Paper*.25f);
   if(P>0&&P<1){Box(17,402-P*382,16,8,Gold*.6f);Box(12,399-P*382,26,14,FLinearColor(1,.65f,.12f,.18f));}
   return L;
  }
 private:TAttribute<float> Progress;
 };
 // Translate the complete sentence before revealing characters. Each line keeps its
 // final centered position, avoiding both translation of fragments and line reflow.
 class SIntertitleTypewriter final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SIntertitleTypewriter){}SLATE_ATTRIBUTE(FString,FullText)SLATE_ATTRIBUTE(float,Elapsed)SLATE_END_ARGS()
  void Construct(const FArguments& A){Source=A._FullText;Elapsed=A._Elapsed;SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(980,230);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& Style,bool)const override
  {
   const FString Full=Source.Get();const float Width=G.GetLocalSize().X;const auto Font=GameLanguage::Font(32,GameLanguage::EFontRole::Heading);
   const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
   if(Full!=Cached||!FMath::IsNearlyEqual(Width,CachedWidth))
   {
    Cached=Full;CachedWidth=Width;Lines.Reset();
    for(int32 Start=0;Start<Full.Len();)
    {
     int32 End=Start,Space=INDEX_NONE;
     while(End<Full.Len()&&Full[End]!='\n'&&Measure->Measure(Full.Mid(Start,End-Start+1),Font).X<=Width){if(Full[End]==' ')Space=End;++End;}
     if(End<Full.Len()&&Full[End]!='\n'&&Space>Start)End=Space;
     if(End==Start)++End;
     Lines.Add({Full.Mid(Start,End-Start),Start});Start=End;
     while(Start<Full.Len()&&(Full[Start]==' '||Full[Start]=='\n'))++Start;
    }
   }
   const float Age=Elapsed.Get(),LineHeight=Measure->Measure(TEXT("Ag"),Font).Y+12;
   const int32 Count=FMath::FloorToInt(Full.Len()*FMath::Clamp(Age/TypingDuration,0.f,1.f));
   const float Alpha=1-FMath::Clamp((Age-TypingDuration-IntertitleHold)/IntertitleFade,0.f,1.f);
   for(int32 I=0;I<Lines.Num();++I)
   {
    const auto& Line=Lines[I];const FString Typed=Line.Text.Left(FMath::Clamp(Count-Line.Start,0,Line.Text.Len()));
    if(!Typed.IsEmpty())FSlateDrawElement::MakeText(Out,L,G.ToPaintGeometry(G.GetLocalSize(),FSlateLayoutTransform(FVector2D((Width-Measure->Measure(Line.Text,Font).X)*.5f,(G.GetLocalSize().Y-LineHeight*Lines.Num())*.5f+I*LineHeight))),Typed,Font,ESlateDrawEffect::None,Paper*FLinearColor(1,1,1,Alpha)*Style.GetColorAndOpacityTint());
   }
   return L;
  }
 private:
  struct FLine{FString Text;int32 Start;};TAttribute<FString> Source;TAttribute<float> Elapsed;
  mutable FString Cached;mutable float CachedWidth=-1;mutable TArray<FLine> Lines;
 };
 TSharedRef<SLocalizedTextBlock> Label(const FString& Text,int32 Size=22,FLinearColor Color=Paper,float Wrap=0,GameLanguage::EFontRole Role=GameLanguage::EFontRole::Automatic)
 {
  return SNew(SLocalizedTextBlock).Text(FText::FromString(Text)).Font(GameLanguage::Font(Size,Role))
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
 // Kept local to the ending; gameplay buttons retain their existing presentation.
 class SEndingButton final:public SAudioButton
 {
 public:
  void Construct(const SButton::FArguments& A){SAudioButton::Construct(A);SetRenderTransformPivot(FVector2D(.5,.5));}
  virtual void Tick(const FGeometry& G,double Now,float DT)override
  {
   SAudioButton::Tick(G,Now,DT);const float Step=FMath::Clamp(DT,0.f,.04f);
   Focus=FMath::FInterpTo(Focus,IsHovered()||HasKeyboardFocus()?1.f:0.f,Step,16);
   const float Target=IsPressed()?.965f:1.f+.025f*Focus;
   Velocity+=(Target-Scale)*420.f*Step-Velocity*29.f*Step;Scale+=Velocity*Step;
   SetRenderTransform(FSlateRenderTransform(FScale2D(Scale),FVector2D(0,IsPressed()?2.f:-3.f*Focus)));
   if(Focus>.001f||FMath::Abs(Velocity)>.001f)Invalidate(EInvalidateWidgetReason::Paint);
  }
  virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& WidgetStyle,bool Enabled)const override
  {
   FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(G.GetLocalSize(),FSlateLayoutTransform(FVector2D(0,4+2*Focus))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(0,0,0,.2f)*WidgetStyle.GetColorAndOpacityTint());
   const int32 Top=SAudioButton::OnPaint(A,G,C,Out,L+1,WidgetStyle,Enabled);
   if(Focus>.001f)
    FSlateDrawElement::MakeBox(Out,Top+1,G.ToPaintGeometry(FVector2D((G.GetLocalSize().X-24)*Focus,2),FSlateLayoutTransform(FVector2D(12,G.GetLocalSize().Y-5))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(Gold.R,Gold.G,Gold.B,Focus)*WidgetStyle.GetColorAndOpacityTint());
   return Top+1;
  }
 private:float Focus=0,Scale=1,Velocity=0;
 };
 class SOverviewCard final:public SBorder
 {
 public:
  void Construct(const SBorder::FArguments& A){SBorder::Construct(A);}
  virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& Style,bool Enabled)const override
  {
   for(int32 I=3;I>0;--I)
    FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(G.GetLocalSize()+FVector2D(I*2,I*2),FSlateLayoutTransform(FVector2D(3-I,6-I))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(0,0,0,.10f)*Style.GetColorAndOpacityTint());
   return SBorder::OnPaint(A,G,C,Out,L+1,Style,Enabled);
  }
 };
 class SAwardImpact final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SAwardImpact){}SLATE_ATTRIBUTE(float,Age)SLATE_END_ARGS()
  void Construct(const FArguments& A){Age=A._Age;SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(340,446);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& Style,bool)const override
  {
   const float T=Age.Get();if(T<0||T>.5f)return L;
   const float Progress=T/.5f,Opacity=FMath::Sin(Progress*PI)*.65f;
   // One small burst around the award, not a full-panel flash or an idle loop.
   for(int32 I=0;I<8;++I)
   {
    const float Angle=(I+.5f)*PI/4;
    const FVector2D Direction(FMath::Cos(Angle),FMath::Sin(Angle)),CenterPoint(170,223);
    const FVector2D Start=CenterPoint+Direction*FVector2D(120+Progress*20,173+Progress*20);
    TArray<FVector2D> Points{Start,Start+Direction*(20*(1-Progress)+5)};
    FSlateDrawElement::MakeLines(Out,L,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor(Gold.R,Gold.G,Gold.B,Opacity)*Style.GetColorAndOpacityTint(),true,3);
   }
   return L;
  }
 private:TAttribute<float> Age;
 };
 TSharedRef<SWidget> TextButton(const FString& Text,TFunction<void()> Action,GameLanguage::EFontRole Role=GameLanguage::EFontRole::Emphasis)
 {
  return SNew(SEndingButton).ButtonStyle(FCoreStyle::Get(),"Button").ContentPadding(FMargin(12,3)).NormalPaddingOverride(FMargin(0)).PressedPaddingOverride(FMargin(0,1,0,0))
   .ButtonColorAndOpacity(Ink).HAlign(HAlign_Center).VAlign(VAlign_Center)
   .OnClicked_Lambda([Action]{Action();return FReply::Handled();})[Label(Text,18,Paper,0,Role)];
 }
 class SCinemaAtmosphere final : public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SCinemaAtmosphere){}SLATE_ATTRIBUTE(float,Elapsed)SLATE_END_ARGS()
  void Construct(const FArguments& Args){Elapsed=Args._Elapsed;SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(1280,720);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool)const override
  {
   const float Breath=.94f+.06f*FMath::Sin(Elapsed.Get(0.f)*.7f);
   const auto Tint=Style.GetColorAndOpacityTint();
   for(int32 I=20;I>0;--I)
   {
    const float Radius=I*2.5f;
    FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(FilmWidth+Radius*2,FilmHeight+Radius*2),FSlateLayoutTransform(FVector2D(FilmX-Radius,FilmY-Radius))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(.5f,.64f,.85f,.005f*Breath)*Tint);
   }
   // Low-opacity fan behind the audience suggests a projector without covering the film.
   for(int32 I=0;I<40;++I)
   {
    const float T=I/39.f;
    TArray<FVector2D> Beam{{640,720},{FilmX+FilmWidth*T,FilmY+FilmHeight+6}};
    FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Beam,ESlateDrawEffect::None,FLinearColor(.45f,.54f,.7f,.007f*Breath)*Tint,true,12);
   }
   return Layer;
  }
 private:TAttribute<float> Elapsed;
 };
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
 if(auto* L=Library.Get())if(auto* W=L->GetWorld())if(auto* Music=UBackgroundMusic::For(W->GetFirstPlayerController()))Music->SetStage(EBackgroundMusicStage::Ending);
 if(auto* L=Library.Get())for(int32 I=0;I<L->Clips.Num();++I)Thumbnails.Add(MakeShared<FThumbnail>());
 NextPop=.3f;LoadArt();Build();
}
void SRecordingReview::LoadArt()
{
 const TCHAR* Files[]={TEXT("Pause"),TEXT("Play"),TEXT("Speed1"),TEXT("Speed2"),TEXT("Speed4"),TEXT("Curtains"),TEXT("GoldShit"),TEXT("SilverBullet"),TEXT("OuchScar"),TEXT("Palme"),TEXT("Audience")};
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
 CurtainBrush=Art[5]->Brush;
 AudienceBrush=Art[10]->Brush;
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
float SRecordingReview::GetAnimatedFinalScore()const
{
 const auto* L=Library.Get();if(!L)return 0;
 const float T=FMath::Clamp((PhaseSeconds-CountStart)/CountDuration,0.f,1.f);
 return L->RoundScore*(1-FMath::Pow(1-T,3.f));
}
float SRecordingReview::GetAwardProgress()const{return FMath::Clamp(GetAnimatedFinalScore()/300.f,0.f,1.f);}
FString SRecordingReview::IntertitleFullText()const
{
 const auto* L=Library.Get();return GameLanguage::Display(L&&L->bEndedByDeath?TEXT("你的朋友帮你投递了 Gunnes。"):TEXT("你拍到了史上最伟大的影片，投递了 Gunnes。"));
}
FString SRecordingReview::GetIntertitleText()const
{
 const FString Full=IntertitleFullText();return Full.Left(FMath::FloorToInt(Full.Len()*FMath::Clamp(PhaseSeconds/TypingDuration,0.f,1.f)));
}
float SRecordingReview::PopScale(int32 Index)const
{
 return PopTimes.IsValidIndex(Index)?ImpactScale(OverviewSeconds-PopTimes[Index],.32f,1.7f):1.f;
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
   const int32 Index=Revealed++;LoadThumbnail(Index);PopTimes.Add(OverviewSeconds);LastPop=OverviewSeconds;
   NextPop=OverviewSeconds+FMath::Max(.055f,.2f/(1+Revealed*.07f));AppendOverviewCard(Index);
  }
  for(int32 I=0;I<OverviewCards.Num();++I)OverviewCards[I]->ForceVolatile(OverviewSeconds-PopTimes[I]<.34f);
  // The grid needs one completed layout pass before the scroll box can read
  // the new row's extent. Scroll the grid, not the card's temporary large transform.
  if(PendingOverviewScrollFrames>0&&--PendingOverviewScrollFrames==0&&OverviewScroll)
   OverviewScroll->ScrollDescendantIntoView(OverviewGrid,true,EDescendantScrollDestination::BottomOrRight,0);
  if(Revealed==L->Clips.Num()&&OverviewSeconds>LastPop+3.f)SetPhase(ERecordingReviewPhase::Intertitle);
 }
 else if(Phase==ERecordingReviewPhase::Intertitle&&PhaseSeconds>=TypingDuration+IntertitleHold+IntertitleFade)
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
 else if(Phase==ERecordingReviewPhase::Summary&&SummaryCanvas)SummaryCanvas->ForceVolatile(PhaseSeconds<3.f);
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
 if(Phase==ERecordingReviewPhase::Review&&E.GetEffectingButton()==EKeys::LeftMouseButton&&Local.X>=FilmX&&Local.X<=FilmX+FilmWidth&&Local.Y>=FilmY&&Local.Y<=FilmY+FilmHeight)
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
 OverviewCards.Reset();OverviewGrid=SNew(SUniformGridPanel).SlotPadding(FMargin(12,12));
 // Four invisible anchors reserve the columns before the first clip, so adding
 // a card never stretches or reflows the cards already on screen.
 for(int32 I=0;I<4;++I)OverviewGrid->AddSlot(I,0)[SNew(SBox).WidthOverride(252).HeightOverride(172.125f).Visibility(EVisibility::Hidden)];
 OverviewScroll=SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed).ConsumeMouseWheel(EConsumeMouseWheel::Always)
  +SScrollBox::Slot()[OverviewGrid.ToSharedRef()];
 for(int32 I=0;I<Revealed;++I)AppendOverviewCard(I);
 Put(C,OverviewScroll.ToSharedRef(),86,98,1108,516);
 Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(28)).Justification(ETextJustify::Center).ColorAndOpacity(Paper)
  .Text_Lambda([this]{auto* Lib=Library.Get();return FText::FromString(Lib&&Revealed==Lib->Clips.Num()?FString::Printf(TEXT("共 %d 段录像"),Lib->Clips.Num()):FString::Printf(TEXT("正在整理素材  %d / %d"),Revealed,Lib?Lib->Clips.Num():0));})),150,634,980,52);
 return C;
}
void SRecordingReview::AppendOverviewCard(int32 I)
{
 auto* L=Library.Get();if(!L||!OverviewGrid||!L->Clips.IsValidIndex(I))return;
  const auto& Clip=L->Clips[I];auto Card=SNew(SVerticalBox);
  Card->AddSlot().AutoHeight()[SNew(SBox).WidthOverride(242).HeightOverride(136.125f)
   [SNew(SOverlay)+SOverlay::Slot()[Fill(Ink)]
    +SOverlay::Slot()[SNew(SImage).Image(&Thumbnails[I]->Brush)]
    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(6)[Label(FString::Printf(TEXT("%04d"),I+1),16)]
    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Label(Thumbnails[I]->Texture.IsValid()?TEXT(""):TEXT("预览不可用"),16)]]];
  Card->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(26)[Center(Label(FString::Printf(TEXT("%02d:%02d"),int32(Clip.Seconds)/60,int32(Clip.Seconds)%60),18,Ink))]];
  auto AnimatedCard=SNew(SOverviewCard).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(5).BorderBackgroundColor(Paper)
   .RenderTransformPivot(FVector2D(.5f,.5f)).RenderTransform_Lambda([this,I]{return FSlateRenderTransform(FScale2D(PopScale(I)));})[Card];
  AnimatedCard->ForceVolatile(true);OverviewCards.Add(AnimatedCard);OverviewGrid->AddSlot(I%4,I/4)[AnimatedCard];
  if(I>=8&&I%4==0)PendingOverviewScrollFrames=2;
}
TSharedRef<SWidget> SRecordingReview::MakeIntertitle()
{
 auto C=SNew(SCanvas);Put(C,Fill(Ink),0,0,1280,720);
 auto Typewriter=SNew(SIntertitleTypewriter).FullText_Lambda([this]{return IntertitleFullText();}).Elapsed_Lambda([this]{return PhaseSeconds;});Typewriter->ForceVolatile(true);
 Put(C,Typewriter,150,245,980,230);
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
 auto C=SNew(SCanvas);Put(C,Fill(FLinearColor::Black),0,0,1280,720);
 Put(C,SNew(SCinemaAtmosphere).Elapsed_Lambda([this]{return PhaseSeconds;}),0,0,1280,720);
 Put(C,Fill(Paper*.7f),FilmX-5,FilmY-5,FilmWidth+10,FilmHeight+10);
 Put(C,SNew(SImage).Image(&VideoBrush),FilmX,FilmY,FilmWidth,FilmHeight);
 // Both supplied layers use their complete 1920x1080 canvas, with no UV crop.
 Put(C,SNew(SImage).Image(&CurtainBrush).Visibility(EVisibility::HitTestInvisible),0,0,1280,720);
 Put(C,SNew(SImage).Image(&AudienceBrush).Visibility(EVisibility::HitTestInvisible)
  .RenderTransformPivot(FVector2D(.5f,1)).RenderTransform_Lambda([this]
  {
   const float T=FMath::Clamp(PulseSeconds/.38f,0.f,1.f);
   const float Squash=1-.07f*FMath::Sin(T*PI)*FMath::Cos(T*PI*.5f);
   return FSlateRenderTransform(FScale2D(1+(1-Squash)*.2f,Squash));
  }),0,0,1280,720);
 Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(34)).ColorAndOpacity(Paper).Justification(ETextJustify::Center)
  .RenderTransformPivot(FVector2D(.5f,.5f)).RenderTransform_Lambda([this]{return FSlateRenderTransform(FScale2D(1+.055f*FMath::Max(0.f,1-PulseSeconds/.3f)));})
  .Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("当前得分  %d"),GetPresentedScore()));})),240,17,800,64);
 Put(C,SNew(SAudioSlider).IsFocusable(false).Value_Lambda([this]{return Duration>0?PlaybackSeconds/Duration:0.f;})
  .OnValueChanged_Lambda([this](float V){Seek(V);}),FilmX,FilmY+FilmHeight+12,FilmWidth,16);
 Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(16)).ColorAndOpacity(Paper)
  .Text_Lambda([this]{auto* Lib=Library.Get();return FText::FromString(FString::Printf(TEXT("录像 %02d / %02d    %02d:%02d / %02d:%02d"),SelectedClip+1,Lib?Lib->Clips.Num():0,int32(PlaybackSeconds)/60,int32(PlaybackSeconds)%60,int32(Duration)/60,int32(Duration)%60));})),FilmX,430,300,38);
 Put(C,SNew(SAudioButton).IsFocusable(false).HAlign(HAlign_Fill).VAlign(VAlign_Fill).ContentPadding(2).NormalPaddingOverride(FMargin(0)).PressedPaddingOverride(FMargin(0,1,0,0)).ToolTipText_Lambda([]{return GameLanguage::Text(TEXT("播放 / 暂停"));})
  .OnClicked_Lambda([this]{TogglePause();return FReply::Handled();})[SNew(SImage).Image_Lambda([this]{return &Art[bPlaying?0:1]->Brush;})],690,427,50,50);
 for(int32 I=0;I<3;++I)
 {
  const int32 Rate=I==0?1:I==1?2:4;
  Put(C,ArtButton(I+2,FString::Printf(TEXT("%d×"),Rate),[this,Rate]{SetPlaybackRate(Rate);},TAttribute<bool>::CreateLambda([this,Rate]{return GetPlaybackRate()==Rate;})),748+I*58,427,50,50);
 }
 for(bool Negative:{false,true})
 {
  const float X=Negative?648.f:304.f;
  const FLinearColor Accent=Negative?FLinearColor(1,.23f,.18f):Gold;
  Put(C,Fill(FLinearColor(.007f,.007f,.01f,.9f)),X,492,328,100);
  Put(C,Fill(Accent*.65f),X,492,328,2);
  Put(C,Center(SNew(SLocalizedTextBlock).Font_Lambda([this,Negative]{return GameLanguage::FittedFont(GameLanguage::Display(ScoreCaption(Negative)),28,306);}).Justification(ETextJustify::Center)
   .ColorAndOpacity(Accent).AutoWrapText(false)
   .Text_Lambda([this,Negative]{return FText::FromString(ScoreCaption(Negative));})),X+10,501,308,80);
 }
 if(!PlaybackError.IsEmpty())Put(C,Center(Label(PlaybackError,20,Gold,FilmWidth-30)),FilmX+15,180,FilmWidth-30,130);
 auto Buttons=SNew(SHorizontalBox);
 Buttons->AddSlot().FillWidth(1).Padding(4,0)[TextButton(TEXT("上一段"),[this]{PlayClip(FMath::Max(0,SelectedClip-1));})];
 Buttons->AddSlot().FillWidth(1).Padding(4,0)[TextButton(TEXT("下一段"),[this]{PlayClip(SelectedClip+1);})];
 Buttons->AddSlot().FillWidth(1).Padding(4,0)[TextButton(TEXT("跳过回看"),[this]{BeginScoreReveal();})];
 Put(C,Buttons,354,644,572,46);
 Put(C,Center(Label(TEXT("空格 暂停 · 1/2/4 倍速 · ←/→ 切换 · 按住影片 2×"),14)),240,695,800,23);
 return C;
}
TSharedRef<SWidget> SRecordingReview::MakeSummary()
{
 auto EndingText=[](const FString& Text,int32 Size=22,FLinearColor Color=Paper,float Wrap=0)
 {return Label(Text,Size,Color,Wrap,GameLanguage::EFontRole::Heading);};
 auto EndingButton=[](const FString& Text,TFunction<void()> Action)
 {return TextButton(Text,MoveTemp(Action),GameLanguage::EFontRole::Heading);};
 auto* L=Library.Get();auto C=SNew(SCanvas);Put(C,Fill(ReviewRed),0,0,1280,720);
 SummaryCanvas.Reset();
 auto Stamp=[this](TSharedRef<SWidget> Content,float Delay,float Angle=0.f)->TSharedRef<SWidget>
 {
  return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0)
   .ColorAndOpacity_Lambda([this,Delay]{return FLinearColor(1,1,1,FMath::Clamp((PhaseSeconds-Delay)/.035f,0.f,1.f));})
   .RenderTransformPivot(FVector2D(.5f,.5f)).RenderTransform_Lambda([this,Delay,Angle]
   {return Concatenate(FSlateRenderTransform(FScale2D(ImpactScale(PhaseSeconds-Delay))),FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Angle))));})[Content];
 };
 Put(C,Fill(Gold),48,42,5,54);auto Logo=EndingText(TEXT("Gunnes"),38);Logo->SetJustification(ETextJustify::Left);
 Put(C,Phase==ERecordingReviewPhase::Summary?Stamp(Logo,0):StaticCastSharedRef<SWidget>(Logo),72,40,280,60);
 if(Phase==ERecordingReviewPhase::ScoreReveal)
 {
  int32 Raw=0;for(const auto& Clip:L->Clips)Raw+=Clip.Score;
  Put(C,Center(EndingText(L->bEndedByDeath?TEXT("导演缺席，电影节表示遗憾。最终得分减半。"):TEXT("影片已送达，评审结果即将揭晓。"),28,Paper,1000)),140,190,1000,100);
  Put(C,Center(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(66)).ColorAndOpacity(Paper).Justification(ETextJustify::Center)
   .Text_Lambda([this,Raw]{auto* Lib=Library.Get();if(!Lib)return FText::GetEmpty();const float T=FMath::Clamp((PhaseSeconds-.6f)/1.4f,0.f,1.f);return FText::FromString(FString::Printf(TEXT("%.1f"),FMath::Lerp(float(Raw),Lib->RoundScore,T)));})),200,325,880,110);
  if(L->bEndedByDeath)Put(C,Center(EndingText(FString::Printf(TEXT("%d × 0.5"),Raw),26,Gold)),200,458,880,55);
  return C;
 }
 const int32 Tier=GetAwardTier();
 const TCHAR* Names[]={TEXT("金粪奖"),TEXT("银子弹奖"),TEXT("痛斯卡奖"),TEXT("屠戮金棕榈奖")};
 const TCHAR* Comments[]={TEXT("厉害！史上最烂的电影！"),TEXT("比那些冲奖片强多了。"),TEXT("真是一部直击脑门的电影！"),TEXT("黑帮巨作！")};
 Put(C,Stamp(Center(EndingText(TEXT("最终得分"),20)),.15f),72,276,430,38);
 auto Highlight=[this]{const float T=(PhaseSeconds-CountEnd)/.26f;return T>0&&T<1?FMath::Sin(PI*T):0.f;};
 auto Number=SNew(SLocalizedTextBlock).Justification(ETextJustify::Center)
  .Font_Lambda([this,Highlight]
  {
   auto Font=GameLanguage::FittedFont(FString::Printf(TEXT("%.1f"),GetAnimatedFinalScore()),68,430);
   Font.OutlineSettings.OutlineSize=FMath::RoundToInt(3*Highlight());Font.OutlineSettings.OutlineColor=Gold;return Font;
  })
  .ColorAndOpacity_Lambda([Highlight]{return FMath::Lerp(Paper,Gold,Highlight());})
  .RenderTransformPivot(FVector2D(.5f,.5f)).RenderTransform_Lambda([Highlight]{return FSlateRenderTransform(FScale2D(1+.065f*Highlight()));})
  .Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%.1f"),GetAnimatedFinalScore()));});
 Put(C,Stamp(Center(Number),.27f),72,318,430,110);
 Put(C,SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0)
  .BorderBackgroundColor_Lambda([Highlight]{return FLinearColor(Gold.R,Gold.G,Gold.B,.8f*Highlight());}),84,432,406,3);
 Put(C,Stamp(Center(EndingText(Names[Tier],28,Gold,438)),1.84f),60,446,460,115);
 Put(C,Stamp(Center(EndingText(Comments[Tier],24,FLinearColor::Black,394)),2.04f,-5.f),146,118,400,136);
 Put(C,SNew(SAwardImpact).Age_Lambda([this]{return PhaseSeconds-2.25f;}),565,130,340,446);
 auto Trophy=SNew(SOverlay)
  +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&Art[6+Tier]->Brush).ColorAndOpacity(FLinearColor(0,0,0,.24f)).RenderTransform(FSlateRenderTransform(FVector2D(7,9)))]]
  +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&Art[6+Tier]->Brush)]];
 Put(C,Stamp(Trophy,2.25f),565,130,340,446);
 Put(C,SNew(SAwardRail).Progress_Lambda([this]{return GetAwardProgress();}),1172,124,50,430);
 for(int32 I=0;I<4;++I)
 {
  const float Y=519-I*(382.f/3);
  auto Marker=Label(FString::Printf(TEXT("%s · %d"),Names[I],I*100),22,Paper,244,GameLanguage::EFontRole::Body);
  Marker->SetColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([this,I,Tier]{return FSlateColor(I==Tier&&PhaseSeconds>=CountEnd?Gold:Paper);}));
  Put(C,Center(Marker),920,Y-25,244,74);
  if(I==Tier)Put(C,SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0).VAlign(VAlign_Center)
   .ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,FMath::Clamp((PhaseSeconds-CountEnd)/.2f,0.f,1.f));})[EndingText(TEXT("◆"),20,Gold)],1212,Y-7,36,36);
 }
 auto Buttons=SNew(SHorizontalBox);
 Buttons->AddSlot().FillWidth(1).Padding(6,0)[EndingButton(TEXT("重新开始"),[this]{Navigate(false);})];
 Buttons->AddSlot().FillWidth(1).Padding(6,0)[EndingButton(TEXT("返回主菜单"),[this]{Navigate(true);})];
 Buttons->AddSlot().FillWidth(1).Padding(6,0)[EndingButton(TEXT("QUIT GAME"),[this]{if(auto* Lib=Library.Get())if(auto* PC=Cast<APlayerController>(Lib->GetOwner()))UKismetSystemLibrary::QuitGame(PC,PC,EQuitPreference::Quit,false);})];
 Put(C,Buttons,270,630,740,56);
 SummaryCanvas=C;SummaryCanvas->ForceVolatile(true);
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
    .ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,Phase==ERecordingReviewPhase::Summary?1.f:FMath::Clamp(PhaseSeconds/.3f,0.f,1.f));})[Body]]]]];
 if(bRestoreFocus)FSlateApplication::Get().SetKeyboardFocus(SharedThis(this),EFocusCause::SetDirectly);
}
