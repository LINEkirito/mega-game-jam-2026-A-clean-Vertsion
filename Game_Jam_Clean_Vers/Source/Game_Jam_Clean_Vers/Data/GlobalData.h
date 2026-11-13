#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GlobalData.generated.h"
UCLASS(Config=Game,DefaultConfig,BlueprintType)
class GAME_JAM_CLEAN_VERS_API UGlobalData:public UObject {
 GENERATED_BODY()
public:
 // Maximum audible distance in cm, including the inner radius. Per-source state stays on audio components.
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="100",Units="cm")) float VoiceAudibleDistance=1800.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="100",Units="cm")) float FootstepAudibleDistance=1000.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="100",Units="cm")) float HandlingAudibleDistance=800.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="100",Units="cm")) float GunshotAudibleDistance=4500.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="100",Units="cm")) float ImpactAudibleDistance=1200.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events") bool bEnableStreetEvents=true;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="1")) float EventAttemptSeconds=18;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="0")) float CommonEventWeight=70;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="0")) float RareEventWeight=26;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="0")) float LegendaryEventWeight=4;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="0")) float LegendaryEarliestSeconds=120;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="0")) int32 CommonEventPoints=50;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="0")) int32 RareEventPoints=200;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Events",meta=(ClampMin="0")) int32 LegendaryEventPoints=800;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon|Impact",meta=(ClampMin="0.08")) float WallImpactSeconds=.32f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon|Impact",meta=(ClampMin="1")) float WallImpactSize=12.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon|Impact",meta=(ClampMin="1")) float WallImpactSparkSpeed=180.f;
 // Visual-only tracer tuning (cm, seconds); damage remains hitscan through DataTransmitter.
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon|Tracer",meta=(ClampMin="1")) float TracerSpeed=45000.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon|Tracer",meta=(ClampMin="1")) float TracerLength=110.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon|Tracer",meta=(ClampMin="0.01")) float TracerWidth=1.8f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="Weapon|Tracer",meta=(ClampMin="0.001")) float TracerFadeSeconds=.025f;
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
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Police") float PoliceSpreadDegrees=0;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang",meta=(ClampMin="0",ClampMax="1")) float GangHitChance=.5f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Police") float GangVsPoliceDamage=3;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang") float GangWarningDistance=500;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang") float GangFilmingWarningDistance=1000;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang") float GangWarningSeconds=3;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Gang",meta=(ClampMin="0",ClampMax="360")) float GangSightAngle=180.f;
 UPROPERTY(Config,EditAnywhere,BlueprintReadOnly,Category="NPC|Weapon",meta=(ClampMin="0")) int32 NPCSpareMagazines=3;
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
