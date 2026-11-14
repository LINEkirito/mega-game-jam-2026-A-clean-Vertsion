#include "LocalUserSettings.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/GameUserSettings.h"
namespace {
 const TCHAR* PerformanceSection=TEXT("Performance");
 struct FQualityField {const TCHAR* Key;int32 Scalability::FQualityLevels::* Member;};
 const FQualityField Fields[]={
  {TEXT("ViewDistance"),&Scalability::FQualityLevels::ViewDistanceQuality},
  {TEXT("AntiAliasing"),&Scalability::FQualityLevels::AntiAliasingQuality},
  {TEXT("Shadow"),&Scalability::FQualityLevels::ShadowQuality},
  {TEXT("GlobalIllumination"),&Scalability::FQualityLevels::GlobalIlluminationQuality},
  {TEXT("Reflection"),&Scalability::FQualityLevels::ReflectionQuality},
  {TEXT("PostProcess"),&Scalability::FQualityLevels::PostProcessQuality},
  {TEXT("Texture"),&Scalability::FQualityLevels::TextureQuality},
  {TEXT("Effects"),&Scalability::FQualityLevels::EffectsQuality},
  {TEXT("Foliage"),&Scalability::FQualityLevels::FoliageQuality},
  {TEXT("Shading"),&Scalability::FQualityLevels::ShadingQuality},
  {TEXT("Landscape"),&Scalability::FQualityLevels::LandscapeQuality}};
}
const FString& GameLocalSettings::Filename() {
 static const FString Path=[] {
  const FString Folder=FPaths::Combine(FPlatformProcess::UserSettingsDir(),TEXT("ThefinalGameJam"));
  IFileManager::Get().MakeDirectory(*Folder,true);
  return FPaths::ConvertRelativePathToFull(Folder/TEXT("Settings.ini"));
 }();
 // UE 5.9 Find() neither creates missing INIs nor permits saving auto-loaded ones.
 // Register this standalone user file explicitly and allow all three settings sections.
 if(GConfig && !GConfig->FindConfigFile(Path)) {
  FConfigFile File;File.Read(Path);File.NoSave=false;File.bCanSaveAllSections=true;
  GConfig->Add(Path,File);
 }
 return Path;
}
void GameLocalSettings::LoadPerformance() {
 auto* S=UGameUserSettings::GetGameUserSettings();if(!S||!GConfig)return;
 // A fresh OS user starts from engine defaults, never another user's bundled Saved directory.
 S->ValidateSettings();auto& Quality=S->ScalabilityQuality;Quality.SetDefaults();
 for(const auto& Field:Fields){int32 Value=Quality.*(Field.Member);GConfig->GetInt(PerformanceSection,Field.Key,Value,Filename());Quality.*(Field.Member)=FMath::Clamp(Value,0,4);}
 float Scale=100,Limit=0;bool VSync=false;
 GConfig->GetFloat(PerformanceSection,TEXT("ResolutionScale"),Scale,Filename());GConfig->GetFloat(PerformanceSection,TEXT("FrameLimit"),Limit,Filename());GConfig->GetBool(PerformanceSection,TEXT("VSync"),VSync,Filename());
 S->SetResolutionScaleValueEx(FMath::IsFinite(Scale)?FMath::Clamp(Scale,50.f,100.f):100.f);
 S->SetFrameRateLimit(FMath::IsFinite(Limit)?FMath::Clamp(Limit,0.f,240.f):0.f);S->SetVSyncEnabled(VSync);S->ApplyNonResolutionSettings();
}
void GameLocalSettings::SavePerformance() {
 const auto* S=UGameUserSettings::GetGameUserSettings();if(!S||!GConfig)return;
 for(const auto& Field:Fields)GConfig->SetInt(PerformanceSection,Field.Key,S->ScalabilityQuality.*(Field.Member),Filename());
 GConfig->SetFloat(PerformanceSection,TEXT("ResolutionScale"),S->ScalabilityQuality.ResolutionQuality,Filename());
 GConfig->SetFloat(PerformanceSection,TEXT("FrameLimit"),S->GetFrameRateLimit(),Filename());GConfig->SetBool(PerformanceSection,TEXT("VSync"),S->IsVSyncEnabled(),Filename());GConfig->Flush(false,Filename());
}
