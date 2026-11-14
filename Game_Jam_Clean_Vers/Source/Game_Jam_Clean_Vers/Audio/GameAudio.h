#pragma once
#include "CoreMinimal.h"
#include "Sound/SlateSound.h"
class AActor;
class USoundBase;
namespace GameAudio
{
 USoundBase* Sound(const TCHAR* Name);
 FSlateSound UISound(const TCHAR* Name);
 void Cue(const UObject* Context,const TCHAR* Name,float Volume=.35f);
 void Handling(AActor* Actor,const TCHAR* Name);
 void PlayerHurt(AActor* Actor);
 void PlayerDeath(AActor* Actor);
}
