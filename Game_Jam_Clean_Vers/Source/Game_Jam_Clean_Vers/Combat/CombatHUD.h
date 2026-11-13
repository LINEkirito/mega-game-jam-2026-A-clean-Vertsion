#pragma once
#include "Widgets/SLeafWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "CoreGlobals.h"
class SCombatHUD : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCombatHUD){} SLATE_ARGUMENT(TWeakObjectPtr<UCombatEquipmentComponent>,Combat) SLATE_END_ARGS()
    void Construct(const FArguments& A){Combat=A._Combat;
        BloodTexture.Reset(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Combat/Effects/Blood/T_BloodScreen")));
        SplatterTexture.Reset(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Combat/Effects/Blood/T_BloodStain")));
        BloodBrush.SetResourceObject(BloodTexture.Get());BloodBrush.DrawAs=ESlateBrushDrawType::Image;
        SplatterBrush.SetResourceObject(SplatterTexture.Get());SplatterBrush.DrawAs=ESlateBrushDrawType::Image;
        SetVisibility(EVisibility::HitTestInvisible);}
    virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
    {
        SLeafWidget::Tick(Geometry, CurrentTime, DeltaTime);
        const double Now = FPlatformTime::Seconds();
        if (SampleStartSeconds == 0.0)
        {
            SampleStartSeconds = Now;
            SampleStartFrame = GFrameCounter;
        }
        const double Elapsed = Now - SampleStartSeconds;
        if (Elapsed >= 0.5)
        {
            // Wall-clock engine frames: unaffected by gameplay time dilation or recording FPS.
            FrameRateText = FString::Printf(TEXT("FPS %.0f"), double(GFrameCounter - SampleStartFrame) / Elapsed);
            SampleStartSeconds = Now;
            SampleStartFrame = GFrameCounter;
        }
    }
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(1920,1080);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool)const override
    {
        auto* E=Combat.Get();auto* PC=E?Cast<APlayerController>(E->GetOwner()):nullptr;auto* C=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;if(!C)return L;
        const FVector2D SZ=G.GetLocalSize();const float W=SZ.X,H=SZ.Y,S=FMath::Clamp(H/900.f,.65f,1.4f);
        auto Box=[&](float X,float Y,float BW,float BH,FLinearColor Color){FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(FVector2D(BW,BH),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);};
        auto Text=[&](float X,float Y,const FString& T,int32 Size,FLinearColor Col=FLinearColor::White){FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(SZ,FSlateLayoutTransform(FVector2D(X,Y))),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::RoundToInt(Size*S)),ESlateDrawEffect::None,Col);};
        const float Red=FMath::Max(C->DamageFlash>0?.45f:0.f,C->Health<=30?.15f:0.f);
        if(Red>0)for(int32 I=0;I<20;++I){const float A=Red*(1.f-I/20.f);Box(I*3,0,3,H,FLinearColor(.7f,0,0,A));Box(W-I*3-3,0,3,H,FLinearColor(.7f,0,0,A));Box(0,I*3,W,3,FLinearColor(.7f,0,0,A));Box(0,H-I*3-3,W,3,FLinearColor(.7f,0,0,A));}
        const float BloodAlpha=FMath::Max(FMath::Clamp(C->DamageFlash/1.2f,0.f,1.f)*.8f,C->Health<=30?.2f:0.f);
        if(BloodAlpha>0 && BloodTexture.IsValid()){
            FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(SZ,FSlateLayoutTransform()),&BloodBrush,ESlateDrawEffect::None,FLinearColor(1,1,1,BloodAlpha));
            if(SplatterTexture.IsValid())for(const FVector2D P:{FVector2D(-85*S,-80*S),FVector2D(W-150*S,-95*S),FVector2D(W-160*S,H-140*S)})
                FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(FVector2D(260*S,260*S),FSlateLayoutTransform(P)),&SplatterBrush,ESlateDrawEffect::None,FLinearColor(1,1,1,BloodAlpha));
        }
        Text(25*S,18*S,FString::Printf(TEXT("HP %.0f / 100"),C->Health),18);Box(25*S,47*S,180*S,8*S,FLinearColor(.1f,.1f,.1f,.8f));Box(25*S,47*S,180*S*C->Health/100,8*S,C->Health<=30?FLinearColor(1,.15f,.1f):FLinearColor(.3f,.8f,.6f));
        // Reserve the empty strip below health; weapon, viewfinder and shortcut UI live elsewhere.
        Box(25*S,65*S,104*S,26*S,FLinearColor(.015f,.022f,.03f,.65f));
        Text(33*S,68*S,FrameRateText,14,FLinearColor(.75f,.85f,.85f));
        if(C->Health<=0){Text(W*.5f-80*S,H*.45f,TEXT("你已死亡"),40,FLinearColor(1,.2f,.15f));return L+2;}
        if(E->Equipment->bBackpackOpen)return L+2;
        const FString Prompt=E->InteractionText();if(!Prompt.IsEmpty())Text(W*.5f-120*S,H*.65f,Prompt,20,FLinearColor(.5f,1,1));
        if(auto* Gun=E->ActiveWeapon.Get())
        {
            if(!E->bADS){const float Gap=10*S;Box(W*.5f-Gap-8*S,H*.5f,8*S,1.5f,FLinearColor::White);Box(W*.5f+Gap,H*.5f,8*S,1.5f,FLinearColor::White);Box(W*.5f,H*.5f-Gap-8*S,1.5f,8*S,FLinearColor::White);Box(W*.5f,H*.5f+Gap,1.5f,8*S,FLinearColor::White);}
            else if(Gun->Kind==ETacticalWeaponKind::Rifle&&!Gun->bPackPresentation&&!E->bReloading){
                const FLinearColor Reticle(1,.12f,.04f,.9f);
                Box(W*.5f-1.3f*S,H*.5f-1.3f*S,2.6f*S,2.6f*S,Reticle);
            }
            const float PanelW=FMath::Min(W*.49f,590*S),X=W-PanelW-20*S;
            Box(X-12*S,18*S,PanelW+24*S,158*S,FLinearColor(.015f,.022f,.03f,.82f));
            Text(X,27*S,Gun->Spec().Name,17);
            const FString Mode=Gun->Kind==ETacticalWeaponKind::Rifle?TEXT("全自动"):Gun->Kind==ETacticalWeaponKind::Shotgun?(Gun->bPumpMode?TEXT("泵动"):TEXT("半自动")):TEXT("半自动");
            Text(X,54*S,TEXT("射击模式  ")+Mode,15,FLinearColor(.65f,.8f,.85f));
            Text(X,80*S,FString::Printf(TEXT("%s %02d / %02d    膛内 %d / 1"),Gun->Kind==ETacticalWeaponKind::Shotgun?TEXT("弹管"):TEXT("弹匣"),Gun->Magazine,Gun->Spec().Capacity,Gun->bChambered?1:0),22,Gun->bChambered?FLinearColor::White:FLinearColor(1,.5f,.25f));
            Text(X,117*S,FString::Printf(TEXT("备用弹药  %03d"),E->Reserve()),18);
            if(Gun->Kind==ETacticalWeaponKind::Rifle)Text(X,145*S,TEXT("全息瞄准 · 1×"),15);
            else if(Gun->Kind==ETacticalWeaponKind::Shotgun)Text(X,145*S,TEXT("逐发装填 · 半自动"),15);
            Text(25*S,H-197*S,TEXT("左键 射击 · 按住右键 瞄准 · R 换弹"),17);
            Text(25*S,H-171*S,TEXT("G 丢枪 · B 收枪 · T 战术灯 · 滚轮 切装备"),16);
            Text(25*S,H-145*S,TEXT("I 背包 · 1–5 快捷槽 · E/F 拾取"),16);
            Text(25*S,H-119*S,TEXT("Shift 冲刺 · Ctrl 蹲起 · Space 跳跃"),16);
            if(E->bReloading){Box(W*.5f-90*S,H*.73f,180*S,5*S,FLinearColor(.1f,.1f,.1f,.8f));Box(W*.5f-90*S,H*.73f,180*S*E->ReloadProgress(),5*S,FLinearColor(.4f,.9f,1));}
            if(E->EquipRemaining>0)Text(W*.5f-70*S,H*.68f,TEXT("装备中…"),22);
        }
        if(E->HitMarker>0)Text(W*.5f-8*S,H*.5f-15*S,E->bKillMarker?TEXT("✕"):TEXT("×"),32,E->bKillMarker?FLinearColor(1,.2f,.1f):FLinearColor::White);
        return L+2;
    }
private:TWeakObjectPtr<UCombatEquipmentComponent> Combat;
 double SampleStartSeconds = 0.0;
 uint64 SampleStartFrame = 0;
 FString FrameRateText = TEXT("FPS --");
 TStrongObjectPtr<UTexture2D> BloodTexture,SplatterTexture;
 FSlateBrush BloodBrush,SplatterBrush;
};
