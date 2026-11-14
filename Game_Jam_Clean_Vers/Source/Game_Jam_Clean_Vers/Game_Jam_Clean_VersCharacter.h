// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Game_Jam_Clean_VersCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class USoundBase;
class UItemInteractionComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AGame_Jam_Clean_VersCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Full body on the gun pack skeleton; head materials hidden only for the owner */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	AGame_Jam_Clean_VersCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnStartCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust) override;
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health") float Health=100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement") bool bSprinting=false;
    bool bSprintRequested=false;
    float DamageFlash=0.f;
    UPROPERTY() TObjectPtr<UItemInteractionComponent> ItemInteraction;
private:
    float ProtectionUntil=-1.f;
    float GroundSpeed=360.f;
    float CrouchCameraOffset=0.f;
    float CrouchViewAlpha=0.f;
    FVector BodyCameraOffset() const;
    UPROPERTY() TObjectPtr<USoundBase> HurtSound;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> FootstepSounds;
    FVector LastFootstepLocation=FVector::ZeroVector;
    float FootstepDistance=0;
    int32 FootstepIndex=0;
public:

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

