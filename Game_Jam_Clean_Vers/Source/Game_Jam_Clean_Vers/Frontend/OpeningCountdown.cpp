#include "OpeningCountdown.h"
#include "Localization/GameLanguage.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"

void SOpeningCountdown::Construct(const FArguments&)
{
 SetVisibility(EVisibility::HitTestInvisible);
 SetCanTick(false);
 ForceVolatile(true);
}
void SOpeningCountdown::SetElapsed(float Seconds)
{
 Elapsed=Seconds;
 Invalidate(EInvalidateWidgetReason::Paint);
}
int32 SOpeningCountdown::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool) const
{
 const bool Action=Elapsed>=3.f;
 const float T=Action?Elapsed-3.f:FMath::Frac(Elapsed);
 const float Fade=1-FMath::Clamp((T-(Action?.38f:.62f))/(Action?.3f:.3f),0.f,1.f);
 if(Fade<=0)return Layer;
 const float Impact=T<.15f?FMath::Lerp(Action?1.6f:2.25f,.92f,1-FMath::Pow(1-T/.15f,3.f)):
  FMath::Lerp(.92f,1.f,FMath::Clamp((T-.15f)/.12f,0.f,1.f));
 const FVector2D Center=G.GetLocalSize()*.5f;
 const FLinearColor Cream(.96f,.93f,.84f,Fade),Red(.6f,.015f,.035f,Fade);
 const FString Text=Action?GameLanguage::Display(TEXT("开拍！")):FString::FromInt(3-FMath::FloorToInt(Elapsed));
 const FSlateFontInfo Font=GameLanguage::Font(Action?100:144,GameLanguage::EFontRole::Heading);
 const FVector2D TextSize=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text,Font);
 const FVector2D Position=Center-TextSize*Impact*.5f;
 auto DrawText=[&](FVector2D Offset,FLinearColor Color,int32 Z)
 {
  FSlateDrawElement::MakeText(Out,Z,G.ToPaintGeometry(TextSize,FSlateLayoutTransform(Impact,Position+Offset)),Text,Font,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint());
 };
 // The short film-leader rings stay local to the numeral, never flashing the viewport.
 if(!Action)
 {
  for(int32 Ring=0;Ring<2;++Ring)
  {
   TArray<FVector2D> Points;
   const float Radius=(Ring?116.f:110.f)*(1+.1f*FMath::Exp(-T*12));
   for(int32 I=0;I<=64;++I){const float A=I*2*PI/64;Points.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius);}
   FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor(0,0,0,.32f*Fade),true,3.f);
  }
  TArray<FVector2D> Arc;
  const int32 Steps=FMath::Max(2,FMath::CeilToInt((1-T)*64));
  for(int32 I=0;I<=Steps;++I){const float A=-PI*.5f+(1-T)*2*PI*I/Steps;Arc.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*113.f);}
  FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Arc,ESlateDrawEffect::None,FLinearColor(.96f,.93f,.84f,.55f*Fade),true,2.f);
 }
 DrawText(FVector2D(7,10),FLinearColor(0,0,0,.6f*Fade),Layer+2);
 DrawText(FVector2D(3,4),Red,Layer+3);
 DrawText(FVector2D::ZeroVector,Cream,Layer+4);
 return Layer+4;
}
