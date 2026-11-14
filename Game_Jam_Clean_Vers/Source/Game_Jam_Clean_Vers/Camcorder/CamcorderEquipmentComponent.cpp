#include "CamcorderEquipmentComponent.h"
#include "Audio/CharacterAudio.h"
#include "RecordingLibrary.h"
#include "RecordingTabletComponent.h"
#include "ItemInteractionComponent.h"
#include "Data/DataTransmitter.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "ShoulderCamcorder.h"
#include "BackpackInventory.h"
#include "InventoryItemPreview.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/StatueTargetComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "CamcorderViewfinderWidget.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "InputCoreTypes.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Localization/GameLanguage.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"

// Consume Escape only while this game's camera/inventory owns it, before PIE's stop shortcut.
class FCamcorderInputGate : public IInputProcessor
{
public:
    explicit FCamcorderInputGate(UCamcorderEquipmentComponent* InOwner):Owner(InOwner) {}
    virtual void Tick(const float,FSlateApplication&,TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&,const FKeyEvent& Event) override
    {
        if(Event.GetKey()!=EKeys::Escape) return false;
        if(bCapturedEscape) return true;
        if(auto* E=Owner.Get())
        {
            if(E->Tablet && E->Tablet->bOpen) { E->Tablet->SetOpen(false); bCapturedEscape=true; return true; }
            if(E->bBackpackOpen && FSlateApplication::Get().AnyMenusVisible()) { FSlateApplication::Get().DismissAllMenus(); bCapturedEscape=true; return true; }
            if(E->bBackpackOpen) { E->HandleBackpackKey(EKeys::Escape); bCapturedEscape=true; return true; }
            if(E->Camcorder && E->Camcorder->bViewfinderRaised) { E->CancelViewfinder(); bCapturedEscape=true; return true; }
        }
        return false;
    }
    virtual bool HandleKeyUpEvent(FSlateApplication&,const FKeyEvent& Event) override
    {
        if(Event.GetKey()!=EKeys::Escape) return false;
        const bool bWasCaptured=bCapturedEscape; bCapturedEscape=false; return bWasCaptured;
    }
private:
    TWeakObjectPtr<UCamcorderEquipmentComponent> Owner;
    bool bCapturedEscape=false;
};

void UCamcorderEquipmentComponent::CancelViewfinder()
{
    bRequireAimRelease=true;
    if(Camcorder) Camcorder->SetViewfinderRaised(false);
}

UCamcorderEquipmentComponent::UCamcorderEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
APlayerController* UCamcorderEquipmentComponent::Player() const { return Cast<APlayerController>(GetOwner()); }
bool UCamcorderEquipmentComponent::CanUse() const { return Camcorder && Camcorder->CanInteract(Player()); }
int32 UCamcorderEquipmentComponent::GetBatteryCount() const { return Inventory?Inventory->Count(UBackpackInventory::BatteryId):0; }
void UCamcorderEquipmentComponent::ToggleCarry()
{
    if(Camcorder && Camcorder->bInBackpack) { EquipCamera(); return; }
    if(!CanUse()) { if(Camcorder) Camcorder->SetNotice(TEXT("距离太远或有遮挡，请靠近摄像机")); return; }
    if(Camcorder->bHeld) { Camcorder->PutDown(); Inventory->Remove(UBackpackInventory::CameraId); }
    else { Camcorder->PickUp(Player()); if(Camcorder->bHeld) AddCameraToInventory(); }
    RefreshBackpack();
}
void UCamcorderEquipmentComponent::ReplaceBattery()
{
    if(!Camcorder || (!Camcorder->bInBackpack && !CanUse())) { if(Camcorder) Camcorder->SetNotice(TEXT("请先靠近摄像机再换电池")); return; }
    if(GetBatteryCount()<=0) { Camcorder->SetNotice(TEXT("背包没有电池；寻找绿色电池，靠近后按 F 拾取")); return; }
    if(Camcorder->InstallFreshBattery()) Inventory->Remove(UBackpackInventory::BatteryId);
    RefreshBackpack();
}
ACamcorderBattery* UCamcorderEquipmentComponent::NearbyBattery() const
{
    const auto* PC=Player(); if(!PC || !PC->GetPawn()) return nullptr;
    const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
    ACamcorderBattery* Best=nullptr; float Dist2=FMath::Square(200.f);
    for(TActorIterator<ACamcorderBattery> It(GetWorld()); It; ++It)
    {
        const float D=FVector::DistSquared(Eye,It->GetActorLocation());
        if(D>=Dist2) continue;
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BatteryInteraction),false,PC->GetPawn()); Q.AddIgnoredActor(*It);
        if(Camcorder) Q.AddIgnoredActor(Camcorder);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,It->GetActorLocation(),ECC_Visibility,Q)) continue;
        Best=*It; Dist2=D;
    }
    return Best;
}
FString UCamcorderEquipmentComponent::PickupPrompt() const
{
 if(NearbyBattery())return GameLanguage::Display(TEXT("[F] 拾取 · 摄像机电池"));
 if(Camcorder&&!Camcorder->bHeld&&!Camcorder->IsPresenting()&&!Camcorder->bInBackpack&&CanUse())return GameLanguage::Display(TEXT("[F] 拾取 · 摄像机"));
 return FString();
}
void UCamcorderEquipmentComponent::CollectBattery()
{
    if(auto* Battery=NearbyBattery())
    {
        if(Inventory && Inventory->Add(UBackpackInventory::BatteryId,TEXT("摄像机电池"),TEXT("消耗一块，将摄像机电量恢复到 100% 并开机。可在背包中使用，或按 R 快速换电池。")))
        { if(auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Player()->GetPawn()))if(C->ItemInteraction)C->ItemInteraction->Play(Battery->Mesh); CharacterAudio::Attached(LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Audio/Handling/ItemPickup")),Player()->GetPawn()->GetRootComponent(),ECharacterSound::Handling,.45f); Battery->Destroy(); if(Camcorder) Camcorder->SetNotice(TEXT("已拾取电池并放入背包")); RefreshBackpack(); }
    }
    else if(Camcorder && !Camcorder->IsPresenting() && !Camcorder->bInBackpack && CanUse()) { if(Camcorder->StoreInBackpack(Player())) { AddCameraToInventory(); EquipCamera(); } }
    else if(Camcorder) Camcorder->SetNotice(TEXT("附近没有可拾取电池；靠近绿色电池后按 F"));
}
void UCamcorderEquipmentComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DT,Type,TickFunction);
    auto* PC=Player(); if(!PC || !PC->IsLocalController() || !PC->GetPawn()) return;
    if(!bInitialized)
    {
        FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
        FActorSpawnParameters P; P.Owner=PC; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Camcorder=GetWorld()->SpawnActor<AShoulderCamcorder>(AShoulderCamcorder::StaticClass(),Eye,View,P);
        if(!Camcorder) return;
        Inventory=NewObject<UBackpackInventory>(this);
        AddCameraToInventory();
        Inventory->Add(UBackpackInventory::BatteryId,TEXT("摄像机电池"),TEXT("消耗一块，将摄像机电量恢复到 100% 并开机。可在背包中使用，或按 R 快速换电池。"),2);
        RecordingLibrary=NewObject<URecordingLibrary>(PC,TEXT("RecordingLibrary"));RecordingLibrary->RegisterComponent();
        Tablet=NewObject<URecordingTabletComponent>(PC,TEXT("RecordingTablet"));Tablet->RegisterComponent();
        SelectedItem=UBackpackInventory::CameraId;
        Camcorder->PickUp(PC); CreateHUD(); CreateBackpackHUD(); bInitialized=true;
        for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("JieGeCombatTarget"))&&!It->FindComponentByClass<UStatueTargetComponent>()){auto* Target=NewObject<UStatueTargetComponent>(*It);Target->RegisterComponent();}
        Combat=NewObject<UCombatEquipmentComponent>(PC,TEXT("CombatEquipment"));Combat->Equipment=this;Combat->RegisterComponent();Combat->InitializeStartingWeapons();Combat->InitializeHUD();
        GetWorld()->GetSubsystem<UDataTransmitter>()->OnDataChanged.AddUniqueDynamic(this,&UCamcorderEquipmentComponent::OnGameplayDataChanged);
        CameraInputGate=MakeShared<FCamcorderInputGate>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(CameraInputGate,1);
        // Keep the wearable pose after the controller's view update.
        Camcorder->AddTickPrerequisiteActor(PC);
    }
    if(!IsValid(Camcorder)) return;
    if(RecordingLibrary && RecordingLibrary->bRoundEnded)return;
    if(auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()))if(C->Health<=0)return;
    if(Tablet && Tablet->bOpen)return;
    if(bBackpackOpen) return;
    if(PC->WasInputKeyJustPressed(EKeys::I)) { ToggleBackpack(); return; }
    const FKey Digits[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four};
    const FKey NumPad[]={EKeys::NumPadOne,EKeys::NumPadTwo,EKeys::NumPadThree,EKeys::NumPadFour};
    for(int32 Index=0;Index<UBackpackInventory::QuickSlotCount;++Index) if(PC->WasInputKeyJustPressed(Digits[Index]) || PC->WasInputKeyJustPressed(NumPad[Index])) ActivateQuickSlot(Index);
    if(PC->WasInputKeyJustPressed(EKeys::Q)) { DropActiveQuickSlot(); return; }
    if(PC->WasInputKeyJustPressed(EKeys::F)) { if(!Combat->TryInteract()) CollectBattery(); return; }
    if(!Camcorder->bViewfinderRaised){if(PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))Combat->Cycle(1);if(PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))Combat->Cycle(-1);}
    if(Combat->ActiveWeapon||Combat->HasLookDownSelection()){Combat->HandleInput();return;}
    if(PC->WasInputKeyJustPressed(EKeys::B)) StoreCamera();
    if(!PC->IsInputKeyDown(EKeys::RightMouseButton))
    {
        bRequireAimRelease=false;

    }
    if(PC->WasInputKeyJustPressed(EKeys::RightMouseButton) && !bRequireAimRelease) Camcorder->ToggleViewfinder();
    if(PC->WasInputKeyJustPressed(EKeys::V)) Camcorder->ToggleSelfie();
    if(PC->WasInputKeyJustPressed(EKeys::Escape)) CancelViewfinder();
    if(PC->WasInputKeyJustPressed(EKeys::R)) ReplaceBattery();
    if(PC->WasInputKeyJustPressed(EKeys::P))
    {
        if(CanUse()) Camcorder->TogglePower(); else Camcorder->SetNotice(TEXT("请靠近摄像机后开关机"));
    }
    const bool Allowed=!Camcorder->bInBackpack && CanUse();
    if(Allowed && Camcorder->bViewfinderRaised && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    { if(Camcorder->bRecording) Camcorder->StopRecording(); else Camcorder->StartRecording(); }
    if(Allowed)
    {
        if(Camcorder->bViewfinderRaised)
        {
            if(PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) Camcorder->AdjustFocalLength(3.f);
            if(PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) Camcorder->AdjustFocalLength(-3.f);
        }
    }
}
void UCamcorderEquipmentComponent::CreateHUD()
{
    if(!GetWorld()->GetGameViewport() || !Camcorder) return;
    const TWeakObjectPtr<UCamcorderEquipmentComponent> WeakThis(this);
    auto Text=[WeakThis](int32 Kind)->FText
    {
        const auto* E=WeakThis.Get(); if(!E || !E->Camcorder) return FText::GetEmpty(); const auto* C=E->Camcorder.Get();
        FString S;
        switch(Kind)
        {
        case 3: S=C->bInBackpack?TEXT("1 装备摄像机  |  I 打开背包"):(E->CanUse()?(C->bHeld?TEXT("右键切换 举机取景  |  Q 丢下  |  B 收入背包"):TEXT("F 拾取摄像机")):TEXT("摄像机已放下：靠近后按 F 拾取")); break;
        case 4: S=C->Notice; break;
        case 5: S=E->NearbyBattery()?TEXT("附近有电池：按 F 拾取"):TEXT("F 拾取附近绿色电池  |  初始携带 2 块备用电池"); break;
        }
        return FText::FromString(S);
    };
    const auto Font=FCoreStyle::GetDefaultFontStyle("Regular",16);
    HUD=SNew(SOverlay)
    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(22,22,22,90)
    [ SNew(SBorder).Visibility_Lambda([WeakThis]{auto* E=WeakThis.Get();return E && ((E->RecordingLibrary && E->RecordingLibrary->bRoundEnded) || (E->Camcorder && E->Camcorder->bViewfinderRaised) || (E->Combat && E->Combat->ActiveWeapon))?EVisibility::Collapsed:EVisibility::HitTestInvisible;}).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.01f,.015f,.02f,.88f)).Padding(12)
      [SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SNew(SLocalizedTextBlock).Font(Font).ColorAndOpacity(FLinearColor(.4f,.9f,1)).Text_Lambda([Text]{return Text(3);})]
       +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SNew(SLocalizedTextBlock).Font(Font).Text(FText::FromString(TEXT("V 自拍/前拍  |  左键开始/停止录像  |  鼠标转向 / 滚轮变焦  |  Esc 放下取景")))]
       +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SNew(SLocalizedTextBlock).Font(Font).Text(FText::FromString(TEXT("R 更换电池  |  P 开关机  |  WASD 移动  |  鼠标环顾  |  Space 跳跃")))]
       +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SNew(SLocalizedTextBlock).Font(Font).Text_Lambda([Text]{return Text(5);})]
       +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SNew(SLocalizedTextBlock).Font(Font).Text(FText::FromString(TEXT("I 背包  |  1–4 快捷槽")))]
       +SVerticalBox::Slot().AutoHeight()[SNew(SLocalizedTextBlock).Font(Font).WrapTextAt(700).ColorAndOpacity(FLinearColor(1,.8f,.25f)).Text_Lambda([Text]{return Text(4);})]
      ] ]
    +SOverlay::Slot()[SNew(SCamcorderViewfinder).Equipment(WeakThis)];
    HUD->SetVisibility(EVisibility::HitTestInvisible);
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(HUD.ToSharedRef(),10);
}
void UCamcorderEquipmentComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(CameraInputGate && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(CameraInputGate);
    CameraInputGate.Reset();
    if(Tablet && Tablet->bOpen)Tablet->SetOpen(false);
    if(bBackpackOpen) SetBackpackOpen(false);
    if(IsValid(ItemPreview)) { ItemPreview->Destroy(); ItemPreview=nullptr; }
    if(IsValid(Camcorder)) Camcorder->StopRecording();
    if(HUD.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(HUD.ToSharedRef());
    if(BackpackHUD.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(BackpackHUD.ToSharedRef());
    BackpackHUD.Reset(); InventoryList.Reset(); HUD.Reset(); Super::EndPlay(Reason);
}

void UCamcorderEquipmentComponent::OnGameplayDataChanged(UObject* Source,FName Field,float Value)
{ if(Source==Inventory && bBackpackOpen) RefreshBackpack(); }
