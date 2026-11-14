#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Data/GlobalData.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/ShoulderCamcorder.h"

float UCombatEquipmentComponent::MinimumHoldPitch(const ATacticalWeapon* Weapon) const
{
 const auto* Config=GetDefault<UGlobalData>();
 if(!Weapon)return -85.f;
 return FMath::Clamp(Weapon->Kind==ETacticalWeaponKind::Pistol?Config->PistolMinimumHoldPitch:
     Weapon->Kind==ETacticalWeaponKind::Shotgun?Config->ShotgunMinimumHoldPitch:Config->RifleMinimumHoldPitch,-85.f,0.f);
}

bool UCombatEquipmentComponent::IsBelowHoldPitch(const ATacticalWeapon* Weapon) const
{
 return Weapon&&Player()&&FMath::UnwindDegrees(Player()->GetControlRotation().Pitch)<MinimumHoldPitch(Weapon);
}

void UCombatEquipmentComponent::TickLookDownHolster()
{
 auto* PC=Player();auto* Character=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 if(!Character||Character->Health<=0||!Equipment||!Equipment->Inventory)return;
 if(ActiveWeapon&&!TransitionWeapon&&IsBelowHoldPitch(ActiveWeapon))
 {
  LookDownWeaponId=ActiveWeapon->InventoryId;
  TransitionWeapon=ActiveWeapon;ActiveWeapon=nullptr;PendingEquip=NAME_None;bPendingDrop=false;
  bADS=false;bReloading=false;EquipRemaining=0;Recoil=0;VisualKick=0;
  // Holster cancels reload safely, including returning an unseated magazine.
  TransitionWeapon->Holster();
  return;
 }
 if(!HasLookDownSelection()||TransitionWeapon||ActiveWeapon||!PendingEquip.IsNone())return;
 auto* Found=Weapons.Find(LookDownWeaponId);
 if(!Found||!IsValid(*Found)||Equipment->Inventory->Count(LookDownWeaponId)<=0||Equipment->Camcorder->IsPresenting())
 {LookDownWeaponId=NAME_None;return;}
 const float RestorePitch=MinimumHoldPitch(*Found)+FMath::Max(0.f,GetDefault<UGlobalData>()->LowLookRestoreHysteresis);
 if(FMath::UnwindDegrees(PC->GetControlRotation().Pitch)>=RestorePitch)
 {
  const FName Restore=LookDownWeaponId;LookDownWeaponId=NAME_None;
  EquipWeapon(Restore);
 }
}
