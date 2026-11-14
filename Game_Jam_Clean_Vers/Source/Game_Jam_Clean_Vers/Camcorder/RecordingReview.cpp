#include "RecordingReview.h"
#include "Audio/AudioWidgets.h"
#include "RecordingLibrary.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Localization/GameLanguage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"

class SRecordingScoreOutline : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SRecordingScoreOutline){}SLATE_ARGUMENT(TWeakPtr<SRecordingReview>,Review)SLATE_END_ARGS()
 void Construct(const FArguments& A){Review=A._Review;SetVisibility(EVisibility::HitTestInvisible);}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(640,360);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override{
  auto R=Review.Pin();auto* L=R?R->GetLibrary():nullptr;if(!L||!L->Clips.IsValidIndex(R->GetClipIndex()))return Layer;
  const FVector2D Size=G.GetLocalSize();const float Time=R->GetPlaybackTime();
  for(const auto& M:L->Clips[R->GetClipIndex()].ScoreMoments){
   if(!M.bHasScreenRect||Time<M.Seconds||Time>M.Seconds+.25f)continue;
   const auto& B=M.ScreenRect;TArray<FVector2D> Points{FVector2D(B.X*Size.X,B.Y*Size.Y),FVector2D(B.Z*Size.X,B.Y*Size.Y),FVector2D(B.Z*Size.X,B.W*Size.Y),FVector2D(B.X*Size.X,B.W*Size.Y),FVector2D(B.X*Size.X,B.Y*Size.Y)};
   FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,M.Points>=0?FLinearColor(.3f,1,.65f):FLinearColor(1,.25f,.2f),true,3);
  }return Layer+1;
 }
private:TWeakPtr<SRecordingReview> Review;
};
static TSharedRef<STextBlock> ReviewText(const FString& Text,int32 Size=22,FLinearColor Color=FLinearColor::White){return SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true).Text(FText::FromString(Text));}
void SRecordingReview::Construct(const FArguments& A){Library=A._Library;if(auto* L=Library.Get())for(int I=0;I<L->Clips.Num();++I)Thumbnails.Add(MakeShared<FThumbnail>());Build();}
void SRecordingReview::LoadThumbnail(int32 Index){
 auto* L=Library.Get();if(!L||!Thumbnails.IsValidIndex(Index)||Thumbnails[Index]->bLoaded)return;
 auto& T=*Thumbnails[Index];T.bLoaded=true;FCamcorderPlayback Reader;TArray<uint8> Pixels;
 if(Reader.Open(L->Clips[Index].Path)&&Reader.ReadFrame(0,Pixels)){
  TArray64<uint8> Data;Data.Append(Pixels.GetData(),Pixels.Num());T.Texture.Reset(UTexture2D::CreateTransient(Reader.GetWidth(),Reader.GetHeight(),PF_B8G8R8A8,NAME_None,Data));
  if(T.Texture.IsValid()){T.Texture->SRGB=true;T.Texture->UpdateResource();T.Brush.SetResourceObject(T.Texture.Get());T.Brush.ImageSize=FVector2D(Reader.GetWidth(),Reader.GetHeight());T.Brush.DrawAs=ESlateBrushDrawType::Image;++LoadedThumbnails;}
 }
}
void SRecordingReview::PlayClip(int32 Index){
 auto* L=Library.Get();if(!L)return;
 if(!L->Clips.IsValidIndex(Index)){ShowSummary();return;}
 Phase=ERecordingReviewPhase::Review;SelectedClip=Index;PlaybackSeconds=0;LastFrame=INDEX_NONE;PlaybackError.Reset();bHoldFast=false;
 bPlaying=Playback.Open(L->Clips[Index].Path);Duration=bPlaying?Playback.NumFrames()/15.f:0;
 if(!bPlaying){PlaybackError=TEXT("该录像无法读取，评分记录仍保留。可选择下一段。");}
 const int32 Width=bPlaying?Playback.GetWidth():640,Height=bPlaying?Playback.GetHeight():360;
 if(!VideoTexture.IsValid() || VideoTexture->GetSizeX()!=Width || VideoTexture->GetSizeY()!=Height){
  TArray64<uint8> Black;Black.SetNumZeroed(Width*Height*4);for(int32 I=3;I<Black.Num();I+=4)Black[I]=255;
  VideoTexture.Reset(UTexture2D::CreateTransient(Width,Height,PF_B8G8R8A8,NAME_None,Black));VideoTexture->SRGB=true;VideoTexture->UpdateResource();VideoBrush.SetResourceObject(VideoTexture.Get());VideoBrush.ImageSize=FVector2D(Width,Height);VideoBrush.DrawAs=ESlateBrushDrawType::Image;
 }
 UpdateFrame();Build();
}
void SRecordingReview::ShowSummary(){Phase=ERecordingReviewPhase::Summary;bPlaying=false;bHoldFast=false;Playback.Close();Build();}
void SRecordingReview::Seek(float Fraction){if(Duration<=0)return;PlaybackSeconds=FMath::Clamp(Fraction,0.f,1.f)*FMath::Max(0.f,Duration-1.f/15);UpdateFrame();}
void SRecordingReview::UpdateFrame(){
 if(!VideoTexture.IsValid()||Playback.NumFrames()==0)return;
 const int32 Frame=FMath::Clamp(FMath::FloorToInt(PlaybackSeconds*15),0,Playback.NumFrames()-1);if(Frame==LastFrame)return;
 TArray<uint8> Pixels;if(!Playback.ReadFrame(Frame,Pixels)){bPlaying=false;PlaybackError=TEXT("录像帧读取失败，评分记录仍保留。");return;}
 auto* Bytes=new uint8[Pixels.Num()];FMemory::Memcpy(Bytes,Pixels.GetData(),Pixels.Num());
 auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Playback.GetWidth(),Playback.GetHeight());VideoTexture->UpdateTextureRegions(0,1,Region,Playback.GetWidth()*4,4,Bytes,[](uint8* Data,const FUpdateTextureRegion2D* R){delete[] Data;delete R;});LastFrame=Frame;
}
void SRecordingReview::Tick(const FGeometry& G,double Now,float DT){
 SCompoundWidget::Tick(G,Now,DT);auto* L=Library.Get();if(!L)return;
 if(Phase==ERecordingReviewPhase::Overview){
  OverviewSeconds+=DT;const int32 Count=FMath::Min(L->Clips.Num(),int32(OverviewSeconds/.3f)+1);
  // Decode at most one thumbnail each UI frame to keep large libraries responsive.
  if(Revealed<Count){LoadThumbnail(Revealed++);Build();}
  if(Revealed==L->Clips.Num()&&OverviewSeconds>L->Clips.Num()*.3f+1.5f){if(L->Clips.IsEmpty())ShowSummary();else PlayClip(0);}
 }else if(Phase==ERecordingReviewPhase::Review&&bPlaying){
  PlaybackSeconds+=DT*GetPlaybackRate();if(PlaybackSeconds>=Duration){PlayClip(SelectedClip+1);return;}UpdateFrame();
 }
}
FReply SRecordingReview::OnKeyDown(const FGeometry&,const FKeyEvent& E){
 if(E.IsRepeat())return FReply::Handled();const FKey K=E.GetKey();
 if(K==EKeys::SpaceBar&&Phase==ERecordingReviewPhase::Review)TogglePause();
 if(K==EKeys::One||K==EKeys::NumPadOne)SetPlaybackRate(1);
 if(K==EKeys::Two||K==EKeys::NumPadTwo)SetPlaybackRate(2);
 if(K==EKeys::Four||K==EKeys::NumPadFour)SetPlaybackRate(4);
 if(K==EKeys::Left&&Phase==ERecordingReviewPhase::Review)PlayClip(FMath::Max(0,SelectedClip-1));
 if(K==EKeys::Right&&Phase==ERecordingReviewPhase::Review)PlayClip(SelectedClip+1);
 return FReply::Handled();
}
FReply SRecordingReview::OnMouseButtonDown(const FGeometry&,const FPointerEvent& E){if(Phase==ERecordingReviewPhase::Review&&E.GetEffectingButton()==EKeys::LeftMouseButton){bHoldFast=true;return FReply::Handled().CaptureMouse(SharedThis(this));}return FReply::Unhandled();}
FReply SRecordingReview::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E){if(E.GetEffectingButton()==EKeys::LeftMouseButton&&bHoldFast){bHoldFast=false;PlaybackRate=1;return FReply::Handled().ReleaseMouseCapture();}return FReply::Unhandled();}
void SRecordingReview::Build(){
 auto* L=Library.Get();if(!L)return;
 auto Root=SNew(SVerticalBox);
 const FString Title=Phase==ERecordingReviewPhase::Overview?TEXT("本局拍摄总览"):Phase==ERecordingReviewPhase::Review?TEXT("录像回看 · 评分说明"):TEXT("本局评分明细");
 Root->AddSlot().AutoHeight().Padding(0,0,0,10)[ReviewText(Title,34)];
 Root->AddSlot().AutoHeight().Padding(0,0,0,16)[ReviewText(FString::Printf(TEXT("%s   |   %d 段录像   |   本局总分 %.1f"),L->bEndedByDeath?TEXT("死亡结算"):TEXT("成功撤离"),L->Clips.Num(),L->RoundScore),22,FLinearColor(.4f,.9f,.75f))];
 Root->AddSlot().AutoHeight().Padding(0,0,0,12)[ReviewText(L->bEndedByDeath?TEXT("死亡：最终分数 × 0.5；录像原始评分保持不变。"):TEXT("成功撤离：保留 100% 分数。"),18,FLinearColor(.8f,.85f,.9f))];
 if(Phase==ERecordingReviewPhase::Overview){
  auto Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(6));
  for(int32 I=0;I<Revealed;++I){const auto& C=L->Clips[I];auto Card=SNew(SVerticalBox);
   Card->AddSlot().AutoHeight()[SNew(SBox).WidthOverride(264).HeightOverride(148.5f)[SNew(SImage).Image(&Thumbnails[I]->Brush)]];
   Card->AddSlot().AutoHeight().Padding(8)[ReviewText(FString::Printf(TEXT("录像 %02d   %02d:%02d"),I+1,int32(C.Seconds)/60,int32(C.Seconds)%60),19)];
   Grid->AddSlot(I%5,I/5)[SNew(SAudioButton).IsFocusable(false).ContentPadding(4).OnClicked_Lambda([this,I]{PlayClip(I);return FReply::Handled();})[Card]];
  }
  Root->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Grid]];
  Root->AddSlot().AutoHeight().Padding(0,12)[ReviewText(TEXT("按拍摄顺序展示首帧，随后自动连续回看。点击任意录像可直接播放。"),20)];
 }else if(Phase==ERecordingReviewPhase::Review&&L->Clips.IsValidIndex(SelectedClip)){
  auto Body=SNew(SHorizontalBox);auto Video=SNew(SVerticalBox);
  Video->AddSlot().AutoHeight()[SNew(SBox).WidthOverride(864).HeightOverride(486)[SNew(SOverlay)
   +SOverlay::Slot()[SNew(SImage).Image(&VideoBrush)]
   +SOverlay::Slot()[SNew(SRecordingScoreOutline).Review(SharedThis(this))]]];
  Video->AddSlot().AutoHeight().Padding(0,8)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",20)).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("录像 %02d   %.1f / %.1f 秒   ·   %d×   %s"),SelectedClip+1,PlaybackSeconds,Duration,GetPlaybackRate(),bPlaying?TEXT("播放中"):TEXT("暂停")));})];
  Video->AddSlot().AutoHeight()[SNew(SAudioSlider).IsFocusable(false).Value_Lambda([this]{return Duration>0?PlaybackSeconds/Duration:0.f;}).OnValueChanged_Lambda([this](float V){Seek(V);})];
  Video->AddSlot().AutoHeight().Padding(0,8)[SNew(SBox).HeightOverride(64)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(FLinearColor(1,.78f,.35f)).AutoWrapText(true).Text_Lambda([this]{
   if(!PlaybackError.IsEmpty())return FText::FromString(PlaybackError);FString Text;if(auto* Lib=Library.Get())if(Lib->Clips.IsValidIndex(SelectedClip))for(const auto& M:Lib->Clips[SelectedClip].ScoreMoments)if(M.Seconds<=PlaybackSeconds&&M.Seconds+2>=PlaybackSeconds)Text+=M.Subject+TEXT(" · ")+M.Explanation+TEXT("\n");return FText::FromString(Text);
  })]]];
  Body->AddSlot().AutoWidth()[Video];auto Details=SNew(SVerticalBox);
  Details->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",24)).Text_Lambda([this]{int32 Score=0;auto* Lib=Library.Get();if(Lib&&Lib->Clips.IsValidIndex(SelectedClip))for(const auto& M:Lib->Clips[SelectedClip].ScoreMoments)if(M.Seconds<=PlaybackSeconds)Score+=M.Points;return FText::FromString(FString::Printf(TEXT("播放至此：%+d 分"),Score));})];
  auto Entries=SNew(SVerticalBox);
  const auto& Clip=L->Clips[SelectedClip];for(int32 I=0;I<Clip.ScoreMoments.Num();++I){const auto M=Clip.ScoreMoments[I];
   Entries->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",19)).AutoWrapText(true).ColorAndOpacity_Lambda([this,M]{return M.Seconds>PlaybackSeconds?FLinearColor(.4f,.45f,.5f):M.Points>=0?FLinearColor(.55f,1,.8f):FLinearColor(1,.55f,.45f);}).Text(FText::FromString(FString::Printf(TEXT("%.1f秒 · %s\n%s"),M.Seconds,*M.Subject,*M.Explanation)))];
  }
  for(const auto& Evidence:Clip.RecordingEvidence)Entries->AddSlot().AutoHeight().Padding(0,8)[ReviewText(Evidence,17,FLinearColor(.65f,.72f,.8f))];
  if(Clip.ScoreMoments.IsEmpty())Entries->AddSlot().AutoHeight()[ReviewText(TEXT("本段没有达到加分或扣分门槛。"),20)];
  Details->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Entries]];
  Body->AddSlot().FillWidth(1).Padding(24,0)[Details];Root->AddSlot().FillHeight(1)[Body];
  auto Controls=SNew(SHorizontalBox);
  auto Button=[&](const FString& Label,TFunction<void()> Action){Controls->AddSlot().AutoWidth().Padding(0,0,12,0)[SNew(SAudioButton).IsFocusable(false).ContentPadding(FMargin(14,8)).OnClicked_Lambda([Action]{Action();return FReply::Handled();})[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).Text(FText::FromString(Label))]];};
  Button(TEXT("← 上一段"),[this]{PlayClip(FMath::Max(0,SelectedClip-1));});Button(TEXT("播放 / 暂停"),[this]{TogglePause();});Button(TEXT("下一段 →"),[this]{PlayClip(SelectedClip+1);});for(int Rate:{1,2,4})Button(FString::Printf(TEXT("%d×"),Rate),[this,Rate]{SetPlaybackRate(Rate);});Button(TEXT("跳过回看 · 总分"),[this]{ShowSummary();});
  Root->AddSlot().AutoHeight().Padding(0,12)[Controls];
  Root->AddSlot().AutoHeight()[ReviewText(TEXT("空格暂停 · 1/2/4切换倍速 · 左右键切换录像 · 按住画面临时2倍速，松开恢复1倍速"),17,FLinearColor(.65f,.72f,.8f))];
 }else{
  auto Scroll=SNew(SVerticalBox);TMap<FString,int32> Categories;for(const auto& C:L->Clips)for(const auto& M:C.ScoreMoments)Categories.FindOrAdd(M.Category)+=M.Points;
  const TCHAR* CategoryNames[]={TEXT("长镜头"),TEXT("事件连续入镜"),TEXT("NPC首次入镜"),TEXT("NPC累计时长"),TEXT("NPC不同行为"),TEXT("死亡瞬间"),TEXT("不同事件种类"),TEXT("同类事件累计时长"),TEXT("不同区域"),TEXT("行人入镜扣分"),TEXT("警察入镜扣分"),TEXT("Logo入镜扣分")};
  for(const TCHAR* Category:CategoryNames)Scroll->AddSlot().AutoHeight().Padding(0,3)[ReviewText(FString::Printf(TEXT("%s：%+d 分"),Category,Categories.FindRef(Category)),22)];
  Scroll->AddSlot().AutoHeight().Padding(0,16)[ReviewText(TEXT("各档奖励逐档相加。事件时长分按Level 1/2/3乘1/2/3；新事件种类固定+5。NPC默认1倍。扣分与奖励同时结算，不把负分截成0。"),20,FLinearColor(.6f,.85f,.9f))];
  for(int32 I=0;I<L->Clips.Num();++I){const auto& C=L->Clips[I];
   Scroll->AddSlot().AutoHeight().Padding(0,14)[SNew(SAudioButton).IsFocusable(false).OnClicked_Lambda([this,I]{PlayClip(I);return FReply::Handled();})[ReviewText(FString::Printf(TEXT("录像 %02d · %02d:%02d · 得分 %+d · 点击重看"),I+1,int32(C.Seconds)/60,int32(C.Seconds)%60,C.Score),24)]];
   for(const auto& Evidence:C.RecordingEvidence)Scroll->AddSlot().AutoHeight().Padding(16,3)[ReviewText(Evidence,17,FLinearColor(.65f,.72f,.8f))];
   for(const auto& Detail:C.ScoreDetails)Scroll->AddSlot().AutoHeight().Padding(16,3)[ReviewText(Detail,19)];
  }
  if(L->Clips.IsEmpty())Scroll->AddSlot().AutoHeight().Padding(0,24)[ReviewText(TEXT("没有成功保存的录像，本局得分0。"),24)];
  Root->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Scroll]];
  Root->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0,12)
   [SNew(SAudioButton).ContentPadding(FMargin(28,12)).OnClicked_Lambda([Weak=Library]{if(auto* Lib=Weak.Get())Lib->ReturnToTitle();return FReply::Handled();})
    [SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("返回标题界面"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",22))]];
 }
 ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.01f,.017f,.026f,.98f)).Padding(20)
  [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1460).HeightOverride(820)[Root]]]];
}
