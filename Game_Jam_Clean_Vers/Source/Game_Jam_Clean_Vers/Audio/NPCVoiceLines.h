#pragma once
#include "CoreMinimal.h"
class APatrolEnemy;
class USoundBase;
namespace NPCVoiceLines { USoundBase* Select(const APatrolEnemy* NPC,const FString& Intent,FString& SpokenText,bool Filming); }
