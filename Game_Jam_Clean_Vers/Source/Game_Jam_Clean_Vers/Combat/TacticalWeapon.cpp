#include "TacticalWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Data/GlobalData.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
static UMaterialInterface* ColorMat(){return LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Camcorder/Materials/M_CamcorderColor"));}
static void Tint(UStaticMeshComponent* M,FLinearColor C){M->SetMaterial(0,ColorMat());M->SetDefaultCustomPrimitiveDataVector4(0,FVector4(C.R,C.G,C.B,1));}
ACombatAmmoPickup::ACombatAmmoPickup(){Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AmmoBox"));SetRootComponent(Mesh);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));Mesh->SetWorldScale3D(FVector(.18f,.12f,.08f));Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));Mesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Mesh->SetSimulatePhysics(true);Tint(Mesh,FLinearColor(.3f,.4f,.15f));Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("AmmoLabel"));Label->SetupAttachment(Mesh);Label->SetAbsolute(false,false,true);Label->SetRelativeLocation(FVector(0,0,90));Label->SetWorldSize(10);}
FName ACombatAmmoPickup::AmmoId()const{return Kind==ETacticalWeaponKind::Pistol?FName(TEXT("Ammo9mm")):Kind==ETacticalWeaponKind::Rifle?FName(TEXT("Ammo556")):FName(TEXT("Ammo12ga"));}
FString ACombatAmmoPickup::AmmoName()const{return Kind==ETacticalWeaponKind::Pistol?TEXT("9mm 手枪弹"):Kind==ETacticalWeaponKind::Rifle?TEXT("5.56 步枪弹"):TEXT("12号霰弹");}
void ACombatAmmoPickup::OnConstruction(const FTransform& T){Super::OnConstruction(T);if(auto* M=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/Combat/Packaging/M_"))+AmmoId().ToString())))Mesh->SetMaterial(0,M);Label->SetText(FText::FromString(AmmoId().ToString()+FString::Printf(TEXT(" x%d"),Amount)));}
ACombatTarget::ACombatTarget(){PrimaryActorTick.bCanEverTick=true;Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Target"));SetRootComponent(Mesh);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));Mesh->SetRelativeScale3D(FVector(.25f,.6f,1.4f));Mesh->SetCollisionProfileName(TEXT("BlockAll"));Tint(Mesh,FLinearColor(.1f,.25f,.4f));Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthLabel"));Label->SetupAttachment(Mesh);Label->SetAbsolute(false,false,true);Label->SetRelativeLocation(FVector(0,0,65));Label->SetWorldSize(18);Label->SetHorizontalAlignment(EHTA_Center);Label->SetText(FText::FromString(TEXT("TARGET 100")));}
float ACombatTarget::TakeDamage(float D,const FDamageEvent&,AController*,AActor*){if(D<=0||Health<=0)return 0;const float Applied=FMath::Min(Health,D);Health-=Applied;HitFlash=.18f;Label->SetText(FText::FromString(Health>0?FString::Printf(TEXT("TARGET %.0f"),Health):TEXT("DOWN")));if(Health<=0)SetActorRotation(GetActorRotation()+FRotator(-80,0,0));return Applied;}
void ACombatTarget::Tick(float DT){Super::Tick(DT);HitFlash-=DT;Mesh->SetCustomPrimitiveDataVector4(0,Health<=0?FVector4(.1f,.1f,.1f,1):HitFlash>0?FVector4(1,.15f,.08f,1):FVector4(.1f,.25f,.4f,1));}
// Adapted from RogueBox / PRO Effects Sci fi FX SMG's moving, short-lived trail.
// This actor is cosmetic: never trace or apply damage here.
ACombatShotEffect::ACombatShotEffect()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ShotRoot")));
    Streak=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer"));
    Streak->SetupAttachment(RootComponent);
    Streak->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane")));
    Streak->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/VFX/M_SoftTracer")));
    Streak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Streak->SetCastShadow(false);
    Streak->SetVisibility(false);
    Streak->SetDefaultCustomPrimitiveDataFloat(0,1.f);
    Impact=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Impact"));
    Impact->SetupAttachment(RootComponent);
    Impact->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane")));
    Impact->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/VFX/M_WallImpactFlash")));
    Impact->SetDefaultCustomPrimitiveDataFloat(0,1.f);
    Impact->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Impact->SetCastShadow(false);
    Impact->SetVisibility(false);
    ImpactDust=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactDust"));
    ImpactDust->SetupAttachment(RootComponent);
    ImpactDust->SetStaticMesh(Impact->GetStaticMesh());
    ImpactDust->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/VFX/M_WallImpactDust")));
    ImpactDust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImpactDust->SetCastShadow(false);
    ImpactDust->SetVisibility(false);
    for(int32 I=0;I<5;++I)
    {
        auto* Spark=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ImpactSpark%d"),I));
        Spark->SetupAttachment(RootComponent);
        Spark->SetStaticMesh(Impact->GetStaticMesh());
        Spark->SetMaterial(0,Streak->GetMaterial(0));
        Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Spark->SetCastShadow(false);
        Spark->SetVisibility(false);
        ImpactSparks.Add(Spark);
    }
    SetLifeSpan(1.f);
}

void ACombatShotEffect::Setup(FVector Start,FVector End,bool Hit,bool Flesh,FVector SurfaceNormal)
{
    Age=0;
    TraceStart=Start; TraceEnd=End;
    TraceDistance=FVector::Distance(Start,End);
    TraceDirection=(End-Start).GetSafeNormal();
    bHasImpact=Hit; bFleshImpact=Flesh;
    const auto* Config=GetDefault<UGlobalData>();
    VisualSpeed=FMath::Max(1.f,Config->TracerSpeed);
    VisualLength=FMath::Max(1.f,Config->TracerLength);
    VisualWidth=FMath::Max(.01f,Config->TracerWidth);
    FadeSeconds=FMath::Max(.001f,Config->TracerFadeSeconds);
    ImpactDuration=FMath::Max(.08f,Config->WallImpactSeconds);
    ImpactSize=FMath::Max(1.f,Config->WallImpactSize);
    ImpactSparkSpeed=FMath::Max(1.f,Config->WallImpactSparkSpeed);
    ImpactNormal=SurfaceNormal.GetSafeNormal();
    if(ImpactNormal.IsNearlyZero())ImpactNormal=-TraceDirection;
    if(ImpactNormal.IsNearlyZero())ImpactNormal=FVector::UpVector;
    ImpactRoll=FMath::FRandRange(0.f,360.f);
    SparkVelocities.Reset();
    for(int32 I=0;I<ImpactSparks.Num();++I)
        SparkVelocities.Add(FMath::VRandCone(ImpactNormal,FMath::DegreesToRadians(65.f))*ImpactSparkSpeed*FMath::FRandRange(.65f,1.3f));
    SetActorLocation(Start);
    SetActorScale3D(FVector::OneVector);
    SetLifeSpan(TraceDistance/VisualSpeed+FMath::Max(FadeSeconds,Hit&&!Flesh?ImpactDuration:0.f)+.02f);
    UpdateVisual();
}

void ACombatShotEffect::UpdateVisual()
{
    const float Arrival=TraceDistance/VisualSpeed;
    // A short initial segment prevents a sub-frame shot from becoming an invisible effect.
    const float Head=FMath::Min(TraceDistance,FMath::Min(VisualLength,TraceDistance)*.5f+Age*VisualSpeed);
    const float Tail=FMath::Max(0.f,Head-VisualLength);
    const float Fade=1.f-FMath::Clamp((Age-Arrival)/FadeSeconds,0.f,1.f);
    Streak->SetVisibility(TraceDistance>1.f && Fade>0.f);
    if(TraceDistance>1.f && Fade>0.f)
    {
        const FVector Center=TraceStart+TraceDirection*((Head+Tail)*.5f);
        FVector Normal=FVector::UpVector;
        if(const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
            Normal=(Camera->GetCameraLocation()-Center).GetSafeNormal();
        if(FMath::Abs(FVector::DotProduct(Normal,TraceDirection))>.99f)
            Normal=FMath::Abs(TraceDirection.Z)<.9f?FVector::UpVector:FVector::RightVector;
        Streak->SetWorldLocationAndRotation(Center,FRotationMatrix::MakeFromXZ(TraceDirection,Normal).Rotator());
        Streak->SetWorldScale3D(FVector((Head-Tail)/100.f,VisualWidth/100.f,1.f));
        Streak->SetCustomPrimitiveDataFloat(0,Fade);
    }
    // Adapted from RogueBox SMG Impact: warm burst + stretched short sparks.
    // Dust is a small project-authored layer using the same pack's noise texture.
    // Entirely cosmetic: the collision result and outward normal come from the shot.
    const float T=Age-Arrival;
    const bool Show=bHasImpact && !bFleshImpact && T>=0.f && T<ImpactDuration;
    const float Progress=FMath::Clamp(T/ImpactDuration,0.f,1.f);
    const FQuat SurfaceRotation=FRotationMatrix::MakeFromZ(ImpactNormal).ToQuat()*FQuat(FVector::UpVector,FMath::DegreesToRadians(ImpactRoll));
    Impact->SetVisibility(Show && T<.045f);
    Impact->SetWorldLocationAndRotation(TraceEnd+ImpactNormal*.8f,SurfaceRotation);
    Impact->SetWorldScale3D(FVector(ImpactSize/100.f));
    Impact->SetCustomPrimitiveDataFloat(0,FMath::Square(1.f-FMath::Clamp(T/.045f,0.f,1.f)));
    ImpactDust->SetVisibility(Show);
    ImpactDust->SetWorldLocationAndRotation(TraceEnd+ImpactNormal*(1.f+Progress*2.f),SurfaceRotation);
    ImpactDust->SetWorldScale3D(FVector(ImpactSize*(.5f+Progress*1.8f)/100.f));
    ImpactDust->SetCustomPrimitiveDataFloat(0,FMath::Clamp(T/.025f,0.f,1.f)*FMath::Square(1.f-Progress));
    const FVector Gravity=FVector::VectorPlaneProject(FVector(0,0,-420.f),ImpactNormal);
    for(int32 I=0;I<ImpactSparks.Num();++I)
    {
        auto* Spark=ImpactSparks[I].Get();
        const float Life=ImpactDuration*(.38f+.055f*I);
        Spark->SetVisibility(Show && T<Life);
        if(!Show || T>=Life || !SparkVelocities.IsValidIndex(I))continue;
        const FVector Velocity=SparkVelocities[I]+Gravity*T;
        const FVector Center=TraceEnd+ImpactNormal*2.f+SparkVelocities[I]*T+Gravity*(.5f*T*T);
        const FVector Axis=Velocity.GetSafeNormal();
        FVector Facing=ImpactNormal;
        if(const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))Facing=(Camera->GetCameraLocation()-Center).GetSafeNormal();
        if(FMath::Abs(FVector::DotProduct(Facing,Axis))>.99f)Facing=FMath::Abs(Axis.Z)<.9f?FVector::UpVector:FVector::RightVector;
        const float SparkFade=1.f-T/Life;
        Spark->SetWorldLocationAndRotation(Center,FRotationMatrix::MakeFromXZ(Axis,Facing).Rotator());
        // Limit length by outward distance so a spark cannot poke through the struck wall.
        const float Outward=FVector::DotProduct(Center-TraceEnd,ImpactNormal);
        const float Length=FMath::Min(ImpactSize*.45f,Outward*1.5f);
        Spark->SetWorldScale3D(FVector(Length/100.f,.0035f*SparkFade,1.f));
        Spark->SetCustomPrimitiveDataFloat(0,SparkFade*SparkFade);
    }
}

void ACombatShotEffect::Tick(float DT)
{
    Super::Tick(DT);
    Age+=FMath::Max(0.f,DT);
    UpdateVisual();
}
