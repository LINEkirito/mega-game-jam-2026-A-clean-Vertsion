#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecordingLibrary.generated.h"
class AShoulderCamcorder;
USTRUCT(BlueprintType)
struct FRecordedEvent {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Id=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Name;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Tier=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Points=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bAlreadyRecorded=false;
};
USTRUCT(BlueprintType)
struct FRecordedPerson {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Name;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Points=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bAlreadyRecorded=false;
};
USTRUCT(BlueprintType)
struct FRecordedClip {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Path;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float Seconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Score=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FRecordedPerson> People;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FRecordedEvent> Events;
};
// Owned by one local player's equipment, never scans recordings from earlier visits.
UCLASS(ClassGroup=(Camera),meta=(BlueprintSpawnableComponent))
class GAME_JAM_CLEAN_VERS_API URecordingLibrary : public UActorComponent {
 GENERATED_BODY()
public:
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FRecordedClip> Clips;
 void BeginClip(AShoulderCamcorder* Camera);
 void Sample(AShoulderCamcorder* Camera);
 void FinishClip(AShoulderCamcorder* Camera,bool bSaved);
private:
 TWeakObjectPtr<AShoulderCamcorder> RecordingCamera;
 TMap<TWeakObjectPtr<AActor>,FRecordedPerson> PersonValues;
 TSet<TWeakObjectPtr<AActor>> Seen;
 TArray<TWeakObjectPtr<AActor>> PendingSubjects;
 FRecordedClip Pending;
 float NextSample=0;
 TMap<int32,float> EventExposure;
};
