#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecordedPersonComponent.generated.h"

// Runtime state belongs to the subject, so changing clips or cameras cannot score them again.
// Added only after a clip is saved; a new world/subject starts unrecorded.
UCLASS(ClassGroup=(Camera))
class GAME_JAM_CLEAN_VERS_API URecordedPersonComponent : public UActorComponent {
 GENERATED_BODY()
public:
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Recording") bool bRecorded=false;
};
