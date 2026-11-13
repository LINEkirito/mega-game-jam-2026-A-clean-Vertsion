#include "CamcorderEquipmentComponent.h"
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
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
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
    if(!Camcorder) return;
    if(Camcorder->StoreInBackpack(Player())) { AddCameraToInventory(); SelectedItem=UBackpackInventory::CameraId; RefreshBackpack(); }
    else Camcorder->SetNotice(TEXT("请先靠近摄像机，才能收入背包"));
}
void UCamcorderEquipmentComponent::EquipCamera()
{
    if(!Camcorder || !Inventory || Inventory->Count(UBackpackInventory::CameraId)==0) return;
    if(Combat)Combat->Unequip();
    Camcorder->PickUp(Player());
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
    SelectInventoryItem(Id); bAssignmentMenu=Inventory && Inventory->CanAssign(Id);
}
void UCamcorderEquipmentComponent::AssignQuickSlot(int32 Index)
{
    if(Inventory && !Inventory->CanAssign(SelectedItem)) { if(Camcorder) Camcorder->SetNotice(TEXT("只有能拿在手上的物品可以登记快捷栏")); return; }
    if(Inventory && Inventory->Assign(Index,SelectedItem))
    {
        bAssignmentMenu=false;
        if(Camcorder) Camcorder->SetNotice(FString::Printf(TEXT("%s 已登记到快捷槽 %d"),*ItemTitle(SelectedItem),Index+1));
    }
}
void UCamcorderEquipmentComponent::ActivateQuickSlot(int32 Index)
{
    if(!Inventory || !Inventory->Slots.IsValidIndex(Index)) return;
    const FName Id=Inventory->Slots[Index];
    if(Inventory->Count(Id)==0) { if(Camcorder) Camcorder->SetNotice(TEXT("这个快捷槽为空；按 I 打开背包登记物品")); return; }
    Inventory->ActiveSlot=Index; SelectedItem=Id; UseSelectedItem();
}
void UCamcorderEquipmentComponent::UseSelectedItem()
{
    if(!Inventory || Inventory->Count(SelectedItem)==0) { if(Camcorder) Camcorder->SetNotice(TEXT("背包里没有该物品")); return; }
    if(SelectedItem==UBackpackInventory::TabletId){if(Tablet)Tablet->SetOpen(true);}
    else if(SelectedItem==UBackpackInventory::BatteryId) ReplaceBattery();
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
    return FString::Printf(TEXT("%d  %s%s"),Index+1,*ItemTitle(Id),Count>0?*FString::Printf(TEXT(" ×%d"),Count):TEXT(""));
}
FString UCamcorderEquipmentComponent::SelectedDescription() const
{
    const auto* E=Inventory?Inventory->Find(SelectedItem):nullptr; if(!E) return TEXT("背包为空。靠近绿色电池或摄像机，按 F 收入背包。");
    FString S=E->Description;
    const int32 Slot=Inventory->SlotIndex(SelectedItem);
    if(!Inventory->CanAssign(SelectedItem)) return S+TEXT("\n\n此物品不能登记快捷栏，请在背包中使用。");
    S+=Slot>=0?FString::Printf(TEXT("\n\n已登记：快捷槽 %d"),Slot+1):TEXT("\n\n尚未登记：选中后按 1–4，或右键选择槽位。");
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
         [SNew(SButton).ButtonStyle(FCoreStyle::Get(),"NoBorder").ContentPadding(FMargin(16)).IsFocusable(false)
          .OnClicked_Lambda([Weak,Id]{if(auto* E=Weak.Get()) E->SelectInventoryItem(Id); return FReply::Handled();})
          [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).Text_Lambda([Weak,Id]{auto* E=Weak.Get();return FText::FromString(E?FString::Printf(TEXT("%s  ×%d\n右键登记快捷槽"),*E->ItemTitle(Id),E->Inventory->Count(Id)):TEXT(""));})]]];
    }
    if(Inventory->Items.IsEmpty()) InventoryList->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("背包为空")))];
}

void UCamcorderEquipmentComponent::CreateBackpackHUD()
{
    const TWeakObjectPtr<UCamcorderEquipmentComponent> Weak(this);
    const auto Font=FCoreStyle::GetDefaultFontStyle("Regular",18);
    auto Label=[Font](const FString& Text){return SNew(STextBlock).Font(Font).Text(FText::FromString(Text));};
    auto Button=[Label](const FString& Text,TFunction<void()> Action){return SNew(SButton).IsFocusable(false).ContentPadding(FMargin(16,10)).OnClicked_Lambda([Action]{Action();return FReply::Handled();})[Label(Text)];};
    auto Bar=SNew(SHorizontalBox);
    auto Slots=SNew(SHorizontalBox);
    auto Assignments=SNew(SVerticalBox);
    for(int32 I=0;I<UBackpackInventory::QuickSlotCount;++I)
    {
        Bar->AddSlot().AutoWidth().Padding(3)[SNew(SBorder).Padding(10).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor_Lambda([Weak,I]{auto* E=Weak.Get();return E && E->Inventory->ActiveSlot==I?FLinearColor(.07f,.35f,.42f,.95f):FLinearColor(.02f,.03f,.05f,.95f);})
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).Text_Lambda([Weak,I]{auto* E=Weak.Get();return FText::FromString(E?E->SlotTitle(I):TEXT(""));})]];
        Slots->AddSlot().FillWidth(1).Padding(3)[SNew(SButton).IsFocusable(false).ContentPadding(8)
            .OnClicked_Lambda([Weak,I]{if(auto* E=Weak.Get()) E->AssignQuickSlot(I);return FReply::Handled();})
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).AutoWrapText(true).Text_Lambda([Weak,I]{auto* E=Weak.Get();return FText::FromString(E?E->SlotTitle(I):TEXT(""));})]];
        Assignments->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).IsFocusable(false).ContentPadding(10)
            .OnClicked_Lambda([Weak,I]{if(auto* E=Weak.Get())E->AssignQuickSlot(I);return FReply::Handled();})
            [SNew(STextBlock).Font(Font).Text_Lambda([Weak,I]{auto* E=Weak.Get();return FText::FromString(TEXT("登记到 ")+(E?E->SlotTitle(I):TEXT("")));})]];
    }
    Assignments->AddSlot().AutoHeight().Padding(0,8)[Button(TEXT("取消"),[Weak]{if(auto* E=Weak.Get()) E->bAssignmentMenu=false;})];
    TSharedRef<SWidget> Panel=SNew(SBorder).Padding(24).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.035f,.05f,1))
    [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
      [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",28)).Text(FText::FromString(TEXT("背包  /  BACKPACK")))]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("关闭  [I]"),[Weak]{if(auto* E=Weak.Get())E->SetBackpackOpen(false);})]]
      +SVerticalBox::Slot().FillHeight(1)
      [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(.27f).Padding(0,0,16,0)
        [SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(InventoryList,SVerticalBox)]]
        +SHorizontalBox::Slot().FillWidth(.43f).Padding(0,0,16,0)
        [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("物体检视 / 3D"))]
          +SVerticalBox::Slot().FillHeight(1)[SNew(SInventoryInspection).Equipment(Weak)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).AutoWrapText(true).Text(FText::FromString(TEXT("左键拖动旋转 · 滚轮缩放 · 双击复位")))]
          +SVerticalBox::Slot().AutoHeight()[Button(TEXT("重置视角"),[Weak]{if(auto* E=Weak.Get())if(E->ItemPreview)E->ItemPreview->ResetView();})]]
        +SHorizontalBox::Slot().FillWidth(.30f)
        [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",24)).Text_Lambda([Weak]{auto* E=Weak.Get();return FText::FromString(E?E->ItemTitle(E->SelectedItem):TEXT(""));})]
          +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(STextBlock).Font(Font).AutoWrapText(true).Text_Lambda([Weak]{auto* E=Weak.Get();return FText::FromString(E?E->SelectedDescription():TEXT(""));})]]
          +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("使用 / 装备  [Enter]"),[Weak]{if(auto* E=Weak.Get())E->UseSelectedItem();})]
            +SVerticalBox::Slot().AutoHeight()[Button(TEXT("摄像机收入背包"),[Weak]{if(auto* E=Weak.Get())E->StoreCamera();})]]]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).ColorAndOpacity(FLinearColor(1,.8f,.3f)).AutoWrapText(true).Text_Lambda([Weak]{auto* E=Weak.Get();return FText::FromString(E && E->Camcorder?E->Camcorder->Notice:TEXT(""));})]
      +SVerticalBox::Slot().AutoHeight()[Label(TEXT("快捷栏登记：选中物品后按 1–4 / 点击槽位，也可右键物品"))]
      +SVerticalBox::Slot().AutoHeight().Padding(0,10)[Slots]
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).Text(FText::FromString(TEXT("游戏中 1–4 切换手持物品 · 电池在背包中使用 · 打开背包不暂停录像")))]];
    BackpackHUD=SNew(SCamcorderBackpackRoot).Equipment(Weak)
      .Visibility_Lambda([Weak]{auto* E=Weak.Get();return E && E->bBackpackOpen?EVisibility::Visible:EVisibility::SelfHitTestInvisible;})
      [SNew(SOverlay)
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,20)
        [SNew(SBox).Visibility_Lambda([Weak]{auto* E=Weak.Get();return E && !E->bBackpackOpen && E->Camcorder && !E->Camcorder->bViewfinderRaised?EVisibility::HitTestInvisible:EVisibility::Collapsed;})[Bar]]
        +SOverlay::Slot()
        [SNew(SBorder).Visibility_Lambda([Weak]{auto* E=Weak.Get();return E && E->bBackpackOpen?EVisibility::Visible:EVisibility::Collapsed;})
         .Padding(35).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.75f))
         [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1200).HeightOverride(680)[Panel]]]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBorder).Visibility_Lambda([Weak]{auto* E=Weak.Get();return E && E->bBackpackOpen && E->bAssignmentMenu?EVisibility::Visible:EVisibility::Collapsed;})
         .Padding(20).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.05f,.09f,.13f))
         [SNew(SBox).WidthOverride(360)[Assignments]]]];
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(BackpackHUD.ToSharedRef(),20);
    RefreshBackpack();
}
