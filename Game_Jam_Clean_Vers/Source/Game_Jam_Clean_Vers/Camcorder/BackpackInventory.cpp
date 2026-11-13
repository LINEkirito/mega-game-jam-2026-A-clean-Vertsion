#include "BackpackInventory.h"
#include "Data/DataTransmitter.h"
#include "Engine/World.h"
namespace { void InventoryChanged(UBackpackInventory* Inventory,FName Field,float Value) { if(auto* Outer=Inventory->GetOuter()) if(auto* World=Outer->GetWorld()) if(auto* Bus=World->GetSubsystem<UDataTransmitter>()) Bus->NotifyChange(Inventory,Field,Value); } }
const FName UBackpackInventory::CameraId(TEXT("ShoulderCamcorder"));
const FName UBackpackInventory::TabletId(TEXT("RecordingTablet"));
const FName UBackpackInventory::BatteryId(TEXT("CamcorderBattery"));
UBackpackInventory::UBackpackInventory() { Slots.Init(NAME_None,5); }
const FBackpackEntry* UBackpackInventory::Find(FName Id) const { return Items.FindByPredicate([Id](const FBackpackEntry& E){return E.Id==Id;}); }
int32 UBackpackInventory::Count(FName Id) const { const auto* E=Find(Id); return E?E->Count:0; }
int32 UBackpackInventory::SlotIndex(FName Id) const { return Id.IsNone()?INDEX_NONE:Slots.IndexOfByKey(Id); }
bool UBackpackInventory::Add(FName Id,const FString& Name,const FString& Description,int32 Amount)
{
    if(Id.IsNone() || Amount<=0 || Count(Id)>MAX_int32-Amount) return false;
    if(auto* E=Items.FindByPredicate([Id](const FBackpackEntry& Item){return Item.Id==Id;})) E->Count+=Amount;
    else { FBackpackEntry Entry; Entry.Id=Id; Entry.Name=Name; Entry.Description=Description; Entry.Count=Amount; Items.Add(Entry); }
    FillEmptySlots(); InventoryChanged(this,Id,float(Count(Id))); return true;
}
bool UBackpackInventory::Remove(FName Id,int32 Amount)
{
    if(Amount<=0 || Count(Id)<Amount) return false;
    auto* E=Items.FindByPredicate([Id](const FBackpackEntry& Item){return Item.Id==Id;});
    E->Count-=Amount;
    if(E->Count==0) { Items.RemoveAll([Id](const FBackpackEntry& Item){return Item.Id==Id;}); for(auto& Slot:Slots) if(Slot==Id) Slot=NAME_None; }
    FillEmptySlots(); InventoryChanged(this,Id,float(Count(Id))); return true;
}
bool UBackpackInventory::Assign(int32 Slot,FName Id)
{
    if(!Slots.IsValidIndex(Slot) || Count(Id)==0) return false;
    for(auto& Existing:Slots) if(Existing==Id) Existing=NAME_None;
    Slots[Slot]=Id; FillEmptySlots(); InventoryChanged(this,TEXT("QuickSlots"),float(Slot)); return true;
}
void UBackpackInventory::FillEmptySlots()
{
    for(const auto& E:Items) if(E.Count>0 && !Slots.Contains(E.Id))
    {
        const int32 Empty=Slots.IndexOfByKey(NAME_None); if(Empty==INDEX_NONE) return;
        Slots[Empty]=E.Id;
    }
}
