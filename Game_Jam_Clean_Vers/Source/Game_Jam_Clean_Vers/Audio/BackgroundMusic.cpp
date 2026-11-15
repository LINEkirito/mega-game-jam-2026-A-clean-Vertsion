#include "Audio/BackgroundMusic.h"
#include "Audio/GameAudio.h"
#include "Frontend/GameFrontend.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "PlayerVoicePresentation.inl"

UBackgroundMusic::UBackgroundMusic()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.bTickEvenWhenPaused=true;
}
UBackgroundMusic* UBackgroundMusic::For(APlayerController* Player)
{
 if(!Player||!Player->IsLocalController())return nullptr;
 if(auto* Existing=Player->FindComponentByClass<UBackgroundMusic>())return Existing;
 auto* Result=NewObject<UBackgroundMusic>(Player,TEXT("BackgroundMusic"));
 Player->AddInstanceComponent(Result);
 Result->MusicClass=LoadObject<USoundClass>(nullptr,TEXT("/Game/Audio/GameSFX/SC_Music.SC_Music"));
 Result->DialogueClass=LoadObject<USoundClass>(nullptr,TEXT("/Game/Audio/GameSFX/SC_Dialogue.SC_Dialogue"));
 Result->RegisterComponent();
 // Carry only the presentation cursor through the existing restart URL.
 if(auto* Mode=Player->GetWorld()->GetAuthGameMode())
  if(UGameplayStatics::HasOption(Mode->OptionsString,TEXT("StartRun")))
   Result->GameplayPlaybackSeconds=FMath::Max(0.f,FCString::Atof(*UGameplayStatics::ParseOption(Mode->OptionsString,TEXT("MainMusicTime"))));
 // Load the independent class roots before applying the saved sliders.
 GameFrontend::ApplyAudio(Player);
 return Result;
}
float UBackgroundMusic::GetGameplayPlaybackSeconds() const
{
 const float Seconds=GameplayPlaybackSeconds+(Stage==EBackgroundMusicStage::Gameplay&&Music?float(FPlatformTime::Seconds()-GameplayStartedAt):0.f);
 const auto* Wave=Cast<USoundWave>(GameAudio::Sound(TEXT("BGM_Main")));
 return Wave&&Wave->Duration>0?FMath::Fmod(Seconds,Wave->Duration):0.f;
}
void UBackgroundMusic::SetStage(EBackgroundMusicStage Next)
{
 if(Stage==Next)return;
 if(Stage==EBackgroundMusicStage::Gameplay)GameplayPlaybackSeconds=GetGameplayPlaybackSeconds();
 if(Music){Music->FadeOut(.4f,0.f);Fading.Add(Music);Music=nullptr;}
 Stage=Next;
 const TCHAR* Name=Next==EBackgroundMusicStage::Menu?TEXT("BGM_Menu"):
  Next==EBackgroundMusicStage::Gameplay?TEXT("BGM_Main"):
  Next==EBackgroundMusicStage::Ending?TEXT("BGM_End"):nullptr;
 if(!Name)return;
 auto* Sound=GameAudio::Sound(Name);if(!Sound)return;
 // Higher music ceiling; the player's independent 0-100% slider still scales it.
 const float Level=Next==EBackgroundMusicStage::Menu?.8f:Next==EBackgroundMusicStage::Gameplay?.9f:.7f;
 Music=UGameplayStatics::CreateSound2D(this,Sound,Level,1.f,0.f,nullptr,false,false);
 if(Music){Music->SoundClassOverride=MusicClass;Music->bIsUISound=true;if(Next==EBackgroundMusicStage::Gameplay)GameplayStartedAt=FPlatformTime::Seconds();
  Music->FadeIn(.5f,Level,Next==EBackgroundMusicStage::Gameplay?GameplayPlaybackSeconds:0.f);}
}
void UBackgroundMusic::UpdateDuck(bool Active)
{
 if(!DialogueMix){DialogueMix=NewObject<USoundMix>(this);DialogueMix->Duration=-1;DialogueMix->FadeInTime=0;UGameplayStatics::PushSoundMixModifier(this,DialogueMix);}
 bDucking=Active;
 for(TObjectIterator<USoundClass> It;It;++It)
 {
  auto* Class=*It;
  if(Class->HasAnyFlags(RF_ClassDefaultObject)||Class->ParentClass||Class==DialogueClass)continue;
  const float Gain=Active?(Class==MusicClass?.32f:.5f):1.f;
  UGameplayStatics::SetSoundMixClassOverride(this,DialogueMix,Class,Gain,1.f,Active?.08f:.65f,true);
 }
}
void UBackgroundMusic::TickComponent(float DT,ELevelTick TickType,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,TickType,Function);
 UpdatePlayerSpeech();
 for(int32 I=Fading.Num()-1;I>=0;--I)if(!IsValid(Fading[I])||!Fading[I]->IsPlaying()){
  if(IsValid(Fading[I]))Fading[I]->DestroyComponent();Fading.RemoveAt(I);
 }
 SpeechHold=GetWorld()->IsPaused()?0.f:FMath::Max(0.f,SpeechHold-DT);ScanRemaining-=DT;
 if(GetWorld()->IsPaused())ScanRemaining=0;
 if(ScanRemaining>0)return;ScanRemaining=.05f;
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC)return;
 FVector Listener;FRotator Rotation;PC->GetPlayerViewPoint(Listener,Rotation);
 // Only voices in this world that can reach this listener reduce the mix.
 if(GameFrontend::VoiceVolume()<=0)SpeechHold=0;
 if(!GetWorld()->IsPaused()&&GameFrontend::VoiceVolume()>0)for(TObjectIterator<UAudioComponent> It;It;++It){
  auto* Voice=*It;
  if(Voice->GetWorld()!=GetWorld()||!Voice->IsPlaying()||Voice->bIsPaused||Voice->VolumeMultiplier<=0||!Voice->Sound)continue;
  if(Voice->Sound->GetSoundClass()!=DialogueClass)continue;
  const auto* Attenuation=Voice->GetAttenuationSettingsToApply();
  if(!Attenuation||!Attenuation->bAttenuate||Attenuation->Evaluate(Voice->GetComponentTransform(),Listener)>.1f){SpeechHold=.3f;break;}
 }
 const bool Active=SpeechHold>0;
 if(Active!=bDucking||!DialogueMix)UpdateDuck(Active);
}
void UBackgroundMusic::EndPlay(const EEndPlayReason::Type Reason)
{
 StopPlayerSpeech();
 if(Music){Music->Stop();Music->DestroyComponent();Music=nullptr;}
 for(auto Audio:Fading)if(IsValid(Audio)){Audio->Stop();Audio->DestroyComponent();}Fading.Empty();
 if(DialogueMix){UGameplayStatics::PopSoundMixModifier(this,DialogueMix);DialogueMix=nullptr;}
 Stage=EBackgroundMusicStage::None;
 Super::EndPlay(Reason);
}
