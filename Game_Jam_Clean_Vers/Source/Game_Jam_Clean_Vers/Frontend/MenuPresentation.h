#pragma once
#include "Audio/AudioWidgets.h"
#include "Localization/GameLanguage.h"
#include "Misc/Paths.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "Rendering/DrawElements.h"

// Opt-in presentation for our settings, pause, language and history panels only.
// Motion and palette follow TitleMenu's clapper buttons; that implementation stays untouched.
namespace MenuPresentation
{
 inline FLinearColor Paper(){return FLinearColor::FromSRGBColor(FColor(238,235,224));}
 inline FLinearColor Ink(){return FLinearColor::FromSRGBColor(FColor(26,30,27));}
 inline FLinearColor Surface(){return FLinearColor::FromSRGBColor(FColor(32,29,28));}
 inline FLinearColor Red(){return FLinearColor(.6f,.006f,.015f);}
 inline const FSliderStyle* SliderStyle()
 {
  static const FSliderStyle Slider=[]
  {
   FSlateColorBrush Bar(FLinearColor(.32f,.29f,.24f));Bar.ImageSize=FVector2D(8,4);
   FSlateColorBrush Thumb(FLinearColor::White);Thumb.ImageSize=FVector2D(6,18);
   return FSliderStyle().SetNormalBarImage(Bar).SetHoveredBarImage(Bar).SetDisabledBarImage(Bar)
    .SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb).SetBarThickness(3);
  }();return &Slider;
 }
 inline const FButtonStyle* ButtonStyle()
 {
  static const FButtonStyle Style=FButtonStyle().SetNormal(FSlateColorBrush(Paper()))
   .SetHovered(FSlateColorBrush(Paper()*FLinearColor(1,.88f,.66f)))
   .SetPressed(FSlateColorBrush(Paper()*FLinearColor(.83f,.75f,.61f)))
   .SetDisabled(FSlateColorBrush(FLinearColor(.2f,.19f,.17f)))
   .SetNormalForeground(Ink()).SetHoveredForeground(Ink()).SetPressedForeground(Ink())
   .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
  return &Style;
 }
 inline FSlateFontInfo Font(int32 Size)
 {
  if(GameLanguage::IsEnglish())return GameLanguage::Font(Size,Size>=28?GameLanguage::EFontRole::Heading:GameLanguage::EFontRole::Emphasis);
  // Chinese uses the same character-range font as HUD, subtitles and world labels.
  return GameLanguage::Font(Size);
 }
}
class SMenuTextBlock final:public SLocalizedTextBlock
{
public:
 void Construct(const STextBlock::FArguments& Args)
 {
  SLocalizedTextBlock::Construct(Args);
  const auto Requested=Args._Font;
  SetFont(TAttribute<FSlateFontInfo>::CreateLambda([Requested]{return MenuPresentation::Font(FMath::RoundToInt(Requested.Get(FCoreStyle::GetDefaultFontStyle("Regular",18)).Size));}));
 }
};
class SMenuButton final:public SAudioButton
{
public:
 void Construct(const SButton::FArguments& Args){auto A=Args;A.ButtonStyle(MenuPresentation::ButtonStyle());SAudioButton::Construct(A);SetRenderTransformPivot(FVector2D(.5,.5));}
 virtual void Tick(const FGeometry& G,double Time,float DT)override
 {
  SAudioButton::Tick(G,Time,DT);const float Step=FMath::Clamp(DT,0.f,.04f);
  Emphasis=FMath::FInterpTo(Emphasis,IsHovered()||HasKeyboardFocus()?1.f:0.f,Step,14.f);
  const float Target=IsPressed()?.965f:1.f+Emphasis*.018f;
  Velocity+=(Target-Scale)*420.f*Step-Velocity*29.f*Step;Scale+=Velocity*Step;
  SetRenderTransform(FSlateRenderTransform(FScale2D(Scale),FVector2D(0,IsPressed()?2.f:-3.f*Emphasis)));
 }
 virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& WidgetStyle,bool Enabled)const override
 {
  const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
  FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(G.GetLocalSize(),FSlateLayoutTransform(FVector2D(2,5+Emphasis*2))),Brush,ESlateDrawEffect::None,FLinearColor(0,0,0,.25f+.12f*Emphasis)*WidgetStyle.GetColorAndOpacityTint());
  const int32 Top=SAudioButton::OnPaint(A,G,C,Out,L+1,WidgetStyle,Enabled);
  if(Emphasis>.01f)FSlateDrawElement::MakeBox(Out,Top+1,G.ToPaintGeometry(FVector2D(3,20),FSlateLayoutTransform(FVector2D(8,(G.GetLocalSize().Y-20)*.5f))),Brush,ESlateDrawEffect::None,FLinearColor(.6f,.006f,.015f,Emphasis)*WidgetStyle.GetColorAndOpacityTint());
  return Top+1;
 }
private:float Emphasis=0,Scale=1,Velocity=0;
};
class SMenuEntrance final:public SBorder
{
public:
 void Construct(const SBorder::FArguments& Args)
 {
  auto A=Args;A.BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0).ForegroundColor(MenuPresentation::Paper());
  SBorder::Construct(A);SetRenderTransformPivot(FVector2D(.5,.5));
 }
 virtual void Tick(const FGeometry& G,double Time,float DT)override
 {
  SBorder::Tick(G,Time,DT);Age+=DT;const float T=FMath::Clamp(Age/.36f,0.f,1.f);
  const float Scale=T<.72f?FMath::Lerp(1.06f,.985f,1-FMath::Pow(1-T/.72f,3.f)):FMath::Lerp(.985f,1.f,1-FMath::Pow(1-(T-.72f)/.28f,2.f));
  SetRenderOpacity(FMath::Clamp(Age/.06f,0.f,1.f));SetRenderTransform(FSlateRenderTransform(FScale2D(Scale),FVector2D(-18,12)*FMath::Pow(1-T,3.f)));
 }
private:float Age=0;
};
