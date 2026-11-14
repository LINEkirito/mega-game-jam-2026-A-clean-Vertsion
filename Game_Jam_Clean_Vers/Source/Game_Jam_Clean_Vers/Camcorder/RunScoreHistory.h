#pragma once
#include "CoreMinimal.h"
struct FRunScoreEntry { FString FileName; FDateTime CompletedUtc; double Score=0; bool bDeath=false; };
class FRunScoreHistory {
public:
 static FString Directory();
 explicit FRunScoreHistory(FString InDirectory=Directory()):Folder(MoveTemp(InDirectory)){}
 TArray<FRunScoreEntry> Load(int32& InvalidFiles) const;
 bool Delete(const FString& FileName) const;
 bool Clear() const;
private: FString Folder;
};
