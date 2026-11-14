#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldEvents.generated.h"

class APatrolEnemy;
UENUM(BlueprintType)
enum class EStreetEvent : uint8 { CivilianEscape, GangMeeting, SecretTransaction, PoliceChase, GangShootout, MassiveGangWar, GangMassacre };

USTRUCT(BlueprintType)
struct FStreetEventRecord {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Id=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) EStreetEvent Kind=EStreetEvent::GangMeeting;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Tier=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Points=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bActive=true;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bPerforming=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector Location=FVector::ZeroVector;
 UPROPERTY() TArray<TWeakObjectPtr<APatrolEnemy>> Participants;
 float Started=0, PerformingSince=0;
 float Duration=60.f, ExpiresAt=0.f;
 bool bHasPerformed=false,bScheduled=false;
#if WITH_EDITOR
 float DebugEndedAt=0;
 FString DebugEndReason;
#endif
 bool bClaimed=false;
};

// Scheduling and event identities belong to this world; configuration remains in GlobalData.
UCLASS()
class GAME_JAM_CLEAN_VERS_API UWorldEvents : public UTickableWorldSubsystem {
 GENERATED_BODY()
public:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FStreetEventRecord> Events;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bAutomatic=true;
 UFUNCTION(BlueprintCallable) bool TryStartEvent(EStreetEvent Kind);
 UFUNCTION(BlueprintPure) static FString EventName(EStreetEvent Kind);
 static EStreetEvent SelectScheduledEvent(float RareChance,float Roll,int32 Choice);
 int32 BeginEvent(EStreetEvent Kind,const TArray<APatrolEnemy*>& Participants);
 void EndEvent(int32 Id);
 void ObserveEscape(APatrolEnemy* NPC);
 void ObserveCombat(APatrolEnemy* NPC,APatrolEnemy* Target);
 bool Claim(int32 Id);
 const FStreetEventRecord* Find(int32 Id) const;
#if WITH_EDITOR
 const TArray<EStreetEvent>& DebugPendingEvents() const {return PendingEvents;}
 float DebugNextAttempt() const {return NextAttempt;}
 float DebugRetryAttempt() const {return RetryAttempt;}
#endif
private:
 int32 NextId=1;
 float NextAttempt=12;
 friend class FStreetEventCycleCheck;
 friend class FStreetEventVisualCheck;
 TArray<EStreetEvent> PendingEvents;
 TArray<TWeakObjectPtr<APatrolEnemy>> PreparedCast;
 float RetryAttempt=0;
 bool bStartingScheduled=false;
 bool PrepareScheduledCast(EStreetEvent Kind);
 TMap<TWeakObjectPtr<APatrolEnemy>,float> EscapeCooldown;
};
