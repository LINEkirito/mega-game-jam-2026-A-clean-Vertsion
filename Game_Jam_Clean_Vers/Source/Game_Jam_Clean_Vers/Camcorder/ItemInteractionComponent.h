#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInteractionComponent.generated.h"
class UStaticMeshComponent;
UCLASS()
class GAME_JAM_CLEAN_VERS_API UItemInteractionComponent:public UActorComponent {
 GENERATED_BODY()
public:
 UItemInteractionComponent();
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn) override;
 void Play(UStaticMeshComponent* Item);
 bool GetLeftWristTarget(FTransform& Out) const { if(Age>=.7f)return false;Out=Wrist;return true; }
private:
 UPROPERTY() TObjectPtr<UStaticMeshComponent> ItemVisual;
 FTransform Wrist;float Age=1;
 void Finish();
};
