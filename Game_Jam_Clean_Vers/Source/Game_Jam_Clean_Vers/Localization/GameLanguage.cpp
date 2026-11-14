#include "GameLanguage.h"
#include "Misc/ConfigCacheIni.h"
#include "Frontend/LocalUserSettings.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "HAL/IConsoleManager.h"

namespace
{
 struct FEntry { FString Zh,En; };
 const TArray<FEntry>& Entries()
 {
  static const TArray<FEntry> Data={
   #define PAIR(ZH, EN) {TEXT(ZH),TEXT(EN)},
   #include "GameLanguageCatalog.inl"
   #undef PAIR
  };return Data;
 }
 struct FTemplate
 {
  TArray<FString> Parts,Types;
  explicit FTemplate(const FString& Value)
  {
   FString Part;
   for(int32 I=0;I<Value.Len();)
   {
    if(Value[I]!='%'){Part+=Value[I++];continue;}
    if(I+1<Value.Len()&&Value[I+1]=='%'){Part+=TEXT('%');I+=2;continue;}
    int32 End=I+1;while(End<Value.Len()&&FCString::Strchr(TEXT("0123456789.+-"),Value[End]))++End;
    if(End<Value.Len()&&FCString::Strchr(TEXT("sdfg"),Value[End]))
    {Parts.Add(Part);Part.Reset();Types.Add(Value.Mid(End,1));I=End+1;}
    else Part+=Value[I++];
   }
   Parts.Add(Part);
  }
 };
 FString Escape(const FString& Text)
 {
  FString Out;for(TCHAR C:Text){if(FCString::Strchr(TEXT("\\.^$|()[]{}*+?"),C))Out+=TEXT('\\');Out+=C;}return Out;
 }
 struct FRule
 {
  FTemplate From,To;
  TSharedPtr<FRegexPattern> Pattern;
  FRule(const FString& Source,const FString& Target):From(Source),To(Target)
  {
   if(From.Types.IsEmpty()||From.Types.Num()!=To.Types.Num())return;
   FString Regex=TEXT("^");
   for(int32 I=0;I<From.Types.Num();++I)
    Regex+=Escape(From.Parts[I])+(From.Types[I]==TEXT("s")?TEXT("([\\s\\S]*?)"):TEXT("([-+0-9.eE]+)"));
   Regex+=Escape(From.Parts.Last())+TEXT("$");Pattern=MakeShared<FRegexPattern>(Regex);
  }
 };
 struct FCatalog
 {
  TMap<FString,FString> Exact;
  TArray<FRule> Rules;
  TArray<FEntry> Fragments;
  explicit FCatalog(bool English)
  {
   TMap<FString,FString> Pieces;
   for(const auto& E:Entries())
   {
    const FString& From=English?E.Zh:E.En;const FString& To=English?E.En:E.Zh;
    if(!Exact.Contains(From))Exact.Add(From,To);
    FRule Rule(From,To);
    if(Rule.Pattern)
    {
     if(English)for(int32 I=0;I<Rule.From.Parts.Num();++I)
     {const auto& P=Rule.From.Parts[I];if(!P.TrimStartAndEnd().IsEmpty()&&!Pieces.Contains(P))Pieces.Add(P,Rule.To.Parts[I]);}
     Rules.Add(MoveTemp(Rule));
    }
    else if(English&&!Pieces.Contains(From))Pieces.Add(From,To);
   }
   for(const auto& P:Pieces)Fragments.Add({P.Key,P.Value});
   Fragments.Sort([](const FEntry& A,const FEntry& B){return A.Zh.Len()>B.Zh.Len();});
  }
 };
 const FCatalog& Catalog(bool English) {static const FCatalog En(true),Zh(false);return English?En:Zh;}
 FString Convert(const FString& Source,bool English,int32 Depth)
 {
  if(Source.IsEmpty()||Depth>8)return Source;
  if(Source.Contains(TEXT("\n")))
  {
   TArray<FString> Lines;Source.ParseIntoArray(Lines,TEXT("\n"),false);
   for(FString& Line:Lines)Line=Convert(Line,English,Depth);
   return FString::Join(Lines,TEXT("\n"));
  }
  if(!English)for(const auto& E:Entries())
  {
   // Old event names contained both languages in a single display string.
   if(Source==E.Zh&&E.Zh.EndsWith(TEXT(" / ")+E.En))return E.Zh.LeftChop(E.En.Len()+3).TrimEnd();
  }
  const auto& C=Catalog(English);
  if(const auto* Found=C.Exact.Find(Source))return *Found;
  for(const auto& Rule:C.Rules)
  {
   FRegexMatcher M(*Rule.Pattern,Source);if(!M.FindNext())continue;
   FString Out;
   for(int32 I=0;I<Rule.From.Types.Num();++I)
   {const FString Capture=M.GetCaptureGroup(I+1);Out+=Rule.To.Parts[I]+(Rule.From.Types[I]==TEXT("s")?Convert(Capture,English,Depth+1):Capture);}
   return Out+Rule.To.Parts.Last();
  }
  if(!English)return Source;
  // Legacy saved strings concatenate labels and formatted values. Translate once, longest phrase first.
  FString Out;
  for(int32 I=0;I<Source.Len();)
  {
   bool Found=false;
   for(const auto& P:C.Fragments)if(Source.Mid(I,P.Zh.Len()).Equals(P.Zh,ESearchCase::CaseSensitive))
   {Out+=P.En;I+=P.Zh.Len();Found=true;break;}
   if(!Found)Out+=Source[I++];
  }
  return Out;
 }
}
bool GameLanguage::IsEnglish(){bool English=false;if(GConfig)GConfig->GetBool(TEXT("GameLanguage"),TEXT("English"),English,GameLocalSettings::Filename());return English;}
void GameLanguage::SetEnglish(bool English){if(GConfig){GConfig->SetBool(TEXT("GameLanguage"),TEXT("English"),English,GameLocalSettings::Filename());GConfig->Flush(false,GameLocalSettings::Filename());}}
FString GameLanguage::Translate(const FString& Source,bool English){return Convert(Source,English,0);}
FSlateFontInfo GameLanguage::Font(int32 Size){return FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),Size);}
FSlateFontInfo GameLanguage::FittedFont(const FString& Text,int32 Size,float Width)
{
 auto Result=Font(Size);
 if(FSlateApplication::IsInitialized())
 {
  const float Measured=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text,Result).X;
  if(Measured>Width&&Width>0)Result.Size=FMath::Max(9,FMath::FloorToInt(Size*Width/Measured));
 }
 return Result;
}

#if !UE_BUILD_SHIPPING
// Background visual QA uses the exact same preference setter as the menu buttons.
static FAutoConsoleCommand LanguagePreviewCommand(TEXT("Game.Language.Set"),TEXT("Set UI language for visual QA: en or zh"),
 FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
 {
  if(Args.Num()==1&&(Args[0]==TEXT("en")||Args[0]==TEXT("zh")))GameLanguage::SetEnglish(Args[0]==TEXT("en"));
  else UE_LOG(LogTemp,Warning,TEXT("Usage: Game.Language.Set en|zh"));
 }));
#endif
