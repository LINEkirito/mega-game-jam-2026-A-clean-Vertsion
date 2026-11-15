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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameTypographyCheck,"Game.Localization.Typography",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameTypographyCheck::RunTest(const FString&)
{
 const auto Body=GameLanguage::Font(18),Heading=GameLanguage::Font(34);
 const auto Ending=GameLanguage::Font(14,GameLanguage::EFontRole::Heading);
 TestEqual(TEXT("Small UI uses body typeface"),Body.TypefaceFontName,FName(TEXT("Body")));
 TestEqual(TEXT("Large UI uses heading typeface"),Heading.TypefaceFontName,FName(TEXT("Heading")));
 auto Label=SNew(SLocalizedTextBlock).Font(Ending).Text(FText::FromString(TEXT("Gunnes")));
 Label->SlatePrepass(1);
 TestEqual(TEXT("Localized widget preserves small final-panel heading typeface"),Label->GetFont().TypefaceFontName,Ending.TypefaceFontName);
 auto Legacy=SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",28));
 Legacy->SlatePrepass(1);
 TestEqual(TEXT("Legacy font requests are converted to project typography"),Legacy->GetFont().TypefaceFontName,FName(TEXT("Heading")));
 const auto& Family=*Body.CompositeFont;
 TestTrue(TEXT("Body resolves to the supplied Yanone font"),Family.DefaultTypeface.Fonts[0].Font.GetFontFilename().EndsWith(TEXT("YanoneKaffeesatz-Regular.ttf")));
 TestTrue(TEXT("Headings resolve to the supplied Slackey font"),Family.DefaultTypeface.Fonts[2].Font.GetFontFilename().EndsWith(TEXT("Slackey-Regular.ttf")));
 TestTrue(TEXT("Chinese retains an explicit character-range fallback"),Family.SubTypefaces[0].CharacterRanges.ContainsByPredicate([](const FInt32Range& Range){return Range.Contains(0x4e2d);}));
 TestTrue(TEXT("Chinese uses the existing engine font"),Family.SubTypefaces[0].Typeface.Fonts[0].Font.GetFontFilename().EndsWith(TEXT("DroidSansFallback.ttf")));
 int32 DynamicSize=28;
 auto Fitted=SNew(SLocalizedTextBlock).Font_Lambda([&DynamicSize]{return GameLanguage::Font(DynamicSize,GameLanguage::EFontRole::Heading);});
 Fitted->SlatePrepass(1);
 TestEqual(TEXT("Dynamic font starts at requested size"),Fitted->GetFont().Size,28.f);
 DynamicSize=20;Fitted->Invalidate(EInvalidateWidgetReason::Prepass);Fitted->SlatePrepass(1);
 TestEqual(TEXT("Dynamic text fitting survives the localized wrapper"),Fitted->GetFont().Size,20.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnglishCoverageCheck,"Game.Localization.EnglishCoverage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnglishCoverageCheck::RunTest(const FString&)
{
 auto HasChinese=[](const FString& Text){for(TCHAR C:Text)if((C>=0x3400&&C<=0x9fff)||(C>=0xf900&&C<=0xfaff))return true;return false;};
 auto Sample=[](const FString& Format)
 {
  FString Out;
  for(int32 I=0;I<Format.Len();++I)
  {
   if(Format[I]!=TEXT('%')){Out+=Format[I];continue;}
   if(I+1<Format.Len()&&Format[I+1]==TEXT('%')){Out+=TEXT('%');++I;continue;}
   int32 End=I+1;while(End<Format.Len()&&FCString::Strchr(TEXT("0123456789.+-"),Format[End]))++End;
   if(End<Format.Len()&&FCString::Strchr(TEXT("sdfg"),Format[End]))
   {Out+=Format[End]==TEXT('s')?TEXT("黑帮成员 #3"):Format[End]==TEXT('d')?TEXT("3"):TEXT("3.5");I=End;}
   else Out+=Format[I];
  }
  return Out;
 };
 struct FPair{FString Zh,En;};const TArray<FPair> Catalog={
  #define PAIR(ZH,EN) {TEXT(ZH),TEXT(EN)},
  #include "GameLanguageCatalog.inl"
  #undef PAIR
 };
 for(const auto& Pair:Catalog)
 {
  TestFalse(*(TEXT("English catalog: ")+Pair.En),HasChinese(Pair.En));
  const FString Display=GameLanguage::Translate(Sample(Pair.Zh),true);
  TestFalse(*(TEXT("Formatted English display: ")+Display),HasChinese(Display));
 }
 TestEqual(TEXT("English language entry"),GameLanguage::Translate(TEXT("语言 / Language"),true),FString(TEXT("Language")));
 TestEqual(TEXT("English Chinese-language choice"),GameLanguage::Translate(TEXT("✓  中文"),true),FString(TEXT("✓  Chinese")));
 return true;
}
#endif
