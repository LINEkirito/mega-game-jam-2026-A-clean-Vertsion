#include "Audio/CharacterAudio.h"
#include "Data/GlobalData.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"

FSoundAttenuationSettings CharacterAudio::Settings(ECharacterSound Kind)
{
    const auto* Config=GetDefault<UGlobalData>();
    float Range=Config->VoiceAudibleDistance;
    switch(Kind)
    {
    case ECharacterSound::Footstep: Range=Config->FootstepAudibleDistance; break;
    case ECharacterSound::Handling: Range=Config->HandlingAudibleDistance; break;
    case ECharacterSound::Gunshot: Range=Config->GunshotAudibleDistance; break;
    case ECharacterSound::Impact: Range=Config->ImpactAudibleDistance; break;
    default: break;
    }
    Range=FMath::Max(100.f,Range);
    FSoundAttenuationSettings Result;
    Result.bAttenuate=true;
    Result.bSpatialize=true;
    Result.DistanceAlgorithm=EAttenuationDistanceModel::Linear;
    Result.AttenuationShape=EAttenuationShape::Sphere;
    Result.AttenuationShapeExtents=FVector(FMath::Min(100.f,Range*.1f),0,0);
    Result.FalloffDistance=Range-Result.AttenuationShapeExtents.X;
    Result.SpatializationAlgorithm=SPATIALIZATION_Default;
    Result.NonSpatializedRadiusStart=0.f;
    Result.NonSpatializedRadiusEnd=0.f;
    Result.StereoSpread=0.f;
    return Result;
}

UAudioComponent* CharacterAudio::At(const UObject* Context, USoundBase* Sound, FVector Location,
    ECharacterSound Kind, float Volume, float Pitch)
{
    if(!Context || !Sound)return nullptr;
    auto* Attenuation=NewObject<USoundAttenuation>();
    Attenuation->Attenuation=Settings(Kind);
    return UGameplayStatics::SpawnSoundAtLocation(Context,Sound,Location,FRotator::ZeroRotator,
        Volume,Pitch,0.f,Attenuation);
}

UAudioComponent* CharacterAudio::Attached(USoundBase* Sound, USceneComponent* Source,
    ECharacterSound Kind, float Volume, float Pitch, float StartTime)
{
    if(!Source || !Sound)return nullptr;
    auto* Attenuation=NewObject<USoundAttenuation>();
    Attenuation->Attenuation=Settings(Kind);
    return UGameplayStatics::SpawnSoundAttached(Sound,Source,NAME_None,FVector::ZeroVector,
        FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,Volume,Pitch,StartTime,Attenuation);
}
