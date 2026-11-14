#include "RunScoreHistory.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
FString FRunScoreHistory::Directory() {
 return FPaths::Combine(FPlatformProcess::UserSettingsDir(),TEXT("ThefinalGameJam"),TEXT("ScoreHistory"));
}
TArray<FRunScoreEntry> FRunScoreHistory::Load(int32& InvalidFiles) const {
 InvalidFiles=0;TArray<FRunScoreEntry> Entries;TArray<FString> Files;
 IFileManager::Get().FindFiles(Files,*(Folder/TEXT("Round_*.score.json")),true,false);
 for(const auto& File:Files) {
  FString Json,Time;TSharedPtr<FJsonObject> Object;FRunScoreEntry Entry;Entry.FileName=File;
  if(!FFileHelper::LoadFileToString(Json,*(Folder/File)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object) || !Object.IsValid() ||
     !Object->TryGetNumberField(TEXT("score"),Entry.Score) || !FMath::IsFinite(Entry.Score) || !Object->TryGetBoolField(TEXT("death"),Entry.bDeath)) {++InvalidFiles;continue;}
  if(!Object->TryGetStringField(TEXT("completed_utc"),Time) || !FDateTime::ParseIso8601(*Time,Entry.CompletedUtc))Entry.CompletedUtc=IFileManager::Get().GetTimeStamp(*(Folder/File));
  Entries.Add(MoveTemp(Entry));
 }
 Entries.Sort([](const FRunScoreEntry& A,const FRunScoreEntry& B){return A.CompletedUtc==B.CompletedUtc?A.FileName>B.FileName:A.CompletedUtc>B.CompletedUtc;});return Entries;
}
bool FRunScoreHistory::Delete(const FString& FileName) const {
 if(FileName!=FPaths::GetCleanFilename(FileName) || !FileName.StartsWith(TEXT("Round_")) || !FileName.EndsWith(TEXT(".score.json")))return false;
 return IFileManager::Get().Delete(*(Folder/FileName),true,false,true);
}
bool FRunScoreHistory::Clear() const {
 TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(Folder/TEXT("Round_*.score.json")),true,false);
 bool Success=true;for(const auto& File:Files)if(!Delete(File))Success=false;return Success;
}
