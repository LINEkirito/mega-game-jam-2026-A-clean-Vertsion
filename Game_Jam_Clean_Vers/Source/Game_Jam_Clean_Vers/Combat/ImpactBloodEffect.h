#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactBloodEffect.generated.h"
class UStaticMeshComponent;
class UMaterialInterface;
UCLASS()
class GAME_JAM_CLEAN_VERS_API AImpactBloodEffect : public AActor
{
 GENERATED_BODY()
public:
 AImpactBloodEffect();
 static void SpawnForHit(UWorld* World,const FHitResult& Hit,const FVector& IncomingDirection);
 virtual void Tick(float DeltaSeconds) override;
private:
 void Initialize(const FHitResult& Hit,const FVector& IncomingDirection);
 void Stain(const FHitResult& Surface,float Size);
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Droplets;
 UPROPERTY() TObjectPtr<UMaterialInterface> StainMaterial;
 TArray<FVector> Velocities;
 TWeakObjectPtr<AActor> Victim;
 float Age=0;
};
