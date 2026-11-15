#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Audio/BackgroundMusic.h"
#include "Audio/GameAudio.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundClass.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AudioDevice.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

class FBGMPlaybackCheck final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 int32 Step=0;
 double Due=0;
 TWeakObjectPtr<UBackgroundMusic> Music;
 TWeakObjectPtr<UAudioComponent> Voice,First;
public:
 explicit FBGMPlaybackCheck(FAutomationTestBase* T):Test(T){}
 virtual bool Update() override
 {
  if(FPlatformTime::Seconds()<Due)return false;
  UWorld* World=nullptr;
  for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
  if(!World){Test->AddError(TEXT("PIE world missing"));return true;}
  auto* PC=World->GetFirstPlayerController();if(!PC)return false;
  switch(Step++)
  {
   case 0:
    Music=UBackgroundMusic::For(PC);
    if(!Test->TestNotNull(TEXT("Music controller"),Music.Get()))return true;
    for(const TCHAR* Name:{TEXT("BGM_Menu"),TEXT("BGM_Main"),TEXT("BGM_End")}){
     auto* Wave=Cast<USoundWave>(GameAudio::Sound(Name));if(!Test->TestNotNull(Name,Wave))continue;
     Test->TestTrue(TEXT("Music loops"),Wave->bLooping);
     Test->TestEqual(TEXT("Stereo music"),Wave->NumChannels,2);
     Test->TestTrue(TEXT("Independent music class"),Wave->GetSoundClass()&&Wave->GetSoundClass()->Properties.bIsMusic&&!Wave->GetSoundClass()->ParentClass);
    }
    Music->SetStage(EBackgroundMusicStage::Menu);First=Music->GetMusic();break;
   case 1:
    Test->TestTrue(TEXT("Menu plays"),First.IsValid()&&First->IsPlaying());
    Music->SetStage(EBackgroundMusicStage::Menu);
    Test->TestEqual(TEXT("Same-stage request does not restart"),Music->GetMusic(),First.Get());
    PC->SetPause(true);break;
   case 2:
    Test->TestTrue(TEXT("Menu remains audible when world is paused"),First.IsValid()&&First->IsPlaying()&&First->bIsUISound&&!First->bIsPaused);
    PC->SetPause(false);Music->SetStage(EBackgroundMusicStage::Gameplay);break;
   case 3:
    Test->TestTrue(TEXT("Old track stopped after crossfade"),!First.IsValid()||!First->IsPlaying());
    Test->TestEqual(TEXT("Gameplay track selected"),Music->GetMusic()->Sound->GetName(),FString(TEXT("BGM_Main")));
    Voice=UGameplayStatics::SpawnSound2D(World,GameAudio::Sound(TEXT("Voice_Boss_02")),1.25f);break;
   case 4:
    Test->TestTrue(TEXT("Dialogue ducks effects and music"),Music->IsDialogueDucking());
    if(Voice.IsValid())Voice->Stop();break;
   case 5:
    Test->TestFalse(TEXT("Mix restores after dialogue"),Music->IsDialogueDucking());
    Music->SetStage(EBackgroundMusicStage::Ending);PC->SetPause(true);break;
   case 6:
    Test->TestEqual(TEXT("Ending track selected"),Music->GetMusic()->Sound->GetName(),FString(TEXT("BGM_End")));
    Test->TestTrue(TEXT("Ending plays over paused settlement"),Music->GetMusic()->IsPlaying()&&Music->GetMusic()->bIsUISound);
    PC->SetPause(false);Music->SetStage(EBackgroundMusicStage::None);break;
   default:return true;
  }
  Due=FPlatformTime::Seconds()+.8;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBGMPlayback,"Game.Audio.BackgroundMusic",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBGMPlayback::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FBGMPlaybackCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}

class FBGMRenderedMixCheck final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Step=0;double Due=0;
 TWeakObjectPtr<UBackgroundMusic> Music;
 TWeakObjectPtr<UAudioComponent> Siren;
public:
 explicit FBGMRenderedMixCheck(FAutomationTestBase* T):Test(T){}
 virtual bool Update() override
 {
  if(FPlatformTime::Seconds()<Due)return false;
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC){Test->AddError(TEXT("Missing PIE player"));return true;}
  auto Device=W->GetAudioDevice();if(!Device){Test->AddError(TEXT("Missing audio device"));return true;}
  if(Step++==0){
   PC->SetPause(false);Music=UBackgroundMusic::For(PC);Music->SetStage(EBackgroundMusicStage::Gameplay);
   Due=FPlatformTime::Seconds()+1;return false;
  }
  if(Step==2){
   Device->StartRecording(nullptr,3);
   UGameplayStatics::SpawnSound2D(W,GameAudio::Sound(TEXT("Voice_Boss_02")),1.25f);
   Siren=UGameplayStatics::SpawnSound2D(W,GameAudio::Sound(TEXT("PoliceSiren")),.3f);
   UGameplayStatics::SpawnSound2D(W,GameAudio::Sound(TEXT("CameraRecordSaved")),.35f);
   Due=FPlatformTime::Seconds()+2;return false;
  }
  float Channels=0,Rate=0;const auto& Samples=Device->StopRecording(nullptr,Channels,Rate);
  double Energy=0;float Peak=0;int32 Clips=0;
  for(float Sample:Samples){Energy+=double(Sample)*Sample;Peak=FMath::Max(Peak,FMath::Abs(Sample));if(FMath::Abs(Sample)>=1.f)++Clips;}
  Test->TestTrue(TEXT("Actual mixed output is non-silent"),Samples.Num()>0&&Peak>.001f);
  Test->TestEqual(TEXT("Dialogue plus SFX plus music has no clipped samples"),Clips,0);
  Test->TestTrue(TEXT("Ducking active during captured dialogue"),Music->IsDialogueDucking());
  const FString Result=FString::Printf(TEXT("samples=%d channels=%.0f sample_rate=%.0f peak=%.6f rms=%.6f clipped=%d"),Samples.Num(),Channels,Rate,Peak,FMath::Sqrt(Energy/FMath::Max(1,Samples.Num())),Clips);
  Test->AddInfo(Result);
  FFileHelper::SaveStringToFile(Result,*(FPaths::ProjectSavedDir()/TEXT("BGM-Mix-Validation.txt")));
  if(Siren.IsValid())Siren->Stop();Music->SetStage(EBackgroundMusicStage::None);
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBGMRenderedMix,"Game.Audio.BackgroundMusicRenderedMix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBGMRenderedMix::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FBGMRenderedMixCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
