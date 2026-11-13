#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateBrush.h"
#include "CamcorderPlayback.h"
#include "RecordingTabletComponent.generated.h"
UCLASS(ClassGroup=(Camera),meta=(BlueprintSpawnableComponent))
class GAME_JAM_CLEAN_VERS_API URecordingTabletComponent : public UActorComponent {
 GENERATED_BODY()
public:
 URecordingTabletComponent();
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bOpen=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bPlaying=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 SelectedClip=INDEX_NONE;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float PlaybackSeconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString PlaybackStatus;
 UPROPERTY(Transient) TObjectPtr<class UTexture2D> VideoTexture;
 UPROPERTY(Transient) TObjectPtr<class ARecordingTabletDisplay> Display;
 UFUNCTION(BlueprintCallable) void SetOpen(bool Open);
 UFUNCTION(BlueprintCallable) void SelectClip(int32 Index);
 UFUNCTION(BlueprintCallable) void TogglePlayback();
 UFUNCTION(BlueprintCallable) void Seek(float Fraction);
 UFUNCTION(BlueprintPure) int32 GetDecodedFrame() const {return LastFrame;}
 bool HandleKey(FKey Key);
private:
 TSharedPtr<class SWidget> UI;
 FSlateBrush VideoBrush,TabletBrush;
 FCamcorderPlayback Playback;
 int32 LastFrame=INDEX_NONE;
 bool PreviousCursor=false;
 void BuildUI();
 void UpdateFrame();
 class URecordingLibrary* Library() const;
 class UCamcorderEquipmentComponent* Equipment() const;
 UFUNCTION() void OnDataChanged(UObject* Source,FName Field,float Value);
};
