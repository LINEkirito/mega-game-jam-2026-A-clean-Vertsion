#include "RecordingTabletDisplay.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
ARecordingTabletDisplay::ARecordingTabletDisplay(){
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.2f;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 Mesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PurchasedPDA"));Mesh->SetupAttachment(RootComponent);
 Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Equipment/PDA/Meshes/SK_PDA")));
 Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->SetVisibleInSceneCaptureOnly(true);
 Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("TabletInspection"));Capture->SetupAttachment(RootComponent);
 Capture->SetRelativeLocation(FVector(1.849392,-.063874,65));Capture->SetRelativeRotation(FRotator(-90,-90,0));
 Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=36;
 Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
 Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
 Capture->ShowFlags.SetMotionBlur(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetAtmosphere(false);Capture->ShowFlags.SetEyeAdaptation(false);
 Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
 Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=0;
 auto* Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("TabletLight"));Light->SetupAttachment(RootComponent);Light->SetRelativeLocation(FVector(0,-20,50));Light->SetIntensity(100);Light->SetAttenuationRadius(300);Light->SetCastShadows(false);
}
void ARecordingTabletDisplay::BeginPlay(){Super::BeginPlay();
 Mesh->SetTextureForceResidentFlag(true);Mesh->PrestreamTextures(30.f,true);
 Texture=NewObject<UTextureRenderTarget2D>(this);Texture->RenderTargetFormat=RTF_RGBA8;Texture->ClearColor=FLinearColor(.012f,.016f,.022f,1);Texture->InitAutoFormat(1200,900);Texture->UpdateResourceImmediate(true);
 Capture->TextureTarget=Texture;Capture->ShowOnlyComponent(Mesh);Capture->CaptureScene();
}

void ARecordingTabletDisplay::Tick(float DT){Super::Tick(DT);Capture->CaptureScene();}
