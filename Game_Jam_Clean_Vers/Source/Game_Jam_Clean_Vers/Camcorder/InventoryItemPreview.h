#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryItemPreview.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UCamcorderEquipmentComponent;

// Transient display copies only: never moves, equips or simulates the owned item.
UCLASS(Transient, NotBlueprintable)
class GAME_JAM_CLEAN_VERS_API AInventoryItemPreview : public AActor
{
    GENERATED_BODY()
public:
    AInventoryItemPreview();
    virtual void BeginPlay() override;
    void ShowItem(UCamcorderEquipmentComponent* Equipment, FName Id);
    UFUNCTION(BlueprintCallable) void Rotate(float X, float Y);
    UFUNCTION(BlueprintCallable) void Zoom(float Delta);
    UFUNCTION(BlueprintCallable) void ResetView();
    UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UTextureRenderTarget2D> Texture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ZoomAmount=1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FRotator ViewRotation=FRotator::ZeroRotator;
    int32 PartCount() const { return Parts.Num(); }
private:
    UPROPERTY() TObjectPtr<USceneComponent> Pivot;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TArray<TObjectPtr<UMeshComponent>> Parts;
    FVector Center=FVector::ZeroVector;
    float Distance=200.f;
    void AddCopy(UStaticMeshComponent* Source, const FTransform& Local);
    void AddSkeletalCopy(USkeletalMeshComponent* Source, const FTransform& Local);
    void CaptureView();
};
