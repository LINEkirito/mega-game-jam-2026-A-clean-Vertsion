#pragma once
#include "CoreMinimal.h"
class APlayerController;
class IInputProcessor;
namespace GameLanguage
{
 GAME_JAM_CLEAN_VERS_API TSharedPtr<IInputProcessor> Install(APlayerController* Player);
 GAME_JAM_CLEAN_VERS_API void Uninstall(TSharedPtr<IInputProcessor>& Handle);
}
