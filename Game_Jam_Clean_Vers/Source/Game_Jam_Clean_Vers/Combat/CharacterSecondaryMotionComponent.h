#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterSecondaryMotion.h"
#include "CharacterSecondaryMotionComponent.generated.h"

// Separate accessory skeletons preserve the purchased weapon rig unchanged.
UCLASS(ClassGroup=(Animation))
class GAME_JAM_CLEAN_VERS_API UCharacterSecondaryMotionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCharacterSecondaryMotionComponent();
 void Initialize(class USkeletalMeshComponent* Source);
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Secondary Motion") TObjectPtr<class UPoseableMeshComponent> Hair;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Secondary Motion") TObjectPtr<class UPoseableMeshComponent> Cloth;
private:
 UPROPERTY() TObjectPtr<class USkeletalMeshComponent> Body;
 FCharacterSecondaryMotion HairMotion,ClothMotion;
 TArray<int32> HairSourceBones,ClothSourceBones;
 FDelegateHandle FinalPoseHandle;
 void RefreshAttachmentsAfterAnimation();
 void CopyAnimatedPose(UPoseableMeshComponent* Target,TArray<int32>& SourceBones);
};
