#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GlobalData.generated.h"
UCLASS(Config=Game,DefaultConfig,BlueprintType)
class GAME_JAM_CLEAN_VERS_API UGlobalData:public UObject {
 GENERATED_BODY()
public:
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Recording") int32 RecordingPersonMinPoints=10;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Recording") int32 RecordingPersonMaxPoints=100;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Recording") float RecordingMaxDistance=8000;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC") float NPCPatrolRadius=1100;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC") float NPCHearingRange=6000;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC") float NPCPanicSeconds=12;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC") float NPCInjuryHealth=30;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC") float NPCMeleeDamage=12;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Police") float PoliceShotDamage=2;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Police") float PoliceShotInterval=1.8f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Police") float PoliceSpreadDegrees=14;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Police") float GangVsPoliceDamage=3;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang") float GangWarningDistance=500;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang") float GangFilmingWarningDistance=1000;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang") float GangWarningSeconds=3;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC") float AimedThreatSeconds=1.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Movement") float NPCWalkSpeed=110.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Movement") float NPCCombatAdvanceSpeed=95.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Movement") float NPCCombatStopDistance=260.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Movement") float NPCSearchSpeed=300.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon") int32 RifleCapacity=30;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon") int32 PistolCapacity=26;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon") int32 ShotgunCapacity=4;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon") float RecoilScale=.5f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon") float SpreadScale=.5f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Camcorder") float BatteryDuration=180;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Player") float DamageProtectionSeconds=.75f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Damage") float RifleBodyDamage=54;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Damage") float PistolBodyDamage=54;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Damage") float BuckshotPelletDamage=39;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Damage") float HeadMultiplier=2;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Enemy") float EnemyHealth=150;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Enemy") float EnemySightAngle=80;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Enemy") float EnemySightRange=2200;
};
