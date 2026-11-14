#if WITH_DEV_AUTOMATION_TESTS
#include "GamePerformanceSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Localization/GameLanguage.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerformanceSettingsTest,"Game.Performance.SettingsDraft",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPerformanceSettingsTest::RunTest(const FString&)
{
 auto* Settings=NewObject<UGameUserSettings>();Settings->SetToDefaults();
 Settings->SetOverallScalabilityLevel(3);Settings->SetResolutionScaleValueEx(100);Settings->SetFrameRateLimit(144);Settings->SetVSyncEnabled(true);
 auto Draft=FGamePerformanceOptions::Read(*Settings);Draft.ResolutionScale=75;Draft.SetPreset(1);Draft.FrameLimit=60;Draft.bVSync=false;
 TestEqual(TEXT("Draft leaves live quality untouched"),Settings->GetShadowQuality(),3);
 TestEqual(TEXT("Draft leaves live FPS limit untouched"),Settings->GetFrameRateLimit(),144.f);
 TestTrue(TEXT("Draft leaves VSync untouched"),Settings->IsVSyncEnabled());
 Draft.Write(*Settings);
 TestEqual(TEXT("Apply writes quality"),Settings->GetShadowQuality(),1);
 TestEqual(TEXT("Preset preserves selected render scale"),Settings->ScalabilityQuality.ResolutionQuality,75.f);
 TestEqual(TEXT("Apply writes FPS limit"),Settings->GetFrameRateLimit(),60.f);
 TestFalse(TEXT("Apply writes VSync"),Settings->IsVSyncEnabled());
 auto Reloaded=FGamePerformanceOptions::Read(*Settings);
 TestEqual(TEXT("Reopen retains scale"),Reloaded.ResolutionScale,75.f);
 TestEqual(TEXT("Reopen retains quality"),Reloaded.Quality.FoliageQuality,1);
 Draft.ResolutionScale=1;Draft.FrameLimit=-1;Draft.Write(*Settings);
 TestEqual(TEXT("Scale cannot drop below UI minimum"),Settings->ScalabilityQuality.ResolutionQuality,50.f);
 TestEqual(TEXT("Invalid limit becomes unlimited"),Settings->GetFrameRateLimit(),0.f);
 TestEqual(TEXT("Settings title supports English"),GameLanguage::Translate(TEXT("性能与画质"),true),FString(TEXT("Performance & Graphics")));
 TestEqual(TEXT("Frame cap preserves value in Chinese"),GameLanguage::Translate(TEXT("144 FPS"),false),FString(TEXT("144 帧/秒")));
 return true;
}
#endif
