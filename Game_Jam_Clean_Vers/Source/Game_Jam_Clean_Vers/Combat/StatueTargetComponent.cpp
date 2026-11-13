#include "StatueTargetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UStatueTargetComponent::UStatueTargetComponent(){PrimaryComponentTick.bCanEverTick=true;}
void UStatueTargetComponent::BeginPlay()
{
    Super::BeginPlay();Health=FMath::Max(1.f,MaxHealth);
    FVector Center,Extent;GetOwner()->GetActorBounds(false,Center,Extent);
    Anchor=Center+FVector(0,0,Extent.Z+35);
    TArray<UStaticMeshComponent*> Geometry;GetOwner()->GetComponents(Geometry);
    for(auto* Mesh:Geometry){Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);}
    auto CreateBar=[this](const TCHAR* Name,FLinearColor Color){auto* M=NewObject<UStaticMeshComponent>(GetOwner(),Name);M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));M->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Camcorder/Materials/M_CamcorderColor")));M->SetDefaultCustomPrimitiveDataVector4(0,FVector4(Color.R,Color.G,Color.B,1));M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCastShadow(false);M->RegisterComponent();return M;};
    Background=CreateBar(TEXT("TargetBarBackground"),FLinearColor(.025f,.025f,.025f));
    Bar=CreateBar(TEXT("TargetHealthBar"),FLinearColor(.1f,.8f,.25f));
    Label=NewObject<UTextRenderComponent>(GetOwner(),TEXT("TargetHealthLabel"));Label->SetHorizontalAlignment(EHTA_Center);Label->SetWorldSize(16);Label->RegisterComponent();
    GetOwner()->OnTakeAnyDamage.AddDynamic(this,&UStatueTargetComponent::OnDamage);
    ResetTarget();
}
void UStatueTargetComponent::ResetTarget(){Health=FMath::Max(1.f,MaxHealth);Flash=0;if(Label)Label->SetText(FText::FromString(FString::Printf(TEXT("JIEGE %.0f / %.0f"),Health,MaxHealth)));}
float UStatueTargetComponent::ApplyHit(float Damage)
{
    if(Damage<=0||Health<=0)return 0;
    const float Applied=FMath::Min(Damage,Health);Health-=Applied;Flash=.15f;
    if(Label)Label->SetText(FText::FromString(Health>0?FString::Printf(TEXT("JIEGE %.0f / %.0f"),Health,MaxHealth):TEXT("JIEGE  DOWN")));
    return Applied;
}
void UStatueTargetComponent::OnDamage(AActor*,float Damage,const UDamageType*,AController*,AActor*){ApplyHit(Damage);}
void UStatueTargetComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn)
{
    Super::TickComponent(DT,Type,Fn);if(!Bar||!Label||!Background)return;
    Flash=FMath::Max(0.f,Flash-DT);
    auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!PC)return;
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
    const FRotator Facing=(Eye-Anchor).Rotation();const FRotationMatrix Frame(Facing);
    const FVector Right=Frame.GetUnitAxis(EAxis::Y);const FVector Front=Frame.GetUnitAxis(EAxis::X);
    const float Ratio=FMath::Clamp(Health/FMath::Max(1.f,MaxHealth),0.f,1.f);
    Background->SetWorldLocationAndRotation(Anchor,Facing);Background->SetWorldScale3D(FVector(.015f,1.24f,.09f));
    Bar->SetWorldLocationAndRotation(Anchor+Front*1.5f+Right*((Ratio-1)*60),Facing);Bar->SetWorldScale3D(FVector(.012f,1.2f*Ratio,.065f));Bar->SetVisibility(Ratio>0);
    Bar->SetCustomPrimitiveDataVector4(0,Flash>0?FVector4(1,.15f,.08f,1):FVector4(.1f,.8f,.25f,1));
    Label->SetWorldLocationAndRotation(Anchor+FVector(0,0,12),Facing);
}
