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

class FWeaponADSMovementCheck final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Kind=0;
    double Started=0;
    float MinDepth=MAX_flt,MaxDepth=-MAX_flt,MaxGrip=0;
    bool Captured=false;
public:
    explicit FWeaponADSMovementCheck(FAutomationTestBase* InTest):Test(InTest){}
    virtual bool Update() override
    {
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
        auto* Combat=PC?PC->FindComponentByClass<UCombatEquipmentComponent>():nullptr;
        if(!Player||!Combat){Test->AddError(TEXT("No PIE player/combat"));return true;}
        if(Started==0)
        {
            Combat->Equipment->StoreCamera();Combat->Unequip();Player->SetCanBeDamaged(false);
            ATacticalWeapon* Gun=nullptr;
            for(const auto& Entry:Combat->Weapons)if(int32(Entry.Value->Kind)==Kind)Gun=Entry.Value;
            if(!Gun){Test->AddError(TEXT("Missing starting weapon"));return true;}
            Combat->ActiveWeapon=Gun;Gun->Equip(Player);Gun->AdvanceHandling(5.f);
            PC->SetControlRotation(FRotator(0,Player->GetActorRotation().Yaw,0));
            Combat->bADS=true;Started=FPlatformTime::Seconds();
            MinDepth=MAX_flt;MaxDepth=-MAX_flt;MaxGrip=0;Captured=false;
            return false;
        }
        const double Elapsed=FPlatformTime::Seconds()-Started;
        auto* Gun=Combat->ActiveWeapon.Get();
        if(!Gun){Test->AddError(TEXT("Weapon lost during ADS"));return true;}
        Combat->bADS=true;
        if(Elapsed>1 && Elapsed<2.5)Player->AddMovementInput(Player->GetActorForwardVector());
        else if(Elapsed<4 && Elapsed>=2.5)Player->AddMovementInput(-Player->GetActorForwardVector());
        else if(Elapsed<5.5 && Elapsed>=4)Player->AddMovementInput(Player->GetActorRightVector());
        if(Elapsed>1)
        {
            const FTransform View(PC->GetControlRotation(),Player->GetFirstPersonCameraComponent()->GetComponentLocation());
            const float Depth=View.InverseTransformPosition(Gun->PackWeaponMesh->GetSocketLocation(TEXT("AimPoint"))).X;
            MinDepth=FMath::Min(MinDepth,Depth);MaxDepth=FMath::Max(MaxDepth,Depth);
            MaxGrip=FMath::Max(MaxGrip,FMath::Max(Gun->GripError(true),Gun->GripError(false)));
        }
        if(Elapsed>2&&!Captured)
        {
            PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1280x720 filename=\"G:/TheTechfiles/ThefinalGameJam/RifleADS/native_%d.png\""),Kind));
            Captured=true;
        }
        if(Elapsed<6.5)return false;
        Test->AddInfo(FString::Printf(TEXT("ADS kind %d: depth %.4f..%.4f cm; max grip error %.4f cm"),Kind,MinDepth,MaxDepth,MaxGrip));
        Test->TestTrue(TEXT("Walking ADS does not pump the weapon towards the camera"),MaxDepth-MinDepth<.05f);
        Test->TestTrue(TEXT("Both hands keep weapon contact during ADS locomotion"),MaxGrip<.2f);
        Combat->Unequip();Started=0;
        return ++Kind>=3;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponADSMovementTest,"Game.Weapons.ADSMovement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWeaponADSMovementTest::RunTest(const FString&)
{
    AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FWeaponADSMovementCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
