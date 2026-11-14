#include "Audio/GameAudio.h"
#include "Audio/CharacterAudio.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
USoundBase* GameAudio::Sound(const TCHAR* Name)
{return LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/Audio/GameSFX/"))+Name));}
FSlateSound GameAudio::UISound(const TCHAR* Name)
{FSlateSound Result;Result.SetResourceObject(Sound(Name));return Result;}
void GameAudio::Cue(const UObject* Context,const TCHAR* Name,float Volume)
{if(Context&&Context->GetWorld())if(auto* S=Sound(Name))UGameplayStatics::PlaySound2D(Context,S,Volume);}
void GameAudio::Handling(AActor* Actor,const TCHAR* Name)
{if(Actor)CharacterAudio::Attached(Sound(Name),Actor->GetRootComponent(),ECharacterSound::Handling,.3f);}
void GameAudio::PlayerHurt(AActor* Actor)
{if(Actor)CharacterAudio::Attached(Sound(TEXT("PlayerHurt")),Actor->GetRootComponent(),ECharacterSound::Voice,.6f);}
void GameAudio::PlayerDeath(AActor* Actor)
{if(Actor)CharacterAudio::Attached(Sound(TEXT("PlayerDeath")),Actor->GetRootComponent(),ECharacterSound::Voice,.65f);}
