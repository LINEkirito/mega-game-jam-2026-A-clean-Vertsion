#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BackgroundMusic.generated.h"
class UAudioComponent;
class USoundClass;
class USoundMix;
class APlayerController;
UENUM()
enum class EBackgroundMusicStage : uint8 { None, Menu, Gameplay, Ending };

// Presentation-only audio, owned by the local controller and destroyed with its world.
UCLASS()
class GAME_JAM_CLEAN_VERS_API UBackgroundMusic : public UActorComponent
{
 GENERATED_BODY()
public:
 UBackgroundMusic();
 static UBackgroundMusic* For(APlayerController* Player);
 void SetStage(EBackgroundMusicStage Next);
 float GetGameplayPlaybackSeconds() const;
 EBackgroundMusicStage GetStage() const { return Stage; }
 UAudioComponent* GetMusic() const { return Music; }
 bool IsDialogueDucking() const { return bDucking; }
 bool SayPlayer(int32 Line);
 void PlayPlayerVocalization(const TCHAR* Name,float Volume);
 void StopPlayerSpeech();
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 friend class FJiegeVoiceCheck;
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> Music;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Fading;
 UPROPERTY(Transient) TObjectPtr<USoundMix> DialogueMix;
 UPROPERTY(Transient) TObjectPtr<USoundClass> MusicClass;
 UPROPERTY(Transient) TObjectPtr<USoundClass> DialogueClass;
 EBackgroundMusicStage Stage=EBackgroundMusicStage::None;
 float GameplayPlaybackSeconds=0;
 double GameplayStartedAt=0;
 bool bDucking=false;
 float SpeechHold=0,ScanRemaining=0;
 void UpdateDuck(bool Active);
 void UpdatePlayerSpeech();
 TWeakObjectPtr<UAudioComponent> PlayerSpeech;
 TSharedPtr<class SWidget> PlayerSubtitle;
 TMap<int32,float> PlayerLineTimes;
 float NextPlayerLine=0;
 bool bReactedToCombat=false,bLowHealth=false;
 int32 SeenClips=0;
};
