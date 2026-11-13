#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatueTargetComponent.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class GAME_JAM_CLEAN_VERS_API UStatueTargetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UStatueTargetComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Fn) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Target") float MaxHealth=1000;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Target") float Health=1000;
    UFUNCTION(BlueprintCallable) float ApplyHit(float Damage);
    UFUNCTION(BlueprintCallable) void ResetTarget();
private:
    UFUNCTION() void OnDamage(AActor* Actor,float Damage,const UDamageType* DamageType,AController* Instigator,AActor* Causer);
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Bar;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Background;
    FVector Anchor;
    float Flash=0;
};
