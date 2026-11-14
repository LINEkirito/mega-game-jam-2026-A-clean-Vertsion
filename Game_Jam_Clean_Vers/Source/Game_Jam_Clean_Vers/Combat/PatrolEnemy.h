#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TacticalWeapon.h"
#include "CharacterSecondaryMotion.h"
#include "PatrolEnemy.generated.h"

UENUM(BlueprintType)
enum class ENPCRole : uint8 { GangMember, GangBoss, Civilian, Merchant, Police, Injured };
UENUM(BlueprintType)
enum class ENPCActivity : uint8 { Idle, Patrol, Investigate, Combat, Threaten, Beat, Trade, Command, Flee, Injured, Dead, WarnPlayer, Meeting, CameraLook, CameraDismiss, CameraAvoid, PursueMelee, Counterattack };
UENUM(BlueprintType)
enum class ENPCWeaponCarry : uint8 { Holstered, Drawing, Drawn, Holstering };

UCLASS()
class GAME_JAM_CLEAN_VERS_API APatrolEnemy : public ACharacter
{
 GENERATED_BODY()
public:
 APatrolEnemy();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 float ShotHitChance(const AActor* Target) const;
 virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC") ENPCRole NPCRole=ENPCRole::GangMember;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") ENPCRole IdentityRole=ENPCRole::GangMember;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC") float Health=150;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC") float SightRange=2200;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC") float SightAngle=80;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC|Weapon") bool bRandomGangWeapon=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC|Weapon") ETacticalWeaponKind WeaponKind=ETacticalWeaponKind::Rifle;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC|Weapon") TObjectPtr<ATacticalWeapon> HeldWeapon;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC|Weapon") ENPCWeaponCarry WeaponCarry=ENPCWeaponCarry::Holstered;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC|Weapon") TObjectPtr<class UBackpackInventory> AmmoReserve;
 // Zero selects stationary guarding; otherwise destinations remain near the spawn point.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC") float PatrolRadius=1100;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC") TArray<FVector> PatrolPoints;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") bool bCombat=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") ENPCActivity Activity=ENPCActivity::Idle;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") TObjectPtr<AActor> CombatTarget;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") FVector InvestigationLocation;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") int32 CompletedTrades=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") float WarningRemaining=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC") float WarningRadius=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC|Event") int32 StreetEventId=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC|Presentation") TMap<ENPCActivity,TObjectPtr<class UAnimSequence>> ActivityAnimations;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC|Presentation") TObjectPtr<class USoundBase> ScreamSound;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC|Presentation") TObjectPtr<class USoundBase> HelpSound;
 // Selected at construction/startup only: injury changes behavior, not identity.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC|Presentation") TMap<ENPCRole,TObjectPtr<class USkeletalMesh>> RoleAppearances;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NPC|Presentation") TObjectPtr<class USkeletalMesh> AppearanceOverride;
 UFUNCTION(BlueprintPure) bool CanSeePlayer() const;
 UFUNCTION(BlueprintPure) float PlayerWarningRange() const;
 UFUNCTION(BlueprintPure) bool CanSeeActor(AActor* Target,bool CheckAngle=true) const;
 UFUNCTION(BlueprintPure) bool IsGang() const {return NPCRole==ENPCRole::GangMember||NPCRole==ENPCRole::GangBoss;}
 UFUNCTION(BlueprintPure) bool IsProtectedPerson() const {return NPCRole==ENPCRole::Civilian||NPCRole==ENPCRole::Merchant||NPCRole==ENPCRole::Injured;}
 UFUNCTION(BlueprintPure) bool IsLawProtected() const {return IdentityRole==ENPCRole::Civilian||IdentityRole==ENPCRole::Merchant||IdentityRole==ENPCRole::Injured;}
 UFUNCTION(BlueprintCallable) void BecomeInjured();
 UFUNCTION() void HearGunshot(AActor* Shooter,FVector Location,float Range);
 UFUNCTION() void WitnessViolence(AActor* Victim,APawn* Attacker,bool ProtectedVictim);
 UFUNCTION(BlueprintImplementableEvent,Category="NPC") void OnActivityChanged(ENPCActivity NewActivity);
private:
 friend class FNPCWeaponCarryCheck;
 UPROPERTY() TObjectPtr<class UAnimSequence> DrawWeaponAnimation;
 UPROPERTY() TObjectPtr<class UAnimSequence> HolsterWeaponAnimation;
 FTransform BackWeaponRelative=FTransform::Identity;
 float WeaponCarryTime=0.f;
 bool bDeathAmmoDropped=false;
 bool WantsWeaponDrawn() const;
 bool IsWeaponTransitioning() const;
 bool IsWeaponInHand() const;
 void InitializeWeaponCarry();
 void ApplyBackCarryPose(const FTransform& AuthoredGun);
 FName WeaponCarrySocket() const;
 void TickWeaponCarry(float DT);
 void DropDeathAmmo();
 friend class UWorldEvents;
 friend class FNPCStreetConflictCheck;
 friend class FFilmingEventMatrixCheck;
 TWeakObjectPtr<APatrolEnemy> AssaultTarget,RetaliationTarget,EscalationTarget,CivilianAggressor;
 float AssaultLostTime=0,CivilianResponseCooldown=0;
 bool bCivilianWillFight=false;
 void BeginAssault(APatrolEnemy* Target);
 void TickAssault(float DT);
 void TickCounterattack(float DT);
 void RespondToAssault(APatrolEnemy* Gang);
 bool IsCombatTargetAlive(AActor* Target) const;
 FVector EventDestination=FVector::ZeroVector,EventFocus=FVector::ZeroVector;
 float CameraReactionTime=0,CameraReactionCooldown=0;
 bool IsBeingFilmed() const;
 bool TickCameraReaction(float DT);
 void TickMeeting(float DT);
 UPROPERTY() TObjectPtr<USkeletalMeshComponent> Rifle;
 UPROPERTY() TObjectPtr<class UPoseableMeshComponent> NPCBody;
 UPROPERTY() TObjectPtr<class UStaticMeshComponent> TradeParcel;
 UPROPERTY() TObjectPtr<class UTextRenderComponent> Speech;
 UPROPERTY() TObjectPtr<class UAudioComponent> Voice;
 UPROPERTY() TObjectPtr<class UAnimSequence> Idle;
 UPROPERTY() TObjectPtr<class UAnimSequence> Walk;
 UPROPERTY() TObjectPtr<class UAnimSequence> AimWalk;
 UPROPERTY() TArray<TObjectPtr<class UAnimSequence>> AimWalkDirections;
 UPROPERTY() TObjectPtr<class UAnimSequence> Run;
 UPROPERTY() TObjectPtr<class UAnimSequence> ReloadAnimation;
 UPROPERTY() TObjectPtr<class UAnimSequence> UnarmedIdle;
 UPROPERTY() TObjectPtr<class UAnimSequence> UnarmedWalk;
 UPROPERTY() TObjectPtr<class UAnimSequence> CurrentAnimation;
 UPROPERTY() TObjectPtr<class UAnimSequence> TurnLeft;
 UPROPERTY() TObjectPtr<class UAnimSequence> TurnRight;
 UPROPERTY() TObjectPtr<class UAnimSequence> TurnLeft180;
 UPROPERTY() TObjectPtr<class UAnimSequence> TurnRight180;
 UPROPERTY() TObjectPtr<class UAnimSequence> ActiveTurn;
 TWeakObjectPtr<APatrolEnemy> Partner;
 TWeakObjectPtr<AActor> PriorityOffender;
 TWeakObjectPtr<AActor> ObservedShooter;
 FVector Home,Destination,LastKnown,DangerLocation;
 float StateTime=0,DecisionTime=0,MoveRetry=0,MoveTimeout=0,ShotTime=0,LostTime=0;
 float PanicTime=0,InvestigationTime=0,SocialCooldown=0,SpeechTime=0;
 bool bMoving=false,bMeleeApplied=false,bSocialArrived=false;
 bool bAimWarning=false;
 FCharacterSecondaryMotion SecondaryMotion;
 bool IsPlayerAimingAtMe() const;
 float TurnDelta=0,ScanYaw=0,NextLookTime=0,WeaponReadyAlpha=0;
 float CombatAimPitch=0;
 int32 RouteIndex=0;
 bool bTurning=false;
 float TurnTime=0,TurnStartYaw=0,TurnAngle=0,TurnNominalAngle=90,TurnRate=1.75f;
 enum class EPatrolPhase : uint8 { Walking, WaitingBeforeTurn, Turning, WaitingAfterTurn };
 EPatrolPhase PatrolPhase=EPatrolPhase::WaitingBeforeTurn;
 FVector PatrolLastPosition,PatrolDirection;
 float PatrolPhaseTime=0,PatrolBlockedTime=0;
 bool bPatrolPhaseActive=false;
 void ChangePatrolPhase(EPatrolPhase Next);
 void EnterPatrolPhase(EPatrolPhase Phase);
 void ExitPatrolPhase(EPatrolPhase Phase);
 void UpdatePatrolWalking(float DT);
 void UpdatePatrolWaiting(float DT);
 void UpdatePatrolTurning(float DT);
 void SetActivity(ENPCActivity Next);
 void ApplyAppearance();
 void SetMotion();
 void UpdateWeaponPose(float DT=0);
 void Say(const FString& Text,USoundBase* Sound=nullptr);
 void SetTarget(AActor* Target);
 void ReleasePartner();
 void FleeFrom(FVector Location);
 void Think();
 void TickCombat(float DT);
 void TickWarning(float DT);
 void TickSocial(float DT);
 void TickPatrol(float DT);
 bool MoveTo(FVector Point,float Acceptance=90);
 bool ChooseDestination(bool Flee);
 void Face(FVector Point,float DT);
 bool IsLiving(AActor* Actor) const;
 friend class FGangRetaliationCheck;
 TWeakObjectPtr<APawn> GangAttackLookTarget;
 FVector GangAttackLookLocation=FVector::ZeroVector;
 float GangAttackLookUntil=0.f;
 bool TickGangAttackObservation(float DT);
};

