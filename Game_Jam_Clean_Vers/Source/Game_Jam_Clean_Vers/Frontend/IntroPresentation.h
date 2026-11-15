#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// Presentation owns only its page/timing state. Starting a run stays with the frontend.
class SIntroPresentation final : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SIntroPresentation){} SLATE_END_ARGS()
 void Construct(const FArguments&,TFunction<void()> Complete);
 virtual ~SIntroPresentation() override;
 virtual bool SupportsKeyboardFocus() const override{return true;}
 virtual void Tick(const FGeometry&,double,float) override;
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
 virtual FReply OnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent&) override;
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
 void Advance();
 int32 GetPageIndex()const{return PageIndex;}
 float GetPageElapsed()const{return PageElapsed;}
 bool IsTransitioning()const{return bLeaving;}
 bool IsFinished()const{return bFinished;}
private:
 struct FArt;
 TArray<TSharedPtr<FArt>> Art;
 TSharedPtr<SWidget> TextRoot,StoryRoot,Caption,Chrome;
 TFunction<void()> OnComplete;
 int32 PageIndex=0;
 float PageElapsed=0,ExitElapsed=0;
 bool bLeaving=false,bFinished=false;
 FString Narration()const;
 FString Subtitle()const;
 float ContentOpacity()const;
 float HoldDuration()const;
 void UpdatePresentation();
};

namespace IntroPresentation
{
 TSharedRef<SIntroPresentation> Make(TFunction<void()> Complete);
}
