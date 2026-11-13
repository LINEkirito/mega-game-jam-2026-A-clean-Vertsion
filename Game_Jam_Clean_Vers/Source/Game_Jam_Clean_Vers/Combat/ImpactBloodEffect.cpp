#include "ImpactBloodEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/DecalActor.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"

AImpactBloodEffect::AImpactBloodEffect()
{
 PrimaryActorTick.bCanEverTick=true;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void AImpactBloodEffect::SpawnForHit(UWorld* World,const FHitResult& Hit,const FVector& IncomingDirection)
{
 if(!World||!IsValid(Hit.GetActor()))return;
 // A shotgun's pellets share a visible burst, but each pellet still applies its damage.
 int32 Count=0;AImpactBloodEffect* Oldest=nullptr;
 for(TActorIterator<AImpactBloodEffect> It(World);It;++It){
  if(It->Victim==Hit.GetActor()&&It->Age<.045f)return;
  ++Count;if(!Oldest||It->Age>Oldest->Age)Oldest=*It;
 }
 if(Count>=24&&Oldest)Oldest->Destroy();
 auto* Effect=World->SpawnActor<AImpactBloodEffect>(Hit.ImpactPoint,FRotator::ZeroRotator);
 if(Effect)Effect->Initialize(Hit,IncomingDirection);
}
void AImpactBloodEffect::Initialize(const FHitResult& Hit,const FVector& IncomingDirection)
{
 Victim=Hit.GetActor();
 auto* SprayMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/Effects/Blood/M_BloodSpray"));
 StainMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/Effects/Blood/M_BloodStain"));
 auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane"));
 const FVector Outward=Hit.ImpactNormal.IsNearlyZero()?-IncomingDirection.GetSafeNormal():Hit.ImpactNormal.GetSafeNormal();
 for(int32 I=0;I<8;++I){
  auto* Drop=NewObject<UStaticMeshComponent>(this);Drop->SetupAttachment(GetRootComponent());Drop->SetStaticMesh(Plane);Drop->SetMaterial(0,SprayMaterial);Drop->CreateDynamicMaterialInstance(0);Drop->SetCollisionEnabled(ECollisionEnabled::NoCollision);Drop->SetCastShadow(false);Drop->RegisterComponent();
  Drop->SetWorldLocation(Hit.ImpactPoint+Outward*6);Drop->SetWorldScale3D(FVector(I==0?.5f:FMath::FRandRange(.06f,.12f)));Droplets.Add(Drop);
  Velocities.Add(Outward*FMath::FRandRange(100.f,200.f)+FVector::UpVector*FMath::FRandRange(120.f,230.f)+FMath::VRand()*75.f);
 }
 // The old RogueBox material and physical-droplet approach are reused, in Unreal units.
 UGameplayStatics::PlaySoundAtLocation(this,LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Effects/Blood/S_BloodHit")),Hit.ImpactPoint,.65f,FMath::FRandRange(.95f,1.05f));
 SetLifeSpan(3.f);
}
void AImpactBloodEffect::Stain(const FHitResult& Surface,float Size)
{
 if(!StainMaterial||!Surface.GetComponent()||Cast<ACharacter>(Surface.GetActor()))return;
 int32 Count=0;ADecalActor* Oldest=nullptr;
 for(TActorIterator<ADecalActor> It(GetWorld());It;++It)if(It->Tags.Contains(TEXT("CombatBloodStain"))){++Count;if(!Oldest||It->GetGameTimeSinceCreation()>Oldest->GetGameTimeSinceCreation())Oldest=*It;}
 if(Count>=128&&Oldest)Oldest->Destroy();
 FRotator Rotation=(-Surface.ImpactNormal).Rotation();Rotation.Roll=FMath::FRandRange(0.f,360.f);
 auto* Decal=GetWorld()->SpawnActor<ADecalActor>(Surface.ImpactPoint+Surface.ImpactNormal*.5f,Rotation);
 if(!Decal)return;Decal->Tags.Add(TEXT("CombatBloodStain"));
 auto* Component=Decal->GetDecal();Component->SetDecalMaterial(StainMaterial);Component->DecalSize=FVector(4.f,Size,Size);Component->SetFadeOut(8.f,2.f,false);
 Decal->AttachToComponent(Surface.GetComponent(),FAttachmentTransformRules::KeepWorldTransform,Surface.BoneName);Decal->SetLifeSpan(10.f);
}
void AImpactBloodEffect::Tick(float DT)
{
 Super::Tick(DT);Age+=DT;
 FVector Eye=GetActorLocation()+FVector(100,0,0);FRotator View;
 if(auto* PC=GetWorld()->GetFirstPlayerController())PC->GetPlayerViewPoint(Eye,View);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BloodDrops),false,this);if(Victim.IsValid())Q.AddIgnoredActor(Victim.Get());
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(DT/.016f),1,8);const float Step=DT/Steps;
 for(int32 I=0;I<Droplets.Num();++I){auto* Drop=Droplets[I].Get();if(!Drop||!Drop->IsVisible())continue;if(I==0&&Age>.65f){Drop->SetVisibility(false);continue;}
  for(int32 S=0;S<Steps;++S){const FVector Old=Drop->GetComponentLocation();Velocities[I].Z-=980.f*Step;const FVector Next=Old+Velocities[I]*Step;FHitResult Hit;
   if(GetWorld()->LineTraceSingleByObjectType(Hit,Old,Next,Objects,Q)){Stain(Hit,FMath::FRandRange(9.f,18.f));Drop->SetVisibility(false);break;}
   Drop->SetWorldLocation(Next);
  }
  if(auto* Material=Cast<UMaterialInstanceDynamic>(Drop->GetMaterial(0))){const int32 Frame=FMath::Clamp(int32(Age*24),0,12);Material->SetVectorParameterValue(TEXT("FrameOffset"),FLinearColor((Frame%4)*.25f,(Frame/4)*.25f,0,0));}
  Drop->SetWorldRotation(FRotationMatrix::MakeFromZ(Eye-Drop->GetComponentLocation()).Rotator());
 }
}
