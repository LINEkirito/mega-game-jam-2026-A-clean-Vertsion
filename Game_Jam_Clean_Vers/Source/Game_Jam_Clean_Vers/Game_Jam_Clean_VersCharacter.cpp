#include "Game_Jam_Clean_VersCharacter.h"
#include "Audio/CharacterAudio.h"
#include "Camcorder/ItemInteractionComponent.h"
#include "Data/GlobalData.h"
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimInstance.h"
#include "Combat/UnifiedBodyAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game_Jam_Clean_Vers.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Data/DataTransmitter.h"

AGame_Jam_Clean_VersCharacter::AGame_Jam_Clean_VersCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;
	FirstPersonCameraComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	FirstPersonCameraComponent->PostProcessSettings.MotionBlurAmount = 0.f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
    PrimaryActorTick.bCanEverTick=true;
    ItemInteraction=CreateDefaultSubobject<UItemInteractionComponent>(TEXT("ItemInteraction"));
    GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch=true;
    GetCharacterMovement()->SetCrouchedHalfHeight(55.f);
    GetCharacterMovement()->MaxWalkSpeedCrouched=180.f;
    JumpMaxCount=1;
    for(int32 I=1;I<=4;++I) FootstepSounds.Add(LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Combat/Audio/Handling/Footstep%d"),I)));
    HurtSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Effects/Blood/S_BloodHit"));
}
void AGame_Jam_Clean_VersCharacter::BeginPlay(){Super::BeginPlay();
FirstPersonCameraComponent->AttachToComponent(GetCapsuleComponent(),FAttachmentTransformRules::KeepWorldTransform);FirstPersonCameraComponent->SetRelativeLocation(FVector(0,0,64));FirstPersonCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);if(auto* Arms=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Combat/Viewmodel/BeachGirls/SK_PlayerBeachHands")))FirstPersonMesh->SetSkeletalMeshAsset(Arms);GetMesh()->SetOwnerNoSee(true);GetMesh()->SetCastShadow(true);GetMesh()->bCastHiddenShadow=true;FirstPersonMesh->SetCastShadow(false);GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;FirstPersonMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;FirstPersonMesh->SetAnimInstanceClass(UUnifiedBodyAnimInstance::StaticClass());FirstPersonMesh->SetDisablePostProcessBlueprint(true);FirstPersonMesh->AddTickPrerequisiteComponent(GetMesh());FirstPersonMesh->FirstPersonPrimitiveType=EFirstPersonPrimitiveType::None;FirstPersonMesh->SetVisibility(true,false);LastFootstepLocation=GetActorLocation();GetCharacterMovement()->MaxWalkSpeed=360;GetCharacterMovement()->AirControl=.2f;FirstPersonCameraComponent->PostProcessSettings.bOverride_MotionBlurAmount=true;FirstPersonCameraComponent->PostProcessSettings.MotionBlurAmount=0;}
void AGame_Jam_Clean_VersCharacter::Tick(float DT)
{
    Super::Tick(DT);
    CrouchCameraOffset=FMath::FInterpTo(CrouchCameraOffset,0.f,DT,10.f);
    FirstPersonCameraComponent->SetRelativeLocation(FVector(0,0,64+CrouchCameraOffset));
    const float Travel=FVector::Dist2D(LastFootstepLocation,GetActorLocation());LastFootstepLocation=GetActorLocation();
    if(Health>0 && GetCharacterMovement()->IsMovingOnGround() && GetVelocity().Size2D()>20 && Travel<150)
    {
        FootstepDistance+=Travel;const float Stride=bIsCrouched?95.f:bSprinting?190.f:145.f;
        if(FootstepDistance>=Stride){FootstepDistance=FMath::Fmod(FootstepDistance,Stride);if(FootstepSounds.Num()){auto* S=FootstepSounds[FootstepIndex++%FootstepSounds.Num()].Get();if(S)CharacterAudio::At(this,S,GetActorLocation()-FVector(0,0,80),ECharacterSound::Footstep,bIsCrouched?.18f:bSprinting?.7f:.45f,FMath::FRandRange(.96f,1.04f));}}
    }
    else FootstepDistance=0;
    DamageFlash=FMath::Max(0.f,DamageFlash-DT);auto* PC=Cast<APlayerController>(GetController());if(!PC||Health<=0)return;
    auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>();auto* Combat=PC->FindComponentByClass<UCombatEquipmentComponent>();if(E&&E->bBackpackOpen)return;
    const bool ADS=(Combat&&Combat->bADS)||(E&&E->Camcorder&&E->Camcorder->bViewfinderRaised);
    if(PC->WasInputKeyJustPressed(EKeys::LeftControl)&&!GetCharacterMovement()->IsFalling()){if(bIsCrouched)UnCrouch();else Crouch();}
    bSprinting=PC->IsInputKeyDown(EKeys::LeftShift)&&PC->IsInputKeyDown(EKeys::W)&&!PC->IsInputKeyDown(EKeys::S)&&!bIsCrouched&&!GetCharacterMovement()->bWantsToCrouch&&!ADS;
    if(!GetCharacterMovement()->IsFalling())GroundSpeed=ADS?270.f:bSprinting?650.f:360.f;
    GetCharacterMovement()->MaxWalkSpeed=GroundSpeed;
}
void AGame_Jam_Clean_VersCharacter::OnStartCrouch(float H,float S)
{
    Super::OnStartCrouch(H,S);
    CrouchCameraOffset+=H;
    FirstPersonCameraComponent->SetRelativeLocation(FVector(0,0,64+CrouchCameraOffset));
}
void AGame_Jam_Clean_VersCharacter::OnEndCrouch(float H,float S)
{
    Super::OnEndCrouch(H,S);
    CrouchCameraOffset-=H;
    FirstPersonCameraComponent->SetRelativeLocation(FVector(0,0,64+CrouchCameraOffset));
}
float AGame_Jam_Clean_VersCharacter::TakeDamage(float D,const FDamageEvent&,AController*,AActor*)
{
    const float Now=GetWorld()->GetTimeSeconds();if(D<=0||Health<=0||Now<ProtectionUntil)return 0;
    const float Applied=FMath::Min(Health,D);Health-=Applied;ProtectionUntil=Health>0?Now+GetDefault<UGlobalData>()->DamageProtectionSeconds:-1;DamageFlash=1.2f;
    if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()) Bus->NotifyChange(this,TEXT("Health"),Health);
    if(HurtSound)CharacterAudio::Attached(HurtSound,GetRootComponent(),ECharacterSound::Voice,.6f);
    if(Health<=0){GetCharacterMovement()->DisableMovement();if(auto* PC=Cast<APlayerController>(GetController())){if(auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>()){E->SetBackpackOpen(false);if(E->Camcorder){E->Camcorder->SetViewfinderRaised(false);E->Camcorder->StopRecording();}}}}
    return Applied;
}

void AGame_Jam_Clean_VersCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AGame_Jam_Clean_VersCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AGame_Jam_Clean_VersCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGame_Jam_Clean_VersCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGame_Jam_Clean_VersCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGame_Jam_Clean_VersCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogGame_Jam_Clean_Vers, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AGame_Jam_Clean_VersCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AGame_Jam_Clean_VersCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AGame_Jam_Clean_VersCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AGame_Jam_Clean_VersCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AGame_Jam_Clean_VersCharacter::DoJumpStart()
{
    if(Health<=0)return;
    if(bIsCrouched){UnCrouch();GetCharacterMovement()->UnCrouch(false);if(bIsCrouched){GetCharacterMovement()->bWantsToCrouch=true;return;}}
	// pass Jump to the character
	Jump();
}

void AGame_Jam_Clean_VersCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
