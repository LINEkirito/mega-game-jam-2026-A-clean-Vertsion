#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Audio/GameAudio.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Combat/PatrolEnemy.h"
#include "Extraction/ExtractionConvoyCar.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "ActiveSound.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/AutomationTest.h"

namespace
{
 int32 Playing(UWorld* World,const TCHAR* Name)
 {
  auto Device=World->GetAudioDevice();auto* Sound=GameAudio::Sound(Name);int32 Count=0;
  if(!Device.IsValid())return 0;
  FAudioThread::RunCommandOnAudioThread([Device,Sound,&Count]{for(auto* Active:Device->GetActiveSounds())if(Active->GetSound()==Sound)++Count;});
  FAudioCommandFence Fence;Fence.BeginFence();Fence.Wait();return Count;
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNewSFXAssets,"Game.Audio.NewSFX.Assets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNewSFXAssets::RunTest(const FString&)
{
 for(const TCHAR* Name:{TEXT("CameraZoom"),TEXT("CameraRecordStart"),TEXT("CameraRecordSaved"),TEXT("ExtractionHeartbeat"),TEXT("PoliceSiren"),TEXT("GangBrake1"),TEXT("GangBrake2")})
 {
  auto* Wave=Cast<USoundWave>(GameAudio::Sound(Name));if(!TestNotNull(Name,Wave))continue;
  TestEqual(FString(Name)+TEXT(" is mono"),Wave->NumChannels,1);
  TestTrue(FString(Name)+TEXT(" has audio data"),Wave->Duration>.1f&&Wave->Duration<20.f);
  TestEqual(FString(Name)+TEXT(" loop policy"),bool(Wave->bLooping),FString(Name)==TEXT("ExtractionHeartbeat")||FString(Name)==TEXT("PoliceSiren"));
 }
 return true;
}
class FNewSFXCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Stage=0;double Started=FPlatformTime::Seconds();float RecordStart=0;
 TWeakObjectPtr<AExtractionConvoyCar> Police;
public:
 explicit FNewSFXCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  if(!W){Test->AddError(TEXT("Missing PIE world"));return true;}
  auto* PC=W->GetFirstPlayerController();auto* E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
  auto* Camera=E?E->Camcorder.Get():nullptr;
  if(!Camera){Test->AddError(TEXT("Missing equipped camera"));return true;}
  if(FPlatformTime::Seconds()-Started>45){Test->AddError(TEXT("SFX integration timeout"));return true;}
  if(Stage==0)
  {
   PC->SetPause(false);PC->bShowMouseCursor=false;
   for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
   if(!Camera->bHeld||Camera->EquipProgress<1.f)return false;
   Test->TestFalse(TEXT("Recording rejected without raised viewfinder"),Camera->StartRecording());
   Test->TestEqual(TEXT("Rejected start is silent"),Playing(W,TEXT("CameraRecordStart")),0);
   Camera->SetViewfinderRaised(true);Camera->AdjustFocalLength(2.f);
   Test->TestTrue(TEXT("Real lens change plays zoom"),IsValid(Camera->ZoomAudio)&&Camera->ZoomAudio->IsPlaying());
   auto* Zoom=Camera->ZoomAudio.Get();Camera->AdjustFocalLength(2.f);
   Test->TestTrue(TEXT("Rapid scrolling keeps a single zoom voice"),Zoom==Camera->ZoomAudio);
   Camera->SetViewfinderRaised(false);
   Test->TestFalse(TEXT("Lowering stops zoom"),IsValid(Zoom)&&Zoom->IsPlaying());
   Camera->FocalLength=100.f;Camera->SetViewfinderRaised(true);Camera->AdjustFocalLength(2.f);
   Test->TestFalse(TEXT("Clamped zoom is silent"),IsValid(Camera->ZoomAudio)&&Camera->ZoomAudio->IsPlaying());
   Test->TestTrue(TEXT("Recording starts successfully"),Camera->StartRecording());
   Test->TestEqual(TEXT("Start cue reaches audio device"),Playing(W,TEXT("CameraRecordStart")),1);
   Camera->StartRecording();Test->TestEqual(TEXT("Repeated start does not duplicate cue"),Playing(W,TEXT("CameraRecordStart")),1);
   RecordStart=W->GetTimeSeconds();++Stage;return false;
  }
  if(Stage==1)
  {
   if(W->GetTimeSeconds()-RecordStart<.6f)return false;
   const int32 Saved=Camera->SavedClipCount;Camera->StopRecording();
   Test->TestEqual(TEXT("Video successfully finalized"),Camera->SavedClipCount,Saved+1);
   Test->TestEqual(TEXT("Successful save reaches audio device"),Playing(W,TEXT("CameraRecordSaved")),1);
   Camera->StopRecording();Test->TestEqual(TEXT("Repeated stop is silent"),Playing(W,TEXT("CameraRecordSaved")),1);
   const FVector Start=PC->GetPawn()->GetActorLocation()+FVector(0,2000,5000);
   FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   auto* Car=W->SpawnActor<AExtractionConvoyCar>(Start,FRotator::ZeroRotator,Params);
   if(!Test->TestNotNull(TEXT("Police audio fixture"),Car))return true;
   Car->SetActorTickEnabled(false);Car->Initialize(PC,Start+FVector(1000,0,0),Start,true);
   Police=Car;
   Test->TestTrue(TEXT("Police arrival starts siren"),IsValid(Car->SirenAudio)&&Car->SirenAudio->IsPlaying());
   Test->TestEqual(TEXT("Siren reaches audio device"),Playing(W,TEXT("PoliceSiren")),1);
   if(Car->SirenAudio)
   {
    Test->TestTrue(TEXT("Siren follows car"),Car->SirenAudio->GetAttachParent()==Car->GetRootComponent());
    const auto* Settings=Car->SirenAudio->GetAttenuationSettingsToApply();
    Test->TestTrue(TEXT("Siren has spatial distance attenuation"),Settings&&Settings->bSpatialize&&Settings->bAttenuate&&Settings->FalloffDistance==5700.f);
   }
   Car->LeaveWithoutUnloading();RecordStart=W->GetTimeSeconds();++Stage;return false;
  }
  if(W->GetTimeSeconds()-RecordStart<.5f)return false;
  auto* Car=Police.Get();
  Test->TestTrue(TEXT("Aborted arrival stops siren after fade"),Car&&(!IsValid(Car->SirenAudio)||!Car->SirenAudio->IsPlaying()));
  if(Car)
  {
   Car->Initialize(PC,Car->GetActorLocation(),Car->GetActorLocation(),true);
   auto* Siren=Car->SirenAudio.Get();Car->Destroy();
   Test->TestFalse(TEXT("Destroying vehicle stops looping siren"),IsValid(Siren)&&Siren->IsPlaying());
  }
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNewSFXIntegration,"Game.Audio.NewSFX.Integration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNewSFXIntegration::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FNewSFXCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
