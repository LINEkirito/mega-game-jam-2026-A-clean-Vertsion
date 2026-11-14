#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CamcorderVideoWriter.h"
#include "CamcorderFrameReadback.h"
#include "ShoulderCamcorder.generated.h"

class UBoxComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UStaticMeshComponent;
class UMaterialInterface;
class USkeletalMeshComponent;
class UCameraComponent;
class UAnimSequence;
class UMaterialInstanceDynamic;
class FCamcorderVideoWriter;

UCLASS()
class GAME_JAM_CLEAN_VERS_API AShoulderCamcorder : public AActor
{
    GENERATED_BODY()
public:
    AShoulderCamcorder();
    virtual ~AShoulderCamcorder() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<USceneComponent> AimPivot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<USkeletalMeshComponent> HoldingHands;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<USkeletalMeshComponent> CameraMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<UStaticMeshComponent> DisplayScreen;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") TObjectPtr<UCameraComponent> ViewfinderCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") bool bViewfinderRaised=false;
    UFUNCTION(BlueprintCallable, Category="Camcorder") void SetViewfinderRaised(bool bRaised);
    UFUNCTION(BlueprintCallable, Category="Camcorder") void ToggleViewfinder();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") bool bSelfie=false;
    UFUNCTION(BlueprintCallable, Category="Camcorder") void ToggleSelfie();
    UPROPERTY(Transient, BlueprintReadOnly, Category="Camcorder") TObjectPtr<UTextureRenderTarget2D> MonitorTexture;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camcorder", meta=(ClampMin="1")) float FullBatterySeconds=180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camcorder", meta=(ClampMin="20",ClampMax="500")) float InteractionDistance=220.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") float BatteryPercent=100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") float FocalLength=35.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") bool bPowered=true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") bool bRecording=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") bool bHeld=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") bool bInBackpack=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") float RecordingSeconds=0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") FRotator AimOffset=FRotator::ZeroRotator;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") FString LastRecordingPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") FString Notice;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camcorder") int32 SavedClipCount=0;

    UFUNCTION(BlueprintCallable, Category="Camcorder") bool CanInteract(APlayerController* Player) const;
    UFUNCTION(BlueprintCallable, Category="Camcorder") void PickUp(APlayerController* Player);
    UFUNCTION(BlueprintCallable, Category="Camcorder") void PutDown();
    void DropOnDeath();
    bool DropFromInventory(APlayerController* Player);
    UFUNCTION(BlueprintCallable, Category="Camcorder") bool StoreInBackpack(APlayerController* Player);
    UFUNCTION(BlueprintCallable, Category="Camcorder") bool StartRecording();
    UFUNCTION(BlueprintCallable, Category="Camcorder") void StopRecording();
    UFUNCTION(BlueprintCallable, Category="Camcorder") void TogglePower();
    UFUNCTION(BlueprintCallable, Category="Camcorder") bool InstallFreshBattery();
    UFUNCTION(BlueprintCallable, Category="Camcorder") void AdjustAim(float PitchDelta, float YawDelta);
    UFUNCTION(BlueprintCallable, Category="Camcorder") void AdjustFocalLength(float DeltaMM);
    UFUNCTION(BlueprintCallable, Category="Camcorder") void SetNotice(const FString& Message);
    UFUNCTION(BlueprintCallable, Category="Camcorder") void ConsumeBattery(float Seconds);
    UFUNCTION(BlueprintPure, Category="Camcorder") bool IsPresenting() const { return bHeld || bLowering; }
private:
    UPROPERTY(Transient) TObjectPtr<APlayerController> Holder;
    UPROPERTY() TObjectPtr<UAnimSequence> HoldStart;
    UPROPERTY() TObjectPtr<UAnimSequence> HoldIdle;
    UPROPERTY() TObjectPtr<UAnimSequence> LookTransition;
    UPROPERTY() TObjectPtr<UAnimSequence> LookIdle;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HousingMaterial;
    TWeakObjectPtr<USkeletalMeshComponent> OriginalFirstPersonMesh;
    bool bOriginalMeshVisible=true;
    void SetHoldingHandsVisible(bool bVisible);
    TUniquePtr<FCamcorderVideoWriter> Writer;
    TUniquePtr<FCamcorderFrameReadback> FrameReadback;
    float EquipProgress=0.f, LookProgress=0.f, AnimationClock=0.f, SprintBlend=0.f;
    float LoweringTime=0.f;
    bool bLowering=false,bStoreAfterLowering=false;
    void BeginLowering(bool Store);
    void FinishLowering();
    float CaptureClock=0.f, SwayClock=0.f, NoticeClock=0.f;
    int32 LastScheduledFrame=-1;
    bool bCaptureReadFailed=false;
    bool bSelfieFollowInitialized=false;
    float SelfieFollowPitch=0.f;
    FVector SelfieFollowEye=FVector::ZeroVector;
    void UpdateHeldPose(float DeltaSeconds);
    void UpdatePresentation(float DeltaSeconds);
    void UpdateScreen();
    void SampleRecording();
};

UCLASS()
class GAME_JAM_CLEAN_VERS_API ACamcorderBattery : public AActor
{
    GENERATED_BODY()
public:
    ACamcorderBattery();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
};
