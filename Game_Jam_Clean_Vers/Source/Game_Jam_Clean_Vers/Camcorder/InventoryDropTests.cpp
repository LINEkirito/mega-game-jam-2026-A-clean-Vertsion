#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "CamcorderEquipmentComponent.h"
#include "BackpackInventory.h"
#include "ShoulderCamcorder.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Combat/PatrolEnemy.h"
#include "Localization/GameLanguage.h"
#include "Components/PrimitiveComponent.h"
#include "Framework/Application/SlateApplication.h"

class FPickupDropCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage=0, Batteries=0, AmmoCount=0, Magazine=0;
    float Next=0;
    FName GunId, AmmoId;
    TWeakObjectPtr<ATacticalWeapon> Gun;
    TArray<FName> Slots;
public:
    explicit FPickupDropCheck(FAutomationTestBase* T):Test(T){}
    virtual bool Update() override
    {
        UWorld* World=nullptr;
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){World=C.World();break;}
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
        if(!E || !E->Inventory || !E->Combat){Test->AddError(TEXT("Missing equipment"));return true;}
        if(World->GetTimeSeconds()<Next)return false;
        auto* Bag=E->Inventory.Get(); auto* Combat=E->Combat.Get();
        auto Key=[PC](FKey K,EInputEvent Event){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Pressed?1.f:0.f));};
        auto Place=[PC](AActor* Item)
        {
            if(!Item)return;
            if(auto* Body=Cast<UPrimitiveComponent>(Item->GetRootComponent()))Body->SetSimulatePhysics(false);
            FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
            Item->SetActorLocation(Eye+View.Vector()*80,false,nullptr,ETeleportType::TeleportPhysics);
        };
        Next=World->GetTimeSeconds()+.3f;
        switch(Stage++)
        {
        case 0:
            PC->SetPause(false);PC->bShowMouseCursor=false;
            for(TActorIterator<APatrolEnemy> It(World);It;++It)It->SetActorTickEnabled(false);
            // Isolate the round trip from preplaced level supplies (the map also has a 34-round box).
            for(TActorIterator<ACombatAmmoPickup> It(World);It;++It)It->Destroy();
            for(TActorIterator<ACamcorderBattery> It(World);It;++It)It->Destroy();
            for(TActorIterator<ATacticalWeapon> It(World);It;++It)
                if(!It->bNPCEquipped && !Combat->Weapons.Contains(It->InventoryId))It->Destroy();
            Slots=Bag->Slots; E->ActivateQuickSlot(0); Next=World->GetTimeSeconds()+3;break;
        case 1: Key(EKeys::Q,IE_Pressed);Next=World->GetTimeSeconds()+2;break;
        case 2:
            Key(EKeys::Q,IE_Released);
            Test->TestEqual(TEXT("Q removes camera ownership"),Bag->Count(UBackpackInventory::CameraId),0);
            Test->TestFalse(TEXT("Camera physically released"),E->Camcorder->IsPresenting());
            Place(E->Camcorder);Key(EKeys::E,IE_Pressed);break;
        case 3:
            Key(EKeys::E,IE_Released);Test->TestEqual(TEXT("E does not pick up"),Bag->Count(UBackpackInventory::CameraId),0);
            Test->TestTrue(TEXT("Camera pickup has F prompt"),Combat->InteractionText().Contains(TEXT("[F]")));
            Key(EKeys::F,IE_Pressed);Next=World->GetTimeSeconds()+3;break;
        case 4:
            Key(EKeys::F,IE_Released);Test->TestTrue(TEXT("F picks camera up"),E->Camcorder->bHeld);
            E->ActivateQuickSlot(1);Next=World->GetTimeSeconds()+4;break;
        case 5:
            Test->TestNotNull(TEXT("Gun equipped"),Combat->ActiveWeapon.Get());
            if(!Combat->ActiveWeapon)return true;
            Gun=Combat->ActiveWeapon;GunId=Gun->InventoryId;Magazine=Gun->Magazine;
            Batteries=Bag->Count(UBackpackInventory::BatteryId);
            Test->TestTrue(TEXT("Drop battery from backpack"),E->DropInventoryItem(UBackpackInventory::BatteryId));
            Test->TestEqual(TEXT("One battery removed"),Bag->Count(UBackpackInventory::BatteryId),Batteries-1);
            for(TActorIterator<ACamcorderBattery> It(World);It;++It){Place(*It);break;}
            Test->TestTrue(TEXT("Battery pickup has F prompt while armed"),Combat->InteractionText().Contains(TEXT("[F]")));
            Key(EKeys::F,IE_Pressed);break;
        case 6:
            Key(EKeys::F,IE_Released);Test->TestEqual(TEXT("F picks battery up while armed"),Bag->Count(UBackpackInventory::BatteryId),Batteries);
            AmmoId=Gun->Spec().Ammo;Bag->Add(AmmoId,TEXT("Test ammo"),TEXT("Drop round trip"),17);AmmoCount=Bag->Count(AmmoId);
            Test->TestTrue(TEXT("Drop reserve ammunition"),E->DropInventoryItem(AmmoId));
            Test->TestEqual(TEXT("Ammo stack removed"),Bag->Count(AmmoId),0);
            for(TActorIterator<ACombatAmmoPickup> It(World);It;++It)if(It->AmmoId()==AmmoId){Test->TestEqual(TEXT("Ammo count preserved"),It->Amount,AmmoCount);Place(*It);break;}
            break;
        case 7: Test->TestTrue(TEXT("Ammo pickup has F prompt"),Combat->InteractionText().Contains(TEXT("[F]")));Key(EKeys::F,IE_Pressed);break;
        case 8:
            Key(EKeys::F,IE_Released);Test->TestEqual(TEXT("Ammo recovered"),Bag->Count(AmmoId),AmmoCount);
            E->SetBackpackOpen(true);E->ShowAssignmentMenu(Slots[2]);
            Test->TestTrue(TEXT("Right-click entry opens context menu"),FSlateApplication::Get().AnyMenusVisible());
            FSlateApplication::Get().DismissAllMenus();
            Test->TestTrue(TEXT("Drop unselected stored gun"),E->DropInventoryItem(Slots[2]));
            Test->TestEqual(TEXT("Stored gun removed"),Bag->Count(Slots[2]),0);
            Test->TestTrue(TEXT("Active pistol retained"),Combat->ActiveWeapon==Gun.Get());
            E->SetBackpackOpen(false);Key(EKeys::Q,IE_Pressed);Next=World->GetTimeSeconds()+3;break;
        case 9:
            Key(EKeys::Q,IE_Released);Test->TestEqual(TEXT("Q drops active gun"),Bag->Count(GunId),0);
            Test->TestEqual(TEXT("Magazine preserved"),Gun->Magazine,Magazine);
            Test->TestTrue(TEXT("Fixed slots preserved"),Bag->Slots==Slots);
            // Keep the other dropped weapon out of the proximity query.
            for(TActorIterator<ATacticalWeapon> It(World);It;++It)if(It->InventoryId==Slots[2])It->SetActorLocation(FVector(0,0,-40000));
            Place(Gun.Get());Test->TestTrue(TEXT("Gun pickup has F prompt"),Combat->InteractionText().Contains(TEXT("[F]")));Key(EKeys::F,IE_Pressed);Next=World->GetTimeSeconds()+3;break;
        default:
            Key(EKeys::F,IE_Released);Test->TestEqual(TEXT("F restores gun ownership"),Bag->Count(GunId),1);
            Test->TestTrue(TEXT("Picked gun equipped"),Combat->ActiveWeapon==Gun.Get());
            Test->TestTrue(TEXT("Stored camera drop succeeds while gun equipped"),E->DropInventoryItem(UBackpackInventory::CameraId));
            Test->TestTrue(TEXT("Dropping stored camera retains active gun"),Combat->ActiveWeapon==Gun.Get());
            return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPickupDropTest,"Game.Inventory.PickupDrop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPickupDropTest::RunTest(const FString&)
{
    AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
    AddCommand(new FPickupDropCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
