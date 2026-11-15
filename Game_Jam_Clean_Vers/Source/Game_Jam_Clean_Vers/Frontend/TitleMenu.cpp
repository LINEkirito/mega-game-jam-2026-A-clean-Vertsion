#include "TitleMenu.h"
#include "Audio/AudioWidgets.h"
#include "Localization/GameLanguage.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Rendering/DrawElements.h"

namespace
{
 const FVector2D BoardSize(1043,745);
 const FLinearColor Paper=FLinearColor::FromSRGBColor(FColor(238,235,224));
 const FLinearColor Ink=FLinearColor::FromSRGBColor(FColor(26,30,27));
 struct FTitleArt
 {
  TStrongObjectPtr<UTexture2D> Texture;
  FSlateBrush Brush;
  explicit FTitleArt(const TCHAR* File)
  {
   const FString Path=FPaths::ProjectContentDir()/TEXT("UI/Title")/File;
   Texture.Reset(FImageUtils::ImportFileAsTexture2D(Path));
   if(Texture.IsValid())
   {
    Brush.SetResourceObject(Texture.Get());Brush.DrawAs=ESlateBrushDrawType::Image;
    Brush.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY());
   }
   else UE_LOG(LogTemp,Error,TEXT("Title artwork missing: %s"),*Path);
  }
 };
 class SClapperButton final:public SAudioButton
 {
 public:
  void Construct(const SButton::FArguments& Args){SAudioButton::Construct(Args);SetRenderTransformPivot(FVector2D(.5,.5));}
  virtual void Tick(const FGeometry& G,double Time,float DT) override
  {
   SAudioButton::Tick(G,Time,DT);
   Emphasis=FMath::FInterpTo(Emphasis,IsHovered()||HasKeyboardFocus()?1.f:0.f,DT,14.f);
   const float Scale=IsPressed()?.98f:1.f+Emphasis*.015f;
   SetRenderTransform(FSlateRenderTransform(FScale2D(Scale),FVector2D(0,IsPressed()?2.f:-2.f*Emphasis)));
  }
  virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& WidgetStyle,bool Enabled)const override
  {
   const int32 Top=SAudioButton::OnPaint(A,G,C,Out,L,WidgetStyle,Enabled);
   if(Emphasis>.01f)FSlateDrawElement::MakeBox(Out,Top+1,G.ToPaintGeometry(FVector2D(4,24),FSlateLayoutTransform(FVector2D(15,(G.GetLocalSize().Y-24)*.5f))),
    FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(.6f,.006f,.015f,Emphasis)*WidgetStyle.GetColorAndOpacityTint());
   return Top+1;
  }
 private:float Emphasis=0;
 };
 class STitleSplashes final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(STitleSplashes){}SLATE_ARGUMENT(const FSlateBrush*,Brush)SLATE_ATTRIBUTE(float,Elapsed)SLATE_END_ARGS()
  void Construct(const FArguments& A)
  {
   Elapsed=A._Elapsed;SetVisibility(EVisibility::HitTestInvisible);
   // Display regions share the untouched source texture; their final UVs tile the whole canvas.
   const FBox2f UVs[]={{{0,0},{1,.30f}},{{0,.30f},{.88f,1}},{{.88f,.30f},{1,1}}};
   for(const auto& UV:UVs){auto B=*A._Brush;B.SetUVRegion(UV);Regions.Add(B);}
  }
  virtual FVector2D ComputeDesiredSize(float)const override{return BoardSize;}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& Style,bool)const override
  {
   const float Time=Elapsed.Get(0);
   const FVector2D Pivots[]={{.89f,.08f},{.07f,.88f},{.99f,.97f}};
   const float Delays[]={.31f,.10f,.52f};
   for(int32 I=0;I<Regions.Num();++I)
   {
    const float T=FMath::Clamp((Time-Delays[I])/.38f,0.f,1.f);if(T<=0)continue;
    // A fast impact, brief overshoot and small settle reads as a splash rather than a fade.
    const float S=T<.48f?FMath::Lerp(.28f,1.055f,1-FMath::Pow(1-T/.48f,3.f)):
     FMath::Lerp(1.055f,1.f,1-FMath::Pow(1-(T-.48f)/.52f,2.f));
    const FBox2f UV(Regions[I].GetUVRegion());const FVector2D Position=FVector2D(UV.Min)*BoardSize,Size=FVector2D(UV.GetSize())*BoardSize;
    const FVector2D Pivot=Pivots[I]*BoardSize,Offset=Pivot+(Position-Pivot)*S;
    FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(Size*S,FSlateLayoutTransform(Offset)),&Regions[I],ESlateDrawEffect::None,
     FLinearColor(1,1,1,FMath::Min(T*16,1.f))*Style.GetColorAndOpacityTint());
   }
   return L;
  }
 private:TArray<FSlateBrush> Regions;TAttribute<float> Elapsed;
 };
 class SClapperRules final:public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SClapperRules){}SLATE_END_ARGS()
  void Construct(const FArguments&){SetVisibility(EVisibility::HitTestInvisible);}
  virtual FVector2D ComputeDesiredSize(float)const override{return BoardSize;}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& Style,bool)const override
  {
   auto Rule=[&](float X,float Y,float W,float H,FLinearColor Color){FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(FVector2D(W,H),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint());};
   const FLinearColor Line(.10f,.12f,.105f,.7f);
   Rule(48,184,430,1,Line);Rule(551,174,1,430,Line);Rule(48,474,430,1,Line);
   Rule(48,515,340,1,Line);Rule(48,549,250,1,Line);Rule(48,581,64,4,FLinearColor(.85f,.49f,.12f));
   return L;
  }
 };
 class SClapperTitle final:public SCompoundWidget
 {
 public:
  SLATE_BEGIN_ARGS(SClapperTitle){}SLATE_END_ARGS()
  void Construct(const FArguments&,TArray<TFunction<void()>> Actions)
  {
   Board=MakeUnique<FTitleArt>(TEXT("Clapperboard.png"));Blood=MakeUnique<FTitleArt>(TEXT("Blood.png"));Button=MakeUnique<FTitleArt>(TEXT("Button.png"));
   auto Base=Button->Brush;Base.DrawAs=ESlateBrushDrawType::Box;Base.Margin=FMargin(4.f/280,4.f/60);
   auto Hover=Base;Hover.TintColor=FLinearColor(1,.88f,.66f);auto Pressed=Base;Pressed.TintColor=FLinearColor(.83f,.75f,.61f);
   ButtonStyle=FButtonStyle().SetNormal(Base).SetHovered(Hover).SetPressed(Pressed).SetDisabled(Base).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
   auto Canvas=SNew(SCanvas);
   auto Put=[&](TSharedRef<SWidget> Widget,float X,float Y,float W,float H){Canvas->AddSlot().Position(FVector2D(X,Y)).Size(FVector2D(W,H))[Widget];};
   Put(SNew(SImage).Image(&Board->Brush).Visibility(EVisibility::HitTestInvisible),0,0,1043,745);
   Put(SNew(SClapperRules),0,0,1043,745);
   Put(SAssignNew(Splashes,STitleSplashes).Brush(&Blood->Brush).Elapsed_Lambda([this]{return Elapsed;}),0,0,1043,745);
   Splashes->ForceVolatile(true);
   const auto TitleFont=GameLanguage::FittedFont(TEXT("DIRECTOR'S"),70,430);
   for(int32 I=0;I<2;++I)Put(SNew(SBox).VAlign(VAlign_Center)[SNew(SLocalizedTextBlock).Font(TitleFont).Text(FText::FromString(I?TEXT("SHOT"):TEXT("DIRECTOR'S"))).ColorAndOpacity(Paper)],48,228+I*99,440,100);
   const TCHAR* Names[]={TEXT("开始游戏"),TEXT("设置"),TEXT("使用教程"),TEXT("游玩记录"),TEXT("退出游戏")};
   for(int32 I=0;I<5;++I)
   {
    Put(SNew(SClapperButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Center).VAlign(VAlign_Center).ContentPadding(FMargin(20,0))
     .OnClicked_Lambda([Action=MoveTemp(Actions[I])]{Action();return FReply::Handled();})
     [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(30,GameLanguage::EFontRole::Emphasis)).ColorAndOpacity(Ink).Text(FText::FromString(Names[I]))],608,225+I*84,384,60);
   }
   SetClipping(EWidgetClipping::ClipToBounds);
   ChildSlot[SNew(SBox).WidthOverride(1043).HeightOverride(745)[Canvas]];
  }
  virtual void Tick(const FGeometry& G,double Time,float DT)override
  {
   SCompoundWidget::Tick(G,Time,DT);
   // Slate time keeps playing while the title pauses the world. Clamp load hitches so
   // first-use texture/font loading cannot skip the entire entrance in one frame.
   if(Elapsed<1.f){Elapsed=FMath::Min(Elapsed+FMath::Clamp(DT,0.f,.08f),1.f);Splashes->Invalidate(EInvalidateWidgetReason::Paint);if(Elapsed>=1.f)Splashes->ForceVolatile(false);}
  }
  void FinishEntrance(){Elapsed=1;Splashes->ForceVolatile(false);Splashes->Invalidate(EInvalidateWidgetReason::Paint);}
 private:
  TUniquePtr<FTitleArt> Board,Blood,Button;TSharedPtr<STitleSplashes> Splashes;FButtonStyle ButtonStyle;float Elapsed=0;
 };
}
TSharedRef<SWidget> TitleMenu::Make(TFunction<void()> Start,TFunction<void()> Settings,TFunction<void()> Tutorial,TFunction<void()> History,TFunction<void()> Quit)
{
 TArray<TFunction<void()>> Actions;Actions.Add(MoveTemp(Start));Actions.Add(MoveTemp(Settings));Actions.Add(MoveTemp(Tutorial));Actions.Add(MoveTemp(History));Actions.Add(MoveTemp(Quit));
 return SNew(SClapperTitle,MoveTemp(Actions));
}
void TitleMenu::FinishEntrance(TSharedRef<SWidget> Title){StaticCastSharedRef<SClapperTitle>(Title)->FinishEntrance();}
