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
 int32 GetWantedLevel() const {return WantedLevel;}
 bool IsSearching() const {return bSearching;}
 FVector GetLastKnownLocation() const {return LastKnownLocation;}
 bool IsWantedPlayer(const AActor* Actor) const;
 bool CanDeliver() const;
 void RegisterPassenger(APatrolEnemy* Passenger);
private:
 friend class FPoliceReinforcementsCheck;
 friend class FPolicePursuitCheck;
 friend class FPoliceWantedCheck;
 friend class FPoliceTrafficCheck;
 friend class FPoliceHUDCheck;
 void ClearWanted();
 void SetWantedLevel(int32 Level);
 void UpdateAwareness(float DT);
 TArray<TWeakObjectPtr<class AExtractionConvoyCar>> Cars;
 TSet<TWeakObjectPtr<APatrolEnemy>> ReportedAssaults,ReportedDeaths;
 FVector LastKnownLocation=FVector::ZeroVector;
 int32 WantedLevel=0,Heat=0;
 float UnseenSeconds=0,DispatchTimer=0;
 bool bSearching=false;
 UFUNCTION() void OnViolence(AActor* Victim,APawn* Attacker,bool ProtectedVictim);
 TWeakObjectPtr<class AExtractionConvoyCar> Car;
 TWeakObjectPtr<APawn> Offender;
 TArray<TWeakObjectPtr<APatrolEnemy>> Squad;
 int32 Pending=0;
 float Retry=0;
};
