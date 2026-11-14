#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecordingScoreRules.h"
#include "RecordingLibrary.generated.h"
class AShoulderCamcorder;
USTRUCT(BlueprintType)
struct FRecordedScoreMoment {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float Seconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Points=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Subject;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Category;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Explanation;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector4 ScreenRect=FVector4(0,0,0,0);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bHasScreenRect=false;
};
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
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString ScoreRecordPath;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FString> ScoreDetails;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FString> RecordingEvidence;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FRecordedScoreMoment> ScoreMoments;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FRecordedPerson> People;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FRecordedEvent> Events;
};
// Owned by one local player's equipment, never scans recordings from earlier visits.
UCLASS(ClassGroup=(Camera),meta=(BlueprintSpawnableComponent))
class GAME_JAM_CLEAN_VERS_API URecordingLibrary : public UActorComponent {
 GENERATED_BODY()
public:
 URecordingLibrary();
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FRecordedClip> Clips;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bRoundEnded=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bEndedByDeath=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float RoundScore=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString RoundRecordPath;
 UFUNCTION(BlueprintCallable) void EndRound(bool bDeath=false);
 void ReturnToTitle();
 void BeginClip(AShoulderCamcorder* Camera);
 void Sample(AShoulderCamcorder* Camera);
 void FinishClip(AShoulderCamcorder* Camera,bool bSaved);
private:
 friend class FRecordingSettlementCheck;
 friend class FDeathPresentationCheck;
 TWeakObjectPtr<AShoulderCamcorder> RecordingCamera;
 FRecordedClip Pending;
 FRecordingScoreRules Scoring;
 TMap<TWeakObjectPtr<AActor>,int32> SubjectNumbers;
 TSharedPtr<class SWidget> SettlementUI;
 TSharedPtr<class SWidget> DeathUI;
 float DeathMessageTime=0;
 bool bSettlementShown=false,bReturningToTitle=false;
 void ShowSettlement();
};
