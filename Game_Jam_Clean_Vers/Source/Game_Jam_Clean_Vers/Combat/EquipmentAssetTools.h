#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EquipmentAssetTools.generated.h"
class USkeletalMesh;
class UStaticMesh;
class UAnimSequence;

UCLASS()
class GAME_JAM_CLEAN_VERS_API UEquipmentAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Equipment|Authoring")
    static bool BuildPackChargeAnimations();
    UFUNCTION(BlueprintCallable,Category="Equipment|Authoring")
    static bool LockUpperBodyPose(UAnimSequence* Animation,UAnimSequence* Pose);
    // Editor-only implementation; creates a new asset and never modifies the source mesh.
    UFUNCTION(BlueprintCallable,Category="Equipment|Authoring")
    static USkeletalMesh* MakeArmsMesh(USkeletalMesh* Source,const FString& Destination,bool ForearmsOnly=false);
    UFUNCTION(BlueprintCallable,Category="Equipment|Authoring")
    static UStaticMesh* ImportReferenceGeometry(const FString& JsonFile,const FString& Destination);
    UFUNCTION(BlueprintCallable,Category="Equipment|Authoring")
    static bool PreviewInput(const FString& Key,bool Pressed);
};
