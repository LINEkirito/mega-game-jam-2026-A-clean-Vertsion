#pragma once
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "CamcorderPlayback.h"
class URecordingLibrary;
enum class ERecordingReviewPhase : uint8 { Overview, Review, Summary };
class SRecordingReview : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SRecordingReview){} SLATE_ARGUMENT(TWeakObjectPtr<URecordingLibrary>,Library) SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 virtual void Tick(const FGeometry&,double,float) override;
 virtual bool SupportsKeyboardFocus()const override{return true;}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&)override;
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&)override;
 virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&)override;
 virtual void OnMouseCaptureLost(const FCaptureLostEvent&)override{if(bHoldFast)PlaybackRate=1;bHoldFast=false;}
 void PlayClip(int32 Index);
 void ShowSummary();
 void SetPlaybackRate(int32 Rate){PlaybackRate=Rate==4?4:Rate==2?2:1;}
 void TogglePause(){bPlaying=!bPlaying;}
 void Seek(float Fraction);
 int32 GetClipIndex()const{return SelectedClip;}
 float GetPlaybackTime()const{return PlaybackSeconds;}
 int32 GetPlaybackRate()const{return bHoldFast?2:PlaybackRate;}
 bool IsPlaying()const{return bPlaying;}
 ERecordingReviewPhase GetPhase()const{return Phase;}
 int32 GetDecodedFrame()const{return LastFrame;}
 int32 GetThumbnailCount()const{return LoadedThumbnails;}
 URecordingLibrary* GetLibrary()const{return Library.Get();}
private:
 struct FThumbnail {TStrongObjectPtr<UTexture2D> Texture;FSlateBrush Brush;bool bLoaded=false;};
 TWeakObjectPtr<URecordingLibrary> Library;
 TArray<TSharedPtr<FThumbnail>> Thumbnails;
 TStrongObjectPtr<UTexture2D> VideoTexture;
 FSlateBrush VideoBrush;
 FCamcorderPlayback Playback;
 ERecordingReviewPhase Phase=ERecordingReviewPhase::Overview;
 int32 SelectedClip=INDEX_NONE,LastFrame=INDEX_NONE,PlaybackRate=1,Revealed=0,LoadedThumbnails=0;
 float PlaybackSeconds=0,OverviewSeconds=0,Duration=0;
 bool bPlaying=false,bHoldFast=false;
 FString PlaybackError;
 void Build();
 void LoadThumbnail(int32 Index);
 void UpdateFrame();
};
