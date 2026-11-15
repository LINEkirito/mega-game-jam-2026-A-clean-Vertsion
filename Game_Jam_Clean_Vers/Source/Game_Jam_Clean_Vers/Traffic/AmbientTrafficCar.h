#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientTrafficCar.generated.h"
UENUM(BlueprintType)
enum class EAmbientVehicleKind : uint8 { Civilian, Police, Gang };
// Ambient traffic never owns extraction responders, wanted state or damage logic.
UCLASS()
class GAME_JAM_CLEAN_VERS_API AAmbientTrafficCar : public AActor
{
 GENERATED_BODY()
public:
 AAmbientTrafficCar();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaTime) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Traffic") EAmbientVehicleKind VehicleKind=EAmbientVehicleKind::Civilian;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Traffic") TArray<FVector> RoutePoints;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Traffic",meta=(ClampMin="0",ClampMax="1")) float StartFraction=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Traffic",meta=(ClampMin="100",ClampMax="600")) float CruiseSpeed=400;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traffic") float Speed=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traffic") float DistanceTravelled=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traffic") int32 CompletedLaps=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traffic") bool bWaitingForClearance=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traffic") TObjectPtr<class UBoxComponent> Body;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traffic") TObjectPtr<class USplineComponent> Route;
 bool IsPathClear(float FromDistance,float ToDistance) const;
private:
 UPROPERTY() TObjectPtr<USceneComponent> VisualRoot;
 UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Parts;
 UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Wheels;
 UPROPERTY() TObjectPtr<class USkeletalMeshComponent> Driver;
 float RouteDistance=0;
 float WheelAngle=0;
 UStaticMeshComponent* AddPart(const TCHAR* Name,const TCHAR* Part,const FVector& Position,const FRotator& Rotation=FRotator::ZeroRotator);
 void BuildRoute();
 FVector RoutePosition(float Distance) const;
};
