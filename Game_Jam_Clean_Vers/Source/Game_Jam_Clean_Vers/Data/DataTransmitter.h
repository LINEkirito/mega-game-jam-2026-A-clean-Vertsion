#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DataTransmitter.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FCombatDamageEvent,AActor*,Target,AActor*,Source,float,Damage,bool,bHeadshot,bool,bKilled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGameplayDataChanged,UObject*,Source,FName,Field,float,Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGunshotEvent,AActor*,Shooter,FVector,Location,float,Range);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FViolenceEvent,AActor*,Victim,APawn*,Attacker,bool,bProtectedVictim);
UCLASS()
class GAME_JAM_CLEAN_VERS_API UDataTransmitter:public UWorldSubsystem {
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintAssignable) FCombatDamageEvent OnCombatDamage;
 UPROPERTY(BlueprintAssignable) FGameplayDataChanged OnDataChanged;
 UPROPERTY(BlueprintAssignable) FGunshotEvent OnGunshot;
 UPROPERTY(BlueprintAssignable) FViolenceEvent OnViolence;
 UFUNCTION(BlueprintCallable) void ReportGunshot(AActor* Shooter,FVector Location,float Range=6000.f){OnGunshot.Broadcast(Shooter,Location,Range);}
 UFUNCTION(BlueprintCallable) void NotifyChange(UObject* Source,FName Field,float Value){OnDataChanged.Broadcast(Source,Field,Value);}
 float TransmitHit(const FHitResult& Hit,float BaseDamage,const FVector& Direction,AController* Instigator,AActor* Source);
 static bool IsHeadHit(const FHitResult& Hit);
};
