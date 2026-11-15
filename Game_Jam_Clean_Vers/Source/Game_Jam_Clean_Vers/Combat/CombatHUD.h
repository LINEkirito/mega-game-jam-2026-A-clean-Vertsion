#pragma once
#include "Localization/GameLanguage.h"
#include "Widgets/SLeafWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "CombatEquipmentComponent.h"
#include "PoliceReinforcements.h"
#include "Engine/World.h"
#include "TacticalWeapon.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

#include "PlayerHUDPanels.h"
class SCombatHUD : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCombatHUD){} SLATE_ARGUMENT(TWeakObjectPtr<UCombatEquipmentComponent>,Combat) SLATE_END_ARGS()
    void Construct(const FArguments& A){Combat=A._Combat;PanelImages.Load();
        BloodTexture.Reset(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Combat/Effects/Blood/T_BloodScreen")));
        SplatterTexture.Reset(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Combat/Effects/Blood/T_BloodStain")));
        BloodBrush.SetResourceObject(BloodTexture.Get());BloodBrush.DrawAs=ESlateBrushDrawType::Image;
        SplatterBrush.SetResourceObject(SplatterTexture.Get());SplatterBrush.DrawAs=ESlateBrushDrawType::Image;
        SetVisibility(EVisibility::HitTestInvisible);}
    virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
    {
        SLeafWidget::Tick(Geometry, CurrentTime, DeltaTime);
        const bool English=GameLanguage::IsEnglish();
        const uint32 Revision=GameLanguage::DisplayRevision();
        if(English!=bDisplayEnglish || Revision!=DisplayRevision){bDisplayEnglish=English;DisplayRevision=Revision;Invalidate(EInvalidateWidgetReason::Paint);}
        const double Now = FPlatformTime::Seconds();
        auto* Equipment=Combat.Get();
        auto* Controller=Equipment?Cast<APlayerController>(Equipment->GetOwner()):nullptr;
        WeaponDisplay.Update(Controller?Cast<AGame_Jam_Clean_VersCharacter>(Controller->GetPawn()):nullptr,Equipment);
        AmmoFeedback.Update(WeaponDisplay.LastWeapon.Get(),Now);
        HealthFeedback.Update(Controller?Cast<AGame_Jam_Clean_VersCharacter>(Controller->GetPawn()):nullptr,Now);
    }
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(1920,1080);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool)const override
    {
        auto* E=Combat.Get();auto* PC=E?Cast<APlayerController>(E->GetOwner()):nullptr;auto* C=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;if(!C)return L;
        const FVector2D SZ=G.GetLocalSize();const float W=SZ.X,H=SZ.Y,S=FMath::Clamp(H/900.f,.65f,1.4f);
        auto Box=[&](float X,float Y,float BW,float BH,FLinearColor Color){FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(FVector2D(BW,BH),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);};
        auto Text=[&](float X,float Y,const FString& T,int32 Size,FLinearColor Col=FLinearColor::White){const FString Display=LanguageCache.Get(T);FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(SZ,FSlateLayoutTransform(FVector2D(X,Y))),Display,GameLanguage::FittedFont(Display,FMath::RoundToInt(Size*S),W-X-20*S),ESlateDrawEffect::None,Col);};
        const float DamageTintAlpha=FMath::Max(C->DamageFlash>0?.45f:0.f,C->Health<=30?.15f:0.f);
        if(DamageTintAlpha>0)for(int32 I=0;I<20;++I){const float A=DamageTintAlpha*(1.f-I/20.f);Box(I*3,0,3,H,FLinearColor(.7f,0,0,A));Box(W-I*3-3,0,3,H,FLinearColor(.7f,0,0,A));Box(0,I*3,W,3,FLinearColor(.7f,0,0,A));Box(0,H-I*3-3,W,3,FLinearColor(.7f,0,0,A));}
        const float BloodAlpha=FMath::Max(FMath::Clamp(C->DamageFlash/1.2f,0.f,1.f)*.8f,C->Health<=30?.2f:0.f);
        if(BloodAlpha>0 && BloodTexture.IsValid()){
            FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(SZ,FSlateLayoutTransform()),&BloodBrush,ESlateDrawEffect::None,FLinearColor(1,1,1,BloodAlpha));
            if(SplatterTexture.IsValid())for(const FVector2D P:{FVector2D(-85*S,-80*S),FVector2D(W-150*S,-95*S),FVector2D(W-160*S,H-140*S)})
                FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(FVector2D(260*S,260*S),FSlateLayoutTransform(P)),&SplatterBrush,ESlateDrawEffect::None,FLinearColor(1,1,1,BloodAlpha));
        }

        PlayerHUDPanels::Paint(G,Out,L+2,*C,*E,LanguageCache,HealthFeedback.Offset(FPlatformTime::Seconds()),AmmoFeedback.Offset(FPlatformTime::Seconds()),PanelImages,WeaponDisplay);
        if(C->Health<=0)return L+4;
        if(auto* Police=C->GetWorld()->GetSubsystem<UPoliceReinforcements>();Police&&Police->GetWantedLevel()>0)
        {
            const float X=W-225*S,Y=198*S;
            Box(X-12*S,Y-8*S,220*S,74*S,FLinearColor(.012f,.01f,.012f,.96f));
            Box(X-12*S,Y-8*S,3*S,74*S,FLinearColor(1.f,.65f,.12f,1));
            const bool Lit=!Police->IsSearching()||FMath::Fmod(C->GetWorld()->GetTimeSeconds(),1.f)<.65f;
            // Geometry avoids font-dependent star glyphs in either language.
            for(int32 I=0;I<5;++I)
            {
                const bool Active=I<Police->GetWantedLevel();TArray<FVector2D> Points;
                for(int32 V=0;V<=10;++V)
                {const float A=V*PI/5.f-PI*.5f,R=(V%2?6.f:14.f)*S;Points.Add(FVector2D(X+(18+I*39)*S+FMath::Cos(A)*R,Y+17*S+FMath::Sin(A)*R));}
                FSlateDrawElement::MakeLines(Out,L+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Active&&Lit?FLinearColor(1.f,.65f,.12f,1):FLinearColor(.25f,.28f,.32f),true,Active?3.f:1.f);
            }
            const FString Status=LanguageCache.Get(Police->IsSearching()?TEXT("搜索中"):TEXT("追捕中"));
            FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(SZ,FSlateLayoutTransform(FVector2D(X+4*S,Y+39*S))),Status,GameLanguage::Font(FMath::RoundToInt(16*S),GameLanguage::EFontRole::Heading),ESlateDrawEffect::None,FLinearColor::White);
        }
        if(E->Equipment->bBackpackOpen)return L+4;
        const FString Prompt=E->InteractionText();if(!Prompt.IsEmpty()){Box(W*.5f-235*S,H*.65f-8*S,470*S,45*S,FLinearColor(.012f,.01f,.012f,.96f));FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(SZ,FSlateLayoutTransform(FVector2D(W*.5f-220*S,H*.65f))),Prompt,GameLanguage::FittedFont(Prompt,FMath::RoundToInt(20*S),440*S),ESlateDrawEffect::None,FLinearColor::White);}
        if(auto* Gun=E->ActiveWeapon.Get())
        {
            if(!E->bADS){const float Gap=10*S;Box(W*.5f-Gap-8*S,H*.5f,8*S,1.5f,FLinearColor::White);Box(W*.5f+Gap,H*.5f,8*S,1.5f,FLinearColor::White);Box(W*.5f,H*.5f-Gap-8*S,1.5f,8*S,FLinearColor::White);Box(W*.5f,H*.5f+Gap,1.5f,8*S,FLinearColor::White);}
            else if(Gun->Kind==ETacticalWeaponKind::Rifle&&!Gun->bPackPresentation&&!E->bReloading){
                const FLinearColor Reticle(1,.12f,.04f,.9f);
                Box(W*.5f-1.3f*S,H*.5f-1.3f*S,2.6f*S,2.6f*S,Reticle);
            }
        }
        if(E->HitMarker>0)
        {
            // Keep the existing hit signal and lifetime; geometry avoids font-dependent glyphs.
            const FVector2D Center(W*.5f,H*.5f);
            const float Radius=9.f*S;
            for(const float Direction:{-1.f,1.f})
            {
                TArray<FVector2D> Points{Center+FVector2D(-Radius,-Radius*Direction),Center+FVector2D(Radius,Radius*Direction)};
                FSlateDrawElement::MakeLines(Out,L+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor::Red,true,2.f*S);
            }
        }
        return L+4;
    }
private:TWeakObjectPtr<UCombatEquipmentComponent> Combat;
 PlayerHUDPanels::FImages PanelImages;
 PlayerHUDPanels::FWeaponDisplay WeaponDisplay;
 PlayerHUDPanels::FHealthFeedback HealthFeedback;
 PlayerHUDPanels::FAmmoFeedback AmmoFeedback;
 FLocalizedDisplayCache LanguageCache;
 bool bDisplayEnglish=false;
 uint32 DisplayRevision=MAX_uint32;
 TStrongObjectPtr<UTexture2D> BloodTexture,SplatterTexture;
 FSlateBrush BloodBrush,SplatterBrush;
};
