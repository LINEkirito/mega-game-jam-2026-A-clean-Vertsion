#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GameLanguage.h"
#include "Camcorder/RecordingScoreRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameLanguageCheck,"Game.Localization.BilingualDisplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameLanguageCheck::RunTest(const FString&)
{
 TestEqual(TEXT("English authored NPC dialogue has Chinese"),GameLanguage::Translate(TEXT("BACK OFF!"),false),FString(TEXT("退后！")));
 TestEqual(TEXT("Chinese authored UI has English"),GameLanguage::Translate(TEXT("你已死亡"),true),FString(TEXT("You died")));
 TestEqual(TEXT("Formatted ammo preserves every value"),GameLanguage::Translate(TEXT("弹匣 02 / 30    膛内 1 / 1"),true),FString(TEXT("Magazine 02 / 30    Chamber 1 / 1")));
 TestEqual(TEXT("Nested score reason translates without changing numbers"),GameLanguage::Translate(TEXT("NPC累计时长达到 30 秒：20 × 2 = +40 分"),true),FString(TEXT("Total NPC duration reached 30 sec: 20 × 2 = +40 points")));
 TestEqual(TEXT("Concatenated saved subject names translate"),GameLanguage::Translate(TEXT("黑帮成员 #3"),true),FString(TEXT("Gang member #3")));
 TestEqual(TEXT("Unknown identifiers are preserved"),GameLanguage::Translate(TEXT("/Game/Combat/Rifle_42"),true),FString(TEXT("/Game/Combat/Rifle_42")));
 TestEqual(TEXT("Chinese health display"),GameLanguage::Translate(TEXT("HP 54 / 100"),false),FString(TEXT("生命 54 / 100")));
 FString ManyLines,ExpectedLines;
 for(int32 I=0;I<20;++I){ManyLines+=TEXT("NPC累计时长达到 30 秒：20 × 2 = +40 分\n");ExpectedLines+=TEXT("Total NPC duration reached 30 sec: 20 × 2 = +40 points\n");}
 TestEqual(TEXT("Long saved score lists translate every line"),GameLanguage::Translate(ManyLines,true),ExpectedLines);
 const FString Saved=TEXT("黑帮成员 #3：首次连续有效入镜3秒");
 GameLanguage::Translate(Saved,true);
 TestEqual(TEXT("Display translation never mutates stored source"),Saved,FString(TEXT("黑帮成员 #3：首次连续有效入镜3秒")));
 const bool Previous=GameLanguage::IsEnglish();
 auto Static=SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("背包为空")));
 FString Notice=TEXT("已丢下；靠近后按 F 拾取");
 auto Dynamic=SNew(SLocalizedTextBlock).Text_Lambda([&Notice]{return FText::FromString(Notice);});
 GameLanguage::SetEnglish(false);
 TestEqual(TEXT("Existing static widget Chinese"),Static->GetText().ToString(),FString(TEXT("背包为空")));
 GameLanguage::SetEnglish(true);
 TestEqual(TEXT("Existing static widget changes immediately"),Static->GetText().ToString(),FString(TEXT("Backpack empty")));
 TestEqual(TEXT("Existing notice changes immediately"),Dynamic->GetText().ToString(),FString(TEXT("Dropped; press F nearby to pick up")));
 Notice=TEXT("你已死亡");
 TestEqual(TEXT("Bound values remain live"),Dynamic->GetText().ToString(),FString(TEXT("You died")));
 GameLanguage::SetEnglish(false);
 TestEqual(TEXT("Switching back preserves source"),Dynamic->GetText().ToString(),FString(TEXT("你已死亡")));
 GameLanguage::SetEnglish(Previous);
 return true;
}
#endif
