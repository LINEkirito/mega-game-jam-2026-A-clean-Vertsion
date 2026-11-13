#pragma once
#include "CoreMinimal.h"
#include "Sound/SoundAttenuation.h"

class UAudioComponent;
class USceneComponent;
class USoundBase;

enum class ECharacterSound : uint8 { Voice, Footstep, Handling, Gunshot, Impact };

namespace CharacterAudio
{
    FSoundAttenuationSettings Settings(ECharacterSound Kind);
    UAudioComponent* At(const UObject* Context, USoundBase* Sound, FVector Location,
        ECharacterSound Kind, float Volume=1.f, float Pitch=1.f);
    UAudioComponent* Attached(USoundBase* Sound, USceneComponent* Source,
        ECharacterSound Kind, float Volume=1.f, float Pitch=1.f, float StartTime=0.f);
}
