#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
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
        switch(Stage++)
        {
        case 0:
            for(TActorIterator<APatrolEnemy> It(World);It;++It)It->SetActorTickEnabled(false);
            Original=Bag->Slots;
            Test->TestEqual(TEXT("Exactly four slots"),Original.Num(),4);
            if(Original.Num()!=4)return true;
            Bag->Add(TEXT("ForeignHandheld"),TEXT("Other"),TEXT("Test"),1,true);
            Test->TestFalse(TEXT("Other handheld cannot replace a slot"),Bag->Assign(1,TEXT("ForeignHandheld")));
            Test->TestFalse(TEXT("Camera cannot move to another slot"),Bag->Assign(3,UBackpackInventory::CameraId));
            Combat->Cycle(-1);
            Test->TestEqual(TEXT("Left wraps camera to rifle"),Bag->ActiveSlot,3);
            Combat->Cycle(1);
            Test->TestEqual(TEXT("Right wraps rifle to camera during transition"),Bag->ActiveSlot,0);
            Combat->Cycle(1);Combat->Cycle(1);Combat->Cycle(1);
            Test->TestEqual(TEXT("Rapid wheel selects latest slot"),Bag->ActiveSlot,3);
            NextTime=World->GetTimeSeconds()+4;return false;
        case 1:
            Test->TestTrue(TEXT("Rapid selection equips rifle"),Combat->ActiveWeapon && Combat->ActiveWeapon->InventoryId==Original[3]);
            E->ActivateQuickSlot(0);
            Test->TestEqual(TEXT("Camera highlight updates immediately"),Bag->ActiveSlot,0);
            Test->TestNull(TEXT("Gun leaves active slot during holster"),Combat->ActiveWeapon.Get());
            NextTime=World->GetTimeSeconds()+4;return false;
        case 2:
            Test->TestTrue(TEXT("Camera equipped after holster"),E->Camcorder->bHeld);
            Test->TestNull(TEXT("No gun remains presented"),Combat->GetPresentationWeapon());
            Bag->Remove(Original[1]);
            Test->TestTrue(TEXT("Removal preserves all bindings"),Bag->Slots==Original);
            Combat->Cycle(1);
            Test->TestEqual(TEXT("Missing item still has selectable slot"),Bag->ActiveSlot,1);
            Test->TestNull(TEXT("Missing slot leaves hands without weapon"),Combat->ActiveWeapon.Get());
            Bag->Add(Original[1],TEXT("Pistol"),TEXT("Returned"),1,true);
            Test->TestTrue(TEXT("Reacquisition restores original binding"),Bag->Slots==Original);
            E->ActivateQuickSlot(1);
            NextTime=World->GetTimeSeconds()+4;return false;
        case 3:
            Test->TestTrue(TEXT("Returned pistol equips"),Combat->ActiveWeapon && Combat->ActiveWeapon->InventoryId==Original[1]);
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
            Test->TestEqual(TEXT("Wheel up advances the visible slot"),Bag->ActiveSlot,2);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollUp,IE_Released,0.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown,IE_Pressed,1.f));
            NextTime=World->GetTimeSeconds()+.2f;return false;
        default:
            Test->TestEqual(TEXT("Wheel down reverses during transition"),Bag->ActiveSlot,1);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollDown,IE_Released,0.f));
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
