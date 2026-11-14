#include "ShoulderCamcorder.h"
#include "Audio/GameAudio.h"
#include "Audio/CharacterAudio.h"
#include "RecordingLibrary.h"
#include "Data/GlobalData.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Data/DataTransmitter.h"
#include "CamcorderVideoWriter.h"
#include "CamcorderFrameReadback.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "TextureResource.h"

AShoulderCamcorder::AShoulderCamcorder()
{
    FullBatterySeconds=GetDefault<UGlobalData>()->BatteryDuration;
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("CameraBodyPhysics"));
    SetRootComponent(Body); Body->SetBoxExtent(FVector(21,10,12));
    Body->SetCollisionProfileName(TEXT("PhysicsActor"));
    Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    Body->SetSimulatePhysics(true); Body->SetLinearDamping(.8f); Body->SetAngularDamping(2.f);
    AimPivot=CreateDefaultSubobject<USceneComponent>(TEXT("AimPivot")); AimPivot->SetupAttachment(Body);
    // Authored camera/hand pair from Functional FPS Equipment. Keep the arms hidden:
    // UnifiedBodyAnimInstance consumes these wrist/finger transforms on the player's mesh.
    HoldingHands=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HoldingHands"));
    HoldingHands->SetupAttachment(AimPivot);
    HoldingHands->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Equipment/Demo/FirstPerson/Character/Mesh/SK_Mannequin_Arms")));
    HoldingHands->SetRelativeLocation(FVector(-4.f,0.f,-151.327179f));
    HoldingHands->SetRelativeRotation(FRotator(0,-90,0));
    HoldStart=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Equipment/Camcorder/Animations/A_Camcorder_Hold_Start"));
    HoldIdle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Equipment/Camcorder/Animations/A_Camcorder_Hold_Idle"));
    LookTransition=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Equipment/Camcorder/Animations/A_Camcorder_LookAtDisplay"));
    LookIdle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Equipment/Camcorder/Animations/A_Camcorder_LookAtDisplay_Idle"));
    HoldingHands->OverrideAnimationData(HoldIdle,false,false,0.f);
    HoldingHands->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HoldingHands->SetCastShadow(false);
    HoldingHands->SetVisibility(false);
    HoldingHands->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    HoldingHands->SetComponentTickEnabled(false); // Explicit sampling keeps the camera and wrists in one pose.

    CameraMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FabCamcorder"));
    CameraMesh->SetupAttachment(AimPivot);
    CameraMesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Equipment/Camcorder/Meshes/SK_Camcorder")));
    CameraMesh->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,TEXT("/Game/Equipment/Camcorder/Blueprints/SK_Camcorder_AnimBlueprint.SK_Camcorder_AnimBlueprint_C")));
    CameraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CameraMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    CameraMesh->SetCastShadow(false);

    DisplayScreen=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FabDisplay"));
    DisplayScreen->SetupAttachment(CameraMesh);
    DisplayScreen->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    DisplayScreen->SetRelativeLocation(FVector(9.807905f,4.064387f,.202412f));
    DisplayScreen->SetRelativeRotation(FRotator(0,-5.956358f,0));
    DisplayScreen->SetRelativeScale3D(FVector(.1f,.01f,.068f));
    DisplayScreen->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Camcorder/Imported/M_FabCameraDisplay")));
    DisplayScreen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayScreen->SetCastShadow(false);
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RecordingLens")); Capture->SetupAttachment(CameraMesh);
    Capture->SetRelativeLocation(FVector(1.38212f,15.804039f,0));
    Capture->SetRelativeRotation(FRotator(0,85,0));
    Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
    Capture->bAlwaysPersistRenderingState=true;
    Capture->PostProcessSettings.bOverride_MotionBlurAmount=true;
    Capture->PostProcessSettings.MotionBlurAmount=0.f;
    Capture->ShowFlags.SetMotionBlur(false);
    ViewfinderCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("ViewfinderCamera"));
    ViewfinderCamera->SetupAttachment(AimPivot);
    ViewfinderCamera->SetRelativeLocation(FVector(32,0,0));
    ViewfinderCamera->PostProcessSettings.bOverride_MotionBlurAmount=true;
    ViewfinderCamera->PostProcessSettings.MotionBlurAmount=0.f;
    ViewfinderCamera->bConstrainAspectRatio=true;
    ViewfinderCamera->AspectRatio=16.f/9.f;
}
AShoulderCamcorder::~AShoulderCamcorder() = default;
void AShoulderCamcorder::BeginPlay()
{
    Super::BeginPlay();
    SetActorScale3D(FVector::OneVector);
    Body->SetBoxExtent(FVector(12,16,9));
    Body->SetMassOverrideInKg(NAME_None,1.4f);
    HoldingHands->TickAnimation(0.f,false);
    HoldingHands->RefreshBoneTransforms();
    CameraMesh->TickAnimation(0.f,false);
    CameraMesh->RefreshBoneTransforms();
    MonitorTexture=NewObject<UTextureRenderTarget2D>(this);
    // JPEG and Slate expect display-encoded bytes, not linear-light RGB.
    MonitorTexture->RenderTargetFormat=RTF_RGBA8_SRGB;
    MonitorTexture->TargetGamma=2.2f;
    MonitorTexture->ClearColor=FLinearColor::Black;
    MonitorTexture->InitAutoFormat(1,1); // No standby feed; allocate HD only when recording.
    MonitorTexture->UpdateResourceImmediate(true);
    Capture->TextureTarget=MonitorTexture; Capture->HiddenActors.Add(this);
    ScreenMaterial=DisplayScreen->CreateAndSetMaterialInstanceDynamic(0);
    HousingMaterial=CameraMesh->CreateAndSetMaterialInstanceDynamic(0);
    if(ScreenMaterial) ScreenMaterial->SetTextureParameterValue(TEXT("CameraFeed"),MonitorTexture);
    UpdateScreen();
    AdjustFocalLength(0);
}
void AShoulderCamcorder::EndPlay(const EEndPlayReason::Type Reason)
{
    SetViewfinderRaised(false);
    bSelfie=false;
    SetHoldingHandsVisible(false);
    StopRecording(); Super::EndPlay(Reason);
}
bool AShoulderCamcorder::CanInteract(APlayerController* Player) const
{
    if(!Player || !Player->GetPawn()) return false;
    if(bHeld) return Holder==Player;
    const FVector Eye=Player->PlayerCameraManager?Player->PlayerCameraManager->GetCameraLocation():Player->GetPawn()->GetActorLocation();
    if(FVector::DistSquared(Eye,GetActorLocation())>FMath::Square(InteractionDistance)) return false;
    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(CamcorderInteraction),false,Player->GetPawn());
    Q.AddIgnoredActor(this);
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,GetActorLocation(),ECC_Visibility,Q);
}
void AShoulderCamcorder::PickUp(APlayerController* Player)
{
    if(!Player || (bHeld && Holder!=Player)) return;
    if(!bHeld && !bInBackpack && !CanInteract(Player)) return;
    bInBackpack=false; SetActorHiddenInGame(false);
    Body->SetSimulatePhysics(false); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GameAudio::Handling(this,TEXT("CameraSwitch"));
    bLowering=false;LoweringTime=0;EquipProgress=0;LookProgress=0;AnimationClock=0;Holder=Player; bHeld=true; AimOffset=FRotator::ZeroRotator;
    SetHoldingHandsVisible(true);
    AimPivot->SetRelativeRotation(AimOffset);
    Capture->HiddenActors.AddUnique(Player->GetPawn());
    UpdateHeldPose(0); SetNotice(TEXT("已拿起摄像机；右键切换取景"));
}
void AShoulderCamcorder::BeginLowering(bool Store)
{
    SetViewfinderRaised(false); StopRecording();
    bSelfie=false;
    bStoreAfterLowering=Store; bInBackpack=Store; bHeld=false;
    bLowering=true; LoweringTime=0;
    AimOffset=FRotator::ZeroRotator; AimPivot->SetRelativeRotation(AimOffset);
    if(!IsValid(Holder)||!Holder->GetPawn()) FinishLowering();
}
void AShoulderCamcorder::FinishLowering()
{
    bLowering=false;
    const FVector Velocity=IsValid(Holder)&&Holder->GetPawn()?Holder->GetPawn()->GetVelocity():FVector::ZeroVector;
    SetHoldingHandsVisible(false);
    if(IsValid(Holder)&&Holder->GetPawn())Capture->HiddenActors.Remove(Holder->GetPawn());
    // Move the physics root to the real prop before enabling simulation. The hidden
    // animation rig sits below the eye and must never become the dropped item's origin.
    const FTransform PropWorld=CameraMesh->GetComponentTransform();
    SetActorTransform(PropWorld,false,nullptr,ETeleportType::TeleportPhysics);
    AimPivot->SetRelativeTransform(FTransform::Identity);
    CameraMesh->SetRelativeTransform(FTransform::Identity);
    Capture->SetRelativeLocation(FVector(1.38212f,15.804039f,0));
    Capture->SetRelativeRotation(FRotator(0,85,0));
    CameraMesh->SetVisibility(true);
    DisplayScreen->SetVisibility(true);
    Holder=nullptr;
    Body->SetCollisionEnabled(bStoreAfterLowering?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
    Body->SetSimulatePhysics(!bStoreAfterLowering);
    if(!bStoreAfterLowering)Body->SetPhysicsLinearVelocity(Velocity);
    SetActorHiddenInGame(bStoreAfterLowering);
    CharacterAudio::Attached(LoadObject<USoundBase>(nullptr,bStoreAfterLowering?TEXT("/Game/Combat/Audio/Handling/Cloth"):TEXT("/Game/Combat/Audio/Handling/ItemDrop")),GetRootComponent(),ECharacterSound::Handling,.4f);
}
bool AShoulderCamcorder::DropFromInventory(APlayerController* Player)
{
    if(!Player || !Player->GetPawn() || (!bHeld && !bInBackpack)) return false;
    if(bLowering) return false;
    if(bHeld) { PutDown(); return true; }
    // Reuse the same actor so battery, focal length and recording history survive.
    PickUp(Player);
    if(!bHeld) return false;
    BeginLowering(false);
    FinishLowering();
    return true;
}
void AShoulderCamcorder::PutDown()
{
    if(!bHeld||bLowering)return;
    BeginLowering(false);
    SetNotice(TEXT("已放下并结束录像；靠近后按 F 拿起，右键举机取景"));
}
bool AShoulderCamcorder::StoreInBackpack(APlayerController* Player)
{
    if(bInBackpack)return true;
    if(!CanInteract(Player))return false;
    if(bHeld)BeginLowering(true);
    else {bStoreAfterLowering=true;bInBackpack=true;FinishLowering();}
    SetNotice(TEXT("摄像机已收入背包，当前录像已结束；电量和焦距保留"));
    return true;
}
void AShoulderCamcorder::UpdatePresentation(float DT)
{
    AnimationClock+=DT;
    const float LookDuration=LookTransition?LookTransition->GetPlayLength():.433333f;
    LookProgress=FMath::FInterpConstantTo(LookProgress,bViewfinderRaised&&!bSelfie?1.f:0.f,DT,1.f/FMath::Max(.01f,LookDuration));
    const float EquipDuration=HoldStart?HoldStart->GetPlayLength():.533333f;
    if(!bLowering || LookProgress<=0)
        EquipProgress=FMath::FInterpConstantTo(EquipProgress,bLowering?0.f:1.f,DT,1.f/FMath::Max(.01f,EquipDuration));
    UAnimSequence* Clip=HoldIdle;
    float Position=0;
    if(EquipProgress<1.f){Clip=HoldStart;Position=EquipProgress*EquipDuration;}
    else if(LookProgress>0 && LookProgress<1){Clip=LookTransition;Position=LookProgress*LookDuration;}
    else {Clip=LookProgress>=1?LookIdle.Get():HoldIdle.Get();Position=Clip?FMath::Fmod(AnimationClock,Clip->GetPlayLength()):0;}
    if(Clip)
    {
        if(!HoldingHands->GetSingleNodeInstance() || HoldingHands->GetSingleNodeInstance()->GetCurrentAsset()!=Clip)HoldingHands->SetAnimation(Clip);
        HoldingHands->SetPosition(Position,false);
        HoldingHands->TickAnimation(0,false);
        HoldingHands->RefreshBoneTransforms();
        HoldingHands->UpdateComponentToWorld();
    }
}
void AShoulderCamcorder::UpdateHeldPose(float DT)
{
    if(!Holder || !Holder->GetPawn()) return;
    auto* Character=Cast<AGame_Jam_Clean_VersCharacter>(Holder->GetPawn());
    FVector Eye; FRotator View=Holder->GetControlRotation();
    if(bSelfie)
    {
        View.Pitch=FMath::Clamp(FRotator::NormalizeAxis(View.Pitch),-10.f,15.f);
        Holder->SetControlRotation(View); // Do not accumulate input beyond the orbit limits.
        const float Alpha=1.f-FMath::Exp(-16.f*FMath::Max(0.f,DT));
        SelfieFollowPitch=bSelfieFollowInitialized?FMath::Lerp(SelfieFollowPitch,View.Pitch,Alpha):View.Pitch;
        View.Pitch=SelfieFollowPitch;
    }
    else bSelfieFollowInitialized=false;
    if(Character) Eye=Character->GetFirstPersonCameraComponent()->GetComponentLocation();
    else { FRotator Unused; Holder->GetPawn()->GetActorEyesViewPoint(Eye,Unused); }
    // Track the animated head in selfies, including crouch/locomotion offsets.
    // The first-person camera intentionally moves independently for body clearance.
    if(bSelfie && Character)
        Eye=Character->GetFirstPersonMesh()->GetSocketLocation(TEXT("head"))+Character->GetActorForwardVector()*8.f+FVector(0,0,6.f);
    if(bSelfie)
    {
        const float Alpha=1.f-FMath::Exp(-20.f*FMath::Max(0.f,DT));
        if(!bSelfieFollowInitialized || FVector::DistSquared(SelfieFollowEye,Eye)>FMath::Square(150.f)) SelfieFollowEye=Eye;
        else SelfieFollowEye=FMath::Lerp(SelfieFollowEye,Eye,Alpha);
        Eye=SelfieFollowEye;
        bSelfieFollowInitialized=true;
    }
    const float Speed=FMath::Clamp(Holder->GetPawn()->GetVelocity().Size2D()/450.f,0.f,1.f);
    SprintBlend=FMath::FInterpTo(SprintBlend,Character&&Character->bSprinting&&!bViewfinderRaised?1.f:0.f,DT,8.f);
    SwayClock+=DT*9.f;
    const float SwayScale=bViewfinderRaised?0.f:Speed;
    FRotator Sway(FMath::Sin(SwayClock)*.65f*SwayScale-12.f*SprintBlend,0,FMath::Cos(SwayClock)*.5f*SwayScale-10.f*SprintBlend);
    SetActorLocationAndRotation(Eye+View.RotateVector(FVector(0,0,-5.f*SprintBlend)),View+Sway,false,nullptr,ETeleportType::TeleportPhysics);
    // Pull the authored grip slightly in/up at high pitch so the original
    // player's right arm stays within reach without changing the hand/prop attachment.
    const float HighPitch=FMath::Clamp((FRotator::NormalizeAxis(View.Pitch)-20.f)/40.f,0.f,1.75f);
    HoldingHands->SetRelativeLocation(FVector(-4.f-4.f*HighPitch,0.f,-151.327179f+4.f*HighPitch-(bSelfie?85.f:0.f)));
    UpdatePresentation(DT);
    // Preserve the seller's hand_r attachment exactly; the prop follows its authored grip.
    const FTransform Grip(FRotator(-.348233f,84.987534f,101.755234f),FVector(-9.067672f,5.660569f,-.9962f));
    CameraMesh->SetWorldTransform(Grip*HoldingHands->GetSocketTransform(TEXT("hand_r")));
    // Full-screen aiming uses the control direction, independent of hand sway.
    const FVector Lens=CameraMesh->GetComponentTransform().TransformPosition(FVector(1.38212f,15.804039f,0));
    ViewfinderCamera->SetWorldLocationAndRotation(Eye+View.Vector()*20.f,View+AimOffset);
    Capture->SetWorldLocationAndRotation(bViewfinderRaised?ViewfinderCamera->GetComponentLocation():Lens,View+AimOffset);
    if(bSelfie)
    {
        // Both the recorded feed and scoring use this same, collision-safe reverse lens.
        // Slightly elevated, off-centre head-and-shoulders framing leaves room for the scene.
        const FVector Desired=Eye+View.RotateVector(FVector(105,30,18));
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SelfieLens),false,Holder->GetPawn());
        Query.AddIgnoredActor(this);
        const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Eye,Desired,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8.f),Query);
        const FVector Position=Blocked?Hit.Location:Desired;
        if(Blocked && (Hit.bStartPenetrating || FVector::DistSquared(Eye,Position)<FMath::Square(55.f)))
        {
            bSelfie=false;
            Capture->HiddenActors.AddUnique(Holder->GetPawn());
            SetNotice(TEXT("自拍空间不足，已切回前拍；请远离遮挡后按 V"));
            return;
        }
        const FVector FramingTarget=Eye+View.RotateVector(FVector(0,15,6));
        const FRotator Reverse=(FramingTarget-Position).Rotation();
        ViewfinderCamera->SetWorldLocationAndRotation(Position,Reverse);
        Capture->SetWorldLocationAndRotation(Position,Reverse);
        Capture->HiddenActors.Remove(Holder->GetPawn());
    }
    else Capture->HiddenActors.AddUnique(Holder->GetPawn());
    // Resample the visible arms after this frame's prop pose, as weapons do,
    // instead of leaving wrists one movement frame behind during sprinting.
    if(Character)
    {
        Character->GetFirstPersonMesh()->TickAnimation(0,false);
        Character->GetFirstPersonMesh()->RefreshBoneTransforms();
    }
}
void AShoulderCamcorder::UpdateScreen()
{
    if(ScreenMaterial)
    {
        ScreenMaterial->SetScalarParameterValue(TEXT("ScreenPower"),bPowered && bRecording?1.f:0.f);
        ScreenMaterial->SetScalarParameterValue(TEXT("IsRecording"),bRecording?1.f:0.f);
        ScreenMaterial->SetScalarParameterValue(TEXT("1bar"),BatteryPercent>0?1.f:0.f);
        ScreenMaterial->SetScalarParameterValue(TEXT("2bar"),BatteryPercent>33?1.f:0.f);
        ScreenMaterial->SetScalarParameterValue(TEXT("3bar"),BatteryPercent>66?1.f:0.f);
    }
    if(HousingMaterial)
    {
        HousingMaterial->SetScalarParameterValue(TEXT("FrontLight"),0.f);
        HousingMaterial->SetScalarParameterValue(TEXT("RecLightIntensity"),bRecording?1.f:0.f);
    }
}
void AShoulderCamcorder::ToggleSelfie()
{
    if(!bHeld || bInBackpack || bLowering || !Holder) return;
    if(!bViewfinderRaised)
    {
        SetViewfinderRaised(true);
        if(!bViewfinderRaised) return;
    }
    const bool WantedSelfie=!bSelfie;
    bSelfieFollowInitialized=false;
    bSelfie=WantedSelfie;
    if(bSelfie) AdjustFocalLength(0);

    UpdateHeldPose(0);
    if(WantedSelfie && !bSelfie) return;
    GameAudio::Handling(this,TEXT("CameraSwitch"));
    SetNotice(bSelfie?TEXT("自拍镜头：画面内目标按原规则计分；V 切回前拍"):TEXT("已切回前拍镜头"));
}
void AShoulderCamcorder::ToggleViewfinder() { SetViewfinderRaised(!bViewfinderRaised); }
void AShoulderCamcorder::SetViewfinderRaised(bool bRaised)
{
    if(bRaised==bViewfinderRaised) return;
    if(bRaised && (!bHeld || bInBackpack || bLowering || !Holder || !bPowered || EquipProgress<1.f))
    { SetNotice(TEXT("请先装备并开启摄像机，再按右键举机取景")); return; }
    bViewfinderRaised=bRaised;
    if(bRaised)GameAudio::Handling(this,TEXT("CameraRaise"));
    CameraMesh->SetVisibility(!bRaised); DisplayScreen->SetVisibility(!bRaised);
    if(bRaised) { Holder->SetViewTargetWithBlend(this,.43f); SetNotice(TEXT("移动鼠标调整镜头，滚轮变焦；左键开始/停止录像，Esc 放下取景")); }
    else
    {
        StopRecording();
        bSelfie=false;
        if(Holder && Holder->GetPawn()) Holder->SetViewTargetWithBlend(Holder->GetPawn(),.43f);
    }
}
void AShoulderCamcorder::SetHoldingHandsVisible(bool bVisible)
{
    HoldingHands->SetVisibility(false);
    if(bVisible && Holder)
    {
        if(auto* Character=Cast<AGame_Jam_Clean_VersCharacter>(Holder->GetPawn()))
        {
            auto* Mesh=Character->GetFirstPersonMesh();
            if(!OriginalFirstPersonMesh.IsValid()) { OriginalFirstPersonMesh=Mesh; bOriginalMeshVisible=Mesh->IsVisible(); }
            // Keep the original skeleton ticking: the player's view is attached to its head.
            Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            Mesh->SetVisibility(true,false);
        }
    }
    else if(auto* Mesh=OriginalFirstPersonMesh.Get()) { Mesh->SetVisibility(true,false); OriginalFirstPersonMesh.Reset(); }

}
void AShoulderCamcorder::SetNotice(const FString& Message) { Notice=Message; NoticeClock=6.f; }
void AShoulderCamcorder::AdjustAim(float PitchDelta,float YawDelta)
{
    AimOffset.Pitch=FMath::Clamp(AimOffset.Pitch+PitchDelta,-70.f,70.f);
    AimOffset.Yaw=FMath::Clamp(AimOffset.Yaw+YawDelta,-100.f,100.f);
    // Aim is applied to the lens/viewfinder; the authored grip remains intact.
}
void AShoulderCamcorder::AdjustFocalLength(float DeltaMM)
{
    FocalLength=FMath::Clamp(FocalLength+DeltaMM,bSelfie?55.f:18.f,100.f);
    Capture->FOVAngle=FMath::RadiansToDegrees(2.f*FMath::Atan(36.f/(2.f*FocalLength)));
    ViewfinderCamera->SetFieldOfView(Capture->FOVAngle);
}
bool AShoulderCamcorder::StartRecording()
{
    if(GetOwner())if(auto* Library=GetOwner()->FindComponentByClass<URecordingLibrary>();Library&&Library->bRoundEnded)return false;
    if(!bViewfinderRaised || !bHeld) { SetNotice(TEXT("只能在取景状态录像：先按鼠标右键举机")); return false; }
    if(bInBackpack) { SetNotice(TEXT("请先从背包装备摄像机再录像")); return false; }
    if(bRecording) return true;
    if(!bPowered || BatteryPercent<=0) { SetNotice(TEXT("无法录像：摄像机已关机，请换电池或按 P 开机")); return false; }
    const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Recordings"));
    if(!IFileManager::Get().MakeDirectory(*Folder,true)) { SetNotice(TEXT("无法创建录像目录")); return false; }
    const FString Path=Folder/FString::Printf(TEXT("Camcorder_%s_%s.avi"),*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")),*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    Writer=MakeUnique<FCamcorderVideoWriter>();
    if(!Writer->Open(Path,1280,720,15)) { Writer.Reset(); SetNotice(TEXT("录像文件创建失败，请检查磁盘空间")); return false; }
    MonitorTexture->ResizeTarget(1280,720);
    FrameReadback=MakeUnique<FCamcorderFrameReadback>();
    LastRecordingPath=Path; CaptureClock=0; RecordingSeconds=0; LastScheduledFrame=-1; bCaptureReadFailed=false; bRecording=true;
    if(GetOwner())if(auto* Library=GetOwner()->FindComponentByClass<URecordingLibrary>())Library->BeginClip(this);
    Capture->CaptureScene(); SampleRecording();
    UpdateScreen();
    SetNotice(TEXT("录像中；右键退出取景时会自动保存")); return true;
}
void AShoulderCamcorder::SampleRecording()
{
    if(!Writer || !MonitorTexture) return;
    const int32 FrameIndex=FMath::FloorToInt(RecordingSeconds*15.f);
    if(FrameIndex<=LastScheduledFrame) return;
    if(FrameReadback && FrameReadback->Schedule(MonitorTexture->GameThread_GetRenderTargetResource(),FrameIndex)) {
        LastScheduledFrame=FrameIndex;
        // Score the captured pose now, not the later GPU completion pose.
        if(GetOwner())if(auto* Library=GetOwner()->FindComponentByClass<URecordingLibrary>())Library->Sample(this);
    }
}
void AShoulderCamcorder::StopRecording()
{
    if(!bRecording || !Writer) return;
    // Include the final captured frame before committing exposure and finalizing the video.
    if(Capture){Capture->CaptureScene();SampleRecording();}
    if(FrameReadback) { FrameReadback->Pump(*Writer,true); bCaptureReadFailed|=FrameReadback->HasFailed(); }
    FrameReadback.Reset();
    bRecording=false;
    Writer->Finish(FMath::Max(1,FMath::CeilToInt(RecordingSeconds*15.f)));
    const bool bOK=!Writer->HasFailed() && Writer->GetFrameCount()>0 && !bCaptureReadFailed;
    Writer.Reset();
    MonitorTexture->ResizeTarget(1,1);
    if(GetOwner())if(auto* Library=GetOwner()->FindComponentByClass<URecordingLibrary>())Library->FinishClip(this,bOK);
    UpdateScreen();
    if(bOK) { ++SavedClipCount; SetNotice(TEXT("录像已保存：Saved/Recordings/")+FPaths::GetCleanFilename(LastRecordingPath)); }
    else SetNotice(TEXT("录像保存异常：检查磁盘空间或输出日志；文件可能不完整"));
    UE_LOG(LogTemp,Log,TEXT("Camcorder clip %s: %s"),bOK?TEXT("saved"):TEXT("incomplete"),*LastRecordingPath);
}
void AShoulderCamcorder::ConsumeBattery(float Seconds)
{
    if(!bRecording || !bPowered) return;
    const float Old=BatteryPercent;
    BatteryPercent=FMath::Max(0.f,BatteryPercent-100.f*FMath::Max(0.f,Seconds)/FMath::Max(1.f,FullBatterySeconds));
    for(float Threshold:{50.f,20.f,10.f})
        if(Old>Threshold && BatteryPercent<=Threshold) SetNotice(FString::Printf(TEXT("电量不足 %.0f%%：按 R 更换电池"),Threshold));
    if(Old!=BatteryPercent) if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()) Bus->NotifyChange(this,TEXT("BatteryPercent"),BatteryPercent);
    if(BatteryPercent<=0)
    {
        StopRecording(); bPowered=false; SetViewfinderRaised(false);
        SetNotice(TEXT("电池耗尽，已结束录像并尝试保存；按 R 换电池。")+Notice);
    }
}
void AShoulderCamcorder::TogglePower()
{
    if(bPowered) { SetViewfinderRaised(false); StopRecording(); bPowered=false; SetNotice(TEXT("摄像机已关机。")+Notice); }
    else if(BatteryPercent>0) { bPowered=true; SetNotice(TEXT("摄像机已开机")); }
    else SetNotice(TEXT("电池耗尽，请按 R 换电池"));
    UpdateScreen();
}
bool AShoulderCamcorder::InstallFreshBattery()
{
    StopRecording(); BatteryPercent=100.f; bPowered=true;
    if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()) Bus->NotifyChange(this,TEXT("BatteryPercent"),BatteryPercent);
    UpdateScreen();
    SetNotice(TEXT("已更换电池并开机；右键切换取景，再按左键开始新录像")); return true;
}
void AShoulderCamcorder::Tick(float DT)
{
    Super::Tick(DT);
    if(bLowering){LoweringTime+=DT;if(!IsValid(Holder)||!Holder->GetPawn())FinishLowering();else {UpdateHeldPose(DT);if(EquipProgress<=0 && LookProgress<=0)FinishLowering();}}

    if(NoticeClock>0) { NoticeClock-=DT; if(NoticeClock<=0) Notice.Empty(); }
    if(bInBackpack) return;
    if(bHeld)
    {
        if(!IsValid(Holder) || !Holder->GetPawn()) PutDown(); else UpdateHeldPose(DT);
    }
    if(bRecording) RecordingSeconds+=DT;
    if(bRecording && FrameReadback && Writer)
    {
        FrameReadback->Pump(*Writer);
        bCaptureReadFailed|=FrameReadback->HasFailed();
    }
    // The viewfinder uses the main view; scene capture is exclusively for recording.
    if(bRecording) CaptureClock+=DT;
    if(bPowered && bRecording && CaptureClock>=1.f/15.f)
    {
        CaptureClock=FMath::Fmod(CaptureClock,1.f/15.f); Capture->CaptureScene();
        if(bRecording) SampleRecording();
    }
    if(bRecording && (bCaptureReadFailed || (Writer && Writer->HasFailed()))) StopRecording();
    ConsumeBattery(DT);
    UpdateScreen();
}

ACamcorderBattery::ACamcorderBattery()
{
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Battery")); SetRootComponent(Mesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube")); Mesh->SetStaticMesh(Cube.Object);
    Mesh->SetWorldScale3D(FVector(.12f,.07f,.16f)); Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(true); Mesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Camcorder/Materials/M_CamcorderColor"));
    if(Mat.Succeeded()) Mesh->SetMaterial(0,Mat.Object);
    Mesh->SetDefaultCustomPrimitiveDataVector4(0,FVector4(.025f,.03f,.035f,1));
    auto Detail=[this](const TCHAR* Name,FVector Position,FVector Scale,FLinearColor Color){
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(Name);Part->SetupAttachment(Mesh);Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));Part->SetRelativeLocation(Position);Part->SetRelativeScale3D(Scale);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Camcorder/Materials/M_CamcorderColor")));Part->SetDefaultCustomPrimitiveDataVector4(0,FVector4(Color.R,Color.G,Color.B,1));
    };
    Detail(TEXT("TerminalPlus"),FVector(-22,0,52),FVector(.15,.35,.06),FLinearColor(.65f,.5f,.12f));
    Detail(TEXT("TerminalMinus"),FVector(22,0,52),FVector(.15,.35,.06),FLinearColor(.65f,.5f,.12f));
    Detail(TEXT("BatteryLabel"),FVector(0,-50.5f,0),FVector(.8,.015,.7),FLinearColor(.16f,.2f,.22f));
    Detail(TEXT("HousingSeam"),FVector(0,0,35),FVector(1.01,1.01,.018),FLinearColor(.006f,.008f,.01f));
    for(int32 I=0;I<4;++I)Detail(*FString::Printf(TEXT("ChargeLED%d"),I),FVector(-27+I*18,-52,20),FVector(.11,.02,.05),FLinearColor(.1f,.8f,.3f));
}

void AShoulderCamcorder::DropOnDeath()
{
 if(!bHeld&&!bLowering)return;
 auto* PC=Holder.Get();
 const FVector Velocity=PC&&PC->GetPawn()?PC->GetPawn()->GetVelocity():FVector::ZeroVector;
 const FVector Forward=PC?FRotator(0,PC->GetControlRotation().Yaw,0).Vector():GetActorForwardVector();
 BeginLowering(false);FinishLowering();
 Body->SetPhysicsLinearVelocity(Velocity+Forward*180.f+FVector(0,0,70));
 Body->SetPhysicsAngularVelocityInDegrees(FVector(100,160,70));
 Notice.Empty();
}
