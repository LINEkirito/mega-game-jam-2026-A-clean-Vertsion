#pragma once
#include "Localization/GameLanguage.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "CamcorderEquipmentComponent.h"
#include "ShoulderCamcorder.h"

// Code-drawn camcorder overlay: all indicators reflect the live recording state.
class SCamcorderViewfinder : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCamcorderViewfinder) {}
        SLATE_ARGUMENT(TWeakObjectPtr<UCamcorderEquipmentComponent>, Equipment)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Equipment=Args._Equipment; SetVisibility(EVisibility::HitTestInvisible); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920,1080); }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto* E=Equipment.Get(); if(!E || !E->Camcorder || !E->Camcorder->bViewfinderRaised || E->bBackpackOpen) return Layer;
        const auto* C=E->Camcorder.Get(); const FVector2D Size=G.GetLocalSize();
        const float W=Size.X,H=Size.Y,S=FMath::Clamp(H/900.f,.6f,1.5f);
        const float Margin=55*S; const auto White=FLinearColor(.95f,.98f,.96f,.92f);
        const FLinearColor Battery=C->BatteryPercent<=10?FLinearColor(1,.15f,.1f):C->BatteryPercent<=20?FLinearColor(1,.45f,.12f):C->BatteryPercent<=50?FLinearColor(1,.8f,.2f):White;
        auto Box=[&](float X,float Y,float BW,float BH,FLinearColor Color)
        { FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(BW,BH),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color); };
        auto Text=[&](float X,float Y,const FString& Value,int32 FontSize,FLinearColor Color)
        { const FString Display=LanguageCache.Get(Value);FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(W,H),FSlateLayoutTransform(FVector2D(X,Y))),Display,GameLanguage::FittedFont(Display,FMath::RoundToInt(FontSize*S),W-X-20*S),ESlateDrawEffect::None,Color); };
        auto Line=[&](FVector2D A,FVector2D B,FLinearColor Color)
        { TArray<FVector2D> P{A,B}; FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,1.5f*S); };
        Text(Margin,Margin,C->bSelfie?TEXT("SELFIE  自拍"):TEXT("前拍"),22,White);
        Text(W*.5f-40*S,Margin,C->bRecording?TEXT("● REC"):TEXT("STBY"),30,C->bRecording?FLinearColor(1,.15f,.1f):White);
        Text(W*.5f-32*S,Margin+40*S,FString::Printf(TEXT("%02d:%02d"),int32(C->RecordingSeconds)/60,int32(C->RecordingSeconds)%60),18,White);
        const float BX=W-Margin-124*S,BY=Margin;
        Box(BX,BY,120*S,34*S,White); Box(BX+3*S,BY+3*S,114*S,28*S,FLinearColor(.015f,.02f,.02f,.9f)); Box(BX-5*S,BY+10*S,5*S,14*S,White);
        for(int32 I=0;I<16;++I) Box(BX+(6+I*6.8f)*S,BY+6*S,4.8f*S,22*S,I<C->BatteryPercent*.16f?Battery:FLinearColor(.15f,.17f,.16f));
        Text(BX,BY+45*S,FString::Printf(TEXT("%.0f%%"),C->BatteryPercent),18,Battery);
        if(C->BatteryPercent<=50) Text(BX-70*S,BY+76*S,TEXT("低电量 · 请节省拍摄时间"),17,Battery);
        const float CX=W*.5f,CY=H*.5f,DX=72*S,DY=42*S,L=22*S;
        for(float XSign:{-1.f,1.f}) for(float YSign:{-1.f,1.f})
        {
            const FVector2D Corner(CX+DX*XSign,CY+DY*YSign);
            Line(Corner,Corner+FVector2D(-XSign*L,0),FLinearColor(1,1,1,.45f));
            Line(Corner,Corner+FVector2D(0,-YSign*L),FLinearColor(1,1,1,.45f));
        }
        const float ZoomY=H-Margin-36*S;
        Text(CX-130*S,ZoomY-7*S,TEXT("W"),22,White); Text(CX+115*S,ZoomY-7*S,TEXT("T"),22,White);
        Box(CX-97*S,ZoomY,194*S,7*S,FLinearColor(.12f,.14f,.13f,.85f));
        Box(CX-97*S,ZoomY,194*S*(C->FocalLength-18.f)/82.f,7*S,White);
        Box(CX-100*S+194*S*(C->FocalLength-18.f)/82.f,ZoomY-3*S,6*S,13*S,White);
        Text(Margin,ZoomY-7*S,FString::Printf(TEXT("%.0f mm"),C->FocalLength),22,White);
        Text(W-Margin-260*S,ZoomY-7*S,TEXT("1280×720 / 15 FPS"),19,White);
        Text(Margin,H-Margin-18*S,TEXT("右键切换取景  ·  左键录像  ·  V 自拍/前拍  ·  滚轮变焦"),17,White);
        return Layer+2;
    }
private:
    FLocalizedDisplayCache LanguageCache;
    TWeakObjectPtr<UCamcorderEquipmentComponent> Equipment;
};
