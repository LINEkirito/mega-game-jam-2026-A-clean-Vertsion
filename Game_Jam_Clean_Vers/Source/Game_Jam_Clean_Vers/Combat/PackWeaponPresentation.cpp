#include "TacticalWeapon.h"
#include "Audio/CharacterAudio.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

namespace PackPresentation
{
const TCHAR* Root=TEXT("/Game/KINEMATION/TacticalShooterPack/");
FString Type(ETacticalWeaponKind K){return K==ETacticalWeaponKind::Rifle?TEXT("TR15"):K==ETacticalWeaponKind::Pistol?TEXT("WK-11_Viper"):TEXT("Herrington_11-87");}
}
UAnimSequence* ATacticalWeapon::PackAnimation(const FString& Group,const FString& Action)
{
    const FString Type=PackPresentation::Type(Kind);
    FString Suffix=Action;
    if(Action==TEXT("Idle")&&Group==TEXT("Character"))Suffix=Kind==ETacticalWeaponKind::Rifle?TEXT("Idle_Pose_Non_Grip"):Kind==ETacticalWeaponKind::Pistol?TEXT("Idle_Pose"):TEXT("Idle");
    FString Path=FString(PackPresentation::Root)+TEXT("Animations/UE5/")+Type+TEXT("/")+Group+TEXT("/A_")+(Group==TEXT("Character")?TEXT("FP_"):TEXT("W_"))+Type+TEXT("_")+Suffix;
    auto& Cached=PackAnimations.FindOrAdd(Path);if(!Cached)Cached=LoadObject<UAnimSequence>(nullptr,*Path);return Cached;
}
float ATacticalWeapon::PackChargeDuration()
{
    if(!bPackPresentation)return .85f;
    const bool Shell=Kind==ETacticalWeaponKind::Shotgun;
    auto* Animation=PackAnimation(TEXT("Character"),Shell?TEXT("Reload_End"):TEXT("Reload_Empty"));
    const float Start=Shell?0.f:Spec().Reload;
    return Animation?FMath::Max(.01f,Animation->GetPlayLength()-Start):.85f;
}
void ATacticalWeapon::SamplePackAnimation(USkeletalMeshComponent* Mesh,UAnimSequence* Animation,float Time)
{
    if(!Mesh||!Animation)return;
    if(!Mesh->GetSingleNodeInstance()||Mesh->GetSingleNodeInstance()->GetCurrentAsset()!=Animation)Mesh->PlayAnimation(Animation,false);
    Mesh->SetPosition(FMath::Clamp(Time,0.f,Animation->GetPlayLength()),false);
    Mesh->TickAnimation(0,false);Mesh->RefreshBoneTransforms();
    if(auto* Node=Mesh->GetSingleNodeInstance())Node->SetPlaying(false);
}
void ATacticalWeapon::SetupPackPresentation()
{
    if(!PackWeaponMesh||!PackMagazineMesh)return;
    const FString Type=PackPresentation::Type(Kind);
    const FString MeshName=Kind==ETacticalWeaponKind::Rifle?TEXT("SKM_TR15_Body"):Kind==ETacticalWeaponKind::Pistol?TEXT("SKM_WK-11_Viper_Body"):TEXT("SKM_Herrington_11-87");
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(FString(PackPresentation::Root)+TEXT("Weapons/")+Type+TEXT("/")+MeshName));
    bPackPresentation=Mesh!=nullptr;if(!bPackPresentation)return;
    if(Kind==ETacticalWeaponKind::Shotgun)bPumpMode=false;
    PackWeaponMesh->SetSkeletalMeshAsset(Mesh);PackWeaponMesh->SetRelativeRotation(FRotator(0,-90,0));PackWeaponMesh->SetDisablePostProcessBlueprint(true);
    SamplePackAnimation(PackWeaponMesh,PackAnimation(TEXT("Weapon"),TEXT("Idle")),0);
    if(Kind==ETacticalWeaponKind::Shotgun)PackWeaponMesh->HideBoneByName(TEXT("Suppressor"),EPhysBodyOp::PBO_None);
    for(auto Part:ModelParts)Part->SetVisibility(false);
    ScopeLens->SetVisibility(false);
    const bool Rifle=Kind==ETacticalWeaponKind::Rifle,Pistol=Kind==ETacticalWeaponKind::Pistol;
    const FString MagPath=Rifle?TEXT("Weapons/TR15/Attachments/Magazine/SKM_Mag_TR15"):Pistol?TEXT("Weapons/WK-11_Viper/Magazine/SKM_Mag_WK-11_Viper"):TEXT("");
    PackMagazineMesh->SetSkeletalMeshAsset(MagPath.IsEmpty()?nullptr:LoadObject<USkeletalMesh>(nullptr,*(FString(PackPresentation::Root)+MagPath)));
    PackMagazineMesh->AttachToComponent(PackWeaponMesh,FAttachmentTransformRules::SnapToTargetNotIncludingScale,Rifle?TEXT("Magazine"):TEXT("MagazineSocket"));
    PackMagazineMesh->SetRelativeRotation(Rifle?FRotator(0,0,90):FRotator::ZeroRotator);
    PackMagazineMesh->SetVisibility(Rifle||Pistol);
    // The author's rifle has a dedicated optic attachment; pistol/shotgun retain their native sights.
    if(Rifle)
    {
        Optic->AttachToComponent(PackWeaponMesh,FAttachmentTransformRules::SnapToTargetNotIncludingScale,TEXT("OpticSocket"));
        auto* Holo=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/TR15/Attachments/Holosight/SM_Attach_AR15_XPS2"));
        if(Holo){Optic->SetStaticMesh(Holo);Optic->EmptyOverrideMaterials();Optic->SetVisibility(true);}
        // Reuse the publisher reticle; adapt its plane to the weapon socket orientation.
        ScopeLens->AttachToComponent(Optic,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        ScopeLens->SetRelativeLocation(FVector(0,-.041703f,3.881461f));
        ScopeLens->SetRelativeRotation(FRotator(0,180,90));ScopeLens->SetRelativeScale3D(FVector(.03f,.03f,.01f));
        ScopeLens->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/TR15/Attachments/Holosight/Materials/MI_XPS2_Reticle")));ScopeLens->SetVisibility(true);
    }
    const FString Fire=Rifle?TEXT("SFX/TR15/Semi/SC_TR15_Fire_Unsuppressed_Semi"):Pistol?TEXT("SFX/WK-11_Viper/S_WK-11_Viper_Shot_1_With_Tail_Cue"):TEXT("SFX/Herrington_11-87/Fire/SC_Herrington_11-87_Semi_Unsuppressed");
    Sounds.Add(Rifle?TEXT("RifleFire"):Pistol?TEXT("PistolFire"):TEXT("ShotgunFire"),LoadObject<USoundBase>(nullptr,*(FString(PackPresentation::Root)+Fire)));
}
void ATacticalWeapon::UpdatePackPose(float DT,bool ADS,float Kick)
{
    auto* C=Holder.Get();if(!C||!Hands||!PackWeaponMesh)return;
    const bool R=Kind==ETacticalWeaponKind::Rifle,P=Kind==ETacticalWeaponKind::Pistol;
    FString Action=TEXT("Idle");float Time=0;
    const float Progress=FMath::Clamp(HandlingTime/FMath::Max(.001f,HandlingDuration),0.f,1.f);
    if(Handling==EWeaponHandling::Equip||Handling==EWeaponHandling::Holster){Action=Handling==EWeaponHandling::Equip?TEXT("Draw"):TEXT("Holster");if(auto* A=PackAnimation(TEXT("Character"),Action))Time=Progress*A->GetPlayLength();}
    else if(Handling==EWeaponHandling::MagazineReload){Action=TEXT("Reload_Empty");Time=Progress*(R?3.f:2.6f);}
    else if(Handling==EWeaponHandling::ShellStart){Action=bPackEmptyReload?TEXT("Reload_Empty_Start"):TEXT("Reload_Tac_Start");Time=HandlingTime;}
    else if(Handling==EWeaponHandling::ShellInsert){Action=TEXT("Reload_Loop");if(auto* A=PackAnimation(TEXT("Character"),Action))Time=Progress*A->GetPlayLength();}
    else if(Handling==EWeaponHandling::Charge)
    {
        // Continue the publisher's empty reload without synthesizing a separate hand pose.
        Action=(R||P)?TEXT("Reload_Empty"):TEXT("Reload_End");
        Time=(R||P?Spec().Reload:0.f)+HandlingTime;
    }
    // The publisher supplies complete, time-aligned action recordings rather than notify sounds.
    const bool NewAudio=Action!=PackAudioAction||Time<PackAudioTime-.01f;
    if(NewAudio)
    {
        if(PackActionAudio)PackActionAudio->Stop();PackActionAudio=nullptr;
        if(Action!=TEXT("Idle")&&Action!=TEXT("Draw")&&Action!=TEXT("Holster"))
        {
            FString Suffix=Action;
            if(P&&Action==TEXT("Reload_Empty"))Suffix=TEXT("Empty");
            if(!R&&!P){if(Action==TEXT("Reload_Empty_Start"))Suffix=TEXT("Start_Empty");if(Action==TEXT("Reload_Tac_Start"))Suffix=TEXT("Start_Normal");if(Action==TEXT("Reload_End"))Suffix=TEXT("End");}
            const FString Type=PackPresentation::Type(Kind);
            const FString Path=FString(PackPresentation::Root)+TEXT("SFX/")+Type+(P?TEXT("/"):TEXT("/Actions/"))+TEXT("S_")+Type+TEXT("_")+Suffix;
            if(auto* Sound=LoadObject<USoundBase>(nullptr,*Path)){PackActionAudio=CharacterAudio::Attached(Sound,WeaponVisual,ECharacterSound::Handling,.65f,1.f,Time);++PackSoundsPlayed;}
        }
    }
    PackAudioAction=Action;PackAudioTime=Time;
    const bool MagVisible=!(Handling==EWeaponHandling::MagazineReload&&Time>(R?.86f:.65f)&&Time<(R?1.1f:1.2f));
    PackMagazineMesh->SetVisibility((R||P)&&MagVisible);
    // Both skeletal streams sample the same authored time, including the weapon hand-off.
    SamplePackAnimation(Hands,PackAnimation(TEXT("Character"),Action),Time);
    SamplePackAnimation(PackWeaponMesh,PackAnimation(TEXT("Weapon"),Handling==EWeaponHandling::FireCycle?TEXT("Fire"):Action),Handling==EWeaponHandling::FireCycle?HandlingTime:Time);
    const bool Aim=ADS&&!IsReloading()&&(Handling==EWeaponHandling::Idle||Handling==EWeaponHandling::FireCycle);
    PoseAlpha=FMath::FInterpTo(PoseAlpha,Aim?1.f:0.f,DT,14);
    SprintAlpha=FMath::FInterpTo(SprintAlpha,C->bSprinting&&!Aim&&!IsReloading()?1.f:0.f,DT,10);
    SwayTime+=DT*FMath::Lerp(8.f,12.f,SprintAlpha);
    const float Motion=FMath::Clamp(C->GetVelocity().Size2D()/650.f,0.f,1.f);
    const FTransform GunLocal=Hands->GetSocketTransform(TEXT("ik_hand_gun"),RTS_Component);
    FTransform Frame;Frame.Blend(PackBaseFrame,PackAimFrame,PoseAlpha);
    const FVector Bob(-Kick*.7f,FMath::Sin(SwayTime)*Motion*.35f,FMath::Sin(SwayTime*2)*Motion*.3f);
    const FTransform ViewMotion(FRotator(Kick*.6f-15*SprintAlpha,4*SprintAlpha,-12*SprintAlpha),Bob+FVector(-3,0,-3)*SprintAlpha);
    const FTransform View(C->GetControlRotation(),C->GetFirstPersonCameraComponent()->GetComponentLocation());
    Frame=Frame*ViewMotion*View;
    // Move along the sight line to keep both authored wrists within anatomical
    // reach. This preserves the optic alignment and the weapon/hand animation.
    const FVector Forward=C->GetControlRotation().Vector();
    float PullBack=0.f;
    // Preserve the solved crouch torso, transformed into the current body frame
    // so a yaw change cannot leave the reach calculation in last frame's world pose.
    auto* FullBody=C->GetFirstPersonMesh();
    const FTransform BodyFrame=C->GetMesh()->GetComponentTransform();
    auto BodyBone=[&](FName Bone){return BodyFrame.TransformPosition(FullBody->GetSocketTransform(Bone,RTS_Component).GetLocation());};
    for(int32 Side=0;Side<2;++Side)
    {
        const FName Upper=Side==0?TEXT("upperarm_l"):TEXT("upperarm_r");
        const FName Lower=Side==0?TEXT("lowerarm_l"):TEXT("lowerarm_r");
        const FName Hand=Side==0?TEXT("hand_l"):TEXT("hand_r");
        const FVector Shoulder=BodyBone(Upper);
        const float Reach=FVector::Distance(Shoulder,BodyBone(Lower))+FVector::Distance(BodyBone(Lower),BodyBone(Hand))-.5f;
        const FVector Target=Frame.TransformPosition(Hands->GetSocketTransform(Hand,RTS_Component).GetLocation());
        const FVector D=Target-Shoulder;
        const float Along=FVector::DotProduct(D,Forward);
        const float PerpendicularSquared=FMath::Max(0.f,D.SizeSquared()-Along*Along);
        if(Reach>1.f && PerpendicularSquared<Reach*Reach)
            PullBack=FMath::Max(PullBack,Along-FMath::Sqrt(Reach*Reach-PerpendicularSquared));
    }
    Frame.AddToTranslation(-Forward*PullBack);
    Hands->SetWorldTransform(Frame);
    const FTransform GunWorld=GunLocal*Frame;
    SetActorTransform(FTransform(FRotator(0,90,0))*GunWorld,false,nullptr,ETeleportType::TeleportPhysics);
    RenderArms->SetWorldTransform(Frame);RenderArms->CopyPoseFromSkeletalComponent(Hands);
    // The full body remains rooted to the capsule; only the authored weapon rig follows aim.
    C->GetFirstPersonMesh()->SetWorldTransform(C->GetMesh()->GetComponentTransform());
    C->GetFirstPersonMesh()->TickAnimation(0,false);C->GetFirstPersonMesh()->RefreshBoneTransforms();
    Hands->SetVisibility(false);RenderArms->SetVisibility(false);
    WeaponLight->SetRelativeLocation(FVector(R?50:P?14:60,0,-2));WeaponLight->SetIntensity(bLightOn?20000:0);
}

void AWeaponDebris::InitializeSkeletal(USkeletalMesh* Mesh,const FTransform& T,FVector Velocity)
{
    if(!Mesh){Destroy();return;}bMag=true;
    SkeletalVisual=NewObject<USkeletalMeshComponent>(this,TEXT("MagazineProp"));SkeletalVisual->SetupAttachment(Collision);SkeletalVisual->RegisterComponent();SkeletalVisual->SetSkeletalMeshAsset(Mesh);SkeletalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);SkeletalVisual->SetCanEverAffectNavigation(false);
    const auto Bounds=Mesh->GetBounds();SkeletalVisual->SetRelativeLocation(-Bounds.Origin);Collision->SetBoxExtent(Bounds.BoxExtent.ComponentMax(FVector(.2f)));SetActorTransform(FTransform(T.GetRotation(),T.TransformPosition(Bounds.Origin),T.GetScale3D()));Collision->SetSimulatePhysics(true);Collision->SetPhysicsLinearVelocity(Velocity);Collision->SetPhysicsAngularVelocityInDegrees(FVector(90,160,70));SetLifeSpan(120);
}

