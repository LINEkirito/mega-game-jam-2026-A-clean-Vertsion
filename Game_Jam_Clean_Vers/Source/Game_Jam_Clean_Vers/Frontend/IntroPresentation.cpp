#include "IntroPresentation.h"
#include "Localization/GameLanguage.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "InputCoreTypes.h"

namespace
{
 const FVector2D CanvasSize(1280,720);
 const FLinearColor Paper=FLinearColor::FromSRGBColor(FColor(238,235,224));
 const FLinearColor Background=FLinearColor::FromSRGBColor(FColor(20,22,22));
 const FLinearColor Accent=FLinearColor::FromSRGBColor(FColor(190,33,43));
 constexpr float EntranceDuration=.45f,ExitDuration=.24f;

 float EaseOut(float T){return 1-FMath::Pow(1-FMath::Clamp(T,0.f,1.f),3.f);}
 float Stamp(float T)
 {
  T=FMath::Clamp(T,0.f,1.f);
  if(T<.62f)return FMath::Lerp(1.24f,.975f,EaseOut(T/.62f));
  return FMath::Lerp(.975f,1.f,EaseOut((T-.62f)/.38f));
 }

 // All image content uses the complete original RGBA canvas; this texture is also
 // reused for a small translucent silhouette shadow, never edited or cropped.
 class SIntroChrome final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SIntroChrome){}SLATE_ATTRIBUTE(int32,Page)SLATE_ATTRIBUTE(float,Progress)SLATE_END_ARGS()
  void Construct(const FArguments& A){Page=A._Page;Progress=A._Progress;SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return CanvasSize;}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool)const override
  {
   auto Box=[&](float X,float Y,float W,float H,FLinearColor Color)
   {
    FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(W,H),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint());
   };
   // A quiet, fixed border keeps page impacts local instead of flashing the screen.
   for(int32 I=0;I<10;++I)
   {
    const float A=.015f*(1-I/10.f);
    Box(I*4,0,4,720,FLinearColor(0,0,0,A));Box(1276-I*4,0,4,720,FLinearColor(0,0,0,A));
   }
   for(float X:{38.f,1218.f})for(float Y:{38.f,638.f})
   {
    const FLinearColor Line=Paper*FLinearColor(1,1,1,.12f);
    Box(X,Y,24,1,Line);Box(X+(X>600?23:0),Y+(Y>300?-23:0),1,24,Line);
   }
   for(int32 I=0;I<7;++I)
   {
    const float X=568+I*22;
    Box(X,673,14,2,Paper*FLinearColor(1,1,1,.12f));
    if(I<Page.Get())Box(X,673,14,2,Paper*FLinearColor(1,1,1,.5f));
    else if(I==Page.Get())Box(X,673,14*FMath::Clamp(Progress.Get(),0.f,1.f),2,Accent);
   }
   return Layer;
  }
 private:TAttribute<int32> Page;TAttribute<float> Progress;
 };

 // Wrapping is cached at complete line positions. The film title uses a real font
 // skew rather than altering the supplied wording or rebuilding partial sentences.
 class SIntroNarration final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SIntroNarration){}SLATE_ATTRIBUTE(FString,Text)SLATE_ATTRIBUTE(bool,Short)SLATE_END_ARGS()
  void Construct(const FArguments& A){Text=A._Text;bShort=A._Short;SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(960,390);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool)const override
  {
   const FString Full=GameLanguage::Display(Text.Get());
   const bool Short=bShort.Get();
   const auto Font=GameLanguage::Font(Short?48:38,Short?GameLanguage::EFontRole::Heading:GameLanguage::EFontRole::Body);
   const float Width=G.GetLocalSize().X;
   const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
   if(Full!=Cached||!FMath::IsNearlyEqual(Width,CachedWidth))
   {
    Cached=Full;CachedWidth=Width;Lines.Reset();
    for(int32 Start=0;Start<Full.Len();)
    {
     int32 End=Start,Space=INDEX_NONE;
     while(End<Full.Len()&&Full[End]!='\n'&&Measure->Measure(Full.Mid(Start,End-Start+1),Font).X<=Width)
     {if(Full[End]==' ')Space=End;++End;}
     if(End<Full.Len()&&Full[End]!='\n'&&Space>Start)End=Space;
     if(End==Start)++End;
     Lines.Add({Full.Mid(Start,End-Start),Start});Start=End;
     while(Start<Full.Len()&&(Full[Start]==' '||Full[Start]=='\n'))++Start;
    }
   }
   const FString Film=GameLanguage::Display(TEXT("《四百击》"));
   const int32 FilmStart=Full.Find(Film),FilmEnd=FilmStart+Film.Len();
   const float LineHeight=Measure->Measure(TEXT("Ag"),Font).Y+10;
   for(int32 I=0;I<Lines.Num();++I)
   {
    const auto& Line=Lines[I];
    const float X=(Width-Measure->Measure(Line.Text,Font).X)*.5f;
    const float Y=(G.GetLocalSize().Y-LineHeight*Lines.Num())*.5f+I*LineHeight;
    auto Draw=[&](int32 Start,int32 Count,bool Emphasized)
    {
     if(Count<=0)return;
     auto RunFont=Font;if(Emphasized)RunFont.SkewAmount=.17f;
     const float Offset=Measure->Measure(Line.Text.Left(Start),Font).X;
     FSlateDrawElement::MakeText(Out,Layer,G.ToPaintGeometry(G.GetLocalSize(),FSlateLayoutTransform(FVector2D(X+Offset,Y))),Line.Text.Mid(Start,Count),RunFont,ESlateDrawEffect::None,Paper*Style.GetColorAndOpacityTint());
     if(Emphasized)
      FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(Measure->Measure(Line.Text.Mid(Start,Count),Font).X,2),FSlateLayoutTransform(FVector2D(X+Offset,Y+LineHeight-7))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Accent*Style.GetColorAndOpacityTint());
    };
    const int32 From=FMath::Clamp(FilmStart-Line.Start,0,Line.Text.Len()),To=FMath::Clamp(FilmEnd-Line.Start,0,Line.Text.Len());
    if(FilmStart!=INDEX_NONE&&To>From){Draw(0,From,false);Draw(From,To-From,true);Draw(To,Line.Text.Len()-To,false);}
    else Draw(0,Line.Text.Len(),false);
   }
   return Layer;
  }
 private:
  struct FLine{FString Text;int32 Start;};TAttribute<FString> Text;TAttribute<bool> bShort;
  mutable FString Cached;mutable float CachedWidth=-1;mutable TArray<FLine> Lines;
 };
}

struct SIntroPresentation::FArt
{
 TStrongObjectPtr<UTexture2D> Texture;
 FSlateBrush Brush;
 explicit FArt(int32 Index)
 {
  const FString Path=FPaths::ProjectContentDir()/TEXT("UI/Intro")/FString::Printf(TEXT("Shot%02d.png"),Index+1);
  Texture.Reset(FImageUtils::ImportFileAsTexture2D(Path));
  if(Texture.IsValid())
  {
   Brush.SetResourceObject(Texture.Get());Brush.DrawAs=ESlateBrushDrawType::Image;
   Brush.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY());
  }
  else UE_LOG(LogTemp,Error,TEXT("Intro artwork missing: %s"),*Path);
 }
};

SIntroPresentation::~SIntroPresentation()=default;
void SIntroPresentation::Construct(const FArguments&,TFunction<void()> Complete)
{
 OnComplete=MoveTemp(Complete);
 for(int32 I=0;I<5;++I)Art.Add(MakeShared<FArt>(I));
 auto Canvas=SNew(SCanvas);
 auto Put=[&](TSharedRef<SWidget> Widget,float X,float Y,float W,float H)
 {Canvas->AddSlot().Position(FVector2D(X,Y)).Size(FVector2D(W,H))[Widget];};
 Put(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Background).Padding(0),0,0,1280,720);
 Put(SAssignNew(Chrome,SIntroChrome).Page_Lambda([this]{return PageIndex;})
  .Progress_Lambda([this]{return FMath::Clamp((PageElapsed-EntranceDuration)/HoldDuration(),0.f,1.f);}),0,0,1280,720);
 auto CurrentBrush=[this]() -> const FSlateBrush* {return &Art[FMath::Clamp(PageIndex-2,0,4)]->Brush;};
 Put(SAssignNew(StoryRoot,SOverlay)
  +SOverlay::Slot().Padding(0,11,0,-11)[SNew(SImage).Image_Lambda(CurrentBrush).ColorAndOpacity(FLinearColor(0,0,0,.42f))]
  +SOverlay::Slot()[SNew(SImage).Image_Lambda(CurrentBrush)],60,6,1160,652.5f);
 StoryRoot->SetVisibility(EVisibility::HitTestInvisible);StoryRoot->SetRenderTransformPivot(FVector2D(.5f,.5f));
 Put(SAssignNew(TextRoot,SIntroNarration).Text_Lambda([this]{return Narration();}).Short_Lambda([this]{return PageIndex==1;}),160,143,960,390);
 TextRoot->SetRenderTransformPivot(FVector2D(.5f,.5f));
 Put(SAssignNew(Caption,SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
  [SNew(SLocalizedTextBlock).Text_Lambda([this]{return FText::FromString(Subtitle());}).Font(GameLanguage::Font(21,GameLanguage::EFontRole::Body))
   .ColorAndOpacity(Paper).ShadowOffset(FVector2D(0,2)).ShadowColorAndOpacity(FLinearColor::Black).Justification(ETextJustify::Center).WrapTextAt(1080)],100,614,1080,52);
 Put(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
  [SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("单击继续"))).Font(GameLanguage::Font(16,GameLanguage::EFontRole::Body))
   .ColorAndOpacity(Paper*FLinearColor(1,1,1,.42f))],480,688,320,22);
 SetClipping(EWidgetClipping::ClipToBounds);
 ChildSlot[SNew(SBox).WidthOverride(1280).HeightOverride(720)[Canvas]];
 ForceVolatile(true);UpdatePresentation();
}

FString SIntroPresentation::Narration()const
{
 return PageIndex==0?TEXT("许多年之后，面对AI短视频，你将会回想起母亲带你去艺术电影院观看《四百击》的那个遥远的下午。"):
  TEXT("但现在嘛……");
}
FString SIntroPresentation::Subtitle()const
{
 if(GameLanguage::IsEnglish()||PageIndex<2)return FString();
 const TCHAR* Captions[]={TEXT("0 次播放"),TEXT("驱逐通知"),TEXT("别再拍晦涩电影了！多来点动作，否则你就被开除了！"),TEXT("……"),TEXT("看来，我得亲自动手了……")};
 return Captions[FMath::Clamp(PageIndex-2,0,4)];
}
float SIntroPresentation::HoldDuration()const{return PageIndex==0?8.f:PageIndex==1?2.f:3.f;}
float SIntroPresentation::ContentOpacity()const
{
 return FMath::Clamp(PageElapsed/(PageIndex<2?.32f:.1f),0.f,1.f)*(1-FMath::Clamp(ExitElapsed/ExitDuration,0.f,1.f));
}
void SIntroPresentation::UpdatePresentation()
{
 const bool Story=PageIndex>=2;
 TextRoot->SetVisibility(Story?EVisibility::Hidden:EVisibility::HitTestInvisible);
 StoryRoot->SetVisibility(Story?EVisibility::HitTestInvisible:EVisibility::Hidden);
 Caption->SetVisibility(Story&&!GameLanguage::IsEnglish()?EVisibility::HitTestInvisible:EVisibility::Hidden);
 const float T=FMath::Clamp(PageElapsed/EntranceDuration,0.f,1.f),Alpha=ContentOpacity();
 const float Exit=EaseOut(ExitElapsed/ExitDuration);
 TextRoot->SetRenderOpacity(Alpha);Caption->SetRenderOpacity(Alpha);StoryRoot->SetRenderOpacity(Alpha);
 TextRoot->SetRenderTransform(FSlateRenderTransform(FScale2D(FMath::Lerp(1.035f,1.f,EaseOut(T))),FVector2D(0,(1-EaseOut(T))*18-Exit*8)));
 // After the decisive impact, a very slow paper drift keeps the held panels alive.
 const float Drift=FMath::Clamp((PageElapsed-EntranceDuration)/HoldDuration(),0.f,1.f);
 const float Direction=PageIndex%2?1.f:-1.f;
 StoryRoot->SetRenderTransform(FSlateRenderTransform(FScale2D(Stamp(T)+Drift*.012f),FVector2D(Direction*((1-EaseOut(T))*28+Drift*3),-10*(1-EaseOut(T))-Exit*12)));
 Caption->SetRenderTransform(FSlateRenderTransform(FVector2D(0,(1-EaseOut(T))*10)));
 Invalidate(EInvalidateWidgetReason::Paint);
}
void SIntroPresentation::Tick(const FGeometry& Geometry,double CurrentTime,float DeltaTime)
{
 SCompoundWidget::Tick(Geometry,CurrentTime,DeltaTime);
 if(bFinished)return;
 // Do not let loading hitches consume readable hold time or swallow an entrance.
 const float Step=FMath::Clamp(DeltaTime,0.f,.08f);
 PageElapsed+=Step;
 if(!bLeaving&&PageElapsed>=EntranceDuration+HoldDuration())Advance();
 if(bLeaving)
 {
  ExitElapsed+=Step;
  if(ExitElapsed>=ExitDuration)
  {
   if(PageIndex==6)
   {
    bFinished=true;UpdatePresentation();
    // The callback may remove this widget from the hierarchy immediately.
    const auto KeepAlive=AsShared();auto Complete=MoveTemp(OnComplete);if(Complete)Complete();return;
   }
   ++PageIndex;PageElapsed=0;ExitElapsed=0;bLeaving=false;
  }
 }
 UpdatePresentation();
}
void SIntroPresentation::Advance()
{
 // Debouncing also prevents a double click from skipping two whole panels.
 if(!bFinished&&!bLeaving&&PageElapsed>=.18f){bLeaving=true;ExitElapsed=0;}
}
FReply SIntroPresentation::OnMouseButtonDown(const FGeometry&,const FPointerEvent& Event)
{
 if(Event.GetEffectingButton()==EKeys::LeftMouseButton){Advance();return FReply::Handled();}
 return FReply::Handled();
}
FReply SIntroPresentation::OnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent&){return FReply::Handled();}
FReply SIntroPresentation::OnKeyDown(const FGeometry&,const FKeyEvent& Event)
{
 if(!Event.IsRepeat()&&(Event.GetKey()==EKeys::Enter||Event.GetKey()==EKeys::SpaceBar))Advance();
 return FReply::Handled();
}
TSharedRef<SIntroPresentation> IntroPresentation::Make(TFunction<void()> Complete)
{
 return SNew(SIntroPresentation,MoveTemp(Complete));
}
