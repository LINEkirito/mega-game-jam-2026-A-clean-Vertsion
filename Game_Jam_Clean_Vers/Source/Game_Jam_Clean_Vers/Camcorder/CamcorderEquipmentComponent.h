#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateBrush.h"
#include "CamcorderEquipmentComponent.generated.h"

class URecordingLibrary;
class URecordingTabletComponent;
class AShoulderCamcorder;
class ACamcorderBattery;
class APlayerController;
class SWidget;
class UBackpackInventory;
class SVerticalBox;
class IInputProcessor;
class UCombatEquipmentComponent;
class AInventoryItemPreview;
struct FKey;

UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class GAME_JAM_CLEAN_VERS_API UCamcorderEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCamcorderEquipmentComponent();
    UPROPERTY(Transient,BlueprintReadOnly) TObjectPtr<URecordingLibrary> RecordingLibrary;
    UPROPERTY(Transient,BlueprintReadOnly) TObjectPtr<URecordingTabletComponent> Tablet;
    virtual void TickComponent(float DT,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<AShoulderCamcorder> Camcorder;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory") TObjectPtr<UBackpackInventory> Inventory;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory") TObjectPtr<UCombatEquipmentComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory") bool bBackpackOpen=false;
    UFUNCTION(BlueprintPure, Category="Inventory") int32 GetBatteryCount() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") float RecordHoldSeconds=0.f;
    UFUNCTION(BlueprintCallable, Category="Camcorder") void ToggleCarry();
    UFUNCTION(BlueprintCallable, Category="Camcorder") void ReplaceBattery();
    UFUNCTION(BlueprintCallable, Category="Camcorder") void CollectBattery();
    UFUNCTION(BlueprintCallable, Category="Inventory") void ToggleBackpack();
    UFUNCTION(BlueprintCallable, Category="Inventory") void SetBackpackOpen(bool bOpen);
    UFUNCTION(BlueprintCallable, Category="Inventory") void StoreCamera();
    UFUNCTION(BlueprintCallable, Category="Inventory") void EquipCamera();
    UFUNCTION(BlueprintCallable, Category="Inventory") void AssignQuickSlot(int32 Index);
    UFUNCTION(BlueprintCallable, Category="Inventory") void ActivateQuickSlot(int32 Index);
    UFUNCTION(BlueprintCallable, Category="Inventory") void SelectInventoryItem(FName Id);
    UFUNCTION(BlueprintCallable, Category="Inventory") void UseSelectedItem();
    UFUNCTION(BlueprintCallable, Category="Inventory") bool DropInventoryItem(FName Id);
    UFUNCTION(BlueprintCallable, Category="Inventory") void DropActiveQuickSlot();
    void CancelViewfinder();
    void ShowAssignmentMenu(FName Id);
    bool HandleBackpackKey(const FKey& Key);
    FName SelectedItem;
    bool bAssignmentMenu=false;
    FString ItemTitle(FName Id) const;
    FString SlotTitle(int32 Index) const;
    FString SelectedDescription() const;
    void RefreshBackpack();
    UFUNCTION() void OnGameplayDataChanged(UObject* Source,FName Field,float Value);
    UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory") TObjectPtr<AInventoryItemPreview> ItemPreview;
    FSlateBrush InspectionBrush;
    void RefreshItemPreview();
private:
    TSharedPtr<SWidget> HUD;
    FSlateBrush MonitorBrush;
    TSharedPtr<SWidget> BackpackHUD;
    TSharedPtr<SVerticalBox> InventoryList;
    bool bPreviousCursor=false;
    bool bRequireAimRelease=false;
    TSharedPtr<IInputProcessor> CameraInputGate;
    void CreateBackpackHUD();
    void AddCameraToInventory();
    bool bHoldTriggered=false;
    bool bInitialized=false;
    bool CanUse() const;
    APlayerController* Player() const;
    ACamcorderBattery* NearbyBattery() const;
    void CreateHUD();
};
