#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Frontend/GameFrontend.h"
#include "Frontend/LocalUserSettings.h"
#include "Audio/GameAudio.h"
#include "Audio/BackgroundMusic.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include <atomic>

struct FVolumeSample {std::atomic<bool> Ready{false};float Values[3]={-1,-1,-1};};
class FIndependentVolumesCheck final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Stage=0;double Due=0;
 const TCHAR* Keys[3]={TEXT("Voice"),TEXT("Effects"),TEXT("Music")};
 float Old[3]={1,1,1};bool Had[3]={false,false,false};bool Captured=false,WasPaused=false;
 TWeakObjectPtr<APlayerController> Player;TArray<USoundClass*> Classes;
 TSharedPtr<FVolumeSample,ESPMode::ThreadSafe> Sample;
 const float Cases[3][3]={{.25f,.6f,.8f},{.25f,0.f,.8f},{0.f,.6f,.8f}};
public:
 explicit FIndependentVolumesCheck(FAutomationTestBase* T):Test(T){}
 ~FIndependentVolumesCheck()
 {
  if(!Captured)return;
  for(int I=0;I<3;++I){if(Had[I])GConfig->SetFloat(TEXT("GameFrontend"),Keys[I],Old[I],GameLocalSettings::Filename());else GConfig->RemoveKey(TEXT("GameFrontend"),Keys[I],GameLocalSettings::Filename());}
  GConfig->Flush(false,GameLocalSettings::Filename());
  if(auto* PC=Player.Get()){GameFrontend::ApplyAudio(PC);PC->SetPause(WasPaused);}
 }
 bool Update()override
 {
  if(FPlatformTime::Seconds()<Due)return false;
  if(Stage==0)
  {
   UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
   auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC){Test->AddError(TEXT("Missing PIE player"));return true;}
   Player=PC;WasPaused=W->IsPaused();PC->SetPause(true);Captured=true;
   for(int I=0;I<3;++I)Had[I]=GConfig->GetFloat(TEXT("GameFrontend"),Keys[I],Old[I],GameLocalSettings::Filename());
   GConfig->RemoveKey(TEXT("GameFrontend"),TEXT("Voice"),GameLocalSettings::Filename());
   Test->TestEqual(TEXT("New voice preference defaults to authored mix"),GameFrontend::VoiceVolume(),1.f);
   auto* Voice=LoadObject<USoundClass>(nullptr,TEXT("/Game/Audio/GameSFX/SC_Dialogue"));
   auto* Music=LoadObject<USoundClass>(nullptr,TEXT("/Game/Audio/GameSFX/SC_Music"));
   auto* Effect=GameAudio::Sound(TEXT("UI_Click"))->GetSoundClass();while(Effect&&Effect->ParentClass)Effect=Effect->ParentClass;
   Classes={Voice,Effect,Music};
   for(auto* C:Classes)if(!Test->TestNotNull(TEXT("Sound class root exists"),C))return true;
   Test->TestTrue(TEXT("Three independent category roots"),Voice!=Effect&&Effect!=Music&&Voice!=Music&&!Voice->ParentClass&&!Music->ParentClass);
   for(const TCHAR* Path:{TEXT("/Game/Audio/GameSFX/NPC_Hurt"),TEXT("/Game/Combat/NPC/Audio/Scream"),TEXT("/Game/Combat/NPC/Audio/Help"),TEXT("/Game/Audio/GameSFX/Voice_Boss_01")})
   {auto* S=LoadObject<USoundBase>(nullptr,Path);Test->TestTrue(TEXT("Human sounds route to voice"),S&&S->GetSoundClass()==Voice);}
   Stage=1;
  }
  const int Case=(Stage-1)/3,Phase=(Stage-1)%3;
  if(Case==3)return true;
  if(Phase==0)
  {
   for(int I=0;I<3;++I)GConfig->SetFloat(TEXT("GameFrontend"),Keys[I],Cases[Case][I],GameLocalSettings::Filename());
   GameFrontend::ApplyAudio(Player.Get());Due=FPlatformTime::Seconds()+.8;++Stage;return false;
  }
  if(Phase==1)
  {
   Sample=MakeShared<FVolumeSample,ESPMode::ThreadSafe>();auto Values=Sample;auto Roots=Classes;
   auto Device=Player->GetWorld()->GetAudioDevice();
   if(!Device.IsValid()){Test->AddError(TEXT("Audio device missing"));return true;}
   FAudioThread::RunCommandOnAudioThread([Device,Values,Roots]() mutable
   {for(int I=0;I<3;++I)if(auto* P=Device->GetSoundClassCurrentProperties(Roots[I]))Values->Values[I]=P->Volume;Values->Ready.store(true);});
   ++Stage;return false;
  }
  if(!Sample->Ready.load())return false;
  for(int I=0;I<3;++I)Test->TestEqual(*FString::Printf(TEXT("Case %d independent %s gain"),Case,Keys[I]),Sample->Values[I],Cases[Case][I]*Classes[I]->Properties.Volume,.025f);
  ++Stage;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndependentVolumes,"Game.Audio.IndependentCategoryVolumes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIndependentVolumes::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FIndependentVolumesCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
