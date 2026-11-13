#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RecordingSceneRegion.generated.h"
class UBoxComponent;
// Authored area identity, independent of map name and event level. Never auto-placed.
UCLASS()
class GAME_JAM_CLEAN_VERS_API ARecordingSceneRegion : public AActor {
 GENERATED_BODY()
public:
 ARecordingSceneRegion();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> Bounds;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recording") FName SceneId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recording") FString SceneName;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recording") int32 Priority=0;
 bool Contains(FVector Point) const;
};
