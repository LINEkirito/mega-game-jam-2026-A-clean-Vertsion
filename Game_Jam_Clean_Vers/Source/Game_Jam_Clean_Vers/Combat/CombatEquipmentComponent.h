#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatEquipmentComponent.generated.h"
class UCamcorderEquipmentComponent;
class ATacticalWeapon;
class SWidget;
class APlayerController;
class USoundBase;
UCLASS()
class GAME_JAM_CLEAN_VERS_API UCombatEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCombatEquipmentComponent();
    virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY() TObjectPtr<UCamcorderEquipmentComponent> Equipment;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<ATacticalWeapon> ActiveWeapon;
    UPROPERTY() TMap<FName,TObjectPtr<ATacticalWeapon>> Weapons;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bADS=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bReloading=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float EquipRemaining=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 ShotsFired=0;
    // Half-strength project baseline. The local RON analysis exposes parameter names, not numeric curves.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Weapon Tuning",meta=(ClampMin="0")) float RecoilScale=.5f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Weapon Tuning",meta=(ClampMin="0")) float SpreadScale=.5f;
    UFUNCTION(BlueprintCallable) bool TryInteract();
    UFUNCTION(BlueprintCallable) bool EquipWeapon(FName Id);
    UFUNCTION(BlueprintCallable) void Unequip();
    UFUNCTION(BlueprintCallable) void DropWeapon();
    UFUNCTION(BlueprintCallable) bool Fire();
    UFUNCTION(BlueprintCallable) bool Reload();
    ATacticalWeapon* GetPresentationWeapon() const { return ActiveWeapon ? ActiveWeapon.Get() : TransitionWeapon.Get(); }
    void Cycle(int32 Direction);
    void HandleInput();
    void InitializeHUD();
    void InitializeStartingWeapons();
    FString InteractionText() const;
    int32 Reserve() const;
    float ReloadProgress() const;
    float HitMarker=0;
    bool bKillMarker=false;
private:
    UPROPERTY() TObjectPtr<ATacticalWeapon> TransitionWeapon;
    FName PendingEquip;
    bool bPendingDrop=false;
    void FinishTransition();
    APlayerController* Player() const;
    AActor* NearbyPickup() const;
    float ReloadRemaining=0, Cooldown=0, Recoil=0, VisualKick=0, Burst=0;
    TSharedPtr<SWidget> HUD;
    UPROPERTY() TObjectPtr<USoundBase> FireSound;
    UPROPERTY() TObjectPtr<USoundBase> ReloadSound;
    UPROPERTY() TObjectPtr<USoundBase> EmptySound;
    UPROPERTY() TObjectPtr<USoundBase> HitSound;
};
