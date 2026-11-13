#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BackpackInventory.generated.h"

USTRUCT(BlueprintType)
struct FBackpackEntry
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Description;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Count=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHandheld=false;
};

// Session inventory. Quick-slot assignment follows SixOClock's unique-slot / auto-fill rules.
UCLASS(BlueprintType)
class GAME_JAM_CLEAN_VERS_API UBackpackInventory : public UObject
{
    GENERATED_BODY()
public:
    UBackpackInventory();
    static constexpr int32 QuickSlotCount=4;
    bool CanAssign(FName Id) const;
    static const FName CameraId;
    static const FName BatteryId;
    static const FName TabletId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FBackpackEntry> Items;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> Slots;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ActiveSlot=0;
    UFUNCTION(BlueprintPure) int32 Count(FName Id) const;
    UFUNCTION(BlueprintCallable) bool Add(FName Id, const FString& Name, const FString& Description, int32 Amount=1, bool bHandheld=false);
    UFUNCTION(BlueprintCallable) bool Remove(FName Id, int32 Amount=1);
    UFUNCTION(BlueprintCallable) bool Assign(int32 Slot, FName Id);
    const FBackpackEntry* Find(FName Id) const;
    int32 SlotIndex(FName Id) const;
private:
    void FillEmptySlots();
};
