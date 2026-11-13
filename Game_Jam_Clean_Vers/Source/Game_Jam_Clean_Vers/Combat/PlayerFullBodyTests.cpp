#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FPlayerFullBodyCheck, FAutomationTestBase*, Test);
bool FPlayerFullBodyCheck::Update()
{
    UWorld* World=nullptr;
    for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
    auto* PC=World?World->GetFirstPlayerController():nullptr;
    auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
    if(!Player){Test->AddError(TEXT("No player in PIE"));return true;}
    auto* Mesh=Player->GetFirstPersonMesh();
    auto* Template=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/KINEMATION/TacticalShooterPack/Character/Operator/UE5/SKM_Operator_Mono"));
    auto* Asset=Mesh->GetSkeletalMeshAsset();
    if(!Test->TestNotNull(TEXT("Full body mesh"),Asset)||!Template)return true;
    Test->TestTrue(TEXT("Player uses Mechanic Girl"),Asset->GetPathName().Contains(TEXT("MechanicGirl_FirstPerson")));
    Test->TestTrue(TEXT("Original gun pack skeleton retained"),Asset->GetSkeleton()==Template->GetSkeleton());
    const auto& Ref=Asset->GetRefSkeleton();const auto& Original=Template->GetRefSkeleton();
    Test->TestEqual(TEXT("Original bone count"),Ref.GetNum(),Original.GetNum());
    for(int32 I=0;I<FMath::Min(Ref.GetNum(),Original.GetNum());++I)
    {
        Test->TestEqual(TEXT("Bone name unchanged"),Ref.GetBoneName(I),Original.GetBoneName(I));
        Test->TestEqual(TEXT("Bone parent unchanged"),Ref.GetParentIndex(I),Original.GetParentIndex(I));
        Test->TestTrue(TEXT("Bind pose unchanged"),Ref.GetRefBonePose()[I].Equals(Original.GetRefBonePose()[I],.001f));
    }
    USkeletalMeshComponent* WorldBody=nullptr;
    TArray<USkeletalMeshComponent*> Components;Player->GetComponents(Components);
    for(auto* Component:Components)if(Component->GetFName()==TEXT("MechanicWorldBody"))WorldBody=Component;
    if(Test->TestNotNull(TEXT("Complete external body exists"),WorldBody))
    {
        auto* FullAsset=WorldBody->GetSkeletalMeshAsset();
        Test->TestTrue(TEXT("External body retains original pack skeleton"),FullAsset&&FullAsset->GetSkeleton()==Template->GetSkeleton());
        const auto* Render=FullAsset?FullAsset->GetResourceForRendering():nullptr;
        if(Render&&Render->LODRenderData.Num())
        {
            TSet<int32> VisibleMaterials;
            const auto* Info=FullAsset->GetLODInfo(0);
            const auto& Sections=Render->LODRenderData[0].RenderSections;
            for(int32 I=0;I<Sections.Num();++I)
            {
                int32 Material=Sections[I].MaterialIndex;
                if(Info&&Info->LODMaterialMap.IsValidIndex(I)&&FullAsset->IsValidMaterialIndex(Info->LODMaterialMap[I]))Material=Info->LODMaterialMap[I];
                if(Sections[I].NumTriangles>0)VisibleMaterials.Add(Material);
            }
            for(int32 Material:{0,1,2,4})Test->TestTrue(*FString::Printf(TEXT("External rendered sections retain body/clothes/shoes/head slot %d"),Material),VisibleMaterials.Contains(Material));
        }
        else Test->AddError(TEXT("External body has no render LOD"));
    }
    auto* Combat=PC->FindComponentByClass<UCombatEquipmentComponent>();
    auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
    if(!Combat||!Equipment){Test->AddError(TEXT("Missing equipment"));return true;}
    Equipment->StoreCamera();
    float MaxGrip=0;
    for(const auto& Entry:Combat->Weapons)
    {
        auto* Gun=Entry.Value.Get();Combat->ActiveWeapon=Gun;Gun->Equip(Player);Gun->AdvanceHandling(5.f);
        for(float Pitch:{-85.f,-65.f,0.f,65.f,85.f})
        {
            PC->SetControlRotation(FRotator(Pitch,0,0));
            const FVector FootBefore=Mesh->GetSocketLocation(TEXT("foot_l"));
            Gun->UpdatePose(1.f,true,0);
            const float Error=FMath::Max(Gun->GripError(false),Gun->GripError(true));MaxGrip=FMath::Max(MaxGrip,Error);
            Test->TestTrue(*FString::Printf(TEXT("Both wrists contact gun kind %d pitch %.0f (%.3fcm)"),int32(Gun->Kind),Pitch,Error),Error<.2f);
            Test->TestTrue(TEXT("Looking does not rotate or lift feet"),FVector::Distance(FootBefore,Mesh->GetSocketLocation(TEXT("foot_l")))<.05f);
            Test->TestTrue(TEXT("Body stays at locomotion frame"),Mesh->GetComponentTransform().Equals(Player->GetMesh()->GetComponentTransform(),.001f));
        }
        Gun->Stow();Combat->ActiveWeapon=nullptr;
    }
    PC->SetControlRotation(FRotator::ZeroRotator);
    Test->AddInfo(FString::Printf(TEXT("Max full-body grip error %.4fcm"),MaxGrip));
    return true;
}
class FPlayerCrouchBodyCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Stage=0;double Started=0;float StandingPelvis=0,StandingFoot=0;
public:
    explicit FPlayerCrouchBodyCheck(FAutomationTestBase* InTest):Test(InTest){}
    virtual bool Update() override
    {
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
        if(!Player){Test->AddError(TEXT("No player for crouch regression"));return true;}
        auto* Mesh=Player->GetFirstPersonMesh();
        if(Stage==0)
        {
            StandingPelvis=Mesh->GetSocketLocation(TEXT("pelvis")).Z;
            StandingFoot=FMath::Min(Mesh->GetSocketLocation(TEXT("foot_l")).Z,Mesh->GetSocketLocation(TEXT("foot_r")).Z);
            Player->Crouch();Started=FPlatformTime::Seconds();Stage=1;return false;
        }
        if(FPlatformTime::Seconds()-Started<.8)return false;
        if(Stage==1)
        {
            Test->TestTrue(TEXT("Real character crouch active"),Player->bIsCrouched);
            Test->TestTrue(TEXT("Visible pelvis actually lowers while crouching"),StandingPelvis-Mesh->GetSocketLocation(TEXT("pelvis")).Z>25.f);
            const float Foot=FMath::Min(Mesh->GetSocketLocation(TEXT("foot_l")).Z,Mesh->GetSocketLocation(TEXT("foot_r")).Z);
            Test->TestTrue(TEXT("Crouch supporting foot stays near floor"),FMath::Abs(Foot-StandingFoot)<12.f);
            auto* Combat=PC->FindComponentByClass<UCombatEquipmentComponent>();
            for(const auto& Entry:Combat->Weapons)
            {
                auto* Gun=Entry.Value.Get();Combat->ActiveWeapon=Gun;Gun->Equip(Player);Gun->AdvanceHandling(5.f);
                for(float Pitch:{-85.f,0.f,85.f})
                {
                    PC->SetControlRotation(FRotator(Pitch,0,0));Gun->UpdatePose(1,true,0);
                    Test->TestTrue(TEXT("Crouched hands retain weapon contact"),FMath::Max(Gun->GripError(false),Gun->GripError(true))<.2f);
                }
                Gun->Stow();Combat->ActiveWeapon=nullptr;
            }
            PC->SetControlRotation(FRotator::ZeroRotator);Player->UnCrouch();Started=FPlatformTime::Seconds();Stage=2;return false;
        }
        Test->TestFalse(TEXT("Player stands again"),Player->bIsCrouched);
        Test->TestTrue(TEXT("Standing pelvis restored"),FMath::Abs(StandingPelvis-Mesh->GetSocketLocation(TEXT("pelvis")).Z)<5.f);
        return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerFullBodyTest,"Game.Characters.PlayerFullBody",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlayerFullBodyTest::RunTest(const FString&)
{
    AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FPlayerFullBodyCheck(this));AddCommand(new FPlayerCrouchBodyCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
