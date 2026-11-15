#include "Audio/GameAudio.h"
#include "Audio/CharacterAudio.h"
#include "Audio/BackgroundMusic.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
USoundBase* GameAudio::Sound(const TCHAR* Name)
{return LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/Audio/GameSFX/"))+Name));}
FSlateSound GameAudio::UISound(const TCHAR* Name)
{FSlateSound Result;const TCHAR* Mapped=FCString::Strcmp(Name,TEXT("UI_Click"))==0?TEXT("UI_JiegeClick"):Name;Result.SetResourceObject(Sound(Mapped));return Result;}
void GameAudio::Cue(const UObject* Context,const TCHAR* Name,float Volume)
{if(Context&&Context->GetWorld())if(auto* S=Sound(Name))UGameplayStatics::PlaySound2D(Context,S,Volume);}
void GameAudio::Handling(AActor* Actor,const TCHAR* Name)
{if(Actor)CharacterAudio::Attached(Sound(Name),Actor->GetRootComponent(),ECharacterSound::Handling,.3f);}
void GameAudio::PlayerLine(const UObject* Context,int32 Line)
{
 if(auto* PC=UGameplayStatics::GetPlayerController(Context,0))
  if(auto* Audio=UBackgroundMusic::For(PC))Audio->SayPlayer(Line);
}
void GameAudio::PlayerHurt(AActor* Actor)
{
 if(!Actor)return;
 if(auto* PC=UGameplayStatics::GetPlayerController(Actor,0))
  if(auto* Audio=UBackgroundMusic::For(PC))Audio->PlayPlayerVocalization(TEXT("Female_Hurt"),.7f);
}
void GameAudio::PlayerDeath(AActor* Actor)
{
 if(!Actor)return;
 if(auto* PC=UGameplayStatics::GetPlayerController(Actor,0))
  if(auto* Audio=UBackgroundMusic::For(PC))Audio->PlayPlayerVocalization(TEXT("Female_PlayerDeath"),.7f);
}
