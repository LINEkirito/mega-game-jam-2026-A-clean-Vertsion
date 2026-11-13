#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NPCAssetTools.generated.h"

UCLASS()
class GAME_JAM_CLEAN_VERS_API UNPCAssetTools : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 // Creates independent Manny animation assets; reference assets are never modified.
 UFUNCTION(BlueprintCallable,Category="NPC|Authoring") static bool BuildNPCAnimations();
 UFUNCTION(BlueprintCallable,Category="NPC|Authoring") static AActor* AddNavigationBounds(UObject* WorldContext,FVector Center,FVector Size);
};
