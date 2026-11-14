#include "CombatEquipmentComponent.h"
#include "Audio/CharacterAudio.h"
#include "Camcorder/ItemInteractionComponent.h"
#include "Data/GlobalData.h"
#include "PatrolEnemy.h"
#include "Data/DataTransmitter.h"
#include "TacticalWeapon.h"
#include "StatueTargetComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"
#include "CombatHUD.h"

UCombatEquipmentComponent::UCombatEquipmentComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;RecoilScale=GetDefault<UGlobalData>()->RecoilScale;SpreadScale=GetDefault<UGlobalData>()->SpreadScale;
FireSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02"));
ReloadSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Audio/Reload"));EmptySound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Audio/Empty"));HitSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Effects/Blood/S_BloodHit"));}
APlayerController* UCombatEquipmentComponent::Player()const{return Cast<APlayerController>(GetOwner());}
int32 UCombatEquipmentComponent::Reserve()const{return ActiveWeapon&&Equipment&&Equipment->Inventory?Equipment->Inventory->Count(ActiveWeapon->Spec().Ammo):0;}
float UCombatEquipmentComponent::ReloadProgress()const{return ActiveWeapon?ActiveWeapon->ReloadProgress():0;}
AActor* UCombatEquipmentComponent::NearbyPickup()const
{
    auto* PC=Player();if(!PC||!PC->GetPawn())return nullptr;FVector Eye;FRotator R;PC->GetPawn()->GetActorEyesViewPoint(Eye,R);
    AActor* Best=nullptr;float BestD=220*220;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        auto* W=Cast<ATacticalWeapon>(*It);auto* A=Cast<ACombatAmmoPickup>(*It);
        if((!W&&!A)||It->IsHidden()|| (W&&(W->bNPCEquipped||Weapons.Contains(W->InventoryId))))continue;
        const float D=FVector::DistSquared(Eye,It->GetActorLocation());if(D>=BestD)continue;
        FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(CombatPickup),false,PC->GetPawn());Q.AddIgnoredActor(*It);if(ActiveWeapon)Q.AddIgnoredActor(ActiveWeapon);if(Equipment->Camcorder)Q.AddIgnoredActor(Equipment->Camcorder);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,It->GetActorLocation(),ECC_Visibility,Q))continue;
        Best=*It;BestD=D;
    }
    return Best;
}
FString UCombatEquipmentComponent::InteractionText()const{auto* A=NearbyPickup();if(auto* W=Cast<ATacticalWeapon>(A))return TEXT("F 拾取 ")+W->Spec().Name;if(auto* Ammo=Cast<ACombatAmmoPickup>(A))return FString::Printf(TEXT("F 拾取 %s ×%d"),*Ammo->AmmoName(),Ammo->Amount);return TEXT("");}
bool UCombatEquipmentComponent::TryInteract()
{
    if(!Equipment||!Equipment->Inventory)return false;
    if(auto* A=NearbyPickup())
    {
        if(auto* W=Cast<ATacticalWeapon>(A);W && Equipment->Inventory->SlotIndex(W->InventoryId)==INDEX_NONE)
        {Equipment->Camcorder->SetNotice(TEXT("快捷栏固定为初始四件道具，不能放入其他武器"));return true;}
        if(auto* W=Cast<ATacticalWeapon>(A)){if(W->InventoryId.IsNone())W->InventoryId=W->GetFName();Weapons.Add(W->InventoryId,W);Equipment->Inventory->Add(W->InventoryId,W->Spec().Name,TEXT("拾回后恢复原固定快捷槽。左键射击，右键切换瞄准，R 换弹；Q 丢下，保留弹匣。"),1,true);W->Stow();EquipWeapon(W->InventoryId);Equipment->Camcorder->SetNotice(TEXT("武器已拿到手上，正在准备上膛"));}
        else if(auto* Ammo=Cast<ACombatAmmoPickup>(A)){Equipment->Inventory->Add(Ammo->AmmoId(),Ammo->AmmoName(),TEXT("对应枪械的备用弹药；装备枪械后按 R 换弹。"),Ammo->Amount);if(auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Player()->GetPawn()))if(C->ItemInteraction)C->ItemInteraction->Play(Ammo->Mesh);CharacterAudio::Attached(LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Audio/Handling/ItemPickup")),Player()->GetPawn()->GetRootComponent(),ECharacterSound::Handling,.5f);Ammo->Destroy();Equipment->Camcorder->SetNotice(TEXT("弹药已收入背包"));}
        Equipment->RefreshBackpack();return true;
    }return false;
}
bool UCombatEquipmentComponent::EquipWeapon(FName Id)
{
    if(!Equipment || !Equipment->Inventory)return false;
    // An explicit selection supersedes an automatically stowed weapon.
    LookDownWeaponId=NAME_None;
    if(Id.IsNone() || Id==UBackpackInventory::CameraId)
    {
        if(TransitionWeapon){PendingEquip=Id;return true;}
        if(ActiveWeapon){TransitionWeapon=ActiveWeapon;PendingEquip=Id;bPendingDrop=false;ActiveWeapon=nullptr;bADS=false;bReloading=false;TransitionWeapon->Holster();return true;}
        PendingEquip=NAME_None;if(!Id.IsNone())Equipment->EquipCamera();return true;
    }
    auto* Found=Weapons.Find(Id);auto* C=Player()?Cast<AGame_Jam_Clean_VersCharacter>(Player()->GetPawn()):nullptr;
    if(!Found||!IsValid(*Found)||!C||C->Health<=0)return false;
    if(TransitionWeapon){PendingEquip=Id;return true;}
    if(ActiveWeapon==*Found)return true;
    if(ActiveWeapon){TransitionWeapon=ActiveWeapon;PendingEquip=Id;bPendingDrop=false;ActiveWeapon=nullptr;bADS=false;bReloading=false;TransitionWeapon->Holster();return true;}
    Unequip();if(Equipment->Camcorder->bHeld)Equipment->StoreCamera();
    if(Equipment->Camcorder->IsPresenting()){PendingEquip=Id;bADS=false;return true;}
    if(IsBelowHoldPitch(*Found))
    {
        LookDownWeaponId=Id;EquipRemaining=0;
        Equipment->Inventory->ActiveSlot=Equipment->Inventory->SlotIndex(Id);Equipment->SelectedItem=Id;Equipment->RefreshBackpack();return true;
    }
    ActiveWeapon=*Found;ActiveWeapon->Equip(C);EquipRemaining=ActiveWeapon->bPackPresentation?1.4f:.5f;
    Equipment->Inventory->ActiveSlot=Equipment->Inventory->SlotIndex(Id);Equipment->SelectedItem=Id;Equipment->RefreshBackpack();return true;
}
void UCombatEquipmentComponent::Unequip(){LookDownWeaponId=NAME_None;PendingEquip=NAME_None;if(TransitionWeapon){TransitionWeapon->Stow();FinishTransition();}bReloading=false;bADS=false;ReloadRemaining=0;for(auto& Pair:Weapons)if(IsValid(Pair.Value)&&Pair.Value->bEquipped)Pair.Value->Stow();ActiveWeapon=nullptr;}
bool UCombatEquipmentComponent::DropInventoryWeapon(FName Id)
{
 auto* Found=Weapons.Find(Id);
 auto* PC=Player();
 if(!Found || !IsValid(*Found) || !PC || !PC->GetPawn())return false;
 // Do not redirect an in-flight holster/drop or leave a queued equip pointing at a dropped actor.
 if(TransitionWeapon || !PendingEquip.IsNone())return false;
 if(ActiveWeapon==*Found || LookDownWeaponId==Id){DropWeapon();return true;}
 auto* Gun=Found->Get();FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 Weapons.Remove(Id);Equipment->Inventory->Remove(Id);
 Gun->Drop(Eye+View.Vector()*60-FVector(0,0,20),View);
 Equipment->RefreshBackpack();return true;
}
void UCombatEquipmentComponent::DropWeapon(){
 if(!ActiveWeapon&&!TransitionWeapon&&HasLookDownSelection())
 {
  if(auto* Found=Weapons.Find(LookDownWeaponId);Found&&IsValid(*Found))
  {TransitionWeapon=*Found;LookDownWeaponId=NAME_None;PendingEquip=NAME_None;bPendingDrop=true;FinishTransition();}
  return;
 }
 if(!ActiveWeapon||TransitionWeapon)return;
 LookDownWeaponId=NAME_None;
 TransitionWeapon=ActiveWeapon;ActiveWeapon=nullptr;PendingEquip=NAME_None;bPendingDrop=true;bADS=false;bReloading=false;TransitionWeapon->Holster();
}
void UCombatEquipmentComponent::FinishTransition(){
 auto* Old=TransitionWeapon.Get();if(!Old||Old->bEquipped)return;
 TransitionWeapon=nullptr;const FName Next=PendingEquip;PendingEquip=NAME_None;
 if(bPendingDrop){bPendingDrop=false;FVector Eye;FRotator R;Player()->GetPawn()->GetActorEyesViewPoint(Eye,R);Weapons.Remove(Old->InventoryId);Equipment->Inventory->Remove(Old->InventoryId);Old->Drop(Eye+R.Vector()*60-FVector(0,0,20),R);CharacterAudio::At(this,LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Audio/Handling/ItemDrop")),Old->GetActorLocation(),ECharacterSound::Handling,.45f);Equipment->RefreshBackpack();}
 if(!Next.IsNone())EquipWeapon(Next);
}

void UCombatEquipmentComponent::Cycle(int32 Direction)
{
    if(!Equipment || !Equipment->Inventory || Direction==0)return;
    auto* Inventory=Equipment->Inventory.Get();
    constexpr int32 Count=UBackpackInventory::QuickSlotCount;
    const int32 Current=FMath::Clamp(Inventory->ActiveSlot,0,Count-1);
    Equipment->ActivateQuickSlot((Current+Direction%Count+Count)%Count);
}
bool UCombatEquipmentComponent::Reload()
{
    if(!ActiveWeapon||bReloading||EquipRemaining>0||TransitionWeapon||IsBelowHoldPitch(ActiveWeapon))return false;
    bReloading=ActiveWeapon->StartReload(Equipment->Inventory);if(bReloading)bADS=false;return bReloading;
}
bool UCombatEquipmentComponent::Fire()
{
    auto* PC=Player();auto* C=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
    if(!ActiveWeapon||!C||C->Health<=0||EquipRemaining>0||TransitionWeapon||IsBelowHoldPitch(ActiveWeapon)||GetWorld()->GetTimeSeconds()<ActiveWeapon->NextFireTime)return false;
    const auto S=ActiveWeapon->Spec();if(bReloading){if(S.bShellReload)ActiveWeapon->RequestReloadEnd();return false;}
    if(!ActiveWeapon->bChambered){ActiveWeapon->NextFireTime=GetWorld()->GetTimeSeconds()+.2f;if(EmptySound)CharacterAudio::Attached(EmptySound,ActiveWeapon->GetRootComponent(),ECharacterSound::Handling,.4f);Equipment->Camcorder->SetNotice(TEXT("膛内无弹：按 R 装填 / 上膛"));return false;}
    if(!ActiveWeapon->ConsumeShot())return false;++ShotsFired;ActiveWeapon->NextFireTime=GetWorld()->GetTimeSeconds()+S.Interval;Burst=FMath::Min(Burst+1,12.f);
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);const FVector Muzzle=ActiveWeapon->MuzzleLocation();
    const float Movement=C->GetCharacterMovement()->IsFalling()?2.f:C->bSprinting?1.8f:C->bIsCrouched?.75f:1.f;
    const float Spread=(bADS?S.ADSSpread:S.HipSpread)*Movement*SpreadScale;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(TacticalShot),true,C);Q.AddIgnoredActor(ActiveWeapon);Q.AddIgnoredActor(Equipment->Camcorder);
    bool bHitThisShot=false;
    for(int32 I=0;I<S.Pellets;++I)
    {
        const FVector Dir=FMath::VRandCone(View.Vector(),FMath::DegreesToRadians(Spread));FVector End=Eye+Dir*S.Range;
        FHitResult AimHit,Hit;if(GetWorld()->LineTraceSingleByChannel(AimHit,Eye,End,ECC_Visibility,Q))End=AimHit.ImpactPoint;
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,End+(End-Muzzle).GetSafeNormal()*3,ECC_Visibility,Q);
        bool Flesh=false;if(Blocked){End=Hit.ImpactPoint;float Damage=S.DamageAtDistance(FVector::Distance(Eye,End));
            auto* Statue=Hit.GetActor()?Hit.GetActor()->FindComponentByClass<UStatueTargetComponent>():nullptr;
            const float Applied=GetWorld()->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,Damage,Dir,PC,ActiveWeapon);
            Flesh=Applied>0&&(Cast<APawn>(Hit.GetActor())||Cast<ACombatTarget>(Hit.GetActor())||Statue);
            if(Flesh){bHitThisShot=true;HitMarker=.18f;bKillMarker=false;if(auto* T=Cast<ACombatTarget>(Hit.GetActor()))bKillMarker=T->Health<=0;if(Statue)bKillMarker=Statue->Health<=0;if(auto* Enemy=Cast<APatrolEnemy>(Hit.GetActor()))bKillMarker=Enemy->Health<=0;}
        }
        if(auto* FX=GetWorld()->SpawnActor<ACombatShotEffect>())FX->Setup(Muzzle,End,Blocked,Flesh || (Blocked && Cast<APawn>(Hit.GetActor())),Blocked?Hit.ImpactNormal:FVector::ZeroVector);
    }
    const float Kick=S.Recoil*RecoilScale*(bADS?.8f:1.f)*(S.bAutomatic?1.f+Burst*.035f:1.f);Recoil+=Kick;VisualKick+=Kick;auto R=PC->GetControlRotation();R.Pitch=FMath::Clamp(FRotator::NormalizeAxis(R.Pitch)+Kick,-70.f,80.f);R.Yaw+=FMath::FRandRange(-.2f,.2f)*Kick;PC->SetControlRotation(R);
    ActiveWeapon->Flash();UAISense_Hearing::ReportNoiseEvent(GetWorld(),Muzzle,1,C,6000,TEXT("Gunshot"));
    GetWorld()->GetSubsystem<UDataTransmitter>()->ReportGunshot(C,Muzzle);

    if(bHitThisShot&&HitSound)UGameplayStatics::PlaySound2D(this,HitSound,.25f);
    return true;
}
void UCombatEquipmentComponent::HandleInput()
{
    auto* PC=Player();if(!PC)return;
    if(HasLookDownSelection()&&!ActiveWeapon)
    {
        if(PC->WasInputKeyJustPressed(EKeys::B))EquipWeapon(NAME_None);
        return;
    }
    if(!ActiveWeapon)return;
    if(IsBelowHoldPitch(ActiveWeapon)){TickLookDownHolster();return;}
    const bool CanAim=EquipRemaining<=0&&!ActiveWeapon->IsReloading()&&(ActiveWeapon->Handling==EWeaponHandling::Idle||ActiveWeapon->Handling==EWeaponHandling::FireCycle);
    if(!CanAim) bADS=false;
    else if(PC->WasInputKeyJustPressed(EKeys::RightMouseButton)) bADS=!bADS;
    if(PC->WasInputKeyJustPressed(EKeys::V))ActiveWeapon->ToggleSightOrMode();
    if(PC->WasInputKeyJustPressed(EKeys::T))ActiveWeapon->ToggleLight();
    if(PC->WasInputKeyJustPressed(EKeys::R))Reload();
    if(PC->WasInputKeyJustPressed(EKeys::B)){TransitionWeapon=ActiveWeapon;PendingEquip=NAME_None;bPendingDrop=false;bADS=false;bReloading=false;ActiveWeapon=nullptr;TransitionWeapon->Holster();return;}
    if((ActiveWeapon->Spec().bAutomatic&&ActiveWeapon->bChambered?PC->IsInputKeyDown(EKeys::LeftMouseButton):PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)))Fire();
}
void UCombatEquipmentComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn)
{
    Super::TickComponent(DT,Type,Fn);auto* PC=Player();auto* C=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;if(!C||!Equipment)return;
    if(TransitionWeapon)FinishTransition();
    if(!TransitionWeapon&&!PendingEquip.IsNone()&&!Equipment->Camcorder->IsPresenting()){const FName Next=PendingEquip;PendingEquip=NAME_None;EquipWeapon(Next);}
    Cooldown=FMath::Max(0.f,Cooldown-DT);EquipRemaining=FMath::Max(0.f,EquipRemaining-DT);HitMarker=FMath::Max(0.f,HitMarker-DT);Burst=FMath::Max(0.f,Burst-DT*3);VisualKick=FMath::FInterpTo(VisualKick,0,DT,15);
    if(C->Health<=0){Unequip();Equipment->Camcorder->SetViewfinderRaised(false);Equipment->Camcorder->StopRecording();return;}
    TickLookDownHolster();
    const float NewRecoil=FMath::FInterpTo(Recoil,0,DT,6);if(Recoil>.001f){auto R=PC->GetControlRotation();R.Pitch-=Recoil-NewRecoil;PC->SetControlRotation(R);}Recoil=NewRecoil;
    if(Equipment->bBackpackOpen)bADS=false;
    if(ActiveWeapon){ActiveWeapon->AdvanceHandling(DT);bReloading=ActiveWeapon->IsReloading();}
    if(ActiveWeapon)ActiveWeapon->UpdatePose(DT,bADS,VisualKick);
    const float TargetFOV=ActiveWeapon&&bADS&&!bReloading?ActiveWeapon->AimFOV():90.f;auto* Camera=C->GetFirstPersonCameraComponent();Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,TargetFOV,DT,12));
}
void UCombatEquipmentComponent::InitializeStartingWeapons()
{
    if(!Equipment||!Equipment->Inventory||!Weapons.IsEmpty()||!Equipment->Inventory->Slots[1].IsNone())return;
    const ETacticalWeaponKind Kinds[]={ETacticalWeaponKind::Pistol,ETacticalWeaponKind::Shotgun,ETacticalWeaponKind::Rifle};
    for(int32 I=0;I<3;++I){
        const FTransform Transform(Player()->GetPawn()->GetActorLocation());
        auto* Gun=GetWorld()->SpawnActorDeferred<ATacticalWeapon>(ATacticalWeapon::StaticClass(),Transform,Player(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!Gun)continue;
        Gun->Kind=Kinds[I];Gun->FinishSpawning(Transform);Gun->Stow();
        Weapons.Add(Gun->InventoryId,Gun);
        Equipment->Inventory->Add(Gun->InventoryId,Gun->Spec().Name,TEXT("左键射击，右键瞄准，R 换弹；Q 丢下。"),1,true);
        Equipment->Inventory->BindStartingWeapon(I+1,Gun->InventoryId);
    }
    Equipment->RefreshBackpack();
}
void UCombatEquipmentComponent::InitializeHUD(){HUD=SNew(SCombatHUD).Combat(this);GetWorld()->GetGameViewport()->AddViewportWidgetContent(HUD.ToSharedRef(),11);}
void UCombatEquipmentComponent::EndPlay(const EEndPlayReason::Type R){if(HUD&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(HUD.ToSharedRef());HUD.Reset();for(auto& P:Weapons)if(IsValid(P.Value))P.Value->Destroy();Super::EndPlay(R);}
