#include "ItemInteractionComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
UItemInteractionComponent::UItemInteractionComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;}
void UItemInteractionComponent::Finish(){if(ItemVisual)ItemVisual->SetVisibility(false);Age=1;}
void UItemInteractionComponent::Play(UStaticMeshComponent* Item){
 auto* C=Cast<AGame_Jam_Clean_VersCharacter>(GetOwner());if(!C||!Item)return;Finish();
 if(!ItemVisual){ItemVisual=NewObject<UStaticMeshComponent>(C);ItemVisual->SetupAttachment(C->GetFirstPersonCameraComponent());ItemVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);ItemVisual->SetCastShadow(false);ItemVisual->RegisterComponent();}
 ItemVisual->SetStaticMesh(Item->GetStaticMesh());ItemVisual->SetWorldScale3D(Item->GetComponentScale());for(int32 I=0;I<Item->GetNumMaterials();++I)ItemVisual->SetMaterial(I,Item->GetMaterial(I));ItemVisual->SetDefaultCustomPrimitiveDataFloatArray(0,Item->GetDefaultCustomPrimitiveData().Data);ItemVisual->SetCustomPrimitiveDataFloatArray(0,Item->GetCustomPrimitiveData().Data);
 Wrist=C->GetFirstPersonMesh()->GetSocketTransform(TEXT("hand_l"));Age=0;ItemVisual->SetVisibility(false);
}
void UItemInteractionComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn){
 Super::TickComponent(DT,Type,Fn);if(Age>=.7f)return;Age+=DT;auto* C=Cast<AGame_Jam_Clean_VersCharacter>(GetOwner());if(!C||C->Health<=0||Age>=.7f){Finish();return;}
 const FTransform View(C->GetControlRotation(),C->GetFirstPersonCameraComponent()->GetComponentLocation());
 auto Smooth=[](float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);};
 const FVector Palm=View.TransformPosition(Age<.3f?FMath::Lerp(FVector(12,-22,-35),FVector(40,-12,-20),Smooth(Age/.3f)):FMath::Lerp(FVector(40,-12,-20),FVector(12,-24,-38),Smooth((Age-.3f)/.4f)));
 Wrist.SetLocation(Palm);
 ItemVisual->SetVisibility(Age>=.27f&&Age<.6f);ItemVisual->SetWorldLocationAndRotation(Palm+View.TransformVectorNoScale(FVector(3,0,3)),View.Rotator());
}
