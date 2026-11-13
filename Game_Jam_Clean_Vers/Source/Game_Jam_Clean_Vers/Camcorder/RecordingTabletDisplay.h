#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RecordingTabletDisplay.generated.h"
UCLASS()
class GAME_JAM_CLEAN_VERS_API ARecordingTabletDisplay : public AActor {
 GENERATED_BODY()
public:
 ARecordingTabletDisplay();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> Mesh;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class USceneCaptureComponent2D> Capture;
 UPROPERTY(Transient) TObjectPtr<class UTextureRenderTarget2D> Texture;
};
