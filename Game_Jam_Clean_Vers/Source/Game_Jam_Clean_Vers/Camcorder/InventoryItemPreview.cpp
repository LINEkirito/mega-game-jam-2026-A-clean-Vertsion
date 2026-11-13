#include "InventoryItemPreview.h"
#include "RecordingTabletDisplay.h"
#include "Materials/MaterialInterface.h"
#include "CamcorderEquipmentComponent.h"
#include "BackpackInventory.h"
#include "ShoulderCamcorder.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Animation/AnimSequence.h"

AInventoryItemPreview::AInventoryItemPreview()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Pivot=CreateDefaultSubobject<USceneComponent>(TEXT("ItemPivot")); Pivot->SetupAttachment(RootComponent);
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PreviewCapture")); Capture->SetupAttachment(RootComponent);
    Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
    Capture->FOVAngle=35;
    Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetBloom(false);
    Capture->ShowFlags.SetEyeAdaptation(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureBias=true;
    Capture->PostProcessSettings.AutoExposureBias=1.f;
    for(int32 I=0; I<2; ++I)
    {
        auto* Light=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("StudioLight%d"),I));
        Light->SetupAttachment(RootComponent); Light->SetRelativeLocation(I==0?FVector(100,-160,160):FVector(-100,100,80));
        Light->SetIntensity(I==0?60000.f:40000.f); Light->SetAttenuationRadius(800); Light->SetCastShadows(false);
    }
}
void AInventoryItemPreview::BeginPlay()
{
    Super::BeginPlay();
    Texture=NewObject<UTextureRenderTarget2D>(this);
    Texture->RenderTargetFormat=RTF_RGBA8; Texture->ClearColor=FLinearColor(.012f,.018f,.026f,1);
    Texture->InitAutoFormat(768,432); Texture->UpdateResourceImmediate(true); Capture->TextureTarget=Texture;
}
void AInventoryItemPreview::AddCopy(UStaticMeshComponent* Source,const FTransform& Local)
{
    if(!Source || !Source->GetStaticMesh()) return;
    auto* Copy=NewObject<UStaticMeshComponent>(this);
    Copy->SetupAttachment(Pivot); Copy->SetStaticMesh(Source->GetStaticMesh()); Copy->SetRelativeTransform(Local);
    Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision); Copy->SetCastShadow(false);
    Copy->SetVisibleInSceneCaptureOnly(true); Copy->SetCanEverAffectNavigation(false);
    for(int32 I=0; I<Source->GetNumMaterials(); ++I) Copy->SetMaterial(I,Source->GetMaterial(I));
    const auto& Defaults=Source->GetDefaultCustomPrimitiveData().Data;
    const auto& Runtime=Source->GetCustomPrimitiveData().Data;
    Copy->SetDefaultCustomPrimitiveDataFloatArray(0,Defaults);
    Copy->SetCustomPrimitiveDataFloatArray(0,Runtime.Num()?Runtime:Defaults);
    Copy->RegisterComponent(); Parts.Add(Copy); Capture->ShowOnlyComponent(Copy);
}
void AInventoryItemPreview::AddSkeletalCopy(USkeletalMeshComponent* Source,const FTransform& Local)
{
    if(!Source || !Source->GetSkeletalMeshAsset()) return;
    auto* Copy=NewObject<USkeletalMeshComponent>(this);
    Copy->SetupAttachment(Pivot);
    Copy->SetSkeletalMeshAsset(Source->GetSkeletalMeshAsset());
    Copy->SetAnimInstanceClass(Source->GetAnimClass());
    Copy->SetRelativeTransform(Local);
    Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Copy->SetCastShadow(false);
    Copy->SetVisibleInSceneCaptureOnly(true);
    Copy->SetCanEverAffectNavigation(false);
    for(int32 I=0;I<Source->GetNumMaterials();++I)Copy->SetMaterial(I,Source->GetMaterial(I));
    Copy->RegisterComponent();
    Copy->TickAnimation(0.f,false);Copy->RefreshBoneTransforms();
    Parts.Add(Copy);Capture->ShowOnlyComponent(Copy);
}
void AInventoryItemPreview::ShowItem(UCamcorderEquipmentComponent* E,FName Id)
{
    if(ItemId==Id && !Parts.IsEmpty()) return;
    for(auto Part:Parts) Part->DestroyComponent(); Parts.Reset(); Capture->ShowOnlyComponents.Reset();
    ItemId=Id; Pivot->SetRelativeTransform(FTransform::Identity);
    if(E && Id==UBackpackInventory::CameraId && E->Camcorder)
    {
        // The purchased prop is skeletal. Copy only the camera and its display,
        // never the hidden animation source or the player's hands.
        AddSkeletalCopy(E->Camcorder->CameraMesh,FTransform::Identity);
        AddCopy(E->Camcorder->DisplayScreen,E->Camcorder->DisplayScreen->GetRelativeTransform());
    }
    else if(E && E->Combat && E->Combat->Weapons.Contains(Id))
    {
        auto* Weapon=E->Combat->Weapons[Id].Get();
        if(Weapon->bPackPresentation && Weapon->PackWeaponMesh->GetSkeletalMeshAsset())
        {
            AddSkeletalCopy(Weapon->PackWeaponMesh,FTransform::Identity);
            auto* Preview=CastChecked<USkeletalMeshComponent>(Parts.Last());
            const FString Type=Weapon->Kind==ETacticalWeaponKind::Rifle?TEXT("TR15"):Weapon->Kind==ETacticalWeaponKind::Pistol?TEXT("WK-11_Viper"):TEXT("Herrington_11-87");
            auto* Idle=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/")+Type+TEXT("/Weapon/A_W_")+Type+TEXT("_Idle")));
            Preview->SetDisablePostProcessBlueprint(true);
            if(Idle){Preview->PlayAnimation(Idle,false);Preview->SetPosition(0,false);Preview->TickAnimation(0,false);Preview->RefreshBoneTransforms();Preview->bPauseAnims=true;}
            if(Weapon->Kind==ETacticalWeaponKind::Shotgun){Preview->HideBoneByName(TEXT("Suppressor"),EPhysBodyOp::PBO_None);Preview->RefreshBoneTransforms();}
            if(Weapon->PackMagazineMesh->GetSkeletalMeshAsset())
                AddSkeletalCopy(Weapon->PackMagazineMesh,Weapon->PackMagazineMesh->GetRelativeTransform()*Preview->GetSocketTransform(Weapon->PackMagazineMesh->GetAttachSocketName(),RTS_Component));
            // Copy attachments relative to the idle preview sockets, never the live reload pose.
            for(auto Part:Weapon->ModelParts)if(Part && Part->GetAttachParent()==Weapon->PackWeaponMesh && Part->IsVisible())
                AddCopy(Part,Part->GetRelativeTransform()*Preview->GetSocketTransform(Part->GetAttachSocketName(),RTS_Component));
        }
        else for(auto Part:Weapon->ModelParts) if(Part && Part->GetFName()!=TEXT("LoadingShell"))
            AddCopy(Part,FTransform::Identity);
    }
    else if(Id==UBackpackInventory::TabletId)
    { AddSkeletalCopy(GetDefault<ARecordingTabletDisplay>()->Mesh,FTransform::Identity); }
    else if(Id==UBackpackInventory::BatteryId)
    {
        auto* Mesh=GetDefault<ACamcorderBattery>()->Mesh.Get();
        AddCopy(Mesh,Mesh->GetRelativeTransform());
        TArray<UStaticMeshComponent*> Details;GetDefault<ACamcorderBattery>()->GetComponents(Details);
        for(auto* Part:Details)if(Part!=Mesh)AddCopy(Part,Part->GetRelativeTransform()*Mesh->GetRelativeTransform());
    }
    else if(Id==TEXT("Ammo9mm") || Id==TEXT("Ammo12ga") || Id==TEXT("Ammo556"))
    {
        auto* Mesh=GetDefault<ACombatAmmoPickup>()->Mesh.Get();
        AddCopy(Mesh,Mesh->GetRelativeTransform());
    }
    if(Id==TEXT("Ammo9mm")||Id==TEXT("Ammo12ga")||Id==TEXT("Ammo556"))
        if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/Combat/Packaging/M_"))+Id.ToString())))for(auto Part:Parts)Part->SetMaterial(0,Mat);
    FBox Bounds(ForceInit);
    for(auto Part:Parts) Bounds+=Part->CalcBounds(Part->GetRelativeTransform()).GetBox();
    Center=Bounds.IsValid?Bounds.GetCenter():FVector::ZeroVector;
    // Fit a bounding sphere so rotating long guns does not crop their ends.
    Distance=Bounds.IsValid?FMath::Max(20.f,Bounds.GetExtent().Size()*6.2f):200.f;
    ResetView();
}
void AInventoryItemPreview::Rotate(float X,float Y)
{
    ViewRotation.Yaw=FMath::UnwindDegrees(ViewRotation.Yaw+X*.6f);
    ViewRotation.Pitch=FMath::Clamp(ViewRotation.Pitch+Y*.6f,-85.f,85.f); CaptureView();
}
void AInventoryItemPreview::Zoom(float Delta)
{
    ZoomAmount=FMath::Clamp(ZoomAmount*FMath::Pow(1.12f,Delta),.65f,1.8f); CaptureView();
}
void AInventoryItemPreview::ResetView()
{
    ZoomAmount=1.f; ViewRotation=FRotator::ZeroRotator; CaptureView();
}
void AInventoryItemPreview::CaptureView()
{
    Pivot->SetRelativeRotation(ViewRotation); Pivot->SetRelativeLocation(-ViewRotation.RotateVector(Center));
    const FVector Eye=FVector(1,-1.6f,.65f).GetSafeNormal()*Distance/ZoomAmount;
    Capture->SetRelativeLocation(Eye); Capture->SetRelativeRotation((-Eye).Rotation());
    Capture->CaptureScene();
}
