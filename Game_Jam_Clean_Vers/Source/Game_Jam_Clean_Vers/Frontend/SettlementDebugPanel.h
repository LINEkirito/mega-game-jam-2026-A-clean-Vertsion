#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
class SWidget;
namespace SettlementDebug { TSharedRef<SWidget> Make(TFunction<void()> Close); }
#endif
