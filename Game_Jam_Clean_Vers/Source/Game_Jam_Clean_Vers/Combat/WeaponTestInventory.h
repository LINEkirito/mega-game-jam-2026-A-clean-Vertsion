#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Engine/World.h"
// Explicit equipment fixture: production players now obtain their guns from the table.
inline void GiveTestWeapons(UCamcorderEquipmentComponent* E)
{
 if(!E||!E->Inventory||!E->Combat)return;
 for(int32 I=0;I<3;++I){
  if(!E->Inventory->Slots[I+1].IsNone())continue;
  const FTransform T(FVector(0,0,1000));
  auto* G=E->GetWorld()->SpawnActorDeferred<ATacticalWeapon>(ATacticalWeapon::StaticClass(),T);
  if(!G)continue;G->Kind=ETacticalWeaponKind(I);G->FinishSpawning(T);G->Stow();
  E->Combat->Weapons.Add(G->InventoryId,G);
  E->Inventory->Add(G->InventoryId,G->Spec().Name,TEXT("Test fixture"),1,true);
  E->Inventory->BindStartingWeapon(I+1,G->InventoryId);
 }
}
#endif
