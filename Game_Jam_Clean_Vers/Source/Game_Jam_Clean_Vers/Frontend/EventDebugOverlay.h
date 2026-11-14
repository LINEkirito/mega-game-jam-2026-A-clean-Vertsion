#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
class APlayerController;
class SWidget;
namespace EventDebug { TSharedRef<SWidget> MakeOverlay(APlayerController* Player); }
#endif
