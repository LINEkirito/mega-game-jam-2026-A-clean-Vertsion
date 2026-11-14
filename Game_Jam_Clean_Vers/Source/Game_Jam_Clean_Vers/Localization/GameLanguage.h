#pragma once
#include "CoreMinimal.h"
#include "Widgets/Text/STextBlock.h"

// Language is a persisted user preference. Gameplay keys and stored evidence stay unchanged.
namespace GameLanguage
{
 GAME_JAM_CLEAN_VERS_API bool IsEnglish();
 GAME_JAM_CLEAN_VERS_API void SetEnglish(bool English);
 GAME_JAM_CLEAN_VERS_API FString Translate(const FString& Source, bool English);
 inline FString Display(const FString& Source) { return Translate(Source, IsEnglish()); }
 inline FText Text(const FString& Source) { return FText::FromString(Display(Source)); }
 GAME_JAM_CLEAN_VERS_API FSlateFontInfo Font(int32 Size);
 GAME_JAM_CLEAN_VERS_API FSlateFontInfo FittedFont(const FString& Text,int32 Size,float Width);
}

// Per-widget bounded cache, never shared across worlds or players.
struct FLocalizedDisplayCache
{
 FString Get(const FString& Source) const
 {
  const bool English=GameLanguage::IsEnglish();
  if(English!=bEnglish || Values.Num()>256){Values.Empty();bEnglish=English;}
  if(const auto* Value=Values.Find(Source))return *Value;
  return Values.Add(Source,GameLanguage::Translate(Source,English));
 }
private:
 mutable TMap<FString,FString> Values;
 mutable bool bEnglish=false;
};

// Retains the original attribute, including dynamic values. Language changes do not destroy UI state.
class SLocalizedTextBlock : public STextBlock
{
public:
 void Construct(const STextBlock::FArguments& Args)
 {
  auto LocalArgs=Args;
  const TAttribute<FText> Source=Args._Text;
  LocalArgs.Text_Lambda([this,Source]
  {
   const FString Value=Source.Get(FText::GetEmpty()).ToString(); const bool English=GameLanguage::IsEnglish();
   if(!bCached || Value!=LastSource || English!=bLastEnglish)
   { LastSource=Value;bLastEnglish=English;bCached=true;LastText=FText::FromString(GameLanguage::Translate(Value,English)); }
   return LastText;
  });
  const auto OriginalFont=Args._Font.Get(FCoreStyle::GetDefaultFontStyle("Regular",18));
  LocalArgs.Font(GameLanguage::Font(OriginalFont.Size));
  STextBlock::Construct(LocalArgs);
 }
private:
 FString LastSource;
 FText LastText;
 bool bCached=false,bLastEnglish=false;
};
