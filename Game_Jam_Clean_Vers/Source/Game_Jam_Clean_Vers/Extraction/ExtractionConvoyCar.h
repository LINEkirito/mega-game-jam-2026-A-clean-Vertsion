#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExtractionConvoyCar.generated.h"

class APlayerController;
class APatrolEnemy;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;

UENUM()
enum class EConvoyCarPhase : uint8 { Approaching, Unloading, Departing };

// A road encounter vehicle, not a player-drivable pawn. Collision is swept at vehicle size.
UCLASS()
class GAME_JAM_CLEAN_VERS_API AExtractionConvoyCar : public AActor
{
 GENERATED_BODY()
public:
 AExtractionConvoyCar();
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 void Initialize(APlayerController* Controller,const FVector& Stop,const FVector& Start,bool Police=false,int32 PassengerCount=4);
 void LeaveWithoutUnloading();
 bool IsDelivering() const {return Phase!=EConvoyCarPhase::Departing;}
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UBoxComponent> CollisionBody;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class USceneComponent> VisualRoot;
 UPROPERTY(VisibleAnywhere) EConvoyCarPhase Phase=EConvoyCarPhase::Approaching;
 UPROPERTY(VisibleAnywhere) int32 Delivered=0;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Doors;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
 UPROPERTY(Transient) TArray<TObjectPtr<USkeletalMeshComponent>> Riders;
 static bool FindArrival(UWorld* World,APlayerController* PC,FVector& Start,FVector& Stop,FRotator& Rotation);
 static bool IsInFront(APlayerController* PC,const FVector& Location);
 static bool IsHiddenFromPlayer(UWorld* World,APlayerController* PC,const FVector& Location,const FRotator& Rotation,const AActor* Ignore=nullptr);
 static bool ClearRoad(UWorld* World,const FVector& Start,const FVector& End,const AActor* Ignore=nullptr);
private:
 friend class FConvoyCheck;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> DriverIdle;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> PassengerIdle;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> ExitLeft;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> ExitRight;
 TWeakObjectPtr<APlayerController> PlayerController;
 FVector StopLocation,EntryLocation,DepartureTarget;
 float Speed=0,WheelAngle=0,PhaseTime=0,BlockedTime=0,HiddenSeconds=0,ExitRetry=0;
 bool bReverse=false,bAborted=false;
 bool bPolice=false;
 int32 PassengerCount=4;
 bool Exited[4]={false,false,false,false};
 FVector Seat(int32 Index) const;
 int32 DoorForSeat(int32 Index) const;
 void UpdatePassengers(float DT);
 void StartDeparture();
 bool MoveAlongRoad(float DT,const FVector& Target,bool Brake);
 UStaticMeshComponent* AddPart(const TCHAR* Name,const TCHAR* Part,const FVector& Location,const FRotator& Rotation=FRotator::ZeroRotator);
};
