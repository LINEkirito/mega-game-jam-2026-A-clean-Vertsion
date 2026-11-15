#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// A transient frontend overlay; it never owns gameplay state or input bindings.
class SOpeningCountdown final : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SOpeningCountdown){} SLATE_END_ARGS()
 void Construct(const FArguments&);
 void SetElapsed(float Seconds);
 float GetElapsed() const { return Elapsed; }
 virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1280,720); }
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
 float Elapsed=0;
 int32 LastSoundStep=INDEX_NONE;
};
