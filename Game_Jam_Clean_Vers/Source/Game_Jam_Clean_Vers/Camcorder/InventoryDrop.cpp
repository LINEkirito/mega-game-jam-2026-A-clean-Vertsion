#include "CamcorderEquipmentComponent.h"
#include "BackpackInventory.h"
#include "ShoulderCamcorder.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void UCamcorderEquipmentComponent::DropActiveQuickSlot()
{
    if(Inventory && Inventory->Slots.IsValidIndex(Inventory->ActiveSlot))
        DropInventoryItem(Inventory->Slots[Inventory->ActiveSlot]);
}

bool UCamcorderEquipmentComponent::DropInventoryItem(FName Id)
{
    auto* PC=Player();
    if(!Inventory || Inventory->Count(Id)<=0 || !PC || !PC->GetPawn()) return false;
    bool Dropped=false;
    if(Id==UBackpackInventory::CameraId)
    {
        if(Camcorder && Camcorder->DropFromInventory(PC))
        { Inventory->Remove(Id); Dropped=true; }
    }
    else if(Combat && Combat->Weapons.Contains(Id)) Dropped=Combat->DropInventoryWeapon(Id);
    else
    {
        FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
        const FTransform Spawn(View,Eye+View.Vector()*65-FVector(0,0,25));
        AActor* Pickup=nullptr;
        int32 Amount=1;
        if(Id==UBackpackInventory::BatteryId)
            Pickup=GetWorld()->SpawnActorDeferred<ACamcorderBattery>(ACamcorderBattery::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
        else if(Id==TEXT("Ammo9mm") || Id==TEXT("Ammo556") || Id==TEXT("Ammo12ga"))
        {
            auto* Ammo=GetWorld()->SpawnActorDeferred<ACombatAmmoPickup>(ACombatAmmoPickup::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
            if(Ammo)
            {
                Ammo->Kind=Id==TEXT("Ammo9mm")?ETacticalWeaponKind::Pistol:Id==TEXT("Ammo556")?ETacticalWeaponKind::Rifle:ETacticalWeaponKind::Shotgun;
                Amount=Inventory->Count(Id); Ammo->Amount=Amount; Pickup=Ammo;
            }
        }
        if(Pickup)
        {
            Pickup->FinishSpawning(Spawn);
            if(IsValid(Pickup) && Inventory->Remove(Id,Amount)) Dropped=true;
            else if(IsValid(Pickup)) Pickup->Destroy();
        }
    }
    if(Camcorder) Camcorder->SetNotice(Dropped?TEXT("已丢下；靠近后按 F 拾取"):TEXT("暂时无法丢下；请等装备动作完成或移到空旷位置"));
    RefreshBackpack();
    return Dropped;
}
