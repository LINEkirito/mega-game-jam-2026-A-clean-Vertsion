#pragma once
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/ShoulderCamcorder.h"

namespace PlayerHUDPanels
{
// HUD-only memory: temporary holstering must not introduce an empty-hand tab.
struct FWeaponDisplay
{
    TWeakObjectPtr<AGame_Jam_Clean_VersCharacter> ObservedPlayer;
    TWeakObjectPtr<ATacticalWeapon> LastWeapon;
    void Update(AGame_Jam_Clean_VersCharacter* Player,const UCombatEquipmentComponent* Combat)
    {
        if(ObservedPlayer.Get()!=Player){ObservedPlayer=Player;LastWeapon.Reset();}
        if(!Player || !Combat){LastWeapon.Reset();return;}
        const auto* Equipment=Combat->Equipment.Get();
        const auto* Inventory=Equipment?Equipment->Inventory.Get():nullptr;
        auto Owned=[&](ATacticalWeapon* Gun)
        {
            if(!IsValid(Gun) || !Inventory || Inventory->Count(Gun->InventoryId)<=0)return false;
            const auto* Entry=Combat->Weapons.Find(Gun->InventoryId);
            return Entry && Entry->Get()==Gun;
        };
        if(!Owned(LastWeapon.Get()))LastWeapon.Reset();
        if(auto* Gun=Combat->ActiveWeapon.Get();Owned(Gun))LastWeapon=Gun;
        // Follow selection immediately, including the wheel's delayed draw and low-look holster.
        if(Inventory && Inventory->Slots.IsValidIndex(Inventory->ActiveSlot))
        {
            const auto* Entry=Combat->Weapons.Find(Inventory->Slots[Inventory->ActiveSlot]);
            if(Entry && Owned(Entry->Get()))LastWeapon=Entry->Get();
        }
    }
};
struct FImages
{
    TStrongObjectPtr<UTexture2D> Textures[5];
    FSlateBrush Brushes[5];
    void Load()
    {
        const TCHAR* Paths[]={TEXT("/Game/UI/HUD/T_HUD_Player"),TEXT("/Game/UI/HUD/T_HUD_Pistol"),TEXT("/Game/UI/HUD/T_HUD_Rifle"),TEXT("/Game/UI/HUD/T_HUD_Shotgun"),TEXT("/Game/UI/HUD/T_HUD_Camera")};
        for(int32 I=0;I<5;++I)
        {
            Textures[I].Reset(LoadObject<UTexture2D>(nullptr,Paths[I]));
            Brushes[I].SetResourceObject(Textures[I].Get());
            Brushes[I].DrawAs=ESlateBrushDrawType::Image;
            if(Textures[I].IsValid())Brushes[I].ImageSize=FVector2D(Textures[I]->GetSizeX(),Textures[I]->GetSizeY());
        }
    }
};
// Per-HUD presentation state; never changes the player's health or damage rules.
struct FHealthFeedback
{
    TWeakObjectPtr<AGame_Jam_Clean_VersCharacter> ObservedPlayer;
    float PreviousHealth=0;
    double HitTime=-1;
    void Update(AGame_Jam_Clean_VersCharacter* Player,double Now)
    {
        if(ObservedPlayer.Get()!=Player){ObservedPlayer=Player;PreviousHealth=Player?Player->Health:0;HitTime=-1;}
        if(!Player)return;
        if(Player->Health<PreviousHealth)HitTime=Now;
        PreviousHealth=Player->Health;
    }
    FVector2D Offset(double Now) const
    {
        const double Age=Now-HitTime;
        if(HitTime<0 || Age<0 || Age>=.25)return FVector2D::ZeroVector;
        const float Fade=1.f-float(Age/.25);
        return FVector2D(FMath::Sin(float(Age)*110.f)*3.f*Fade,FMath::Sin(float(Age)*150.f)*1.5f*Fade);
    }
};
// Presentation only: a new weapon establishes a baseline without a shake.
struct FAmmoFeedback
{
    TWeakObjectPtr<ATacticalWeapon> ObservedWeapon;
    int32 PreviousMagazine=0;
    double ShotTime=-1;
    void Update(ATacticalWeapon* Weapon,double Now)
    {
        if(ObservedWeapon.Get()!=Weapon){ObservedWeapon=Weapon;PreviousMagazine=Weapon?Weapon->Magazine:0;ShotTime=-1;}
        if(!Weapon){ShotTime=-1;return;}
        if(Weapon->Magazine<PreviousMagazine)ShotTime=Now;
        PreviousMagazine=Weapon->Magazine;
    }
    FVector2D Offset(double Now) const
    {
        const double Age=Now-ShotTime;
        if(ShotTime<0 || Age<0 || Age>=.2)return FVector2D::ZeroVector;
        const float Fade=1.f-float(Age/.2);
        return FVector2D(FMath::Cos(float(Age)*110.f)*3.f*Fade,FMath::Sin(float(Age)*150.f)*2.f*Fade);
    }
};
inline FString WeaponLabel(ETacticalWeaponKind Kind)
{
    return Kind==ETacticalWeaponKind::Rifle?TEXT("步枪"):Kind==ETacticalWeaponKind::Shotgun?TEXT("霰弹枪"):TEXT("手枪");
}
inline void Paint(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer,
    const AGame_Jam_Clean_VersCharacter& Player,const UCombatEquipmentComponent& Combat,
    const FLocalizedDisplayCache& Cache,const FVector2D& HealthOffset,const FVector2D& AmmoOffset,const FImages& Images,const FWeaponDisplay& WeaponDisplay)
{
    const auto* Equipment=Combat.Equipment.Get();
    if(!Equipment || Equipment->bBackpackOpen)return;
    const FVector2D Size=G.GetLocalSize();
    if(Size.X<=0 || Size.Y<=0)return;
    const float Scale=FMath::Min(Size.X/1170.f,Size.Y/661.f);
    // Same palette and composite fonts as RecordingReview's current settlement page.
    const FLinearColor HUDRed(.55f,.012f,.018f,.96f),HUDPaper(.92f,.89f,.8f,1),HUDGold(1.f,.65f,.12f,1),HUDInk(.012f,.01f,.012f,.96f);
    auto Rect=[&](float X,float Y,float W,float H,FLinearColor Color)
    {
        FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(W,H)*Scale,
            FSlateLayoutTransform(FVector2D(X,Y)*Scale)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
    };
    auto Label=[&](float X,float Y,float W,const FString& Value,int32 FontSize,FLinearColor Color,GameLanguage::EFontRole Role=GameLanguage::EFontRole::Heading,float CenterHeight=0.f)
    {
        const FString Display=Cache.Get(Value);
        auto Font=GameLanguage::Font(FMath::Max(8,FMath::RoundToInt(FontSize*Scale)),Role);
        const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const float Width=Measure->Measure(Display,Font).X;
        if(Width>W*Scale)Font.Size=FMath::Max(1,FMath::FloorToInt(Font.Size*W*Scale/Width));
        while(Font.Size>1 && Measure->Measure(Display,Font).X>W*Scale)--Font.Size;
        if(CenterHeight>0)Y+=(CenterHeight-Measure->Measure(Display,Font).Y/Scale)*.5f;
        FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(FVector2D(X,Y)*Scale)),Display,Font,ESlateDrawEffect::None,Color);
    };
    auto Image=[&](int32 Index,float X,float Y,float W,float H)
    {
        if(!Images.Textures[Index].IsValid())return;
        const FVector2D Native=Images.Brushes[Index].ImageSize;
        if(Native.X<=0 || Native.Y<=0)return;
        const FVector2D Fit=Native*FMath::Min(W/Native.X,H/Native.Y);
        const FVector2D Position(X+(W-Fit.X)*.5f,Y+(H-Fit.Y)*.5f);
        FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(Fit*Scale,FSlateLayoutTransform(Position*Scale)),&Images.Brushes[Index],ESlateDrawEffect::None,FLinearColor::White);
    };
    const float Margin=40,PanelWidth=280,PanelHeight=82,TabHeight=26;
    const float Bottom=Size.Y/Scale-Margin,Left=Margin,PanelY=Bottom-PanelHeight-TabHeight;
    Rect(Left+3,PanelY+3,PanelWidth,PanelHeight,FLinearColor(0,0,0,.35f));
    Rect(Left,PanelY,PanelWidth,PanelHeight,HUDRed);Rect(Left,PanelY,3,PanelHeight,HUDGold);
    // Preserve the original image slots; fit textures without stretching.
    Image(0,Left+14,PanelY+17,46,48);
    Label(Left+74,PanelY+14,150,TEXT("ME"),18,HUDPaper);
    const float HealthX=Left+74+HealthOffset.X,HealthY=PanelY+48+HealthOffset.Y;
    Rect(HealthX,HealthY,188,16,HUDInk);
    Rect(HealthX+2,HealthY+2,184,12,FLinearColor::White);
    Rect(HealthX+2,HealthY+2,184*FMath::Clamp(Player.Health/100.f,0.f,1.f),12,HUDGold);
    const auto* Camera=Equipment->Camcorder.Get();
    const auto* DisplayInventory=Equipment->Inventory.Get();
    const bool CameraSelected=DisplayInventory && DisplayInventory->Slots.IsValidIndex(DisplayInventory->ActiveSlot)
        && DisplayInventory->Slots[DisplayInventory->ActiveSlot]==UBackpackInventory::CameraId
        && DisplayInventory->Count(UBackpackInventory::CameraId)>0;
    const bool CameraHeld=Camera && CameraSelected;
    const auto* Gun=CameraHeld?nullptr:WeaponDisplay.LastWeapon.Get();
    if(!Gun && !CameraHeld)return;
    const float Right=Size.X/Scale-Margin,PanelX=Right-PanelWidth;
    Rect(PanelX+3,PanelY+3,PanelWidth,PanelHeight,FLinearColor(0,0,0,.35f));
    Rect(PanelX,PanelY,PanelWidth,PanelHeight,HUDRed);Rect(PanelX,PanelY,3,PanelHeight,HUDGold);
    if(Gun)Image(Gun->Kind==ETacticalWeaponKind::Rifle?2:Gun->Kind==ETacticalWeaponKind::Shotgun?3:1,PanelX+15,PanelY+20,112,42);
    else if(CameraHeld)Image(4,PanelX+15,PanelY+20,112,42);
    const FString Amount=Gun?FString::FromInt(Gun->Magazine):CameraHeld?FString::Printf(TEXT("%.0f"),FMath::Clamp(Camera->BatteryPercent,0.f,100.f)):TEXT("—");
    const int64 TotalAmmo=Gun?int64(FMath::Max(0,Gun->Magazine))+(Gun->bChambered?1:0)
        +(DisplayInventory?int64(FMath::Max(0,DisplayInventory->Count(Gun->Spec().Ammo))):0):0;
    const FString Capacity=Gun?FString::Printf(TEXT("/ %lld"),static_cast<long long>(TotalAmmo)):CameraHeld?TEXT("/ 100"):TEXT("");
    Label(PanelX+140+AmmoOffset.X,PanelY+AmmoOffset.Y,67,Amount,32,HUDPaper,GameLanguage::EFontRole::Heading,PanelHeight);
    Label(PanelX+212,PanelY,60,Capacity,22,HUDPaper,GameLanguage::EFontRole::Body,PanelHeight);
    // Stable positions match the quick slots; selection only changes the tint.
    const FString Names[]={TEXT("摄像机"),WeaponLabel(ETacticalWeaponKind::Pistol),WeaponLabel(ETacticalWeaponKind::Rifle),WeaponLabel(ETacticalWeaponKind::Shotgun)};
    const int32 SelectedTab=CameraHeld?0:Gun->Kind==ETacticalWeaponKind::Rifle?2:Gun->Kind==ETacticalWeaponKind::Shotgun?3:1;
    bool Owned[4]={false,false,false,false};
    if(const auto* Inventory=Equipment->Inventory.Get())
    {
        Owned[0]=Inventory->Count(UBackpackInventory::CameraId)>0;
        for(const auto& Entry:Combat.Weapons)
        {
            const auto* Weapon=Entry.Value.Get();
            if(!IsValid(Weapon) || Inventory->Count(Entry.Key)<=0)continue;
            const int32 Tab=Weapon->Kind==ETacticalWeaponKind::Rifle?2:Weapon->Kind==ETacticalWeaponKind::Shotgun?3:1;
            Owned[Tab]=true;
        }
    }
    const float TabWidth=PanelWidth/4.f;
    for(int32 I=0;I<4;++I)
    {
        const bool Selected=Owned[I] && I==SelectedTab;
        FLinearColor TextColor=Selected?FLinearColor::Black:HUDPaper;
        if(!Owned[I])TextColor.A=.35f;
        Rect(PanelX+I*TabWidth,Bottom-TabHeight,TabWidth-1,TabHeight,Selected?HUDRed:HUDInk);
        Label(PanelX+I*TabWidth+5,Bottom-TabHeight,TabWidth-10,Names[I],11,TextColor,GameLanguage::EFontRole::Heading,TabHeight);
    }
}
}
