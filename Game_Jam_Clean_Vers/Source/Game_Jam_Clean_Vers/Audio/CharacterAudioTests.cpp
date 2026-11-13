#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Audio/CharacterAudio.h"
#include "Combat/PatrolEnemy.h"
#include "Components/AudioComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterAudioRangeTest,"Game.Audio.CharacterSpatialRange",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterAudioRangeTest::RunTest(const FString&)
{
    const ECharacterSound Kinds[]={ECharacterSound::Voice,ECharacterSound::Footstep,
        ECharacterSound::Handling,ECharacterSound::Gunshot,ECharacterSound::Impact};
    for(auto Kind:Kinds)
    {
        const auto Settings=CharacterAudio::Settings(Kind);
        const float Inner=Settings.AttenuationShapeExtents.X;
        const float Range=Inner+Settings.FalloffDistance;
        TestTrue(TEXT("Every physical sound attenuates and spatializes"),Settings.bAttenuate && Settings.bSpatialize);
        TestTrue(TEXT("Finite positive audible range"),FMath::IsFinite(Range) && Range>Inner);
        TestEqual(TEXT("Near source is audible"),Settings.Evaluate(FTransform::Identity,FVector::ZeroVector),1.f);
        const float Mid=Settings.Evaluate(FTransform::Identity,FVector(Inner+Settings.FalloffDistance*.5f,0,0));
        TestTrue(TEXT("Volume fades with distance"),Mid>0.f && Mid<1.f);
        for(const FVector Direction:{FVector::ForwardVector,FVector::RightVector,FVector::UpVector,-FVector::ForwardVector})
        {
            TestEqual(TEXT("Silent at outer boundary in every direction"),Settings.Evaluate(FTransform::Identity,Direction*Range),0.f);
            TestEqual(TEXT("Silent beyond range"),Settings.Evaluate(FTransform::Identity,Direction*(Range+1000.f)),0.f);
        }
        TestEqual(TEXT("No omnidirectional near-field blend"),Settings.NonSpatializedRadiusEnd,0.f);
        TestEqual(TEXT("Stereo source remains localized"),Settings.StereoSpread,0.f);
    }
    auto* Voice=GetDefault<APatrolEnemy>()->FindComponentByClass<UAudioComponent>();
    if(TestNotNull(TEXT("NPC voice source exists"),Voice))
    {
        TestTrue(TEXT("NPC voice overrides asset attenuation"),Voice->bOverrideAttenuation);
        TestTrue(TEXT("NPC voice is spatial"),Voice->bAllowSpatialization && Voice->AttenuationOverrides.bSpatialize);
        TestNotNull(TEXT("NPC voice follows its character"),Voice->GetAttachParent());
    }
    return true;
}
#endif
