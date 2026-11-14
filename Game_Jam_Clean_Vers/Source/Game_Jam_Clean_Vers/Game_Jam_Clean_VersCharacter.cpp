#include "Game_Jam_Clean_VersCharacter.h"
#include "Audio/CharacterAudio.h"
#include "Camcorder/ItemInteractionComponent.h"
#include "Data/GlobalData.h"
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimInstance.h"
#include "Combat/UnifiedBodyAnimInstance.h"
#include "Combat/CharacterSecondaryMotionComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
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
#include "Camcorder/RecordingTabletComponent.h"
#include "Camcorder/RecordingLibrary.h"
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
FVector AGame_Jam_Clean_VersCharacter::BodyCameraOffset() const
{
    const float LookingDown=FMath::Clamp((-FMath::UnwindDegrees(GetControlRotation().Pitch)-25.f)/40.f,0.f,1.f);
    const auto* Equipment=GetController()?GetController()->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
    const bool CameraPresented=Equipment&&Equipment->Camcorder&&Equipment->Camcorder->IsPresenting();
    const float DeepLook=CameraPresented?0.f:FMath::Clamp((-FMath::UnwindDegrees(GetControlRotation().Pitch)-55.f)/25.f,0.f,1.f);
    // Lean over the body when looking at the feet. This keeps shoulders and upper
    // arms outside the near view without cutting geometry or changing selfies.
    const float Lean=38.f*DeepLook*DeepLook*(3.f-2.f*DeepLook);
    // Follow the crouched head; give the near view clearance from the elbows when looking down.
    return FVector(22+Lean-20*CrouchViewAlpha*(1-LookingDown),0,72-12*CrouchViewAlpha+CrouchCameraOffset);
}
void AGame_Jam_Clean_VersCharacter::BeginPlay(){Super::BeginPlay();
FirstPersonCameraComponent->AttachToComponent(GetCapsuleComponent(),FAttachmentTransformRules::KeepWorldTransform);FirstPersonCameraComponent->SetRelativeLocation(FVector(22,0,72));FirstPersonCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);if(auto* Body=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/MechanicGirl/SK_MechanicGirl_PackRig")))
{
    FirstPersonMesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/MechanicGirl/SK_MechanicGirl_FirstPerson")));
    FirstPersonMesh->EmptyOverrideMaterials();
    // Hide head/hair only in the owner's view, without shrinking any bones.
    // A second owner-hidden representation retains the complete head and shadow.
    auto* WorldBody=NewObject<USkeletalMeshComponent>(this,TEXT("MechanicWorldBody"));
    WorldBody->SetupAttachment(FirstPersonMesh);
    WorldBody->SetSkeletalMeshAsset(Body);
    WorldBody->SetOwnerNoSee(true);
    WorldBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WorldBody->SetCanEverAffectNavigation(false);
    WorldBody->SetCastShadow(true);WorldBody->bCastHiddenShadow=true;
    WorldBody->RegisterComponent();
    WorldBody->SetLeaderPoseComponent(FirstPersonMesh);
    if(auto* Hidden=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/MechanicGirl/Materials/M_OwnerHidden")))
    {
        for(int32 Slot=3;Slot<FirstPersonMesh->GetNumMaterials();++Slot)FirstPersonMesh->SetMaterial(Slot,Hidden);
        WorldBody->SetMaterial(3,Hidden);
    }
}
// The template offsets its hidden body 20 cm backwards; a visible body must stay centred.
GetMesh()->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
FirstPersonMesh->SetRelativeTransform(FTransform::Identity);
GetMesh()->SetOwnerNoSee(true);GetMesh()->SetVisibility(false,false);GetMesh()->SetCastShadow(false);GetMesh()->bCastHiddenShadow=false;FirstPersonMesh->SetCastShadow(false);GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;FirstPersonMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;FirstPersonMesh->SetAnimInstanceClass(UUnifiedBodyAnimInstance::StaticClass());FirstPersonMesh->SetDisablePostProcessBlueprint(true);FirstPersonMesh->AddTickPrerequisiteComponent(GetMesh());FirstPersonMesh->FirstPersonPrimitiveType=EFirstPersonPrimitiveType::None;FirstPersonMesh->SetVisibility(true,false);
auto* Secondary=NewObject<UCharacterSecondaryMotionComponent>(this,TEXT("CharacterSecondaryMotion"));
AddInstanceComponent(Secondary);Secondary->RegisterComponent();Secondary->Initialize(FirstPersonMesh);
LastFootstepLocation=GetActorLocation();GetCharacterMovement()->MaxWalkSpeed=360;GetCharacterMovement()->AirControl=.2f;FirstPersonCameraComponent->PostProcessSettings.bOverride_MotionBlurAmount=true;FirstPersonCameraComponent->PostProcessSettings.MotionBlurAmount=0;}
void AGame_Jam_Clean_VersCharacter::Tick(float DT)
{
    Super::Tick(DT);
    CrouchViewAlpha=FMath::FInterpTo(CrouchViewAlpha,bIsCrouched?1.f:0.f,DT,12.f);
    CrouchCameraOffset=FMath::FInterpTo(CrouchCameraOffset,0.f,DT,10.f);
    FirstPersonCameraComponent->SetRelativeLocation(BodyCameraOffset());
    const float Travel=FVector::Dist2D(LastFootstepLocation,GetActorLocation());LastFootstepLocation=GetActorLocation();
    if(Health>0 && GetCharacterMovement()->IsMovingOnGround() && GetVelocity().Size2D()>20 && Travel<150)
    {
        FootstepDistance+=Travel;const float Stride=bIsCrouched?95.f:bSprinting?190.f:145.f;
        if(FootstepDistance>=Stride){FootstepDistance=FMath::Fmod(FootstepDistance,Stride);if(FootstepSounds.Num()){auto* S=FootstepSounds[FootstepIndex++%FootstepSounds.Num()].Get();if(S)CharacterAudio::At(this,S,GetActorLocation()-FVector(0,0,80),ECharacterSound::Footstep,bIsCrouched?.18f:bSprinting?.7f:.45f,FMath::FRandRange(.96f,1.04f));}}
    }
    else FootstepDistance=0;
    DamageFlash=FMath::Max(0.f,DamageFlash-DT);auto* PC=Cast<APlayerController>(GetController());if(!PC||Health<=0){bSprintRequested=false;bSprinting=false;return;}
    auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>();auto* Combat=PC->FindComponentByClass<UCombatEquipmentComponent>();if(E&&(E->bBackpackOpen||(E->Tablet&&E->Tablet->bOpen)||(E->RecordingLibrary&&E->RecordingLibrary->bRoundEnded)))
    {bSprintRequested=false;bSprinting=false;GroundSpeed=360.f;GetCharacterMovement()->MaxWalkSpeed=GroundSpeed;return;}
    if(PC->WasInputKeyJustPressed(EKeys::LeftShift))
    {
        bSprintRequested=!bSprintRequested;
        if(bSprintRequested)
        {
            if(Combat) Combat->bADS=false;
            if(E && E->Camcorder) E->CancelViewfinder();
        }
    }
    const bool ADS=(Combat&&Combat->bADS)||(E&&E->Camcorder&&E->Camcorder->bViewfinderRaised);
    if(PC->WasInputKeyJustPressed(EKeys::LeftControl)&&!GetCharacterMovement()->IsFalling()){if(bIsCrouched)UnCrouch();else Crouch();}
    if(ADS || bIsCrouched || GetCharacterMovement()->bWantsToCrouch) bSprintRequested=false;
    bSprinting=bSprintRequested&&PC->IsInputKeyDown(EKeys::W)&&!PC->IsInputKeyDown(EKeys::S)&&!bIsCrouched&&!GetCharacterMovement()->bWantsToCrouch&&!ADS;
    if(!GetCharacterMovement()->IsFalling())GroundSpeed=ADS?270.f:bSprinting?650.f:360.f;
    GetCharacterMovement()->MaxWalkSpeed=GroundSpeed;
}
void AGame_Jam_Clean_VersCharacter::OnStartCrouch(float H,float S)
{
    Super::OnStartCrouch(H,S);
    CrouchCameraOffset+=H;
    FirstPersonCameraComponent->SetRelativeLocation(BodyCameraOffset());
}
void AGame_Jam_Clean_VersCharacter::OnEndCrouch(float H,float S)
{
    Super::OnEndCrouch(H,S);
    CrouchCameraOffset-=H;
    FirstPersonCameraComponent->SetRelativeLocation(BodyCameraOffset());
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
		const auto* PC=Cast<APlayerController>(GetController());
		const auto* Equipment=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
		const bool Selfie=Equipment&&Equipment->Camcorder&&Equipment->Camcorder->bSelfie;
		// The reverse lens pitches opposite to its orbit. Compensate only here,
		// so mouse-up still looks up, just like the forward camera.
		AddControllerPitchInput(Selfie?-Pitch:Pitch);
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
