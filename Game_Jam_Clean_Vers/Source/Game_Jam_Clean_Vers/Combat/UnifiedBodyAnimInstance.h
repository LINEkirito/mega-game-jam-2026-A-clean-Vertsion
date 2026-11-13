#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "UnifiedBodyAnimInstance.generated.h"

// Animates the existing first-person body from the original body pose plus weapon layers.
// Never creates another rendering component.
UCLASS(Transient)
class GAME_JAM_CLEAN_VERS_API UUnifiedBodyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TMap<FString,TObjectPtr<class UAnimSequence>> CachedAnimations;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
