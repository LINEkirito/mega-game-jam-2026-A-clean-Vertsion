#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExtractionPoint.generated.h"

class AExtractionPoint;
class AGame_Jam_Clean_VersCharacter;

// World-local arbitration only. The point owns its countdown and extracting player.
UCLASS()
class GAME_JAM_CLEAN_VERS_API UExtractionSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool Claim(AExtractionPoint* Point);
 void Release(AExtractionPoint* Point);
 UFUNCTION(BlueprintPure) AExtractionPoint* GetActivePoint() const { return ActivePoint.Get(); }
 UFUNCTION(BlueprintPure) AGame_Jam_Clean_VersCharacter* GetExtractingPlayer() const;
private:
 UPROPERTY(Transient) TWeakObjectPtr<AExtractionPoint> ActivePoint;
};

UENUM(BlueprintType)
enum class EExtractionVehicle : uint8 { Sedan, SUV, BoxTruck, Campervan };
UENUM(BlueprintType)
enum class EExtractionState : uint8 { Idle, Extracting, Completed };

UCLASS(Blueprintable)
class GAME_JAM_CLEAN_VERS_API AExtractionPoint : public AActor
{
 GENERATED_BODY()
public:
 AExtractionPoint();
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> SceneRoot;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> VehicleMesh;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USkeletalMeshComponent> VehicleSkeletalMesh;
 // A parked vehicle uses one simple blocking body; no driving or simulation dependencies.
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> VehicleBody;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Extraction") EExtractionVehicle VehicleType=EExtractionVehicle::Sedan;
 UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Extraction") EExtractionState State=EExtractionState::Idle;
 UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Extraction") float RemainingSeconds=0.f;
 UPROPERTY(Transient,VisibleInstanceOnly,BlueprintReadOnly,Category="Extraction") TObjectPtr<AGame_Jam_Clean_VersCharacter> ExtractingPlayer;
 UFUNCTION(BlueprintPure) FString GetDisplayName() const;
 UFUNCTION(BlueprintPure) float DistanceToVehicle(const FVector& Position) const;
 UFUNCTION(BlueprintPure) bool IsPlayerInRange(const AGame_Jam_Clean_VersCharacter* Player) const;
 bool IsLookedAtBy(class APlayerController* PC) const;
 bool TryStart(class APlayerController* PC);
 void Cancel();
 FString StatusText() const;
private:
 friend class FExtractionRulesTest;
 friend class FExtractionFlowCheck;
 bool bAudioPlayerInRange=false;
 float NoticeSeconds=0.f;
 FString Notice;
 TSharedPtr<class SWidget> Overlay;
 void NotifyState();
 void UpdateOverlay(class APlayerController* PC,bool Visible);
 void RemoveOverlay();
 void Advance(float DeltaSeconds);
};
