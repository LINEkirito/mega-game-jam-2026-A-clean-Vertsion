#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/GameAudio.h"
#include "Audio/AudioWidgets.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "UObject/GarbageCollection.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "ActiveSound.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameAudioAssetsTest,"Game.Audio.RequiredSFXAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameAudioAssetsTest::RunTest(const FString&)
{
 for(const TCHAR* Name:{TEXT("UI_Click"),TEXT("UI_Hover"),TEXT("UI_Slide"),TEXT("ExtractionEnter"),TEXT("ExtractionTick"),TEXT("Victory"),TEXT("PlayerHurt"),TEXT("NPC_Hurt"),TEXT("PlayerDeath"),TEXT("CameraRaise"),TEXT("CameraSwitch"),TEXT("NPC_Footstep1"),TEXT("NPC_Footstep2"),TEXT("NPC_Footstep3"),TEXT("NPC_Footstep4")})
 {
  auto* Wave=Cast<USoundWave>(GameAudio::Sound(Name));
  if(TestNotNull(Name,Wave))
  {
   TestTrue(FString(Name)+TEXT(" has decoded duration"),Wave->Duration>0&&Wave->Duration<10);
   TestEqual(FString(Name)+TEXT(" mono source"),Wave->NumChannels,1);
   TestFalse(FString(Name)+TEXT(" is one shot"),Wave->bLooping);
  }
 }
 if(auto* Hover=Cast<USoundWave>(GameAudio::Sound(TEXT("UI_Hover"))))if(auto* Click=Cast<USoundWave>(GameAudio::Sound(TEXT("UI_Click"))))
  TestTrue(TEXT("Hover is quieter than click"),Hover->Volume<Click->Volume);
 auto Button=SNew(SAudioButton).OnClicked_Lambda([]{return FReply::Handled();});
 TestTrue(TEXT("Audio wrapper retains enabled button"),Button->IsEnabled());
 auto Slider=SNew(SAudioSlider).Value(.5f);
 TestEqual(TEXT("Audio wrapper retains slider value"),Slider->GetValue(),.5f);
 TWeakObjectPtr<USoundBase> ClickSound=GameAudio::Sound(TEXT("UI_Click"));
 TWeakObjectPtr<USoundBase> HoverSound=GameAudio::Sound(TEXT("UI_Hover"));
 CollectGarbage(RF_NoFlags);
 TestTrue(TEXT("Open button keeps click audio alive through GC"),ClickSound.IsValid());
 TestTrue(TEXT("Open button keeps hover audio alive through GC"),HoverSound.IsValid());
 return true;
}
class SAudioButtonProbe : public SAudioButton
{
public: void TriggerPress(){Press();}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameAudioButtonEvents,"Game.Audio.ButtonEventsReachDevice",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameAudioButtonEvents::RunTest(const FString&)
{
 auto Device=GEngine->GetActiveAudioDevice();
 if(!TestTrue(TEXT("Active audio device exists"),Device.IsValid()))return false;
 auto Button=SNew(SAudioButtonProbe);
 auto* Click=GameAudio::Sound(TEXT("UI_Click"));auto* Hover=GameAudio::Sound(TEXT("UI_Hover"));
 Button->OnMouseEnter(FGeometry(),FPointerEvent());Button->TriggerPress();
 bool ClickPlaying=false,HoverPlaying=false;
 FAudioThread::RunCommandOnAudioThread([Device,Click,Hover,&ClickPlaying,&HoverPlaying]{
  for(const auto* Active:Device->GetActiveSounds()){ClickPlaying|=Active->GetSound()==Click;HoverPlaying|=Active->GetSound()==Hover;}
 });
 FAudioCommandFence Fence;Fence.BeginFence();Fence.Wait();
 TestTrue(TEXT("Actual press event submits UI click to audio device"),ClickPlaying);
 TestTrue(TEXT("Actual hover event submits UI hover to audio device"),HoverPlaying);
 return true;
}
#endif
