#include "Game_Jam_Clean_VersPlayerController.h"
// Copyright Epic Games, Inc. All Rights Reserved.


#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Game_Jam_Clean_VersCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Game_Jam_Clean_Vers.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Camcorder/CamcorderEquipmentComponent.h"

AGame_Jam_Clean_VersPlayerController::AGame_Jam_Clean_VersPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AGame_Jam_Clean_VersCameraManager::StaticClass();
}

void AGame_Jam_Clean_VersPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController())
	{
		CamcorderEquipment = NewObject<UCamcorderEquipmentComponent>(this, TEXT("CamcorderEquipment"));
		CamcorderEquipment->RegisterComponent();
	}

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogGame_Jam_Clean_Vers, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AGame_Jam_Clean_VersPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool AGame_Jam_Clean_VersPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
