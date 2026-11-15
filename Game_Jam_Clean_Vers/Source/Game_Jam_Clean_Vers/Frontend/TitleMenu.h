#pragma once
#include "CoreMinimal.h"
class SWidget;

namespace TitleMenu
{
 // Presentation only; actions continue through the existing frontend delegates.
 TSharedRef<SWidget> Make(TFunction<void()> Start,TFunction<void()> Settings,TFunction<void()> Tutorial,TFunction<void()> History,TFunction<void()> Quit);
 void FinishEntrance(TSharedRef<SWidget> Title);
}
