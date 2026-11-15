#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Audio/BackgroundMusic.h"
#include "Audio/GameAudio.h"
#include "Audio/NPCVoiceLines.h"
#include "Combat/PatrolEnemy.h"
#include "Components/AudioComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "Localization/GameLanguage.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

class FJiegeVoiceCheck final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int Step=0;double Due=0;bool OldEnglish=GameLanguage::IsEnglish();
 TWeakObjectPtr<UBackgroundMusic> Audio;TWeakObjectPtr<APatrolEnemy> NPC;
public:
 explicit FJiegeVoiceCheck(FAutomationTestBase* T):Test(T){}
 ~FJiegeVoiceCheck(){GameLanguage::SetEnglish(OldEnglish);if(NPC.IsValid())NPC->Destroy();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()<Due)return false;
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC||!PC->GetPawn()){Test->AddError(TEXT("Missing player"));return true;}
  auto Device=W->GetAudioDevice();if(!Device){Test->AddError(TEXT("Missing audio device"));return true;}
  if(Step==0){
   PC->SetPause(false);Audio=UBackgroundMusic::For(PC);Audio->SetStage(EBackgroundMusicStage::Gameplay);
   auto* Dialogue=LoadObject<USoundClass>(nullptr,TEXT("/Game/Audio/GameSFX/SC_Dialogue"));
   const TCHAR* Groups[]={TEXT("Civilian"),TEXT("CivilianRun"),TEXT("FemaleHelp"),TEXT("Merchant"),TEXT("Player")};const int Counts[]={8,5,8,6,7};
   for(int G=0;G<5;++G)for(int I=1;I<=Counts[G];++I){const FString Name=FString::Printf(TEXT("Voice_%s_%02d"),Groups[G],I);auto* S=GameAudio::Sound(*Name);Test->TestTrue(*Name,S&&S->GetSoundClass()==Dialogue&&S->GetDuration()>0);}
   for(const TCHAR* Name:{TEXT("Female_Hurt"),TEXT("Female_PlayerDeath"),TEXT("Female_NPCDeath")}){auto* S=GameAudio::Sound(Name);Test->TestTrue(Name,S&&S->GetSoundClass()==Dialogue);}
   for(const TCHAR* Name:{TEXT("UI_JiegeClick"),TEXT("UI_JiegeHover"),TEXT("UI_JiegeSlide"),TEXT("UI_JiegeTick"),TEXT("UI_BloodSplash")}){auto* S=GameAudio::Sound(Name);Test->TestTrue(Name,S&&S->GetSoundClass()!=Dialogue&&S->GetDuration()>0);}
   FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
   const FTransform Spawn(View,PC->GetPawn()->GetActorLocation()+View.Vector()*600);
   NPC=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   NPC->NPCRole=ENPCRole::Civilian;NPC->FinishSpawning(Spawn);NPC->SetActorTickEnabled(false);
   NPC->Speech->SetWorldRotation((Eye-NPC->Speech->GetComponentLocation()).Rotation());
   FString Text;auto* Selected=NPCVoiceLines::Select(NPC.Get(),TEXT("HELP!"),Text,false);Test->TestTrue(TEXT("Female injured voice selection"),Selected&&Selected->GetName().StartsWith(TEXT("Voice_FemaleHelp_")));
   NPC->Say(TEXT("HELP!"));const FText Before=NPC->Speech->Text;NPC->Say(TEXT("PLEASE, NO CAMERA."));Test->TestTrue(TEXT("Text-only requests cannot overwrite current voice bubble"),NPC->Speech->Text.EqualTo(Before));NPC->Voice->Stop();
   ++Step;return false;
  }
  if(Step<=7){
   Audio->StopPlayerSpeech();Audio->NextPlayerLine=0;Audio->PlayerLineTimes.Empty();
   GameLanguage::SetEnglish(Step%2==1);
   Test->TestTrue(TEXT("Player line starts"),Audio->SayPlayer(Step));
   Test->TestFalse(TEXT("Player lines cannot overlap"),Audio->SayPlayer(Step));
   Test->TestTrue(TEXT("Actual player component is playing"),Audio->PlayerSpeech.IsValid()&&Audio->PlayerSpeech->IsPlaying());
   ++Step;Due=FPlatformTime::Seconds()+.7;return false;
  }
  if(Step==8){
   Audio->StopPlayerSpeech();Audio->NextPlayerLine=0;Audio->PlayerLineTimes.Empty();Audio->SayPlayer(6);
   Device->StartRecording(nullptr,4);
   NPC->NextVoiceTime=0;NPC->Say(TEXT("HELP!"));
   UGameplayStatics::SpawnSound2D(W,LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/Audio/Handling/PistolFire")),.5f);
   UGameplayStatics::SpawnSound2D(W,GameAudio::Sound(TEXT("UI_JiegeClick")),1.f);
   ++Step;Due=FPlatformTime::Seconds()+1;return false;
  }
  if(Step==9){
   Test->TestTrue(TEXT("Female dialogue ducks music and SFX"),Audio->IsDialogueDucking());
   GameLanguage::SetEnglish(false);
   ++Step;Due=FPlatformTime::Seconds()+.25;return false;
  }
  if(Step==10){
   TArray<FColor> Pixels;FIntVector Size;
   if(FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Jiege-subtitle-zh.png")));}
   GameLanguage::SetEnglish(true);++Step;Due=FPlatformTime::Seconds()+.2;return false;
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Jiege-subtitle-en.png")));}
  float Channels=0,Rate=0;const auto& Samples=Device->StopRecording(nullptr,Channels,Rate);float Peak=0;int Clips=0;
  TArray<int16> PCM;for(float S:Samples){Peak=FMath::Max(Peak,FMath::Abs(S));if(FMath::Abs(S)>=1)++Clips;PCM.Add(int16(FMath::Clamp(S,-1.f,1.f)*32767));}
  Test->TestTrue(TEXT("Rendered mix audible"),Peak>.001f);Test->TestEqual(TEXT("Rendered mix does not clip"),Clips,0);
  Test->AddInfo(FString::Printf(TEXT("Jiege rendered mix peak=%f samples=%d clipped=%d"),Peak,Samples.Num(),Clips));
  TArray<uint8> WAV;SerializeWaveFile(WAV,reinterpret_cast<const uint8*>(PCM.GetData()),PCM.Num()*sizeof(int16),int32(Channels),int32(Rate));FFileHelper::SaveArrayToFile(WAV,*(FPaths::ProjectSavedDir()/TEXT("Jiege-mix.wav")));
  GameAudio::PlayerHurt(PC->GetPawn());
  Test->TestTrue(TEXT("Player hurt uses the same speech channel"),Audio->PlayerSpeech.IsValid()&&Audio->PlayerSpeech->Sound->GetName()==TEXT("Female_Hurt"));
  Test->TestFalse(TEXT("Dialogue cannot overlap player hurt"),Audio->SayPlayer(1));
  GameAudio::PlayerDeath(PC->GetPawn());
  Test->TestTrue(TEXT("Player death replaces hurt on the same channel"),Audio->PlayerSpeech.IsValid()&&Audio->PlayerSpeech->Sound->GetName()==TEXT("Female_PlayerDeath"));
  Audio->StopPlayerSpeech();return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJiegeVoices,"Game.Audio.JiegeVoices",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJiegeVoices::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FJiegeVoiceCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
