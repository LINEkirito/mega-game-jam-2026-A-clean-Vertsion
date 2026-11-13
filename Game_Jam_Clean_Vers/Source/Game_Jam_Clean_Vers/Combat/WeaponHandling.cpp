#include "Data/DataTransmitter.h"
#include "Audio/CharacterAudio.h"
#include "TacticalWeapon.h"
#include "CombatEquipmentComponent.h"
#include "Data/GlobalData.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Camcorder/BackpackInventory.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WeaponMotion
{
float Smooth(float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);}
FVector Blend(FVector A,FVector B,float T){return FMath::Lerp(A,B,Smooth(T));}
UStaticMesh* Mesh(const TCHAR* Name){return LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Game/Combat/Reference/Meshes/"))+Name));}
}
FTacticalWeaponSpec ATacticalWeapon::Spec()const
{
    FTacticalWeaponSpec S;S.Reload=2.6f;S.Capacity=FMath::Max(1,GetDefault<UGlobalData>()->PistolCapacity);S.Damage=GetDefault<UGlobalData>()->PistolBodyDamage;S.Name=TEXT("WK-11 Viper 9×19 手枪");S.Ammo=TEXT("Ammo9mm");
    if(Kind==ETacticalWeaponKind::Rifle){S.Name=TEXT("TR15 5.56×45 自动步枪");S.Ammo=TEXT("Ammo556");S.Capacity=FMath::Max(1,GetDefault<UGlobalData>()->RifleCapacity);S.Damage=GetDefault<UGlobalData>()->RifleBodyDamage;S.Range=12000;S.Interval=.095f;S.Reload=3.f;S.Recoil=.8f;S.HeadMultiplier=1.75f;S.bAutomatic=true;}
    if(Kind==ETacticalWeaponKind::Shotgun){S.Name=TEXT("Herrington 11-87 12铅径半自动霰弹枪");S.Ammo=TEXT("Ammo12ga");S.Capacity=FMath::Max(1,GetDefault<UGlobalData>()->ShotgunCapacity);S.Pellets=8;S.Damage=GetDefault<UGlobalData>()->BuckshotPelletDamage;S.Range=2500;S.Interval=bPumpMode?.85f:.32f;S.Reload=.916667f;S.Recoil=4;S.HipSpread=5;S.ADSSpread=3;S.HeadMultiplier=1;S.bShellReload=true;}
    return S;
}
ATacticalWeapon::ATacticalWeapon()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("Physics"));SetRootComponent(Body);Body->SetBoxExtent(FVector(30,3,9));Body->SetCollisionProfileName(TEXT("PhysicsActor"));Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Body->SetSimulatePhysics(true);
    WeaponVisual=CreateDefaultSubobject<USceneComponent>(TEXT("WeaponVisual"));WeaponVisual->SetupAttachment(Body);
    auto Part=[this](const TCHAR* Name){auto* P=CreateDefaultSubobject<UStaticMeshComponent>(Name);P->SetupAttachment(WeaponVisual);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->SetCanEverAffectNavigation(false);ModelParts.Add(P);return P;};
    ModelBody=Part(TEXT("ReferenceBody"));MagazineVisual=Part(TEXT("DetachableMagazine"));MovingAction=Part(TEXT("BoltOrSlide"));ChargeHandle=Part(TEXT("ChargingHandleOrPump"));Optic=Part(TEXT("Optics"));Barrel=Part(TEXT("Barrel"));ShellInHand=Part(TEXT("LoadingShell"));
    Hands=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hands"));Hands->SetupAttachment(WeaponVisual);Hands->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Combat/Viewmodel/SK_TacticalForearms")));Hands->SetCollisionEnabled(ECollisionEnabled::NoCollision);Hands->SetVisibility(false);Hands->SetCastShadow(false);Hands->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    RenderArms=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("ArticulatedArms"));RenderArms->SetupAttachment(WeaponVisual);RenderArms->SetSkinnedAssetAndUpdate(Hands->GetSkeletalMeshAsset());RenderArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);RenderArms->SetVisibility(false);RenderArms->SetCastShadow(false);RenderArms->SetBoundsScale(4);
    FlashMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlash"));FlashMesh->SetupAttachment(WeaponVisual);FlashMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere")));FlashMesh->SetRelativeScale3D(FVector(.12f,.025f,.025f));FlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);FlashMesh->SetVisibility(false);FlashMesh->SetCastShadow(false);
    FlashMesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Camcorder/Materials/M_CamcorderColor")));FlashMesh->SetDefaultCustomPrimitiveDataVector4(0,FVector4(1,.55f,.08f,1));
    FlashLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));FlashLight->SetupAttachment(WeaponVisual);FlashLight->SetIntensity(0);FlashLight->SetLightColor(FLinearColor(1,.65f,.2f));FlashLight->SetAttenuationRadius(250);FlashLight->SetCastShadows(false);
    WeaponLight=CreateDefaultSubobject<USpotLightComponent>(TEXT("TacticalLight"));WeaponLight->SetupAttachment(WeaponVisual);WeaponLight->SetIntensity(0);WeaponLight->SetAttenuationRadius(2500);WeaponLight->SetOuterConeAngle(24);WeaponLight->SetInnerConeAngle(14);WeaponLight->SetCastShadows(true);
    ScopeCapture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("OpticalCapture"));ScopeCapture->SetupAttachment(WeaponVisual);ScopeCapture->bCaptureEveryFrame=false;ScopeCapture->bCaptureOnMovement=false;ScopeCapture->bAlwaysPersistRenderingState=true;ScopeCapture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;ScopeCapture->ShowFlags.SetMotionBlur(false);ScopeCapture->ShowFlags.SetBloom(false);
    ScopeLens=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OpticalLens"));ScopeLens->SetupAttachment(WeaponVisual);ScopeLens->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane")));ScopeLens->SetRelativeLocation(FVector(-6.02f,0,8.4f));ScopeLens->SetRelativeRotation(FRotator(90,0,0));ScopeLens->SetRelativeScale3D(FVector(.036f));ScopeLens->SetCollisionEnabled(ECollisionEnabled::NoCollision);ScopeLens->SetCastShadow(false);
    PackWeaponMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PackWeaponMesh"));PackWeaponMesh->SetupAttachment(WeaponVisual);PackWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);PackWeaponMesh->SetCanEverAffectNavigation(false);PackWeaponMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    PackMagazineMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PackMagazineMesh"));PackMagazineMesh->SetupAttachment(PackWeaponMesh);PackMagazineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);PackMagazineMesh->SetCanEverAffectNavigation(false);
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));Label->SetupAttachment(Body);Label->SetRelativeLocation(FVector(0,0,21));Label->SetWorldSize(8);Label->SetHorizontalAlignment(EHTA_Center);
    for(const TCHAR* N:{TEXT("RifleFire"),TEXT("RifleMagIn"),TEXT("RifleMagOut"),TEXT("RifleCharge"),TEXT("PistolFire"),TEXT("PistolMagIn"),TEXT("PistolMagOut"),TEXT("PistolCharge"),TEXT("ShotgunFire"),TEXT("ShotgunShell"),TEXT("ShotgunCharge"),TEXT("Cloth"),TEXT("Equip"),TEXT("AimIn"),TEXT("AimOut")})Sounds.Add(N,LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/Combat/Audio/Handling/"))+N)));
}
void ATacticalWeapon::SetupModel()
{
    const bool P=Kind==ETacticalWeaponKind::Pistol,R=Kind==ETacticalWeaponKind::Rifle;
    ModelBody->SetStaticMesh(WeaponMotion::Mesh(P?TEXT("G19X_Frame"):R?TEXT("ADAR_Body"):TEXT("M3_Body")));
    MagazineVisual->SetStaticMesh(P?WeaponMotion::Mesh(TEXT("G19X_Magazine")):R?WeaponMotion::Mesh(TEXT("ADAR_Magazine")):nullptr);
    MovingAction->SetStaticMesh(WeaponMotion::Mesh(P?TEXT("G19X_Slide"):R?TEXT("ADAR_Bolt"):TEXT("M3_Bolt")));
    ChargeHandle->SetStaticMesh(P?nullptr:WeaponMotion::Mesh(R?TEXT("ADAR_ChargingHandle"):TEXT("M3_Pump")));
    Optic->SetStaticMesh(R?WeaponMotion::Mesh(TEXT("BRAVO4_PK06")):nullptr);Barrel->SetStaticMesh(P?WeaponMotion::Mesh(TEXT("G19X_Barrel")):nullptr);
    ShellInHand->SetStaticMesh(WeaponMotion::Mesh(TEXT("Shell12")));ShellInHand->SetVisibility(false);
    for(auto Part:ModelParts)Part->SetRelativeTransform(FTransform::Identity);
    Body->SetBoxExtent(P?FVector(10,2,7):FVector(48,3,12));
    Label->SetText(FText::FromString(Spec().Name));
    SetupPackPresentation();
    bMagnified=false;if(!bPackPresentation){ScopeLens->SetVisibility(false);ScopeLens->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/Reference/Materials/M_OpticView")));}
}
void ATacticalWeapon::OnConstruction(const FTransform& T){Super::OnConstruction(T);SetupModel();}
void ATacticalWeapon::BeginPlay()
{
    Super::BeginPlay();SetupModel();if(Magazine<0)Magazine=Spec().Capacity;Magazine=FMath::Clamp(Magazine,0,Spec().Capacity);InventoryId=GetFName();
    // Existing OFPA actors keep their serialized component overrides from the previous viewmodel.
    WeaponVisual->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform);WeaponVisual->SetRelativeTransform(FTransform::Identity);
    if(!Hands){Hands=NewObject<USkeletalMeshComponent>(this,TEXT("RecoveredPoseSource"));Hands->SetupAttachment(WeaponVisual);Hands->RegisterComponent();}
    Hands->AttachToComponent(WeaponVisual,FAttachmentTransformRules::KeepRelativeTransform);
    Hands->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Combat/Viewmodel/SK_TacticalForearms")));Hands->SetVisibility(false);Hands->SetCollisionEnabled(ECollisionEnabled::NoCollision);Hands->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    RenderArms->SetSkinnedAssetAndUpdate(Hands->GetSkeletalMeshAsset());Hands->SetDisablePostProcessBlueprint(true);SetBasePose();

}
void ATacticalWeapon::SetBasePose()
{
    if(bPackPresentation)
    {
        Hands->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/KINEMATION/TacticalShooterPack/Character/Operator/UE5/SKM_Operator_Mono")));
        Hands->SetAbsolute(true,true,true);Hands->SetWorldTransform(FTransform::Identity);
        SamplePackAnimation(Hands,PackAnimation(TEXT("Character"),TEXT("Idle")),0);
        RenderArms->SetSkinnedAssetAndUpdate(Hands->GetSkeletalMeshAsset());RenderArms->SetAbsolute(true,true,true);RenderArms->SetWorldTransform(FTransform::Identity);RenderArms->CopyPoseFromSkeletalComponent(Hands);
        PackBaseFrame=FTransform(FRotator(0,-90,0),FVector(8,-3,-155));
        const FTransform IdleGun=Hands->GetSocketTransform(TEXT("ik_hand_gun"),RTS_Component);
        PackAimFrame=PackBaseFrame;PackAimFrame.SetRotation(FRotator(0,-90,0).Quaternion()*IdleGun.GetRotation().Inverse());
        const FVector Aim=PackAimFrame.TransformPosition(IdleGun.TransformPosition(PackWeaponMesh->GetSocketTransform(TEXT("AimPoint"),RTS_Component).GetLocation()));
        PackAimFrame.AddToTranslation(FVector(Kind==ETacticalWeaponKind::Shotgun?-15.f:0.f,-Aim.Y,-Aim.Z));
        return;
    }
    Hands->SetRelativeTransform(FTransform::Identity);
    const TCHAR* Path=Kind==ETacticalWeaponKind::Pistol?TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS"):TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS");
    if(auto* A=LoadObject<UAnimSequence>(nullptr,Path)){Hands->PlayAnimation(A,false);Hands->SetPosition(0,false);Hands->TickAnimation(0,false);Hands->RefreshBoneTransforms();if(auto* I=Hands->GetSingleNodeInstance())I->SetPlaying(false);}
    const FTransform Grip=Hands->GetSocketTransform(TEXT("HandGrip_R"),RTS_Component);
    BaseHands=Grip.Inverse()*FTransform(FRotator(0,-90,0));
    const FTransform RH=Hands->GetSocketTransform(TEXT("hand_r"),RTS_Component),LH=Hands->GetSocketTransform(TEXT("hand_l"),RTS_Component);
    const FVector RP=(RH.GetLocation()+Hands->GetSocketTransform(TEXT("middle_01_r"),RTS_Component).GetLocation())*.5f;
    const FVector LP=(LH.GetLocation()+Hands->GetSocketTransform(TEXT("middle_01_l"),RTS_Component).GetLocation())*.5f;
    RightPalmOffset=RH.InverseTransformPosition(RP);LeftPalmOffset=LH.InverseTransformPosition(LP);
    const FVector RightGrip(-2,0,Kind==ETacticalWeaponKind::Pistol?-5.f:-6.f);
    BaseHands.AddToTranslation(RightGrip-BaseHands.TransformPosition(RP));
    BaseRightRotation=FQuat(FVector::RightVector,FMath::DegreesToRadians(GripPitch))*(RH*BaseHands).GetRotation();
    BaseLeftRotation=(LH*BaseHands).GetRotation();
    if(Kind==ETacticalWeaponKind::Pistol)BaseLeftRotation=FQuat(FVector::RightVector,FMath::DegreesToRadians(GripPitch))*BaseLeftRotation;
    Hands->SetRelativeTransform(BaseHands);RenderArms->SetRelativeTransform(BaseHands);RenderArms->CopyPoseFromSkeletalComponent(Hands);
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Camcorder/Materials/M_CamcorderColor"));
    for(int32 I=0;I<RenderArms->GetNumMaterials();++I)RenderArms->SetMaterial(I,Material);
    RenderArms->SetCustomPrimitiveDataVector4(0,FVector4(.07f,.08f,.09f,1));
}
void ATacticalWeapon::Equip(AGame_Jam_Clean_VersCharacter* C)
{
    if(!C)return;Holder=C;C->GetFirstPersonMesh()->SetAbsolute(true,true,true);bOriginalVisible=C->GetFirstPersonMesh()->IsVisible();C->GetFirstPersonMesh()->SetVisibility(true,false);Body->SetSimulatePhysics(false);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);SetActorHiddenInGame(false);bEquipped=true;C->GetFirstPersonMesh()->PrimaryComponentTick.TickGroup=TG_PostUpdateWork;C->GetFirstPersonMesh()->AddTickPrerequisiteActor(this);if(auto* Controller=C->GetController())if(auto* Equipment=Controller->FindComponentByClass<UCombatEquipmentComponent>())C->GetFirstPersonMesh()->AddTickPrerequisiteComponent(Equipment);RenderArms->SetVisibility(false);Label->SetVisibility(false);SetBasePose();BeginHandling(EWeaponHandling::Equip,bPackPresentation?1.4f:.5f);Sound(TEXT("Equip"),.45f);
}
void ATacticalWeapon::CancelHandling()
{
    if(PackActionAudio)PackActionAudio->Stop();PackActionAudio=nullptr;PackAudioAction.Empty();PackAudioTime=-1;
    // Any unseated remaining rounds return to the bag exactly once. Never refund committed ammunition.
    if(!bMagazineSeated&&Magazine>0&&ReloadInventory){ReloadInventory->Add(Spec().Ammo,TEXT("备用弹药"),TEXT("对应枪械弹药"),Magazine);Magazine=0;}
    bMagazineSeated=true;ReloadInventory=nullptr;bReloadEnd=false;Handling=EWeaponHandling::Idle;HandlingTime=0;HandlingDuration=0;AnimateParts();
}
void ATacticalWeapon::Stow()
{
    CancelHandling();if(auto* C=Holder.Get()){C->GetFirstPersonMesh()->SetVisibility(true,false);C->GetFirstPersonMesh()->SetAbsolute(false,false,false);C->GetFirstPersonMesh()->SetWorldTransform(C->GetMesh()->GetComponentTransform());}Holder.Reset();bEquipped=false;bNPCEquipped=false;RenderArms->SetVisibility(false);WeaponLight->SetIntensity(0);SetActorHiddenInGame(true);Body->SetSimulatePhysics(false);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ATacticalWeapon::Drop(FVector P,FRotator R){Stow();SetActorLocationAndRotation(P,R,false,nullptr,ETeleportType::TeleportPhysics);SetActorHiddenInGame(false);Label->SetVisibility(true);Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Body->SetSimulatePhysics(true);}
void ATacticalWeapon::BeginHandling(EWeaponHandling State,float Duration){Handling=State;HandlingTime=0;HandlingDuration=Duration;HandlingEvents=0;}
bool ATacticalWeapon::IsReloading()const{return Handling==EWeaponHandling::ShellStart||Handling==EWeaponHandling::MagazineReload||Handling==EWeaponHandling::ShellInsert||Handling==EWeaponHandling::Charge;}
bool ATacticalWeapon::CanFire()const{return (bEquipped||bNPCEquipped)&&bMagazineSeated&&(Handling==EWeaponHandling::Idle||Handling==EWeaponHandling::FireCycle)&&bChambered;}
float ATacticalWeapon::ReloadProgress()const{return IsReloading()?FMath::Clamp(HandlingTime/FMath::Max(.01f,HandlingDuration),0.f,1.f):0.f;}
bool ATacticalWeapon::StartReload(UBackpackInventory* Inventory)
{
    if(!Inventory||IsReloading()||Handling==EWeaponHandling::Equip||Handling==EWeaponHandling::Holster)return false;
    if(Magazine>=Spec().Capacity&&bChambered)return false;
    if(Inventory->Count(Spec().Ammo)<=0){if(!bChambered&&Magazine>0){ReloadInventory=Inventory;BeginHandling(EWeaponHandling::Charge,PackChargeDuration());return true;}return false;}
    ReloadInventory=Inventory;bReloadEnd=false;bPackEmptyReload=!bChambered;
    if(bNPCEquipped&&NPCReloadDuration>0){BeginHandling(Kind==ETacticalWeaponKind::Shotgun?EWeaponHandling::ShellInsert:EWeaponHandling::MagazineReload,NPCReloadDuration);Sound(TEXT("Cloth"),.3f);return true;}
    BeginHandling(Kind==ETacticalWeaponKind::Shotgun?EWeaponHandling::ShellStart:EWeaponHandling::MagazineReload,Kind==ETacticalWeaponKind::Shotgun?(bPackEmptyReload?2.666667f:.666667f):Spec().Reload);Sound(TEXT("Cloth"),.3f);return true;
}
void ATacticalWeapon::RequestReloadEnd(){if(Handling==EWeaponHandling::ShellInsert||Handling==EWeaponHandling::ShellStart)bReloadEnd=true;}
void ATacticalWeapon::Sound(FName Name,float Volume){if(bPackPresentation&&!bNPCEquipped&&(Name.ToString().Contains(TEXT("MagIn"))||Name.ToString().Contains(TEXT("MagOut"))||Name.ToString().Contains(TEXT("Charge"))||Name==TEXT("ShotgunShell")))return;if(auto* S=Sounds.FindRef(Name).Get())CharacterAudio::Attached(S,WeaponVisual,Name.ToString().EndsWith(TEXT("Fire"))?ECharacterSound::Gunshot:ECharacterSound::Handling,Volume);}
void ATacticalWeapon::EventAt(float Old,float Now,float At,int32 Bit,TFunctionRef<void()> Event){if(!(HandlingEvents&(1u<<Bit))&&Now>=At){HandlingEvents|=1u<<Bit;Event();}}
void ATacticalWeapon::DropMagazine()
{
    if(auto* Debris=GetWorld()->SpawnActor<AWeaponDebris>())
    {
        if(bPackPresentation&&PackMagazineMesh->GetSkeletalMeshAsset())Debris->InitializeSkeletal(PackMagazineMesh->GetSkeletalMeshAsset(),PackMagazineMesh->GetComponentTransform(),GetActorRotation().RotateVector(FVector(-10,-45,-70)));
        else Debris->Initialize(MagazineVisual->GetStaticMesh(),MagazineVisual->GetComponentTransform(),GetActorRotation().RotateVector(FVector(-10,-45,-70)),true);
    }
    // Preserve unused cartridges as backpack ammo; the discarded mesh is not a second inventory copy.
    if(ReloadInventory&&Magazine>0)ReloadInventory->Add(Spec().Ammo,TEXT("备用弹药"),TEXT("对应枪械弹药"),Magazine);
    Magazine=0;bMagazineSeated=false;MagazineVisual->SetVisibility(false);
}
void ATacticalWeapon::AdvanceHandling(float DT)
{
    const int32 OldMagazine=Magazine;const bool OldChamber=bChambered;
    // Consume all elapsed time across states, including frames that straddle several animation events.
    for(int32 Guard=0;Guard<16&&DT>0&&Handling!=EWeaponHandling::Idle;++Guard)
    {
        const auto State=Handling;const float Old=HandlingTime;const float Used=FMath::Min(DT,FMath::Max(0.f,HandlingDuration-HandlingTime));HandlingTime+=Used;DT-=Used;
        const float T=HandlingDuration>0?HandlingTime/HandlingDuration:1;
        const bool NPCAction=bNPCEquipped&&NPCReloadDuration>0&&(State==EWeaponHandling::MagazineReload||State==EWeaponHandling::ShellInsert);
        if(NPCAction)
        {
            if(State==EWeaponHandling::MagazineReload){
                EventAt(Old,T,.15f,0,[this]{bMagazineSeated=false;Sound(Kind==ETacticalWeaponKind::Rifle?TEXT("RifleMagOut"):TEXT("PistolMagOut"));});
                EventAt(Old,T,.28f,1,[this]{DropMagazine();});
                EventAt(Old,T,.68f,2,[this]{if(ReloadInventory){const int32 N=FMath::Min(Spec().Capacity,ReloadInventory->Count(Spec().Ammo));if(N>0){ReloadInventory->Remove(Spec().Ammo,N);Magazine=N;}}bMagazineSeated=true;Sound(Kind==ETacticalWeaponKind::Rifle?TEXT("RifleMagIn"):TEXT("PistolMagIn"));});
                EventAt(Old,T,.9f,3,[this]{if(!bChambered&&Magazine>0){--Magazine;bChambered=true;}Sound(Kind==ETacticalWeaponKind::Rifle?TEXT("RifleCharge"):TEXT("PistolCharge"));});
            }else{
                EventAt(Old,T,.65f,0,[this]{if(ReloadInventory&&ReloadInventory->Count(Spec().Ammo)>0){ReloadInventory->Remove(Spec().Ammo,1);if(!bChambered)bChambered=true;else if(Magazine<Spec().Capacity)++Magazine;Sound(TEXT("ShotgunShell"));}});
            }
        }
        else if(State==EWeaponHandling::MagazineReload)
        {
            EventAt(Old,T,(Kind==ETacticalWeaponKind::Rifle?.72f:.48f)/Spec().Reload,0,[this]{bMagazineSeated=false;Sound(Kind==ETacticalWeaponKind::Rifle?TEXT("RifleMagOut"):TEXT("PistolMagOut"));});
            EventAt(Old,T,(Kind==ETacticalWeaponKind::Rifle?.86f:.65f)/Spec().Reload,1,[this]{DropMagazine();});
            EventAt(Old,T,.44f,2,[this]{Sound(TEXT("Cloth"),.3f);});
            EventAt(Old,T,2.2f/Spec().Reload,3,[this]{if(ReloadInventory){const int32 N=FMath::Min(Spec().Capacity,ReloadInventory->Count(Spec().Ammo));if(N>0){ReloadInventory->Remove(Spec().Ammo,N);Magazine=N;}}bMagazineSeated=true;Sound(Kind==ETacticalWeaponKind::Rifle?TEXT("RifleMagIn"):TEXT("PistolMagIn"));});
        }
        if(State==EWeaponHandling::ShellStart&&bPackEmptyReload)
        {
            EventAt(Old,T,.62f,0,[this]{if(ReloadInventory&&ReloadInventory->Count(Spec().Ammo)>0){ReloadInventory->Remove(Spec().Ammo,1);bChambered=true;}});
        }
        if(State==EWeaponHandling::ShellInsert&&!NPCAction)
        {
            EventAt(Old,T,.70f,0,[this]{if(ReloadInventory&&Magazine<Spec().Capacity&&ReloadInventory->Count(Spec().Ammo)>0){ReloadInventory->Remove(Spec().Ammo,1);++Magazine;Sound(TEXT("ShotgunShell"));}});
        }
        if(State==EWeaponHandling::Equip)EventAt(Old,T,.68f,0,[this]{if(!bChambered&&Magazine>0){--Magazine;bChambered=true;}});
        if(State==EWeaponHandling::Charge)
        {
            EventAt(Old,T,.20f,0,[this]{Sound(Kind==ETacticalWeaponKind::Pistol?TEXT("PistolCharge"):Kind==ETacticalWeaponKind::Rifle?TEXT("RifleCharge"):TEXT("ShotgunCharge"));});
            EventAt(Old,T,.76f,1,[this]{if(!bChambered&&Magazine>0){--Magazine;bChambered=true;}});
        }
        if(State==EWeaponHandling::FireCycle&&Kind==ETacticalWeaponKind::Shotgun&&bPumpMode)
        {
            EventAt(Old,T,.25f,0,[this]{Sound(TEXT("ShotgunCharge"),.5f);});
            EventAt(Old,T,.8f,1,[this]{if(!bChambered&&Magazine>0){--Magazine;bChambered=true;}});
        }
        if(HandlingTime+KINDA_SMALL_NUMBER<HandlingDuration)break;
        if(NPCAction){
            if(State==EWeaponHandling::ShellInsert&&Magazine<Spec().Capacity&&ReloadInventory&&ReloadInventory->Count(Spec().Ammo)>0)BeginHandling(EWeaponHandling::ShellInsert,NPCReloadDuration);
            else {Handling=EWeaponHandling::Idle;HandlingTime=0;HandlingDuration=0;ReloadInventory=nullptr;}
        }
        else if(State==EWeaponHandling::Equip){Handling=EWeaponHandling::Idle;HandlingTime=0;HandlingDuration=0;}
        else if(State==EWeaponHandling::MagazineReload)BeginHandling(EWeaponHandling::Charge,PackChargeDuration());
        else if(State==EWeaponHandling::ShellStart){if(!bReloadEnd&&ReloadInventory&&Magazine<Spec().Capacity&&ReloadInventory->Count(Spec().Ammo)>0)BeginHandling(EWeaponHandling::ShellInsert,Spec().Reload);else BeginHandling(EWeaponHandling::Charge,PackChargeDuration());}
        else if(State==EWeaponHandling::ShellInsert&&!NPCAction)
        {
            if(!bReloadEnd&&ReloadInventory&&Magazine<Spec().Capacity&&ReloadInventory->Count(Spec().Ammo)>0)BeginHandling(EWeaponHandling::ShellInsert,Spec().Reload);
            else BeginHandling(EWeaponHandling::Charge,PackChargeDuration());
        }
        else if(State==EWeaponHandling::Holster){Stow();break;}
        else {Handling=EWeaponHandling::Idle;HandlingTime=0;HandlingDuration=0;ReloadInventory=nullptr;}
    }
    AnimateParts();
    if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()){if(OldMagazine!=Magazine)Bus->NotifyChange(this,TEXT("Magazine"),Magazine);if(OldChamber!=bChambered)Bus->NotifyChange(this,TEXT("Chambered"),bChambered?1.f:0.f);}
}
bool ATacticalWeapon::ConsumeShot()
{
    if(!CanFire())return false;bChambered=false;
    if(!(Kind==ETacticalWeaponKind::Shotgun&&bPumpMode)&&Magazine>0){--Magazine;bChambered=true;}
    BeginHandling(EWeaponHandling::FireCycle,Spec().Interval);if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()){Bus->NotifyChange(this,TEXT("Magazine"),Magazine);Bus->NotifyChange(this,TEXT("Chambered"),bChambered?1.f:0.f);}return true;
}
void ATacticalWeapon::EjectCase()
{
    const FTransform T(FRotator(0,90,0),FVector(Kind==ETacticalWeaponKind::Pistol?2:7,3,1));
    if(auto* D=GetWorld()->SpawnActor<AWeaponDebris>())D->Initialize(WeaponMotion::Mesh(Kind==ETacticalWeaponKind::Pistol?TEXT("Case9"):Kind==ETacticalWeaponKind::Rifle?TEXT("Case556"):TEXT("Shell12")),T*WeaponVisual->GetComponentTransform(),GetActorRotation().RotateVector(FVector(25,150,90)),false);
}
void ATacticalWeapon::Flash()
{
    ShotAge=0;bCaseEjected=false;FlashTime=.045f;FlashMesh->SetWorldLocation(MuzzleLocation());FlashLight->SetWorldLocation(MuzzleLocation());FlashMesh->SetVisibility(true);FlashLight->SetIntensity(4500);
    if(bPackPresentation)
    {
        const FString Name=Kind==ETacticalWeaponKind::Pistol?TEXT("NS_MuzzleFlash_Pistol"):Kind==ETacticalWeaponKind::Rifle?TEXT("NS_MuzzleFlash_Rifle_01"):TEXT("NS_MuzzleFlash_Shotgun_01");
        if(auto* FX=LoadObject<UNiagaraSystem>(nullptr,*(FString(TEXT("/Game/KINEMATION/Common/VFX/MuzzleFlashes/"))+Name)))
        {
            UNiagaraFunctionLibrary::SpawnSystemAttached(FX,PackWeaponMesh,NAME_None,PackWeaponMesh->GetComponentTransform().InverseTransformPosition(MuzzleLocation()),FRotator(0,90,0),EAttachLocation::KeepRelativeOffset,true);
            FlashMesh->SetVisibility(false);
        }
    }
    Sound(Kind==ETacticalWeaponKind::Pistol?TEXT("PistolFire"):Kind==ETacticalWeaponKind::Rifle?TEXT("RifleFire"):TEXT("ShotgunFire"),.7f);
}
void ATacticalWeapon::AnimateParts()
{
    if(bPackPresentation){for(auto Part:ModelParts)if(Part!=Optic)Part->SetVisibility(false);return;}
    using namespace WeaponMotion;
    const bool P=Kind==ETacticalWeaponKind::Pistol,R=Kind==ETacticalWeaponKind::Rifle;
    const float T=HandlingDuration>0?HandlingTime/HandlingDuration:0;
    const FVector Support=P?FVector(-3,-2.5f,-7.0f):R?FVector(19,-.5f,-4):FVector(21,0,-4);
    LeftPalmTarget=Support;RightPalmTarget=FVector(-3,1.7f,P?-6.5f:R?-8.5f:-7.f);LeftHandRotation=BaseLeftRotation;RightHandRotation=BaseRightRotation;
    MagazineVisual->SetRelativeTransform(FTransform::Identity);MagazineVisual->SetVisibility(!Spec().bShellReload&&bMagazineSeated);
    MovingAction->SetRelativeLocation(FVector::ZeroVector);ChargeHandle->SetRelativeLocation(FVector::ZeroVector);ShellInHand->SetVisibility(false);
    const FVector MagGrip=P?FVector(-3,-.6f,-10):FVector(7,-.8f,-12);
    const FVector Pouch(-17,-20,-34);
    if(Handling==EWeaponHandling::MagazineReload)
    {
        if(T<.14f)LeftPalmTarget=Blend(Support,MagGrip,T/.14f);
        else if(T<.29f){const FVector Pull=FVector(0,0,-12)*Smooth((T-.14f)/.15f);LeftPalmTarget=MagGrip+Pull;MagazineVisual->SetVisibility(true);MagazineVisual->SetRelativeLocation(Pull);}
        else if(T<.44f)LeftPalmTarget=Blend(MagGrip+FVector(0,0,-12),Pouch,(T-.29f)/.15f);
        else if(T<.69f){LeftPalmTarget=Blend(Pouch,MagGrip,(T-.44f)/.25f);MagazineVisual->SetVisibility(true);MagazineVisual->SetRelativeLocation(LeftPalmTarget-MagGrip);}
        else LeftPalmTarget=Blend(MagGrip,Support,(T-.69f)/.31f);
        // Palm turns to enclose the magazine, then turns over the action for charging.
        LeftHandRotation=FQuat(FVector::ForwardVector,FMath::DegreesToRadians(-35*Smooth(FMath::Min(T/.14f,(1-T)/.16f))))*BaseLeftRotation;
    }
    else if(Handling==EWeaponHandling::ShellInsert)
    {
        const FVector Port(7,-1.8f,-4.8f);
        if(T<.25f)LeftPalmTarget=Blend(Support,Pouch,T/.25f);
        else if(T<.7f)LeftPalmTarget=Blend(Pouch,Port,(T-.25f)/.45f);
        else LeftPalmTarget=Blend(Port,Support,(T-.7f)/.3f);
        LeftHandRotation=FQuat(FVector::ForwardVector,FMath::DegreesToRadians(-65*Smooth(T<.7f?T/.25f:(1-T)/.3f)))*BaseLeftRotation;
        ShellInHand->SetVisibility(T>.23f&&T<.70f);ShellInHand->SetRelativeLocation(LeftPalmTarget+FVector(1,0,.4f));
    }
    else if(Handling==EWeaponHandling::Charge)
    {
        const FVector Charge=P?FVector(-1,-2.8f,1.6f):R?FVector(-6,0,3.2f):FVector(27,0,-2.8f);
        const float Pull=T<.52f?Smooth((T-.2f)/.32f):1-Smooth((T-.52f)/.24f);
        const FVector Motion(-Pull*(P?3.f:R?7.f:8.f),0,0);
        if(R)
        {
            const FVector Grip=RightPalmTarget;
            RightPalmTarget=T<.2f?Blend(Grip,Charge,T/.2f):T<.76f?Charge+Motion:Blend(Charge,Grip,(T-.76f)/.24f);
            RightHandRotation=FQuat(FVector::ForwardVector,FMath::DegreesToRadians(25*Smooth(T<.2f?T/.2f:T>.76f?(1-T)/.24f:1)))*BaseRightRotation;
        }
        else LeftPalmTarget=T<.2f?Blend(Support,Charge,T/.2f):T<.76f?Charge+Motion:Blend(Charge,Support,(T-.76f)/.24f);
        if(P||R)MovingAction->SetRelativeLocation(Motion);
        if(!P)ChargeHandle->SetRelativeLocation(Motion);
        if(!P&&!R)MovingAction->SetRelativeLocation(Motion);
        LeftHandRotation=FQuat(FVector::ForwardVector,FMath::DegreesToRadians((P?-35.f:0)*Smooth(T<.2f?T/.2f:T>.76f?(1-T)/.24f:1)))*BaseLeftRotation;
    }
    // Bolt and slide recoil are separate from the whole-gun recoil spring.
    if(ShotAge<.12f&&Handling==EWeaponHandling::FireCycle&&!(Kind==ETacticalWeaponKind::Shotgun&&bPumpMode))MovingAction->SetRelativeLocation(FVector(-(P?3.f:6.f)*FMath::Sin(FMath::Clamp(ShotAge/.12f,0.f,1.f)*PI),0,0));
    if(P&&!bChambered&&Magazine==0&&!IsReloading())MovingAction->SetRelativeLocation(FVector(-3,0,0));
    if(Kind==ETacticalWeaponKind::Shotgun&&bPumpMode&&Handling==EWeaponHandling::FireCycle)
    {
        const float Pump=FMath::Sin(FMath::Clamp((T-.15f)/.7f,0.f,1.f)*PI);ChargeHandle->SetRelativeLocation(FVector(-8*Pump,0,0));MovingAction->SetRelativeLocation(FVector(-8*Pump,0,0));LeftPalmTarget=Support+FVector(-8*Pump,0,0);
    }
}
void ATacticalWeapon::SolveArm(bool Left,const FVector& Palm,const FQuat& Rotation)
{
    const FName Upper=Left?TEXT("upperarm_l"):TEXT("upperarm_r"),Lower=Left?TEXT("lowerarm_l"):TEXT("lowerarm_r"),Hand=Left?TEXT("hand_l"):TEXT("hand_r");
    const FTransform Frame=RenderArms->GetComponentTransform();
    FTransform A=Hands->GetSocketTransform(Upper,RTS_Component),B=Hands->GetSocketTransform(Lower,RTS_Component),H=Hands->GetSocketTransform(Hand,RTS_Component);
    const float L1=FVector::Distance(A.GetLocation(),B.GetLocation()),L2=FVector::Distance(B.GetLocation(),H.GetLocation());
    if(L1<1||L2<1)return;
    auto* C=Holder.Get();if(!C)return;auto* Camera=C->GetFirstPersonCameraComponent();const FRotator View=C->GetControlRotation();
    const FVector WristWorld=Palm-Rotation.RotateVector((Left?LeftPalmOffset:RightPalmOffset));
    // First-person forearms terminate behind/below the camera. Keep this anchor independent
    // of the weapon's reload tilt so the cropped elbow can never enter the image.
    const FVector ShoulderWorld=Camera->GetComponentLocation()+View.RotateVector(FVector(-28,Left?-28:24,-65));
    const FVector ElbowWorld=Camera->GetComponentLocation()+View.RotateVector(FVector(-8,Left?-25:25,-52));
    const FVector Shoulder=Frame.InverseTransformPosition(ShoulderWorld),Elbow=Frame.InverseTransformPosition(ElbowWorld),Wrist=Frame.InverseTransformPosition(WristWorld);
    const FQuat AR=FQuat::FindBetweenNormals((B.GetLocation()-A.GetLocation()).GetSafeNormal(),(Elbow-Shoulder).GetSafeNormal())*A.GetRotation();
    const FQuat BR=FQuat::FindBetweenNormals((H.GetLocation()-B.GetLocation()).GetSafeNormal(),(Wrist-Elbow).GetSafeNormal())*B.GetRotation();
    RenderArms->SetBoneTransformByName(Upper,FTransform(AR,Shoulder,FVector(.82f)),EBoneSpaces::ComponentSpace);
    RenderArms->SetBoneTransformByName(Lower,FTransform(BR,Elbow,FVector(FVector::Distance(Wrist,Elbow)/L2,.82f,.82f)),EBoneSpaces::ComponentSpace);
    RenderArms->SetBoneTransformByName(Hand,FTransform(Frame.GetRotation().Inverse()*Rotation,Wrist,FVector(1.f)),EBoneSpaces::ComponentSpace);
}
void ATacticalWeapon::CurlFingers(bool Left)
{
    const bool P=Kind==ETacticalWeaponKind::Pistol;
    const FString Side=Left?TEXT("_l"):TEXT("_r");
    const FTransform Gun=WeaponVisual->GetComponentTransform();
    for(int32 Finger=0;Finger<4;++Finger)
    {
        const TCHAR* Prefixes[]={TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky"),TEXT("thumb")};
        FName Bones[3];for(int32 I=0;I<3;++I)Bones[I]=FName(*(FString(Prefixes[Finger])+FString::Printf(TEXT("_%02d"),I+1)+Side));
        FVector Target;
        if(!Left&&Kind==ETacticalWeaponKind::Rifle&&Handling==EWeaponHandling::Charge)Target=RightPalmTarget+FVector((Finger-1.5f)*.7f,0,1.2f);
        else if(!Left)Target=Finger==0?FVector(1.3f,0,-3.0f):Finger==4?FVector(-1.4f,-1.8f,P?-2.5f:-3.5f):FVector(-.9f,-1.45f,(P?-5.6f:-6.3f)-(Finger-1)*1.45f);
        else if(P&&!IsReloading())Target=Finger==4?FVector(.5f,-1.8f,-1.8f):FVector(-.1f,1.1f,-5.5f-Finger*1.15f);
        else if(!IsReloading())Target=LeftPalmTarget+FVector((Finger-1.5f)*1.2f,Finger==4?-2.4f:1.6f,Finger==4?1.0f:2.5f);
        else Target=LeftPalmTarget+FVector((Finger-1.5f)*.8f,Finger==4?-1.6f:1.2f,1.1f);
        Target=Gun.TransformPosition(Target);
        // Hinge-only finger closure: never rotate a finger sideways to chase an unreachable target.
        const FVector Hinge=(RenderArms->GetBoneLocationByName(FName(*(FString(TEXT("index_01"))+Side)),EBoneSpaces::WorldSpace)-RenderArms->GetBoneLocationByName(FName(*(FString(TEXT("pinky_01"))+Side)),EBoneSpaces::WorldSpace)).GetSafeNormal();
        float Applied[3]={0,0,0};
        for(int32 Iter=0;Iter<8;++Iter)for(int32 Joint=2;Joint>=0;--Joint)
        {
            const FTransform TipBone=RenderArms->GetBoneTransformByName(Bones[2],EBoneSpaces::WorldSpace);
            // Manny's right finger bones point along -X; the left hand uses +X.
            const FVector Tip=TipBone.TransformPosition(FVector((Left?1.f:-1.f)*(Finger==4?2.f:2.5f),0,0));
            FTransform Bone=RenderArms->GetBoneTransformByName(Bones[Joint],EBoneSpaces::WorldSpace);
            const FVector A=(Tip-Bone.GetLocation()).GetSafeNormal(),B=(Target-Bone.GetLocation()).GetSafeNormal();
            if(A.IsNearlyZero()||B.IsNearlyZero())continue;
            const FVector AP=(A-Hinge*FVector::DotProduct(A,Hinge)).GetSafeNormal(),BP=(B-Hinge*FVector::DotProduct(B,Hinge)).GetSafeNormal();
            if(AP.IsNearlyZero()||BP.IsNearlyZero())continue;
            const float Angle=FMath::Atan2(FVector::DotProduct(Hinge,FVector::CrossProduct(AP,BP)),FVector::DotProduct(AP,BP));
            const float Next=FMath::Clamp(Applied[Joint]+FMath::Clamp(Angle,-.1f,.1f),-.9f,.9f);
            const FQuat Delta(Hinge,Next-Applied[Joint]);Applied[Joint]=Next;
            Bone.SetRotation((Delta*Bone.GetRotation()).GetNormalized());RenderArms->SetBoneTransformByName(Bones[Joint],Bone,EBoneSpaces::WorldSpace);
        }
    }
}
void ATacticalWeapon::PoseHands()
{
    if(!Holder.IsValid())return;
    Hands->SetRelativeTransform(BaseHands);RenderArms->SetRelativeTransform(BaseHands);RenderArms->CopyPoseFromSkeletalComponent(Hands);
    const FTransform W=WeaponVisual->GetComponentTransform();
    SolveArm(false,W.TransformPosition(RightPalmTarget),W.GetRotation()*RightHandRotation);
    SolveArm(true,W.TransformPosition(LeftPalmTarget),W.GetRotation()*LeftHandRotation);
    if(bFingerContactIK){CurlFingers(false);CurlFingers(true);}
    if(Handling==EWeaponHandling::Charge && Kind==ETacticalWeaponKind::Pistol){
        const float T=HandlingTime/FMath::Max(.01f,HandlingDuration);
        const float Curl=WeaponMotion::Smooth(T<.2f?T/.2f:T>.76f?(1-T)/.24f:1.f);
        for(const TCHAR* BoneName:{TEXT("thumb_01_l"),TEXT("thumb_02_l"),TEXT("thumb_03_l")}){
            FTransform Bone=RenderArms->GetBoneTransformByName(BoneName,EBoneSpaces::WorldSpace);
            Bone.SetRotation((Bone.GetRotation()*FRotator(25.f*Curl,0,0).Quaternion()).GetNormalized());
            RenderArms->SetBoneTransformByName(BoneName,Bone,EBoneSpaces::WorldSpace);
        }
    }
    RenderArms->RefreshBoneTransforms();
    if(Handling==EWeaponHandling::ShellInsert && ShellInHand->IsVisible())
    {
        // A shell sits between the thumb and index finger, rather than on the back of the hand.
        const FVector Pinch=(RenderArms->GetBoneLocationByName(TEXT("index_03_l"),EBoneSpaces::WorldSpace)+RenderArms->GetBoneLocationByName(TEXT("thumb_03_l"),EBoneSpaces::WorldSpace))*.5f;
        ShellInHand->SetWorldLocation(Pinch);
    }
}
float ATacticalWeapon::GripError(bool Left)const
{
    if(bPackPresentation&&Holder.IsValid())return FVector::Distance(Holder->GetFirstPersonMesh()->GetSocketLocation(Left?TEXT("hand_l"):TEXT("hand_r")),Hands->GetSocketLocation(Left?TEXT("hand_l"):TEXT("hand_r")));
    const FVector Palm=RenderArms->GetSocketTransform(Left?TEXT("hand_l"):TEXT("hand_r")).TransformPosition(Left?LeftPalmOffset:RightPalmOffset);
    return FVector::Distance(Palm,WeaponVisual->GetComponentTransform().TransformPosition(Left?LeftPalmTarget:RightPalmTarget));
}
FVector ATacticalWeapon::SightRear()const{if(bPackPresentation&&PackWeaponMesh)return WeaponVisual->GetComponentTransform().InverseTransformPosition(PackWeaponMesh->GetSocketLocation(TEXT("AimPoint")));return Kind==ETacticalWeaponKind::Pistol?FVector(-3.9f,0,2.9f):Kind==ETacticalWeaponKind::Shotgun?FVector(-4,0,6.1f):FVector(-6,0,8.4f);}
FVector ATacticalWeapon::SightFront()const{return SightRear()+FVector(Kind==ETacticalWeaponKind::Shotgun?65:Kind==ETacticalWeaponKind::Pistol?14.9f:15,0,0);}
float ATacticalWeapon::AimFOV()const{return 70.f;}
void ATacticalWeapon::UpdatePose(float DT,bool ADS,float Kick)
{
    if(bPackPresentation){UpdatePackPose(DT,ADS,Kick);return;}
    auto* C=Holder.Get();if(!C)return;const bool Aim=ADS&&!IsReloading()&&Handling!=EWeaponHandling::Equip&&Handling!=EWeaponHandling::Holster;
    if(Aim!=bLastADS){Sound(Aim?TEXT("AimIn"):TEXT("AimOut"),.25f);bLastADS=Aim;}
    // Keep the current fore/aft hold distance through reload and charging, including
    // reloads entered from ADS. Returning to hip pose here used to push the gun away.
    if(!IsReloading())PoseAlpha=FMath::FInterpTo(PoseAlpha,Aim?1.f:0.f,DT,15);
    // Recover a usable hip-fire pose during a sprint shot without interrupting locomotion.
    const bool Sprint=C->bSprinting&&!Aim&&!IsReloading()&&Handling==EWeaponHandling::Idle&&ShotAge>.18f;
    SprintAlpha=FMath::FInterpTo(SprintAlpha,Sprint?1.f:0.f,DT,Sprint?8.f:16.f);
    SwayTime+=DT*FMath::Lerp(8.f,12.f,SprintAlpha);
    const FVector Sight=SightRear();const float EyeRelief=Kind==ETacticalWeaponKind::Pistol?42:Kind==ETacticalWeaponKind::Shotgun?24:30;
    const FVector AimOffset(EyeRelief-Sight.X,-Sight.Y,-Sight.Z);
    FVector Offset=FMath::Lerp(Kind==ETacticalWeaponKind::Pistol?FVector(40,11,-14):FVector(29,13,-17),AimOffset,PoseAlpha);
    const float Motion=FMath::Clamp(C->GetVelocity().Size2D()/650.f,0.f,1.f)*(1-PoseAlpha*.85f);
    Offset+=FVector(-Kick*.9f,FMath::Cos(SwayTime*.5f)*Motion*.3f,FMath::Sin(SwayTime)*Motion*.35f);
    Offset+=FVector(-5,-2,-5)*SprintAlpha;
    Offset.Y+=FMath::Sin(SwayTime)*Motion*SprintAlpha*1.1f;
    Offset.Z+=FMath::Cos(SwayTime*2)*Motion*SprintAlpha*.7f;
    FRotator Local(Kick*.65f-18*SprintAlpha,6*SprintAlpha,-9*SprintAlpha);
    if(Handling==EWeaponHandling::Equip||Handling==EWeaponHandling::Holster){const float A=Handling==EWeaponHandling::Equip?1-WeaponMotion::Smooth(HandlingTime/HandlingDuration):WeaponMotion::Smooth(HandlingTime/HandlingDuration);Offset.Z-=20*A;Local.Pitch-=40*A;}
    const FRotator View=C->GetControlRotation();
    const FVector Eye=C->GetFirstPersonCameraComponent()->GetComponentLocation();
    const FQuat GunRotation=View.Quaternion()*Local.Quaternion();
    // Keep the support wrist within the original body's anatomical reach, along
    // the sight line, rather than stretching the arm or letting go of the gun.
    // Reserve both resting grips throughout the hand-off; no depth jump on state changes.
    auto* Original=C->GetMesh();
    for(int32 Iter=0;Iter<24;++Iter)
    {
        bool Reachable=true;float PullBack=0;
        for(int32 Side=0;Side<2;++Side)
        {
            const bool Left=Side==0;
            const FVector A=Original->GetSocketLocation(Left?TEXT("upperarm_l"):TEXT("upperarm_r"));
            const FVector B=Original->GetSocketLocation(Left?TEXT("lowerarm_l"):TEXT("lowerarm_r"));
            const FVector H=Original->GetSocketLocation(Left?TEXT("hand_l"):TEXT("hand_r"));
            const FVector Palm=Left?(Kind==ETacticalWeaponKind::Pistol?FVector(-3,-2.5f,-7):Kind==ETacticalWeaponKind::Rifle?FVector(19,-.5f,-4):FVector(21,0,-4)):FVector(-3,1.7f,Kind==ETacticalWeaponKind::Pistol?-6.5f:Kind==ETacticalWeaponKind::Rifle?-8.5f:-7);
            const FQuat HandRotation=GunRotation*(Left?BaseLeftRotation:BaseRightRotation);
            const FVector Wrist=Eye+View.RotateVector(Offset)+GunRotation.RotateVector(Palm)-HandRotation.RotateVector(Left?LeftPalmOffset:RightPalmOffset);
            const float Reach=FVector::Distance(A,B)+FVector::Distance(B,H)-1.f;
            const FVector Delta=Wrist-A;
            if(Delta.Size()>Reach){Reachable=false;const float Along=FVector::DotProduct(Delta,View.Vector());const float Disc=Along*Along-(Delta.SizeSquared()-Reach*Reach);PullBack=FMath::Max(PullBack,Along-FMath::Sqrt(FMath::Max(0.f,Disc)));}
        }
        if(Reachable||Offset.X<=12.f)break;
        Offset.X=FMath::Max(12.f,Offset.X-FMath::Max(.01f,PullBack));
    }
    SetActorLocationAndRotation(Eye+View.RotateVector(Offset),GunRotation.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
    AnimateParts();PoseHands();WeaponLight->SetRelativeLocation(FVector(Kind==ETacticalWeaponKind::Pistol?12:50,0,-2.8f));WeaponLight->SetIntensity(bLightOn?20000:0);

}
FVector ATacticalWeapon::MuzzleLocation()const{if(bPackPresentation&&PackWeaponMesh&&PackWeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))return PackWeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"));if(bPackPresentation&&PackWeaponMesh)return PackWeaponMesh->GetComponentTransform().TransformPosition(FVector(0,Kind==ETacticalWeaponKind::Pistol?15.74f:Kind==ETacticalWeaponKind::Rifle?53.15f:61.35f,Kind==ETacticalWeaponKind::Pistol?1.86f:2.22f));return WeaponVisual->GetComponentTransform().TransformPosition(FVector(Kind==ETacticalWeaponKind::Pistol?13:Kind==ETacticalWeaponKind::Rifle?61:65,0,1));}
void ATacticalWeapon::ToggleSightOrMode(){if(bPackPresentation)return;if(IsReloading())return;if(Kind!=ETacticalWeaponKind::Shotgun)return;bPumpMode=!bPumpMode;Sound(TEXT("Cloth"),.25f);}
void ATacticalWeapon::ToggleLight(){if(Kind!=ETacticalWeaponKind::Shotgun)bLightOn=!bLightOn;}
void ATacticalWeapon::Holster(){CancelHandling();BeginHandling(EWeaponHandling::Holster,bPackPresentation&&PackAnimation(TEXT("Character"),TEXT("Holster"))?PackAnimation(TEXT("Character"),TEXT("Holster"))->GetPlayLength():.3f);Sound(TEXT("Cloth"),.25f);}
FString ATacticalWeapon::CurrentAnimationName()const{return StaticEnum<EWeaponHandling>()->GetNameStringByValue(int64(Handling));}
void ATacticalWeapon::Tick(float DT)
{
    Super::Tick(DT);FlashTime-=DT;ShotAge+=DT;if(FlashTime<=0){FlashMesh->SetVisibility(false);FlashLight->SetIntensity(0);}
    if(!bCaseEjected&&ShotAge>=(Kind==ETacticalWeaponKind::Shotgun&&bPumpMode?.3f:.03f)){bCaseEjected=true;if(ShotAge<1.5f)EjectCase();}
    if(Handling==EWeaponHandling::Holster){AdvanceHandling(DT);UpdatePose(DT,false,0);}
}

AWeaponDebris::AWeaponDebris()
{
    PrimaryActorTick.bCanEverTick=true;Collision=CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));SetRootComponent(Collision);Collision->SetCollisionProfileName(TEXT("PhysicsActor"));Collision->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Collision->SetNotifyRigidBodyCollision(true);Collision->OnComponentHit.AddDynamic(this,&AWeaponDebris::OnContact);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));Visual->SetupAttachment(Collision);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCanEverAffectNavigation(false);
}
void AWeaponDebris::Initialize(UStaticMesh* Mesh,const FTransform& T,FVector Velocity,bool Magazine)
{
    if(!Mesh){Destroy();return;}bMag=Magazine;Visual->SetStaticMesh(Mesh);const FBox Box=Mesh->GetBoundingBox();Visual->SetRelativeLocation(-Box.GetCenter());Collision->SetBoxExtent(Box.GetExtent().ComponentMax(FVector(.15f)));SetActorTransform(FTransform(T.GetRotation(),T.TransformPosition(Box.GetCenter()),T.GetScale3D()));Collision->SetSimulatePhysics(true);Collision->SetPhysicsLinearVelocity(Velocity);Collision->SetPhysicsAngularVelocityInDegrees(FVector(90,160,70));SetLifeSpan(120);
}
void AWeaponDebris::OnContact(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,FVector,const FHitResult& Hit)
{
    if(bLanded||!Other||Cast<AWeaponDebris>(Other)||Hit.ImpactNormal.Z<.45f)return;bLanded=true;LandedAt=GetWorld()->GetTimeSeconds();SetLifeSpan(bMag?30:12);
    if(auto* S=LoadObject<USoundBase>(nullptr,bMag?TEXT("/Game/Combat/Audio/Handling/MagImpact"):TEXT("/Game/Combat/Audio/Handling/ShellImpact")))CharacterAudio::At(this,S,Hit.ImpactPoint,ECharacterSound::Impact,bMag?.55f:.2f);
}
void AWeaponDebris::Tick(float DT){Super::Tick(DT);if(GetActorLocation().Z<-10000)Destroy();}

FVector ATacticalWeapon::MuzzleDirection() const
{
 // Pack guns are authored along local +Y; the fallback gun uses local +X.
 return bPackPresentation&&PackWeaponMesh
  ?PackWeaponMesh->GetComponentTransform().TransformVectorNoScale(FVector::RightVector).GetSafeNormal()
  :WeaponVisual->GetComponentTransform().TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
}
