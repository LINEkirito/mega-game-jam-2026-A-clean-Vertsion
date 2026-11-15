#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Combat/WeaponTestInventory.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PatrolEnemy.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/ShoulderCamcorder.h"

class FFixedQuickSlotCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage=0;
    int32 WheelPulses=0;
    int32 HolsterWheelPulses=0;
    float NextTime=0;
    TArray<FName> Original;
public:
    explicit FFixedQuickSlotCheck(FAutomationTestBase* InTest):Test(InTest){}
    virtual bool Update() override
    {
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Combat=PC?PC->FindComponentByClass<UCombatEquipmentComponent>():nullptr;
        auto* E=Combat?Combat->Equipment.Get():nullptr;
        if(!E || !E->Inventory){Test->AddError(TEXT("Missing PIE equipment"));return true;}
        auto* Bag=E->Inventory.Get();
        if(World->GetTimeSeconds()<NextTime)return false;
        if(Stage==1 && WheelPulses<16)
        {
            Test->TestNull(TEXT("Continuous wheel input keeps every gun stowed"),Combat->ActiveWeapon.Get());
            Combat->Cycle(1);
            ++WheelPulses;
            NextTime=World->GetTimeSeconds()+(WheelPulses==16?4.f:.1f);
            return false;
        }
        if(Stage==2 && HolsterWheelPulses<16)
        {
            Test->TestNull(TEXT("Scrolling past holster completion never draws an intermediate gun"),Combat->ActiveWeapon.Get());
            Combat->Cycle(1);
            ++HolsterWheelPulses;
            NextTime=World->GetTimeSeconds()+(HolsterWheelPulses==16?4.f:.1f);
            return false;
        }
        switch(Stage++)
        {
        case 0:
            GiveTestWeapons(E);
            for(TActorIterator<APatrolEnemy> It(World);It;++It)It->SetActorTickEnabled(false);
            Original=Bag->Slots;
            Test->TestEqual(TEXT("Exactly four slots"),Original.Num(),4);
            if(Original.Num()!=4)return true;
            Test->TestEqual(TEXT("Slot 2 is pistol"),Combat->Weapons[Original[1]]->Kind,ETacticalWeaponKind::Pistol);
            Test->TestEqual(TEXT("Slot 3 is rifle"),Combat->Weapons[Original[2]]->Kind,ETacticalWeaponKind::Rifle);
            Test->TestEqual(TEXT("Slot 4 is shotgun"),Combat->Weapons[Original[3]]->Kind,ETacticalWeaponKind::Shotgun);
            Bag->Add(TEXT("ForeignHandheld"),TEXT("Other"),TEXT("Test"),1,true);
            Test->TestFalse(TEXT("Other handheld cannot replace a slot"),Bag->Assign(1,TEXT("ForeignHandheld")));
            Test->TestFalse(TEXT("Camera cannot move to another slot"),Bag->Assign(3,UBackpackInventory::CameraId));
            Combat->Cycle(-1);
            Test->TestEqual(TEXT("Left wraps camera to shotgun"),Bag->ActiveSlot,3);
            Combat->Cycle(1);
            Test->TestEqual(TEXT("Right wraps shotgun to camera during transition"),Bag->ActiveSlot,0);
            Combat->Cycle(1);Combat->Cycle(1);Combat->Cycle(1);
            Test->TestEqual(TEXT("Rapid wheel selects latest slot"),Bag->ActiveSlot,3);
            Test->TestNull(TEXT("Scrolling never draws intermediate guns"),Combat->ActiveWeapon.Get());
            NextTime=World->GetTimeSeconds()+.1f;return false;
        case 1:
            Test->TestTrue(TEXT("Rapid selection equips shotgun"),Combat->ActiveWeapon && Combat->ActiveWeapon->Kind==ETacticalWeaponKind::Shotgun);
            Combat->Cycle(1);
            Test->TestEqual(TEXT("Camera highlight updates immediately"),Bag->ActiveSlot,0);
            Test->TestNull(TEXT("Gun leaves active slot during holster"),Combat->ActiveWeapon.Get());
            NextTime=World->GetTimeSeconds()+.1f;return false;
        case 2:
            Test->TestTrue(TEXT("Camera equipped after holster"),E->Camcorder->bHeld);
            Test->TestNull(TEXT("No gun remains presented"),Combat->GetPresentationWeapon());
            Bag->Remove(Original[1]);
            Test->TestTrue(TEXT("Removal preserves all bindings"),Bag->Slots==Original);
            Bag->Remove(Original[2]);Bag->Remove(Original[3]);
            Combat->Cycle(1);Combat->Cycle(-1);
            E->ActivateQuickSlot(1);E->ActivateQuickSlot(2);E->ActivateQuickSlot(3);
            Test->TestEqual(TEXT("Camera-only switching preserves selection"),Bag->ActiveSlot,0);
            Test->TestTrue(TEXT("Camera-only switching does not stow camera"),E->Camcorder->bHeld&&!E->Camcorder->bInBackpack);
            Bag->Add(Original[2],TEXT("Rifle"),TEXT("Returned"),1,true);
            Bag->Add(Original[3],TEXT("Shotgun"),TEXT("Returned"),1,true);
            Combat->Cycle(1);
            Test->TestEqual(TEXT("Wheel skips missing pistol"),Bag->ActiveSlot,2);
            E->ActivateQuickSlot(1);
            Test->TestEqual(TEXT("Missing hotkey preserves pending selection"),Bag->ActiveSlot,2);
            Bag->Add(Original[1],TEXT("Pistol"),TEXT("Returned"),1,true);
            Test->TestTrue(TEXT("Reacquisition restores original binding"),Bag->Slots==Original);
            E->ActivateQuickSlot(1);
            NextTime=World->GetTimeSeconds()+4;return false;
        case 3:
            Test->TestTrue(TEXT("Returned pistol equips"),Combat->ActiveWeapon && Combat->ActiveWeapon->InventoryId==Original[1]);
            Bag->Remove(Original[2]);
            E->ActivateQuickSlot(2);
            Test->TestTrue(TEXT("Missing hotkey does not holster held gun"),Combat->ActiveWeapon && Combat->ActiveWeapon->InventoryId==Original[1]);
            Bag->Add(Original[2],TEXT("Rifle"),TEXT("Returned"),1,true);
            Test->TestTrue(TEXT("Four fixed slots survive inventory mutations"),Bag->Slots==Original);
            Combat->DropWeapon();
            NextTime=World->GetTimeSeconds()+3;return false;
        case 4:
            Test->TestEqual(TEXT("Actual drop removes ownership"),Bag->Count(Original[1]),0);
            Test->TestTrue(TEXT("Actual drop preserves bindings"),Bag->Slots==Original);
            for(TActorIterator<ATacticalWeapon> It(World);It;++It)if(It->InventoryId==Original[1])
            {
                FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
                It->SetActorLocation(Eye+View.Vector()*80,false,nullptr,ETeleportType::TeleportPhysics);
                break;
            }
            Test->TestTrue(TEXT("Dropped weapon can be picked up"),Combat->TryInteract());
            NextTime=World->GetTimeSeconds()+3;return false;
        case 5:
            Test->TestEqual(TEXT("Pickup restores ownership"),Bag->Count(Original[1]),1);
            Test->TestTrue(TEXT("Pickup retains fixed order"),Bag->Slots==Original);
            Test->TestTrue(TEXT("Pickup equips original pistol"),Combat->ActiveWeapon && Combat->ActiveWeapon->InventoryId==Original[1]);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollUp,IE_Pressed,1.f));
            NextTime=World->GetTimeSeconds()+.2f;return false;
        case 6:
            Test->TestEqual(TEXT("Wheel up selects previous slot"),Bag->ActiveSlot,0);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollUp,IE_Released,0.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown,IE_Pressed,1.f));
            NextTime=World->GetTimeSeconds()+.2f;return false;
        case 7:
            Test->TestEqual(TEXT("Wheel down reverses during transition"),Bag->ActiveSlot,1);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown,IE_Released,0.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Three,IE_Pressed,1.f));
            NextTime=World->GetTimeSeconds()+4;return false;
        case 8:
            Test->TestTrue(TEXT("Key 3 equips rifle"),Combat->ActiveWeapon && Combat->ActiveWeapon->Kind==ETacticalWeaponKind::Rifle);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Three,IE_Released,0.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Four,IE_Pressed,1.f));
            NextTime=World->GetTimeSeconds()+4;return false;
        case 9:
            Test->TestTrue(TEXT("Key 4 equips shotgun"),Combat->ActiveWeapon && Combat->ActiveWeapon->Kind==ETacticalWeaponKind::Shotgun);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Four,IE_Released,0.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::One,IE_Pressed,1.f));
            NextTime=World->GetTimeSeconds()+4;return false;
        case 10:
            Test->TestTrue(TEXT("Key 1 equips camera"),E->Camcorder->bHeld && !Combat->ActiveWeapon);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::One,IE_Released,0.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Two,IE_Pressed,1.f));
            NextTime=World->GetTimeSeconds()+4;return false;
        default:
            Test->TestTrue(TEXT("Key 2 equips pistol"),Combat->ActiveWeapon && Combat->ActiveWeapon->Kind==ETacticalWeaponKind::Pistol);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Two,IE_Released,0.f));
            return true;
        }
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedQuickSlots,"Game.Inventory.FixedQuickSlots",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFixedQuickSlots::RunTest(const FString&)
{
    AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
    AddCommand(new FFixedQuickSlotCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
