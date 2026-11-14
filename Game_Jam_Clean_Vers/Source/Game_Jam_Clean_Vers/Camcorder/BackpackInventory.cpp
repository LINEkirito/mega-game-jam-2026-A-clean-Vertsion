#include "BackpackInventory.h"
#include "Data/DataTransmitter.h"
#include "Engine/World.h"
namespace { void InventoryChanged(UBackpackInventory* Inventory,FName Field,float Value) { if(auto* Outer=Inventory->GetOuter()) if(auto* World=Outer->GetWorld()) if(auto* Bus=World->GetSubsystem<UDataTransmitter>()) Bus->NotifyChange(Inventory,Field,Value); } }
const FName UBackpackInventory::CameraId(TEXT("ShoulderCamcorder"));
const FName UBackpackInventory::TabletId(TEXT("RecordingTablet"));
const FName UBackpackInventory::BatteryId(TEXT("CamcorderBattery"));
UBackpackInventory::UBackpackInventory() { Slots.Init(NAME_None,QuickSlotCount); Slots[0]=CameraId; FixedSlotNames.Init(TEXT("未持有"),QuickSlotCount); FixedSlotNames[0]=TEXT("手持摄像机"); }
FString UBackpackInventory::SlotName(int32 Slot) const { return FixedSlotNames.IsValidIndex(Slot)?FixedSlotNames[Slot]:FString(); }
const FBackpackEntry* UBackpackInventory::Find(FName Id) const { return Items.FindByPredicate([Id](const FBackpackEntry& E){return E.Id==Id;}); }
int32 UBackpackInventory::Count(FName Id) const { const auto* E=Find(Id); return E?E->Count:0; }
int32 UBackpackInventory::SlotIndex(FName Id) const { return Id.IsNone()?INDEX_NONE:Slots.IndexOfByKey(Id); }
bool UBackpackInventory::Add(FName Id,const FString& Name,const FString& Description,int32 Amount,bool bHandheld)
{
    if(Id==BatteryId || Id.IsNone() || Amount<=0 || Count(Id)>MAX_int32-Amount) return false;
    if(auto* E=Items.FindByPredicate([Id](const FBackpackEntry& Item){return Item.Id==Id;})) E->Count+=Amount;
    else { FBackpackEntry Entry; Entry.Id=Id; Entry.Name=Name; Entry.Description=Description; Entry.Count=Amount; Entry.bHandheld=bHandheld; Items.Add(Entry); }
    InventoryChanged(this,Id,float(Count(Id))); return true;
}
bool UBackpackInventory::Remove(FName Id,int32 Amount)
{
    if(Amount<=0 || Count(Id)<Amount) return false;
    auto* E=Items.FindByPredicate([Id](const FBackpackEntry& Item){return Item.Id==Id;});
    E->Count-=Amount;
    if(E->Count==0) Items.RemoveAll([Id](const FBackpackEntry& Item){return Item.Id==Id;});
    InventoryChanged(this,Id,float(Count(Id))); return true;
}
bool UBackpackInventory::CanAssign(FName Id) const { return !Id.IsNone() && Slots.Contains(Id) && Count(Id)>0; }
bool UBackpackInventory::Assign(int32 Slot,FName Id)
{
    return Slots.IsValidIndex(Slot) && CanAssign(Id) && Slots[Slot]==Id;
}
bool UBackpackInventory::BindStartingWeapon(int32 Slot,FName Id)
{
    if(Slot<=0 || !Slots.IsValidIndex(Slot) || !Slots[Slot].IsNone() || Id.IsNone() || Slots.Contains(Id) || Count(Id)==0) return false;
    Slots[Slot]=Id;
    FixedSlotNames[Slot]=Find(Id)->Name;
    InventoryChanged(this,TEXT("QuickSlots"),float(Slot));
    return true;
}
