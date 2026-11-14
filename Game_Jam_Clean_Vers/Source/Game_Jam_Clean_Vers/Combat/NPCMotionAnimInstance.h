#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NPCMotionAnimInstance.generated.h"

// Per-NPC animation state. Locomotion never resets when the weapon fires.
UCLASS(Transient)
class GAME_JAM_CLEAN_VERS_API UNPCMotionAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 UPROPERTY(Transient,BlueprintReadOnly,Category="NPC|Motion") TObjectPtr<class UAnimSequence> Sequence;
 UPROPERTY(Transient,BlueprintReadOnly,Category="NPC|Motion") TObjectPtr<class UAnimSequence> UpperBodyPose;
 // Turn playback and actor yaw share the same clock.
 UPROPERTY(Transient,BlueprintReadOnly,Category="NPC|Motion") float ExplicitTime=-1.f;
 UPROPERTY(Transient,BlueprintReadOnly,Category="NPC|Motion") float PlaybackRate=1.f;
 bool bLoop=true;
 bool bLocomotion=false;
 float PoseBlendDuration=.18f;
 // The owning NPC retains the idle asset; this is only a per-frame pose input.
 UAnimSequence* StrideIdle=nullptr;
 float StrideWeight=1.f;
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
