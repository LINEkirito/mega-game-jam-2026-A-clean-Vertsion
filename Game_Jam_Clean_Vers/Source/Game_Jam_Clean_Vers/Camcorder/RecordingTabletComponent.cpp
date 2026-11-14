#include "RecordingTabletComponent.h"
#include "Audio/AudioWidgets.h"
#include "RecordingLibrary.h"
#include "RecordingTabletDisplay.h"
#include "CamcorderEquipmentComponent.h"
#include "ShoulderCamcorder.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Data/DataTransmitter.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Localization/GameLanguage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
class SRecordingTabletRoot : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SRecordingTabletRoot){} SLATE_ARGUMENT(TWeakObjectPtr<URecordingTabletComponent>,Tablet) SLATE_DEFAULT_SLOT(FArguments,Content) SLATE_END_ARGS()
 void Construct(const FArguments& A){Tablet=A._Tablet;ChildSlot[A._Content.Widget];}
 virtual bool SupportsKeyboardFocus() const override{return true;}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& E)override {if(auto* T=Tablet.Get())if(!E.IsRepeat())T->HandleKey(E.GetKey());return FReply::Handled();}
private:TWeakObjectPtr<URecordingTabletComponent> Tablet;
};
URecordingTabletComponent::URecordingTabletComponent(){PrimaryComponentTick.bCanEverTick=true;}
URecordingLibrary* URecordingTabletComponent::Library()const{return GetOwner()?GetOwner()->FindComponentByClass<URecordingLibrary>():nullptr;}
UCamcorderEquipmentComponent* URecordingTabletComponent::Equipment()const{return GetOwner()?GetOwner()->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;}
bool URecordingTabletComponent::HandleKey(FKey Key){
 if(!bOpen)return false;
 if(Key==EKeys::Escape||Key==EKeys::Tab){SetOpen(false);return true;}
 if(Key==EKeys::SpaceBar){TogglePlayback();return true;}
 if(Key==EKeys::Left)Seek((PlaybackSeconds-5)/FMath::Max(.01f,Playback.NumFrames()/15.f));
 if(Key==EKeys::Right)Seek((PlaybackSeconds+5)/FMath::Max(.01f,Playback.NumFrames()/15.f));
 return true;
}
void URecordingTabletComponent::SetOpen(bool Open){
 // Recording review is retained but temporarily unavailable from every entry point.
 if(Open)return;
 if(bOpen==Open)return;
 auto* PC=Cast<APlayerController>(GetOwner());auto* E=Equipment();
 if(!PC||!E||!GetWorld()->GetGameViewport())return;
 if(Open){
  if(auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()))if(C->Health<=0)return;
  if(E->bBackpackOpen)E->SetBackpackOpen(false);
  E->CancelViewfinder();if(E->Camcorder)E->Camcorder->StopRecording();
  if(E->Combat)E->Combat->bADS=false;
  PreviousCursor=PC->bShowMouseCursor;
  PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);PC->bShowMouseCursor=true;
  if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
  bOpen=true;
  if(!VideoTexture){TArray64<uint8> Black;Black.SetNumZeroed(640*360*4);for(int32 I=3;I<Black.Num();I+=4)Black[I]=255;VideoTexture=UTexture2D::CreateTransient(640,360,PF_B8G8R8A8,NAME_None,Black);VideoTexture->SRGB=true;VideoTexture->UpdateResource();VideoBrush.SetResourceObject(VideoTexture);VideoBrush.ImageSize=FVector2D(640,360);VideoBrush.DrawAs=ESlateBrushDrawType::Image;}
  if(!Display){FActorSpawnParameters P;P.Owner=PC;P.ObjectFlags|=RF_Transient;Display=GetWorld()->SpawnActor<ARecordingTabletDisplay>(FVector(0,0,-70000),FRotator::ZeroRotator,P);}
  if(Display){TabletBrush.SetResourceObject(Display->Texture);TabletBrush.ImageSize=FVector2D(1200,900);TabletBrush.DrawAs=ESlateBrushDrawType::Image;Display->Capture->CaptureScene();}
  GetWorld()->GetSubsystem<UDataTransmitter>()->OnDataChanged.AddUniqueDynamic(this,&URecordingTabletComponent::OnDataChanged);
  if(auto* L=Library();L && !L->Clips.IsEmpty())SelectClip(L->Clips.IsValidIndex(SelectedClip)?SelectedClip:L->Clips.Num()-1);
  BuildUI();
 }else{
  bOpen=false;bPlaying=false;Playback.Close();LastFrame=INDEX_NONE;
  if(UI)GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(UI.ToSharedRef());UI.Reset();
  TabletBrush.SetResourceObject(nullptr);if(Display){Display->Destroy();Display=nullptr;}
  PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);PC->bShowMouseCursor=PreviousCursor;
  if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
  PC->SetInputMode(FInputModeGameOnly());FSlateApplication::Get().ReleaseAllPointerCapture();FSlateApplication::Get().SetAllUserFocusToGameViewport();
 }
}
void URecordingTabletComponent::SelectClip(int32 Index){
 auto* L=Library();if(!L||!L->Clips.IsValidIndex(Index))return;
 SelectedClip=Index;PlaybackSeconds=0;LastFrame=INDEX_NONE;bPlaying=false;
 if(Playback.Open(L->Clips[Index].Path)){PlaybackStatus=TEXT("播放中");bPlaying=true;UpdateFrame();}
 else PlaybackStatus=TEXT("无法读取录像文件；评分记录仍保留");
 if(bOpen && UI)BuildUI();
}
void URecordingTabletComponent::TogglePlayback(){
 if(!Playback.NumFrames())return;
 if(PlaybackSeconds>=Playback.NumFrames()/15.f)PlaybackSeconds=0;
 bPlaying=!bPlaying;PlaybackStatus=bPlaying?TEXT("播放中"):TEXT("已暂停");
}
void URecordingTabletComponent::Seek(float Fraction){
 if(!Playback.NumFrames())return;
 PlaybackSeconds=FMath::Clamp(Fraction,0.f,1.f)*(Playback.NumFrames()-1)/15.f;UpdateFrame();
}
void URecordingTabletComponent::UpdateFrame(){
 const int32 Frame=FMath::Clamp(FMath::FloorToInt(PlaybackSeconds*15),0,Playback.NumFrames()-1);
 if(Frame==LastFrame||!VideoTexture||Playback.NumFrames()==0)return;
 TArray<uint8> Pixels;
 if(!Playback.ReadFrame(Frame,Pixels)){bPlaying=false;PlaybackStatus=TEXT("录像帧读取失败");return;}
 if(VideoTexture->GetSizeX()!=Playback.GetWidth() || VideoTexture->GetSizeY()!=Playback.GetHeight()){
  VideoTexture=UTexture2D::CreateTransient(Playback.GetWidth(),Playback.GetHeight(),PF_B8G8R8A8);
  VideoTexture->SRGB=true;VideoTexture->UpdateResource();VideoBrush.SetResourceObject(VideoTexture);
  VideoBrush.ImageSize=FVector2D(Playback.GetWidth(),Playback.GetHeight());
 }
 auto* Bytes=new uint8[Pixels.Num()];FMemory::Memcpy(Bytes,Pixels.GetData(),Pixels.Num());
 auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Playback.GetWidth(),Playback.GetHeight());
 VideoTexture->UpdateTextureRegions(0,1,Region,Playback.GetWidth()*4,4,Bytes,[](uint8* Data,const FUpdateTextureRegion2D* R){delete[] Data;delete R;});LastFrame=Frame;
}
void URecordingTabletComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn){
 Super::TickComponent(DT,Type,Fn);if(!bOpen)return;
 auto* PC=Cast<APlayerController>(GetOwner());auto* C=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 if(!C||C->Health<=0){SetOpen(false);return;}
 if(bPlaying){PlaybackSeconds+=DT;if(PlaybackSeconds>=Playback.NumFrames()/15.f){PlaybackSeconds=Playback.NumFrames()/15.f;bPlaying=false;PlaybackStatus=TEXT("播放结束");}UpdateFrame();}
}
void URecordingTabletComponent::OnDataChanged(UObject* Source,FName Field,float){if(bOpen && Source==Library() && Field==TEXT("RecordedClips"))BuildUI();}
void URecordingTabletComponent::EndPlay(const EEndPlayReason::Type Reason){if(bOpen)SetOpen(false);Playback.Close();UI.Reset();Super::EndPlay(Reason);}
void URecordingTabletComponent::BuildUI(){
 if(!bOpen)return;auto* L=Library();if(!L)return;
 if(UI)GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(UI.ToSharedRef());
 const TWeakObjectPtr<URecordingTabletComponent> W(this);
 const auto Font=FCoreStyle::GetDefaultFontStyle("Regular",16),Small=FCoreStyle::GetDefaultFontStyle("Regular",13);
 auto List=SNew(SVerticalBox);
 for(int32 I=0;I<L->Clips.Num();++I){const auto& Clip=L->Clips[I];
  List->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SAudioButton).OnClicked_Lambda([W,I]{if(auto* T=W.Get())T->SelectClip(I);return FReply::Handled();})
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(SLocalizedTextBlock).Font(Font).ColorAndOpacity(I==SelectedClip?FLinearColor(.2f,.85f,1):FLinearColor::White).Text(FText::FromString(FString::Printf(TEXT("录像 %02d   %02d:%02d"),I+1,int32(Clip.Seconds)/60,int32(Clip.Seconds)%60)))]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(SLocalizedTextBlock).Font(Small).Text(FText::FromString(FString::Printf(TEXT("评分 %d   ·   %d 人"),Clip.Score,Clip.People.Num())))]]];
 }
 FString Score=TEXT("尚未选择录像"),People=TEXT("举起摄像机开始拍摄，保存的视频会出现在这里。");
 if(L->Clips.IsValidIndex(SelectedClip)){
  const auto& Clip=L->Clips[SelectedClip];Score=FString::Printf(TEXT("本段评分  %d 分     |     拍到 %d 人"),Clip.Score,Clip.People.Num());
  People=TEXT("");for(const auto& Line:Clip.ScoreDetails)People+=Line+TEXT("\n");
 }

 auto Detail=SNew(SVerticalBox);
 Detail->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(345)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(640).HeightOverride(360)[SNew(SImage).Image(&VideoBrush).ColorAndOpacity_Lambda([W]{auto* T=W.Get();return T&&T->LastFrame>=0?FLinearColor::White:FLinearColor::Black;})]]]];
 Detail->AddSlot().AutoHeight().Padding(0,6)[SNew(SAudioSlider).Value_Lambda([W]{auto* T=W.Get();return T?T->PlaybackSeconds/FMath::Max(.01f,T->Playback.NumFrames()/15.f):0.f;}).OnValueChanged_Lambda([W](float V){if(auto* T=W.Get())T->Seek(V);})];
 auto Controls=SNew(SHorizontalBox);
 Controls->AddSlot().AutoWidth()[SNew(SAudioButton).OnClicked_Lambda([W]{if(auto* T=W.Get())T->TogglePlayback();return FReply::Handled();})[SNew(SLocalizedTextBlock).Text_Lambda([W]{auto* T=W.Get();return FText::FromString(T&&T->bPlaying?TEXT("暂停"):TEXT("播放 / 重播"));})]];
 Controls->AddSlot().FillWidth(1).Padding(12,0)[SNew(SLocalizedTextBlock).Font(Small).Text_Lambda([W]{auto* T=W.Get();return T?FText::FromString(FString::Printf(TEXT("%s  %.1f / %.1f 秒"),*T->PlaybackStatus,T->PlaybackSeconds,T->Playback.NumFrames()/15.f)):FText::GetEmpty();})];
 Detail->AddSlot().AutoHeight()[Controls];
 Detail->AddSlot().AutoHeight().Padding(0,12,0,6)[SNew(SLocalizedTextBlock).Font(Font).ColorAndOpacity(FLinearColor(.35f,.9f,.72f)).Text(FText::FromString(Score))];
 Detail->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SLocalizedTextBlock).Font(Small).AutoWrapText(true).Text(FText::FromString(People))]];
 auto Columns=SNew(SHorizontalBox);
 Columns->AddSlot().AutoWidth().Padding(0,0,16,0)[SNew(SBox).WidthOverride(205)[SNew(SScrollBox)+SScrollBox::Slot()[List]]];
 Columns->AddSlot().FillWidth(1)[Detail];
 auto Header=SNew(SHorizontalBox);
 Header->AddSlot().FillWidth(1)[SNew(SLocalizedTextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",23)).Text(FText::FromString(TEXT("现场影像  /  本次进场")))];
 Header->AddSlot().AutoWidth()[SNew(SAudioButton).OnClicked_Lambda([W]{if(auto* T=W.Get())T->SetOpen(false);return FReply::Handled();})[SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("关闭  Esc")))]];
 auto Screen=SNew(SVerticalBox);
 Screen->AddSlot().AutoHeight()[Header];
 Screen->AddSlot().AutoHeight().Padding(0,10)[SNew(SLocalizedTextBlock).Font(Small).Text(FText::FromString(FString::Printf(TEXT("已保存 %d 段  ·  按策划案累计计分 · 事件 Level 1/2/3＝1/2/3 倍"),L->Clips.Num())))];
 Screen->AddSlot().FillHeight(1)[Columns];
 Screen->AddSlot().AutoHeight().Padding(0,10,0,0)[SNew(SLocalizedTextBlock).Font(Small).Text(FText::FromString(TEXT("空格 播放/暂停  ·  ← → 跳转 5 秒  ·  Tab / Esc 收起平板")))];
 auto Shell=SNew(SOverlay);
 Shell->AddSlot()[SNew(SImage).Image(&TabletBrush)];
 Shell->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(910).HeightOverride(680)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.025f,.035f)).Padding(18)[Screen]]];
 UI=SNew(SRecordingTabletRoot).Tablet(W)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.005f,.009f,.014f,.98f)).Padding(12)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1200).HeightOverride(900)[Shell]]]];
 GetWorld()->GetGameViewport()->AddViewportWidgetContent(UI.ToSharedRef(),50);
 auto* PC=Cast<APlayerController>(GetOwner());FInputModeUIOnly Mode;Mode.SetWidgetToFocus(UI);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Mode);FSlateApplication::Get().SetKeyboardFocus(UI,EFocusCause::SetDirectly);
}
