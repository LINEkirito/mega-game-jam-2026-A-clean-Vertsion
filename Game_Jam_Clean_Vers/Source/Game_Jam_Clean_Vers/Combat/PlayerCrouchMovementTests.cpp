#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"

// Exercise real movement ticks: static crouch grip tests miss the raised,
// forward-leaning torso in the locomotion clips shared by all four slots.
class FPlayerCrouchMovementCheck final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Slot=0;
    double Started=0;
    float MaxGrip=0,MinEyeClearance=MAX_flt,MaxSpeed=0;
public:
    explicit FPlayerCrouchMovementCheck(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())
            if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
        auto* Equipment=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
        auto* Combat=PC?PC->FindComponentByClass<UCombatEquipmentComponent>():nullptr;
        if(!Player||!Equipment||!Combat){Test->AddError(TEXT("Missing crouch test player/equipment"));return true;}
        if(Started==0)
        {
            Player->SetCanBeDamaged(false);
            PC->SetControlRotation(FRotator(0,Player->GetActorRotation().Yaw,0));
            Player->Crouch();Equipment->ActivateQuickSlot(Slot);
            Started=FPlatformTime::Seconds();MaxGrip=0;MinEyeClearance=MAX_flt;MaxSpeed=0;
            return false;
        }
        const double Elapsed=FPlatformTime::Seconds()-Started;
        if(Elapsed<4)return false; // Complete the real holster/draw transition.
        if(Elapsed<7)
        {
            Player->AddMovementInput(Player->GetActorForwardVector());
            MaxSpeed=FMath::Max(MaxSpeed,static_cast<float>(Player->GetVelocity().Size2D()));
            if(Elapsed>4.5 && Player->GetVelocity().Size2D()>100.f)
            {
                auto* Body=Player->GetFirstPersonMesh();
                const float EyeZ=Player->GetFirstPersonCameraComponent()->GetComponentLocation().Z;
                for(const TCHAR* Side:{TEXT("l"),TEXT("r")})
                {
                    const FName Shoulder(*FString::Printf(TEXT("upperarm_%s"),Side));
                    MinEyeClearance=FMath::Min(MinEyeClearance,EyeZ-static_cast<float>(Body->GetSocketLocation(Shoulder).Z));
                }
                if(Slot==0 && Equipment->Camcorder)
                {
                    for(const TCHAR* Hand:{TEXT("hand_l"),TEXT("hand_r")})
                        MaxGrip=FMath::Max(MaxGrip,static_cast<float>(FVector::Distance(
                            Body->GetSocketLocation(Hand),Equipment->Camcorder->HoldingHands->GetSocketLocation(Hand))));
                }
                else if(auto* Gun=Combat->ActiveWeapon.Get())
                    MaxGrip=FMath::Max(MaxGrip,FMath::Max(Gun->GripError(true),Gun->GripError(false)));
                else Test->AddError(TEXT("Selected weapon lost during crouch movement"));
            }
            return false;
        }
        if(Elapsed<8)return false;
        Test->TestTrue(TEXT("Actual crouch movement reached test speed"),MaxSpeed>100.f);
        Test->TestTrue(TEXT("Player remains crouched while moving"),Player->bIsCrouched);
        Test->TestTrue(TEXT("Moving crouch eye stays above both shoulders"),MinEyeClearance!=MAX_flt&&MinEyeClearance>8.f);
        Test->TestTrue(TEXT("Both hands retain equipment contact while crouch walking"),MaxGrip<.2f);
        Test->AddInfo(FString::Printf(TEXT("Crouch slot %d: min shoulder clearance %.3f cm; max grip %.3f cm"),Slot,MinEyeClearance,MaxGrip));
        Started=0;
        if(++Slot<4)return false;
        Player->UnCrouch();return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerCrouchMovementTest,"Game.Characters.PlayerCrouchMovement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlayerCrouchMovementTest::RunTest(const FString&)
{
    AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
    AddCommand(new FPlayerCrouchMovementCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
