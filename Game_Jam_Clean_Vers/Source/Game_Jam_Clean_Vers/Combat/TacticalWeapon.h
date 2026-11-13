#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TacticalWeapon.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UPoseableMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UTextRenderComponent;
class UAnimSequence;
class UStaticMesh;
class USkeletalMesh;
class UAudioComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class USoundBase;
class UBackpackInventory;
class AGame_Jam_Clean_VersCharacter;

UENUM(BlueprintType)
enum class ETacticalWeaponKind : uint8 { Pistol, Shotgun, Rifle };
UENUM(BlueprintType)
enum class EWeaponHandling : uint8 { Idle, Equip, MagazineReload, ShellInsert, Charge, FireCycle, Holster, ShellStart };
struct FTacticalWeaponSpec
{
    FString Name;
    FName Ammo;
    int32 Capacity=19, Pellets=1;
    // Tuning baseline: project-authored values, NOT extracted Ready or Not numeric curves.
    float Damage=30, Range=5000, Interval=.22f, Reload=3.f, Recoil=1.4f, HipSpread=2, ADSSpread=.25f, HeadMultiplier=2;
    bool bAutomatic=false, bShellReload=false;
    float DamageAtDistance(float Distance) const
    {
        return Damage*FMath::GetMappedRangeValueClamped(FVector2D(Range*.3f,Range),FVector2D(1,.35f),Distance);
    }
};

UCLASS()
class GAME_JAM_CLEAN_VERS_API ATacticalWeapon : public AActor
{
    GENERATED_BODY()
public:
    ATacticalWeapon();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DT) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") ETacticalWeaponKind Kind=ETacticalWeaponKind::Pistol;
    // Magazine is strictly the detachable magazine/tube count; the chamber is separate.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") int32 Magazine=-1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") bool bChambered=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") bool bPumpMode=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEquipped=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bNPCEquipped=false;
    UPROPERTY(Transient,BlueprintReadWrite,Category="NPC") float NPCReloadDuration=0;
    void EquipForNPC(USkeletalMeshComponent* CharacterMesh);
    void UpdateNPCGripPose(USkeletalMeshComponent* Source,UPoseableMeshComponent* BodyPose,float ReadyAlpha=1.f);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bMagnified=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bMagazineSeated=true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EWeaponHandling Handling=EWeaponHandling::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HandlingTime=0;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> WeaponVisual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Hands;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> RenderArms;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> MagazineVisual;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> ModelParts;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Grip") float GripPitch=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Grip") bool bFingerContactIK=true;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> PackWeaponMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> PackMagazineMesh;
    UPROPERTY(VisibleAnywhere) bool bPackPresentation=false;
    FName InventoryId;
    float NextFireTime=0;
    FTacticalWeaponSpec Spec() const;
    void Equip(AGame_Jam_Clean_VersCharacter* Character);
    void Stow();
    void Drop(FVector Position,FRotator Rotation);
    UFUNCTION(BlueprintCallable) void UpdatePose(float DT,bool bADS,float Kick);
    FVector MuzzleLocation() const;
    FVector MuzzleDirection() const;
    void Flash();
    bool StartReload(UBackpackInventory* Inventory);
    UFUNCTION(BlueprintCallable) void AdvanceHandling(float DT);
    void CancelHandling();
    void RequestReloadEnd();
    bool IsReloading() const;
    bool CanFire() const;
    bool ConsumeShot();
    float ReloadProgress() const;
    void ToggleSightOrMode();
    void ToggleLight();
    float AimFOV() const;
    FVector SightRear() const;
    FVector SightFront() const;
    void Holster();
    UFUNCTION(BlueprintPure) FString CurrentAnimationName() const;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 PackSoundsPlayed=0;
    UFUNCTION(BlueprintPure) float GripError(bool bLeft) const;
private:
    void SetupModel();
    void SetupPackPresentation();
    void UpdatePackPose(float DT,bool bADS,float Kick);
    float PackChargeDuration();
    UAnimSequence* PackAnimation(const FString& Group,const FString& Action);
    void SamplePackAnimation(USkeletalMeshComponent* Mesh,UAnimSequence* Animation,float Time);
    UPROPERTY(Transient) TMap<FString,TObjectPtr<UAnimSequence>> PackAnimations;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> PackActionAudio;
    FString PackAudioAction;
    float PackAudioTime=-1;
    FTransform PackBaseFrame,PackAimFrame;
    bool bPackEmptyReload=false;

    void SetBasePose();
    void AnimateParts();
    void PoseHands();
    void SolveArm(bool bLeft,const FVector& Palm,const FQuat& Rotation);
    void CurlFingers(bool bLeft);
    void BeginHandling(EWeaponHandling State,float Duration);
    void EventAt(float Old,float Now,float Time,int32 Bit,TFunctionRef<void()> Event);
    void Sound(FName Name,float Volume=.65f);
    void DropMagazine();
    void EjectCase();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ModelBody;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MovingAction;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ChargeHandle;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Optic;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Barrel;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShellInHand;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FlashMesh;
    UPROPERTY() TObjectPtr<UPointLightComponent> FlashLight;
    UPROPERTY() TObjectPtr<USpotLightComponent> WeaponLight;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> ScopeCapture;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ScopeLens;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> ScopeTexture;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ScopeMaterial;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UBackpackInventory> ReloadInventory;
    UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> Sounds;
    TWeakObjectPtr<AGame_Jam_Clean_VersCharacter> Holder;
    FTransform BaseHands;
    FQuat BaseRightRotation,BaseLeftRotation;
    FVector RightPalmOffset,LeftPalmOffset,LeftPalmTarget,RightPalmTarget;
    FQuat LeftHandRotation,RightHandRotation;
    bool bOriginalVisible=true,bReloadEnd=false,bLightOn=false,bLastADS=false,bCaseEjected=false;
    float FlashTime=0,PoseAlpha=0,SprintAlpha=0,SwayTime=0,HandlingDuration=0,ShotAge=10;
    uint32 HandlingEvents=0;
};

// Disposable visual props have independent world physics and begin their cleanup on contact.
UCLASS()
class GAME_JAM_CLEAN_VERS_API AWeaponDebris : public AActor
{
    GENERATED_BODY()
public:
    AWeaponDebris();
    virtual void Tick(float DT) override;
    void InitializeSkeletal(USkeletalMesh* Mesh,const FTransform& Transform,FVector Velocity);
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> SkeletalVisual;
    void Initialize(UStaticMesh* Mesh,const FTransform& Transform,FVector Velocity,bool bMagazine);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bLanded=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float LandedAt=0;
private:
    UFUNCTION() void OnContact(UPrimitiveComponent* Hit,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Result);
    bool bMag=false;
};

UCLASS()
class GAME_JAM_CLEAN_VERS_API ACombatAmmoPickup : public AActor
{
    GENERATED_BODY()
public:
    ACombatAmmoPickup();
    virtual void OnConstruction(const FTransform& Transform) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo") ETacticalWeaponKind Kind=ETacticalWeaponKind::Pistol;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo",meta=(ClampMin="1")) int32 Amount=34;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    FName AmmoId() const;
    FString AmmoName() const;
};

UCLASS()
class GAME_JAM_CLEAN_VERS_API ACombatTarget : public AActor
{
    GENERATED_BODY()
public:
    ACombatTarget();
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
    virtual void Tick(float DT) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Health=100;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
private:
    float HitFlash=0;
};

UCLASS()
class GAME_JAM_CLEAN_VERS_API ACombatShotEffect : public AActor
{
    GENERATED_BODY()
public:
    ACombatShotEffect();
    UFUNCTION(BlueprintCallable,Category="Combat|VFX")
    void Setup(FVector Start,FVector End,bool bImpact,bool bFlesh,FVector SurfaceNormal=FVector::ZeroVector);
    virtual void Tick(float DT) override;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Streak;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Impact;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ImpactDust;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ImpactSparks;
    TArray<FVector> SparkVelocities;
    FVector ImpactNormal=FVector::UpVector;
    float ImpactDuration=.32f,ImpactSize=12.f,ImpactSparkSpeed=180.f,ImpactRoll=0;
    float Age=0;
    FVector TraceStart=FVector::ZeroVector,TraceEnd=FVector::ZeroVector,TraceDirection=FVector::ForwardVector;
    float TraceDistance=0,VisualSpeed=45000,VisualLength=110,VisualWidth=1.8f,FadeSeconds=.025f;
    bool bHasImpact=false,bFleshImpact=false;
    void UpdateVisual();
};
