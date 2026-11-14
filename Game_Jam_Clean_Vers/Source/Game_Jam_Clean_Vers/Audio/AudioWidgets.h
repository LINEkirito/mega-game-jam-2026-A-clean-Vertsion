#pragma once
#include "Audio/GameAudio.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "UObject/StrongObjectPtr.h"
// Preserve every caller's style and delegates, including keyboard activation.
class SAudioButton : public SButton
{
public:
 void Construct(const SButton::FArguments& Args)
 {
  auto WithAudio=Args;
  const FSlateSound Click=GameAudio::UISound(TEXT("UI_Click")),Hover=GameAudio::UISound(TEXT("UI_Hover"));
  // Slate widgets are not UObjects: explicitly keep override assets alive across GC.
  ClickResource.Reset(Click.GetResourceObject());HoverResource.Reset(Hover.GetResourceObject());
  WithAudio.PressedSoundOverride(Click);
  WithAudio.HoveredSoundOverride(Hover);
  SButton::Construct(WithAudio);
  // UE 5.9 Construct calls SetButtonStyle last, replacing argument sound overrides.
  SetPressedSound(Click);
  SetHoveredSound(Hover);
 }
private:
 TStrongObjectPtr<UObject> ClickResource,HoverResource;
};
class SAudioSlider : public SSlider
{
public:
 void Construct(const SSlider::FArguments& Args){SSlider::Construct(Args);}
protected:
 virtual void CommitValue(float NewValue) override
 {
  const double Now=FPlatformTime::Seconds();
  if(!FMath::IsNearlyEqual(NewValue,GetValue())&&Now-LastTick>.08)
  {FSlateApplication::Get().PlaySound(GameAudio::UISound(TEXT("UI_Slide")));LastTick=Now;}
  SSlider::CommitValue(NewValue);
 }
private:
 double LastTick=0;
};
