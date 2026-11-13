#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "TacticalWeapon.h"
#include "CombatEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/RecordingTabletComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWeaponReferenceCheck,FAutomationTestBase*,Test);
bool FWeaponReferenceCheck::Update()
{
    UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){W=Context.World();break;}
    auto* C=W&&W->GetFirstPlayerController()?Cast<AGame_Jam_Clean_VersCharacter>(W->GetFirstPlayerController()->GetPawn()):nullptr;
    if(!C){Test->AddError(TEXT("No PIE character"));return true;}
    auto* StartingCombat=W->GetFirstPlayerController()->FindComponentByClass<UCombatEquipmentComponent>();
    auto* Equipment=StartingCombat?StartingCombat->Equipment.Get():nullptr;
    if(!Equipment||!Equipment->Inventory){Test->AddError(TEXT("No starting inventory"));return true;}
    auto* Inventory=Equipment->Inventory.Get();
    Test->TestEqual(TEXT("Four quick slots"),Inventory->Slots.Num(),4);
    Test->TestEqual(TEXT("Camera starts in slot one"),Inventory->Slots[0],UBackpackInventory::CameraId);
    Test->TestEqual(TEXT("Three starting guns"),StartingCombat->Weapons.Num(),3);
    const ETacticalWeaponKind StartingKinds[]={ETacticalWeaponKind::Pistol,ETacticalWeaponKind::Shotgun,ETacticalWeaponKind::Rifle};
    for(int32 I=0;I<3;++I){
        const auto Id=Inventory->Slots[I+1];const auto* Gun=StartingCombat->Weapons.Find(Id);
        if(Test->TestTrue(TEXT("Quick slot resolves to owned gun"),Gun&&IsValid(*Gun))){
            Test->TestEqual(TEXT("Starting gun order"),(*Gun)->Kind,StartingKinds[I]);
            Test->TestEqual(TEXT("One gun per inventory entry"),Inventory->Count(Id),1);
            Test->TestTrue(TEXT("Starting guns are stowed"),(*Gun)->IsHidden()&&!(*Gun)->bEquipped);
        }
    }
    const auto SlotsBefore=Inventory->Slots;
    Test->TestFalse(TEXT("Battery registration is rejected"),Inventory->Assign(1,UBackpackInventory::BatteryId));
    Inventory->Add(TEXT("Ammo9mm"),TEXT("Ammo"),TEXT("Test"),1);
    Test->TestFalse(TEXT("Ammo registration is rejected"),Inventory->Assign(1,TEXT("Ammo9mm")));
    Inventory->Remove(TEXT("Ammo9mm"));
    Test->TestTrue(TEXT("Rejected registration preserves slots"),Inventory->Slots==SlotsBefore);
    Test->TestFalse(TEXT("No fifth slot"),Inventory->Assign(4,UBackpackInventory::CameraId));
    Test->TestEqual(TEXT("Tablet is not a starting item"),Inventory->Count(UBackpackInventory::TabletId),0);
    Equipment->Tablet->SetOpen(true);
    Test->TestFalse(TEXT("Recording review cannot open"),Equipment->Tablet->bOpen);
    StartingCombat->InitializeStartingWeapons();
    Test->TestEqual(TEXT("Starting loadout cannot duplicate"),StartingCombat->Weapons.Num(),3);
    for(int32 Kind=0;Kind<3;++Kind)
    {
        const FTransform Spawn(FVector(0,0,700));auto* G=W->SpawnActorDeferred<ATacticalWeapon>(ATacticalWeapon::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);G->Kind=ETacticalWeaponKind(Kind);G->FinishSpawning(Spawn);
        auto* Bag=NewObject<UBackpackInventory>(G);Bag->Add(G->Spec().Ammo,TEXT("Test ammo"),TEXT("Test"),100);
        auto* Combat=W->GetFirstPlayerController()->FindComponentByClass<UCombatEquipmentComponent>();
        auto* Previous=Combat->ActiveWeapon.Get();Combat->ActiveWeapon=G;G->Equip(C);Test->TestFalse(TEXT("Draw blocks firing"),G->ConsumeShot());G->AdvanceHandling(5.f);G->UpdatePose(1,true,0);
        Test->TestEqual(TEXT("Reference magazine capacity"),G->Spec().Capacity,Kind==0?26:Kind==1?4:30);
        Test->TestTrue(TEXT("Right hand follows grip"),G->GripError(false)<.05f);Test->TestTrue(TEXT("Left hand follows support"),G->GripError(true)<.05f);
        const FTransform T=G->WeaponVisual->GetComponentTransform();const FVector Eye=C->GetFirstPersonCameraComponent()->GetComponentLocation(),Aim=C->GetControlRotation().Vector();
        Test->TestTrue(TEXT("Rear sight is on view axis"),FVector::CrossProduct((T.TransformPosition(G->SightRear())-Eye).GetSafeNormal(),Aim).Size()<.002f);
        Test->TestTrue(TEXT("Front sight is on view axis"),FVector::CrossProduct((T.TransformPosition(G->SightFront())-Eye).GetSafeNormal(),Aim).Size()<.002f);
        Test->TestTrue(TEXT("Loaded weapon fires"),G->ConsumeShot());G->AdvanceHandling(2);
        const int32 TotalBefore=G->Magazine+int32(G->bChambered)+Bag->Count(G->Spec().Ammo);
        Test->TestTrue(TEXT("Reload starts"),G->StartReload(Bag));
        Test->TestFalse(TEXT("Reload blocks firing"),G->ConsumeShot());
        if(Kind!=1){G->AdvanceHandling(G->Spec().Reload*.5f);Test->TestFalse(TEXT("Magazine removed mid reload"),G->bMagazineSeated);Test->TestEqual(TEXT("Removed magazine rounds returned once"),G->Magazine,0);}
        G->AdvanceHandling(20);
        Test->TestFalse(TEXT("Reload including charge completes"),G->IsReloading());Test->TestTrue(TEXT("Magazine seated"),G->bMagazineSeated);Test->TestTrue(TEXT("Chamber loaded"),G->bChambered);
        Test->TestEqual(TEXT("Reload conserves ammunition"),G->Magazine+int32(G->bChambered)+Bag->Count(G->Spec().Ammo),TotalBefore);
        // Empty reload must remain fire-blocked through all authored loading stages.
        G->Magazine=0;G->bChambered=false;const int32 EmptyTotal=Bag->Count(G->Spec().Ammo);
        G->StartReload(Bag);G->AdvanceHandling(.1f);Test->TestFalse(TEXT("Cannot fire during empty reload"),G->CanFire());
        G->AdvanceHandling(20);Test->TestTrue(TEXT("Empty reload feeds chamber"),G->bChambered);
        Test->TestEqual(TEXT("Empty reload conserves ammunition"),G->Magazine+int32(G->bChambered)+Bag->Count(G->Spec().Ammo),EmptyTotal);
        Test->TestEqual(TEXT("Loaded capacity"),G->Magazine,Kind==1?G->Spec().Capacity:G->Spec().Capacity-1);
        // Cancellation may recover unseated rounds, but cannot duplicate a committed magazine.
        const int32 PreCancel=G->Magazine+int32(G->bChambered)+Bag->Count(G->Spec().Ammo);
        G->StartReload(Bag);G->AdvanceHandling(G->Spec().Reload*.2f);G->Stow();G->Stow();
        Test->TestEqual(TEXT("Repeated cancellation preserves total ammunition"),G->Magazine+int32(G->bChambered)+Bag->Count(G->Spec().Ammo),PreCancel);
        if(Kind==1){G->Equip(C);G->AdvanceHandling(1.5f);G->bPumpMode=true;G->Magazine=3;G->bChambered=true;Test->TestTrue(TEXT("Pump shot fires"),G->ConsumeShot());Test->TestFalse(TEXT("Pump shot leaves empty chamber"),G->bChambered);G->AdvanceHandling(2);Test->TestTrue(TEXT("Pump cycle feeds round across a large timestep"),G->bChambered);Test->TestEqual(TEXT("Pump consumes exactly one tube round"),G->Magazine,2);}
        G->Stow();Combat->ActiveWeapon=Previous;G->Destroy();
    }
    int32 Debris=0;for(TActorIterator<AWeaponDebris> It(W);It;++It){++Debris;Test->TestFalse(TEXT("Magazine props are not ammunition pickups"),It->IsA<ACombatAmmoPickup>());}
    Test->TestTrue(TEXT("Magazine reloads create physical discarded magazines"),Debris>=2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceWeapons,"Game.Weapons.ReferenceMechanics",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceWeapons::RunTest(const FString&)
{
    AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FWeaponReferenceCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
