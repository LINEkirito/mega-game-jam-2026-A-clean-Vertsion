#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PoliceReinforcements.generated.h"

class APatrolEnemy;
class APawn;
UCLASS()
class GAME_JAM_CLEAN_VERS_API UPoliceReinforcements : public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override;
 bool CanDeliver() const;
 void RegisterPassenger(APatrolEnemy* Passenger);
private:
 friend class FPoliceReinforcementsCheck;
 friend class FPolicePursuitCheck;
 UFUNCTION() void OnViolence(AActor* Victim,APawn* Attacker,bool ProtectedVictim);
 TWeakObjectPtr<class AExtractionConvoyCar> Car;
 TWeakObjectPtr<APawn> Offender;
 TArray<TWeakObjectPtr<APatrolEnemy>> Squad;
 int32 Pending=0;
 float Retry=0;
};
