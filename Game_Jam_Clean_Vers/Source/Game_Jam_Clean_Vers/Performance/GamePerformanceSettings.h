#pragma once
#include "CoreMinimal.h"
#include "Scalability.h"

class UGameUserSettings;
class SWidget;

// A draft is separate from the engine settings: cancel and visiting language settings never apply it.
struct FGamePerformanceOptions
{
 Scalability::FQualityLevels Quality;
 float ResolutionScale=100.f;
 float FrameLimit=0.f;
 bool bVSync=false;
 static FGamePerformanceOptions Read(const UGameUserSettings& Settings);
 void SetPreset(int32 Level);
 void Write(UGameUserSettings& Settings) const;
};

namespace GamePerformance
{
 TSharedRef<SWidget> MakePanel(TFunction<void()> Close,TFunction<void()> Language);
}
