#include "CamcorderEquipmentComponent.h"
#include "Audio/AudioWidgets.h"
#include "RecordingTabletComponent.h"
#include "BackpackInventory.h"
#include "InventoryItemPreview.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ShoulderCamcorder.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Localization/GameLanguage.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "InputCoreTypes.h"

class SInventoryInspection : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInventoryInspection) {}
        SLATE_ARGUMENT(TWeakObjectPtr<UCamcorderEquipmentComponent>, Equipment)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Equipment=Args._Equipment;
        ChildSlot[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.012f,.018f,.026f))
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [SNew(SBox).WidthOverride(768).HeightOverride(432)
                    [SNew(SImage).Image(&Equipment.Get()->InspectionBrush)]]]];
    }
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent& Event) override
    {
        if(Event.GetEffectingButton()==EKeys::LeftMouseButton) return FReply::Handled().CaptureMouse(SharedThis(this));
        return FReply::Unhandled();
    }
    virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent& Event) override
    {
        if(Event.GetEffectingButton()==EKeys::LeftMouseButton && HasMouseCapture()) return FReply::Handled().ReleaseMouseCapture();
        return FReply::Unhandled();
    }
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent& Event) override
    {
        if(!HasMouseCapture()) return FReply::Unhandled();
        if(auto* E=Equipment.Get()) if(E->ItemPreview) E->ItemPreview->Rotate(Event.GetCursorDelta().X,Event.GetCursorDelta().Y);
        return FReply::Handled();
    }
    virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent& Event) override
    {
        if(auto* E=Equipment.Get()) if(E->ItemPreview) E->ItemPreview->Zoom(Event.GetWheelDelta());
        return FReply::Handled();
    }
    virtual FReply OnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent& Event) override
    {
        if(Event.GetEffectingButton()==EKeys::LeftMouseButton)
        { if(auto* E=Equipment.Get()) if(E->ItemPreview) E->ItemPreview->ResetView(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    TWeakObjectPtr<UCamcorderEquipmentComponent> Equipment;
};

class SCamcorderBackpackRoot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCamcorderBackpackRoot) {}
        SLATE_ARGUMENT(TWeakObjectPtr<UCamcorderEquipmentComponent>, Equipment)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Equipment=Args._Equipment; ChildSlot[Args._Content.Widget]; }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        if(auto* E=Equipment.Get()) if(!Event.IsRepeat() && E->HandleBackpackKey(Event.GetKey())) return FReply::Handled();
        return FReply::Handled(); // UI owns input while browsing, including movement / jump / recording keys.
    }
private:
    TWeakObjectPtr<UCamcorderEquipmentComponent> Equipment;
};

void UCamcorderEquipmentComponent::AddCameraToInventory()
{
    if(Inventory && Inventory->Count(UBackpackInventory::CameraId)==0)
        Inventory->Add(UBackpackInventory::CameraId,TEXT("手持摄像机"),TEXT("可手持、放在场景中或收入背包。收入背包会保存并结束当前录像，保留电量、开关机状态与焦距；装备后可继续拍摄。"),1,true);
}
void UCamcorderEquipmentComponent::StoreCamera()
{
    if(!Camcorder || (!Camcorder->bHeld && !Camcorder->bInBackpack)) return;
    if(Camcorder->StoreInBackpack(Player())) { AddCameraToInventory(); SelectedItem=UBackpackInventory::CameraId; RefreshBackpack(); }
    else Camcorder->SetNotice(TEXT("请先靠近摄像机，才能收入背包"));
}
void UCamcorderEquipmentComponent::EquipCamera()
{
    if(!Camcorder || !Inventory || Inventory->Count(UBackpackInventory::CameraId)==0) return;
    if(Combat && Combat->GetPresentationWeapon()){Combat->EquipWeapon(UBackpackInventory::CameraId);return;}
    if(Combat)Combat->Unequip();
    if(!Camcorder->bHeld) Camcorder->PickUp(Player());
    if(Camcorder->bHeld) { Inventory->ActiveSlot=Inventory->SlotIndex(UBackpackInventory::CameraId); SelectedItem=UBackpackInventory::CameraId; }
    RefreshBackpack();
}
void UCamcorderEquipmentComponent::ToggleBackpack() { SetBackpackOpen(!bBackpackOpen); }
void UCamcorderEquipmentComponent::SetBackpackOpen(bool bOpen)
{
    auto* PC=Player(); if(!PC || !Inventory || !BackpackHUD || bOpen==bBackpackOpen) return;
    if(bOpen && Tablet && Tablet->bOpen)Tablet->SetOpen(false);
    bBackpackOpen=bOpen; bAssignmentMenu=false; RecordHoldSeconds=0; bHoldTriggered=true;
    if(PC->PlayerInput) PC->PlayerInput->FlushPressedKeys();
    if(bOpen)
    {
        bPreviousCursor=PC->bShowMouseCursor;
        PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); PC->bShowMouseCursor=true;
        RefreshBackpack();
        FInputModeUIOnly Mode; Mode.SetWidgetToFocus(BackpackHUD); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode); FSlateApplication::Get().SetKeyboardFocus(BackpackHUD,EFocusCause::SetDirectly);
    }
    else
    {
        FSlateApplication::Get().DismissAllMenus();
        FSlateApplication::Get().ReleaseAllPointerCapture();
        InspectionBrush.SetResourceObject(nullptr);
        if(IsValid(ItemPreview)) { ItemPreview->Destroy(); ItemPreview=nullptr; }
        PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); PC->bShowMouseCursor=bPreviousCursor;
        PC->SetInputMode(FInputModeGameOnly());
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
        RefreshBackpack();
    }
}
void UCamcorderEquipmentComponent::SelectInventoryItem(FName Id)
{
    if(Inventory && Inventory->Count(Id)>0) { SelectedItem=Id; bAssignmentMenu=false; RefreshItemPreview(); }
}
void UCamcorderEquipmentComponent::RefreshItemPreview()
{
    if(!bBackpackOpen || !Inventory) return;
    if(!IsValid(ItemPreview))
    {
        FActorSpawnParameters P; P.Owner=GetOwner(); P.ObjectFlags|=RF_Transient;
        P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ItemPreview=GetWorld()->SpawnActor<AInventoryItemPreview>(FVector(0,0,-50000),FRotator::ZeroRotator,P);
        if(!ItemPreview) return;
        InspectionBrush.SetResourceObject(ItemPreview->Texture); InspectionBrush.ImageSize=FVector2D(768,432);
        InspectionBrush.DrawAs=ESlateBrushDrawType::Image;
    }
    ItemPreview->ShowItem(this,SelectedItem);
}
void UCamcorderEquipmentComponent::ShowAssignmentMenu(FName Id)
{
    if(!bBackpackOpen || !Inventory || Inventory->Count(Id)==0) return;
    SelectInventoryItem(Id);
    const TWeakObjectPtr<UCamcorderEquipmentComponent> Weak(this);
    FSlateApplication::Get().PlaySound(GameAudio::UISound(TEXT("UI_Click")));
    FMenuBuilder Menu(true,nullptr);
    Menu.AddMenuEntry(GameLanguage::Text(TEXT("丢下")),GameLanguage::Text(TEXT("丢下此物品（弹药整组）")),FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([Weak,Id]{FSlateApplication::Get().PlaySound(GameAudio::UISound(TEXT("UI_Click")));if(auto* E=Weak.Get()) E->DropInventoryItem(Id);})));
    FSlateApplication::Get().PushMenu(BackpackHUD.ToSharedRef(),FWidgetPath(),Menu.MakeWidget(),
        FSlateApplication::Get().GetCursorPos(),FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}
void UCamcorderEquipmentComponent::AssignQuickSlot(int32 Index)
{
    bAssignmentMenu=false;
    ActivateQuickSlot(Index);
}
void UCamcorderEquipmentComponent::ActivateQuickSlot(int32 Index)
{
    if(!Inventory || !Inventory->Slots.IsValidIndex(Index)) return;
    const FName Id=Inventory->Slots[Index];
    Inventory->ActiveSlot=Index; SelectedItem=Id;
    if(Inventory->Count(Id)==0)
    {
        if(Combat) Combat->EquipWeapon(NAME_None);
        if(Camcorder && Camcorder->bHeld) StoreCamera();
        if(Camcorder) Camcorder->SetNotice(TEXT("该道具未持有，拾回后恢复原槽位"));
        return;
    }
    UseSelectedItem();
}
void UCamcorderEquipmentComponent::UseSelectedItem()
{
    if(!Inventory || Inventory->Count(SelectedItem)==0) { if(Camcorder) Camcorder->SetNotice(TEXT("背包里没有该物品")); return; }
    if(SelectedItem==UBackpackInventory::TabletId){if(Tablet)Tablet->SetOpen(true);}
    else if(SelectedItem==UBackpackInventory::CameraId) EquipCamera();
    else if(Combat && Combat->Weapons.Contains(SelectedItem)) Combat->EquipWeapon(SelectedItem);
    else if(Combat && Combat->ActiveWeapon && Combat->ActiveWeapon->Spec().Ammo==SelectedItem) Combat->Reload();
    else if(Camcorder) Camcorder->SetNotice(TEXT("该物品暂无使用效果"));
}
bool UCamcorderEquipmentComponent::HandleBackpackKey(const FKey& Key)
{
    if(!bBackpackOpen) return false;
    if(Key==EKeys::Escape && bAssignmentMenu) { bAssignmentMenu=false; return true; }
    if(Key==EKeys::I || Key==EKeys::Escape) { SetBackpackOpen(false); return true; }
    const FKey Digits[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four};
    const FKey NumPad[]={EKeys::NumPadOne,EKeys::NumPadTwo,EKeys::NumPadThree,EKeys::NumPadFour};
    for(int32 I=0;I<UBackpackInventory::QuickSlotCount;++I) if(Key==Digits[I] || Key==NumPad[I]) { AssignQuickSlot(I); return true; }
    if(Key==EKeys::Enter) { UseSelectedItem(); return true; }
    if(Key==EKeys::Up || Key==EKeys::Down)
    {
        const int32 Current=Inventory->Items.IndexOfByPredicate([this](const FBackpackEntry& E){return E.Id==SelectedItem;});
        const int32 Next=FMath::Clamp(Current+(Key==EKeys::Up?-1:1),0,Inventory->Items.Num()-1);
        if(Inventory->Items.IsValidIndex(Next)) SelectInventoryItem(Inventory->Items[Next].Id);
        return true;
    }
    return false;
}
FString UCamcorderEquipmentComponent::ItemTitle(FName Id) const
{
    const auto* E=Inventory?Inventory->Find(Id):nullptr; return E?E->Name:TEXT("空槽");
}
FString UCamcorderEquipmentComponent::SlotTitle(int32 Index) const
{
    if(!Inventory || !Inventory->Slots.IsValidIndex(Index)) return TEXT("—");
    const FName Id=Inventory->Slots[Index]; const int32 Count=Inventory->Count(Id);
    FString Title=FString::Printf(TEXT("%d  %s"),Index+1,*Inventory->SlotName(Index));
    if(Count==0)return Title+TEXT("\n未持有");
    if(Id==UBackpackInventory::CameraId && Camcorder)
        return Title+FString::Printf(TEXT("\n电量 %.0f%% · 焦距 %.0f mm"),Camcorder->BatteryPercent,Camcorder->FocalLength);
    if(Combat)if(const auto* Found=Combat->Weapons.Find(Id);Found && IsValid(*Found))
    {
        const auto* Gun=Found->Get();const auto Spec=Gun->Spec();
        return FString::Printf(TEXT("%d  %s\n%s %d/%d · 膛内 %d · 备用 %d"),Index+1,*Spec.Name,
            Gun->Kind==ETacticalWeaponKind::Shotgun?TEXT("弹管"):TEXT("弹匣"),Gun->Magazine,Spec.Capacity, Gun->bChambered?1:0,Inventory->Count(Spec.Ammo));
    }
    return Title;
}
FString UCamcorderEquipmentComponent::SelectedDescription() const
{
    const auto* E=Inventory?Inventory->Find(SelectedItem):nullptr; if(!E) return TEXT("背包为空。靠近物品，按 F 收入背包。");
    FString S=E->Description;
    const int32 Slot=Inventory->SlotIndex(SelectedItem);
    if(!Inventory->CanAssign(SelectedItem)) return S+TEXT("\n\n此物品不占用固定快捷栏，请在背包中使用。");
    S+=Slot>=0?FString::Printf(TEXT("\n\n固定快捷槽 %d；滚轮 / 数字键切换。"),Slot+1):TEXT("\n\n请在背包中使用。");
    if(SelectedItem==UBackpackInventory::CameraId && Camcorder)
        S+=FString::Printf(TEXT("\n\n状态：%s\n电量 %.0f%%    焦距 %.0f mm"),Camcorder->bInBackpack?TEXT("在背包中"):TEXT("已手持装备"),Camcorder->BatteryPercent,Camcorder->FocalLength);
    return S;
}
void UCamcorderEquipmentComponent::RefreshBackpack()
{
    if(!Inventory) return;
    if(Inventory->Count(SelectedItem)==0) SelectedItem=Inventory->Items.IsEmpty()?NAME_None:Inventory->Items[0].Id;
    RefreshItemPreview();
    if(!InventoryList) return;
    InventoryList->ClearChildren();
    const TWeakObjectPtr<UCamcorderEquipmentComponent> Weak(this);
    for(const auto& Item:Inventory->Items)
    {
        const FName Id=Item.Id;
        InventoryList->AddSlot().AutoHeight().Padding(0,0,0,10)
        [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
         .BorderBackgroundColor_Lambda([Weak,Id]{auto* E=Weak.Get();return E && E->SelectedItem==Id?FLinearColor(.08f,.4f,.48f):FLinearColor(.06f,.09f,.13f);})
         .OnMouseButtonDown_Lambda([Weak,Id](const FGeometry&,const FPointerEvent& Event){if(Event.GetEffectingButton()==EKeys::RightMouseButton){if(auto* E=Weak.Get())E->ShowAssignmentMenu(Id);return FReply::Handled();}return FReply::Unhandled();})
         [SNew(SAudioButton).ButtonStyle(FCoreStyle::Get(),"NoBorder").ContentPadding(FMargin(16)).IsFocusable(false)
          .OnClicked_Lambda([Weak,Id]{if(auto* E=Weak.Get()) E->SelectInventoryItem(Id); return FReply::Handled();})
          [SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).WrapTextAt(230).Text_Lambda([Weak,Id]{auto* E=Weak.Get();return FText::FromString(E?FString::Printf(TEXT("%s  ×%d\n左键查看 · 右键丢下"),*E->ItemTitle(Id),E->Inventory->Count(Id)):TEXT(""));})]]];
    }
    if(Inventory->Items.IsEmpty()) InventoryList->AddSlot().AutoHeight()[SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("背包为空")))];
}

void UCamcorderEquipmentComponent::CreateBackpackHUD()
{
    const TWeakObjectPtr<UCamcorderEquipmentComponent> Weak(this);
    const auto Font=FCoreStyle::GetDefaultFontStyle("Regular",18);
    auto Label=[Font](const FString& Text){return SNew(SLocalizedTextBlock).Font(Font).Text(FText::FromString(Text));};
    auto Button=[Label](const FString& Text,TFunction<void()> Action){return SNew(SAudioButton).IsFocusable(false).ContentPadding(FMargin(16,10)).OnClicked_Lambda([Action]{Action();return FReply::Handled();})[Label(Text)];};
    auto Bar=SNew(SHorizontalBox);
    auto Slots=SNew(SHorizontalBox);
    for(int32 I=0;I<UBackpackInventory::QuickSlotCount;++I)
    {
        Bar->AddSlot().AutoWidth().Padding(3)[SNew(SBorder).Padding(10).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor_Lambda([Weak,I]{auto* E=Weak.Get();return E && E->Inventory->ActiveSlot==I?FLinearColor(.07f,.35f,.42f,.95f):FLinearColor(.02f,.03f,.05f,.95f);})
            [SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).WrapTextAt(270).Text_Lambda([Weak,I]{auto* E=Weak.Get();return FText::FromString(E?E->SlotTitle(I):TEXT(""));})]];
        Slots->AddSlot().FillWidth(1).Padding(3)[SNew(SAudioButton).IsFocusable(false).ContentPadding(8)
            .OnClicked_Lambda([Weak,I]{if(auto* E=Weak.Get()) E->AssignQuickSlot(I);return FReply::Handled();})
            [SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).AutoWrapText(true).Text_Lambda([Weak,I]{auto* E=Weak.Get();return FText::FromString(E?E->SlotTitle(I):TEXT(""));})]];
    }
    TSharedRef<SWidget> Panel=SNew(SBorder).Padding(24).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.035f,.05f,1))
    [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
      [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",28)).Text(FText::FromString(TEXT("背包  /  BACKPACK")))]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("关闭  [I]"),[Weak]{if(auto* E=Weak.Get())E->SetBackpackOpen(false);})]]
      +SVerticalBox::Slot().FillHeight(1)
      [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(.27f).Padding(0,0,16,0)
        [SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(InventoryList,SVerticalBox)]]
        +SHorizontalBox::Slot().FillWidth(.43f).Padding(0,0,16,0)
        [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("物体检视 / 3D"))]
          +SVerticalBox::Slot().FillHeight(1)[SNew(SInventoryInspection).Equipment(Weak)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).AutoWrapText(true).Text(FText::FromString(TEXT("左键拖动旋转 · 滚轮缩放 · 双击复位")))]
          +SVerticalBox::Slot().AutoHeight()[Button(TEXT("重置视角"),[Weak]{if(auto* E=Weak.Get())if(E->ItemPreview)E->ItemPreview->ResetView();})]]
        +SHorizontalBox::Slot().FillWidth(.30f)
        [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",24)).AutoWrapText(true).Text_Lambda([Weak]{auto* E=Weak.Get();return FText::FromString(E?E->ItemTitle(E->SelectedItem):TEXT(""));})]
          +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SLocalizedTextBlock).Font(Font).AutoWrapText(true).Text_Lambda([Weak]{auto* E=Weak.Get();return FText::FromString(E?E->SelectedDescription():TEXT(""));})]]
          +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("使用 / 装备  [Enter]"),[Weak]{if(auto* E=Weak.Get())E->UseSelectedItem();})]
            +SVerticalBox::Slot().AutoHeight()[Button(TEXT("摄像机收入背包"),[Weak]{if(auto* E=Weak.Get())E->StoreCamera();})]]]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).ColorAndOpacity(FLinearColor(1,.8f,.3f)).AutoWrapText(true).Text_Lambda([Weak]{auto* E=Weak.Get();return FText::FromString(E && E->Camcorder?E->Camcorder->Notice:TEXT(""));})]
      +SVerticalBox::Slot().AutoHeight()[Label(TEXT("固定快捷栏：摄像机 / 手枪 / 霰弹枪 / 步枪；按 1–4 或点击槽位切换"))]
      +SVerticalBox::Slot().AutoHeight().Padding(0,10)[Slots]
      +SVerticalBox::Slot().AutoHeight()[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).Text(FText::FromString(TEXT("游戏中 F 拾取 · Q 丢下当前槽物品 · 背包右键丢下 · 打开背包不暂停录像")))]];
    BackpackHUD=SNew(SCamcorderBackpackRoot).Equipment(Weak)
      .Visibility_Lambda([Weak]{auto* E=Weak.Get();return E && E->bBackpackOpen?EVisibility::Visible:EVisibility::SelfHitTestInvisible;})
      [SNew(SOverlay)
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,20)
        [SNew(SBox).Visibility_Lambda([Weak]{auto* E=Weak.Get();return E && !E->bBackpackOpen && E->Camcorder && !E->Camcorder->bViewfinderRaised?EVisibility::HitTestInvisible:EVisibility::Collapsed;})[Bar]]
        +SOverlay::Slot()
        [SNew(SBorder).Visibility_Lambda([Weak]{auto* E=Weak.Get();return E && E->bBackpackOpen?EVisibility::Visible:EVisibility::Collapsed;})
         .Padding(35).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.75f))
         [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1200).HeightOverride(680)[Panel]]]]];
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(BackpackHUD.ToSharedRef(),20);
    RefreshBackpack();
}
