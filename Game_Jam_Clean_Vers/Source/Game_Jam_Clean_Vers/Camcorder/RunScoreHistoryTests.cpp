#if WITH_DEV_AUTOMATION_TESTS
#include "RunScoreHistory.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunHistoryPersistenceTest,"Game.ScoreHistory.PersistenceAndDeletion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRunHistoryPersistenceTest::RunTest(const FString&) {
 const FString Folder=FPaths::AutomationTransientDir()/FGuid::NewGuid().ToString();
 IFileManager::Get().MakeDirectory(*Folder,true);
 FRunScoreHistory Store(Folder);int32 Invalid=0;
 TestEqual(TEXT("Empty on first launch"),Store.Load(Invalid).Num(),0);
 auto Write=[&](const TCHAR* Name,const TCHAR* Json){TestTrue(TEXT("Write fixture"),FFileHelper::SaveStringToFile(Json,*(Folder/Name)));};
 Write(TEXT("Round_old.score.json"),TEXT("{\"score\":-7.5,\"death\":true,\"completed_utc\":\"2026-01-01T10:00:00Z\"}"));
 Write(TEXT("Round_new.score.json"),TEXT("{\"score\":15,\"death\":false,\"completed_utc\":\"2026-01-02T10:00:00Z\"}"));
 Write(TEXT("Round_bad.score.json"),TEXT("broken"));Write(TEXT("keep.avi"),TEXT("video"));
 auto Entries=FRunScoreHistory(Folder).Load(Invalid);
 TestEqual(TEXT("Fresh store reloads two disk records"),Entries.Num(),2);TestEqual(TEXT("Malformed record skipped"),Invalid,1);
 if(Entries.Num()==2){TestEqual(TEXT("Newest first"),Entries[0].Score,15.0);TestEqual(TEXT("Negative fractional death score retained"),Entries[1].Score,-7.5);}
 TestFalse(TEXT("Reject parent traversal"),Store.Delete(TEXT("../Round_other.score.json")));
 TestFalse(TEXT("Reject unrelated file"),Store.Delete(TEXT("keep.avi")));
 TestTrue(TEXT("Single delete succeeds"),Store.Delete(TEXT("Round_new.score.json")));
 TestEqual(TEXT("Deleted record stays deleted on reopen"),FRunScoreHistory(Folder).Load(Invalid).Num(),1);
 TestTrue(TEXT("Clear succeeds"),Store.Clear());TestEqual(TEXT("Clear persists"),FRunScoreHistory(Folder).Load(Invalid).Num(),0);TestEqual(TEXT("Clear removes corrupt records too"),Invalid,0);
 TestTrue(TEXT("Videos untouched"),IFileManager::Get().FileExists(*(Folder/TEXT("keep.avi"))));
 TestFalse(TEXT("Production store is outside project"),FPaths::IsUnderDirectory(FRunScoreHistory::Directory(),FPaths::ProjectDir()));
 IFileManager::Get().Delete(*(Folder/TEXT("keep.avi")));IFileManager::Get().DeleteDirectory(*Folder,false,false);return true;
}
#endif
