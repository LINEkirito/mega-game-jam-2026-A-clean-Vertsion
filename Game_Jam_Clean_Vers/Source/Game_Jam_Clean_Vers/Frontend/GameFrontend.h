#pragma once
#include "CoreMinimal.h"
class APlayerController;
class SWidget;
namespace GameFrontend
{
 float Sensitivity();
 void ApplyAudio(APlayerController* Player);
 void ReleaseAudio(APlayerController* Player);
 TSharedRef<SWidget> Main(TFunction<void()> Start,TFunction<void()> Settings,TFunction<void()> Tutorial,TFunction<void()> History,TFunction<void()> Quit);
 TSharedRef<SWidget> Settings(APlayerController* Player,TFunction<void()> Close,TFunction<void()> Language);
 TSharedRef<SWidget> Tutorial(TFunction<void()> Close);
 TSharedRef<SWidget> Pause(TFunction<void()> Resume,TFunction<void()> Settings,TFunction<void()> Unstuck,TFunction<void()> Title,bool bUnstuckFailed=false);
}
