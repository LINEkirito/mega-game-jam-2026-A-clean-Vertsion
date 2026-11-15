#include "GameLanguageUI.h"
#include "Frontend/MenuPresentation.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Audio/AudioWidgets.h"
#include "Audio/BackgroundMusic.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameLanguage.h"
#include "Frontend/GameFrontend.h"
#include "Frontend/TitleMenu.h"
#include "Frontend/IntroPresentation.h"
#include "Frontend/OpeningCountdown.h"
#include "Frontend/LocalUserSettings.h"
#include "Frontend/RunScoreHistoryPanel.h"
#if WITH_EDITOR
#include "Frontend/EventDebugOverlay.h"
#include "Frontend/SettlementDebugPanel.h"
#endif
#include "Kismet/KismetSystemLibrary.h"
#include "Performance/GamePerformanceSettings.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Combat/PatrolEnemy.h"
#include "Combat/StatueTargetComponent.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Camcorder/RecordingTabletComponent.h"
#include "Camcorder/RecordingLibrary.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SViewport.h"

namespace
{
 class FLanguageInput final : public IInputProcessor
 {
 public:
  explicit FLanguageInput(APlayerController* PC):Player(PC){}
  virtual ~FLanguageInput() override {Shutdown();}
  void Shutdown()
  {
   RestoreCountdownCamera();
   RemoveCountdown();
   bOnboarding=false;bCountingDown=false;
#if WITH_EDITOR
   if(auto* PC=Player.Get())if(auto* V=PC->GetWorld()->GetGameViewport())if(DebugRoot)V->RemoveViewportWidgetContent(DebugRoot.ToSharedRef());
   DebugRoot.Reset();
#endif
   GameFrontend::ReleaseAudio(Player.Get());
   if(bOpen){bFrontend=false;bPauseMenu=false;bPerformance=true;Close();}
   if(auto* PC=Player.Get())if(auto* V=PC->GetWorld()->GetGameViewport())if(Root)V->RemoveViewportWidgetContent(Root.ToSharedRef());
   Root.Reset();
   for(auto& Pair:Labels){if(auto* S=Pair.Key.Get())S->SetHiddenInGame(Pair.Value.bHidden);if(auto* W=Pair.Value.Widget.Get())W->DestroyComponent();}
   Labels.Empty();
  }
  bool InGameWindow() const
  {
   auto* PC=Player.Get();if(!PC||!PC->GetWorld()||!PC->GetWorld()->IsGameWorld())return false;
   auto* V=PC->GetWorld()->GetGameViewport();if(!V)return false;
   auto View=V->GetGameViewportWidget();if(!View)return false;
   auto& App=FSlateApplication::Get();
   if(App.GetActiveTopLevelWindow()!=App.FindWidgetWindow(View.ToSharedRef()))return false;
   for(auto Focus=App.GetKeyboardFocusedWidget();Focus;Focus=Focus->GetParentWidget())
   {
    const FString Type=Focus->GetTypeAsString();
    if(Type.Contains(TEXT("Console")))return false;
    if(Focus==View||Focus==Root||Type==TEXT("SCamcorderBackpackRoot")||Type==TEXT("SRecordingTabletRoot")||Type==TEXT("SRecordingReview"))return true;
   }
   return false;
  }
  virtual bool HandleKeyDownEvent(FSlateApplication&,const FKeyEvent& E) override
  {
   if(!InGameWindow())return false;
   if(bCountingDown)return true;
   if(bOnboarding&&E.GetKey()==EKeys::Escape)return true;
#if WITH_EDITOR
   if(E.GetKey()==EKeys::F2){if(!E.IsRepeat())ToggleDebug();return true;}
   if(E.GetKey()==EKeys::F3){if(!E.IsRepeat()&&!bOnboarding){if(!bOpen)Open(true);ShowSettlementDebug();}return true;}
#endif
   if(E.GetKey()==EKeys::Escape)
   {
    if(bEatEscape)return true;
    if(bOpen){if(!E.IsRepeat())Close();bEatEscape=true;return true;}
    auto* PC=Player.Get();auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
    auto* Equipment=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
    // Existing contextual Escape actions retain priority (inventory, tablet, viewfinder).
    if(Equipment&&(Equipment->bBackpackOpen||(Equipment->Tablet&&Equipment->Tablet->bOpen)||(Equipment->Camcorder&&Equipment->Camcorder->bViewfinderRaised)))return false;
    if(Equipment&&Equipment->RecordingLibrary&&Equipment->RecordingLibrary->bRoundEnded){if(!E.IsRepeat())Open(true);bEatEscape=true;return true;}
    if(!P||P->Health<=0||P->HasDeathFallStarted())
    {bEatEscape=true;return true;} // Never let gameplay Escape fall through to PIE's Stop shortcut.
    if(!E.IsRepeat()){bPauseMenu=true;Open(true);ShowPause();}
    bEatEscape=true;return true;
   }
   // Let Slate handle keyboard navigation, but stop other preprocessors from performing gameplay actions.
   if(bOpen)return E.GetKey()!=EKeys::Tab&&E.GetKey()!=EKeys::Enter&&E.GetKey()!=EKeys::SpaceBar&&E.GetKey()!=EKeys::Up&&E.GetKey()!=EKeys::Down&&E.GetKey()!=EKeys::Left&&E.GetKey()!=EKeys::Right;
   return false;
  }
  virtual bool HandleKeyUpEvent(FSlateApplication&,const FKeyEvent& E) override
  {
   if(E.GetKey()==EKeys::Escape&&bEatEscape){bEatEscape=false;return true;}
   if(!InGameWindow())return false;
   if(bCountingDown)return true;
   const FKey Key=E.GetKey();
#if WITH_EDITOR
   if(Key==EKeys::F2||Key==EKeys::F3)return true;
#endif
   return (bOpen&&Key!=EKeys::Tab&&Key!=EKeys::Enter&&Key!=EKeys::SpaceBar&&Key!=EKeys::Up&&Key!=EKeys::Down&&Key!=EKeys::Left&&Key!=EKeys::Right);
  }
  virtual bool HandleMouseMoveEvent(FSlateApplication&,const FPointerEvent& E) override
  {
   if(!bCountingDown||!InGameWindow())return false;
   // Consume look input until ACTION; no pawn or weapon input is dispatched while paused.
   return true;
  }
  virtual bool HandleAnalogInputEvent(FSlateApplication&,const FAnalogInputEvent& E) override
  {
   if(!bCountingDown||!InGameWindow())return false;
   return true;
  }
  virtual bool HandleMouseButtonDownEvent(FSlateApplication&,const FPointerEvent&) override{return bCountingDown&&InGameWindow();}
  virtual bool HandleMouseButtonUpEvent(FSlateApplication&,const FPointerEvent&) override{return bCountingDown&&InGameWindow();}
  virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication&,const FPointerEvent&) override{return bCountingDown&&InGameWindow();}
  virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&,const FPointerEvent&,const FPointerEvent*) override{return bCountingDown&&InGameWindow();}
  virtual void Tick(float DT,FSlateApplication&,TSharedRef<ICursor>) override
  {
   auto* PC=Player.Get();if(!PC||!PC->GetWorld())return;
   TickOpening(FMath::Clamp(DT,0.f,.1f));
   if(bOpen&&!bFrontend&&!bEndingSettings)
   {
    auto* P=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn());
    auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
    if(!P||P->Health<=0||P->HasDeathFallStarted()||(Equipment&&Equipment->RecordingLibrary&&Equipment->RecordingLibrary->bRoundEnded))ResumeGame();
   }
   if(bBoot && PC->GetPawn() && PC->GetWorld()->GetGameViewport() && PC->GetWorld()->GetAuthGameMode() && UGameplayStatics::HasOption(PC->GetWorld()->GetAuthGameMode()->OptionsString,TEXT("StartRun")))
   {bBoot=false;GameLocalSettings::LoadPerformance();GameFrontend::ApplyAudio(PC);ShowOpeningTutorial();}
   if(bBoot && GIsAutomationTesting && !(PC->GetWorld()->GetAuthGameMode() && UGameplayStatics::HasOption(PC->GetWorld()->GetAuthGameMode()->OptionsString,TEXT("ShowTitle"))))bBoot=false;
   if(bBoot && PC->GetPawn() && PC->GetWorld()->GetGameViewport()){bBoot=false;bFrontend=true;GameLocalSettings::LoadPerformance();Open(false);ShowMain();GameFrontend::ApplyAudio(PC);}
   ScanTime-=DT;
   if(ScanTime<=0)
   {
    ScanTime=.5f;
    for(TActorIterator<AActor> It(PC->GetWorld());It;++It)
    {
     AActor* A=*It;
     if(!A->IsA<APatrolEnemy>()&&!A->IsA<ATacticalWeapon>()&&!A->IsA<ACombatTarget>()&&!A->IsA<ACombatAmmoPickup>()&&!A->FindComponentByClass<UStatueTargetComponent>())continue;
     TArray<UTextRenderComponent*> Texts;A->GetComponents(Texts);
     for(auto* S:Texts)
     {
      if(Labels.Contains(S))continue;
      auto* W=NewObject<UWidgetComponent>(A,NAME_None,RF_Transient);
      W->SetWidgetSpace(EWidgetSpace::World);W->SetDrawSize(FVector2D(600,100));W->SetDrawAtDesiredSize(true);
      W->SetPivot(FVector2D(.5f,1.f));W->SetTwoSided(true);W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
      W->SetCastShadow(false);W->RegisterComponent();
      const TWeakObjectPtr<UTextRenderComponent> Weak(S);
      W->SetSlateWidget(SNew(SLocalizedTextBlock).Font(GameLanguage::Font(30)).Justification(ETextJustify::Center)
       .ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(1,1)).ShadowColorAndOpacity(FLinearColor::Black)
       .Text_Lambda([Weak]
       {
        if(!Weak.IsValid())return FText::GetEmpty();
        if(auto* Ammo=Cast<ACombatAmmoPickup>(Weak->GetOwner()))return FText::FromString(Ammo->AmmoName()+FString::Printf(TEXT(" ×%d"),Ammo->Amount));
        return Weak->Text;
       }));
      Labels.Add(S,{W,bool(S->bHiddenInGame)});S->SetHiddenInGame(true);
     }
    }
   }
   for(auto It=Labels.CreateIterator();It;++It)
   {
    auto* S=It.Key().Get();auto* W=It.Value().Widget.Get();
    if(!S||!W){if(W)W->DestroyComponent();It.RemoveCurrent();continue;}
    W->SetWorldLocationAndRotation(S->GetComponentLocation(),S->GetComponentRotation());
    W->SetWorldScale3D(FVector(S->WorldSize/30.f));
    // IsVisible includes bHiddenInGame, which we set to suppress the original glyphs.
    W->SetVisibility(S->GetVisibleFlag()&&!It.Value().bHidden&&!S->GetOwner()->IsHidden());
   }
  }
 private:
#if WITH_EDITOR
  TSharedPtr<SWidget> DebugRoot;
  void ShowSettlementDebug()
  {
   if(!bOpen)return;
   bStyledMenu=false;bMain=false;bPauseRoot=false;
   PerformancePanel=SettlementDebug::Make([this]{Close();});ShowPerformance();
  }
  void ToggleDebug()
  {
   auto* PC=Player.Get();auto* V=PC?PC->GetWorld()->GetGameViewport():nullptr;if(!V)return;
   if(DebugRoot){V->RemoveViewportWidgetContent(DebugRoot.ToSharedRef());DebugRoot.Reset();return;}
   DebugRoot=SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
    +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0,12,12,0)[EventDebug::MakeOverlay(PC)];
   V->AddViewportWidgetContent(DebugRoot.ToSharedRef(),1500);
  }
#endif
  struct FLabel {TWeakObjectPtr<UWidgetComponent> Widget;bool bHidden=false;};
  TMap<TWeakObjectPtr<UTextRenderComponent>,FLabel> Labels;
  TWeakObjectPtr<APlayerController> Player;
  TSharedPtr<SWidget> Root,PerformancePanel,MainPanel;
  TSharedPtr<SBorder> PanelHost,Backdrop;
  TSharedPtr<SScaleBox> PanelScale;
  bool bPerformance=false;
  bool bBoot=true,bFrontend=false,bMain=false,bPausedByMenu=false;
  bool bPauseMenu=false,bPauseRoot=false;
  TWeakPtr<SWidget> PreviousFocus;
  bool bPreviousUIOnly=false,bEndingSettings=false;
  bool bOpen=false,bCursor=false,bEatEscape=false;
  bool bStyledMenu=false;
  bool bOnboarding=false,bIntro=false,bCountingDown=false,bLookReleased=false;
  bool bSavedPausedCamera=false,bCountdownCamera=false;
  float CountdownElapsed=0;
  double CountdownStartedAt=0,ActionStartedAt=0;
  FVector2D PendingMouseLook=FVector2D::ZeroVector,StickLook=FVector2D::ZeroVector;
  TSharedPtr<SWidget> CountdownRoot;
  TSharedPtr<SOpeningCountdown> Countdown;
  float ScanTime=0;
  void ShowIntro()
  {
   bStyledMenu=false;bMain=false;bOnboarding=true;bIntro=true;
   PerformancePanel=IntroPresentation::Make([this]{ShowOpeningTutorial();});ShowPerformance();
  }
  void ShowOpeningTutorial()
  {
   // Initial onboarding retains menu music until READY starts the countdown.
   if(auto* Music=UBackgroundMusic::For(Player.Get()))
    if(Music->GetStage()==EBackgroundMusicStage::None)
    {
     const auto* Mode=Player->GetWorld()->GetAuthGameMode();
     const bool bRestart=Mode&&UGameplayStatics::HasOption(Mode->OptionsString,TEXT("StartRun"));
     Music->SetStage(bRestart?EBackgroundMusicStage::Gameplay:EBackgroundMusicStage::Menu);
    }
   if(!bOpen){bFrontend=true;Open(false);}
   bStyledMenu=true;bMain=false;bIntro=false;bOnboarding=true;
   PerformancePanel=GameFrontend::Tutorial([this]{BeginCountdown();},true);ShowPerformance();
  }
  void BeginCountdown()
  {
   auto* PC=Player.Get();auto* W=PC?PC->GetWorld():nullptr;auto* V=W?W->GetGameViewport():nullptr;
   if(!V||bCountingDown)return;
   if(auto* Music=UBackgroundMusic::For(PC))Music->SetStage(EBackgroundMusicStage::Gameplay);
   bCountingDown=true;CountdownElapsed=0;CountdownStartedAt=FPlatformTime::Seconds();PendingMouseLook=StickLook=FVector2D::ZeroVector;
   bSavedPausedCamera=W->bIsCameraMoveableWhenPaused;W->bIsCameraMoveableWhenPaused=true;bCountdownCamera=true;
   if(Root)V->RemoveViewportWidgetContent(Root.ToSharedRef());
   CountdownRoot=SNew(SScaleBox).Stretch(EStretch::ScaleToFit).Visibility(EVisibility::HitTestInvisible)
    [SAssignNew(Countdown,SOpeningCountdown)];
   V->AddViewportWidgetContent(CountdownRoot.ToSharedRef(),2000);
   // Keep the existing menu movement/look locks until ACTION.
   bLookReleased=false;
   PC->bShowMouseCursor=false;
   if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
   PC->SetInputMode(FInputModeGameOnly());FSlateApplication::Get().SetAllUserFocusToGameViewport();
  }
  void RestoreCountdownCamera()
  {
   if(bCountdownCamera)if(auto* PC=Player.Get())if(auto* W=PC->GetWorld())W->bIsCameraMoveableWhenPaused=bSavedPausedCamera;
   bCountdownCamera=false;
  }
  void RemoveCountdown()
  {
   if(auto* PC=Player.Get())if(auto* V=PC->GetWorld()->GetGameViewport())if(CountdownRoot)V->RemoveViewportWidgetContent(CountdownRoot.ToSharedRef());
   CountdownRoot.Reset();Countdown.Reset();
  }
  void TickOpening(float DT)
  {
   if(!CountdownRoot)return;
   // A countdown is wall-clock time, even when rendering stalls. Give ACTION its
   // own clock so the release cue is always drawn before the overlay disappears.
   const double Now=FPlatformTime::Seconds();
   CountdownElapsed=bCountingDown?FMath::Min(float(Now-CountdownStartedAt),3.f):3.f+float(Now-ActionStartedAt);
   Countdown->SetElapsed(CountdownElapsed);
   auto* PC=Player.Get();
   if(bCountingDown&&PC)
   {
    PendingMouseLook=StickLook=FVector2D::ZeroVector;
    if(CountdownElapsed>=3.f)
    {
     ActionStartedAt=Now;
     RestoreCountdownCamera();bCountingDown=false;bOnboarding=false;bFrontend=false;bPerformance=true;
     if(auto* Music=UBackgroundMusic::For(PC))Music->SetStage(EBackgroundMusicStage::Gameplay);
     Close();FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
   }
   if(CountdownElapsed>=3.7f)RemoveCountdown();
  }
  void Open(bool Performance)
  {
   auto* PC=Player.Get();auto* V=PC?PC->GetWorld()->GetGameViewport():nullptr;if(!V)return;
   auto& App=FSlateApplication::Get();PreviousFocus=App.GetKeyboardFocusedWidget();bCursor=PC->bShowMouseCursor;
   auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
   bEndingSettings=PC->IsPaused()&&Equipment&&Equipment->RecordingLibrary&&Equipment->RecordingLibrary->bRoundEnded;
   bPreviousUIOnly=Equipment&&(Equipment->bBackpackOpen||(Equipment->Tablet&&Equipment->Tablet->bOpen)||(Equipment->RecordingLibrary&&Equipment->RecordingLibrary->bRoundEnded));
   App.DismissAllMenus();App.ReleaseAllPointerCapture();
   // Gameplay menus block player controls, but simulation and recording keep running.
   bPausedByMenu=bFrontend&&!PC->IsPaused()&&PC->SetPause(true);
   bOpen=true;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);PC->bShowMouseCursor=true;
   if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
   Root=SAssignNew(Backdrop,SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.008f,.013f,.022f,1.f)).Padding(20).HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SAssignNew(PanelScale,SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
     [SAssignNew(PanelHost,SBorder).Padding(26).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.04f,.06f))]];
   V->AddViewportWidgetContent(Root.ToSharedRef(),2000);
   if(Performance)
   {
    ShowSettings();
   }
   else ShowLanguage();
  }
  void ShowSettings()
  {
   bPauseRoot=false;
   bStyledMenu=true;bMain=false;PerformancePanel=GameFrontend::Settings(Player.Get(),[this]{Close();},[this]{ShowLanguage();});ShowPerformance();
  }
  void ResumeGame()
  {
   bPauseMenu=false;bPauseRoot=false;bPerformance=true;Close();
  }
  void ShowPause(bool bUnstuckFailed=false)
  {
   bStyledMenu=true;bPauseRoot=true;
   PerformancePanel=GameFrontend::Pause([this]{ResumeGame();},[this]{ShowSettings();},[this]
   {
    auto* PC=Player.Get();auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
    ResumeGame();
    if(P&&P->Health>0&&!P->HasDeathFallStarted()&&!P->ReturnToSpawn()){bPauseMenu=true;Open(true);ShowPause(true);}
   },[this]
   {
    auto* PC=Player.Get();if(!PC)return;
    ResumeGame();
    UGameplayStatics::OpenLevel(PC,FName(*UGameplayStatics::GetCurrentLevelName(PC,true)),true,TEXT("ShowTitle"));
   },bUnstuckFailed);
   ShowPerformance();
  }
  void ShowMain()
  {
   if(auto* Music=UBackgroundMusic::For(Player.Get()))Music->SetStage(EBackgroundMusicStage::Menu);
   bStyledMenu=false;bMain=true;
   // Reuse this title view when returning from a submenu so its entrance never restarts.
   if(!MainPanel)MainPanel=GameFrontend::Main([this]{ShowIntro();},[this]{ShowSettings();},[this]
   {
    bStyledMenu=true;bMain=false;PerformancePanel=GameFrontend::Tutorial([this]{ShowMain();});ShowPerformance();
   },[this]{bStyledMenu=true;bMain=false;PerformancePanel=GameFrontend::RunHistory([this]{ShowMain();});ShowPerformance();},[this]{if(auto* PC=Player.Get())UKismetSystemLibrary::QuitGame(PC,PC,EQuitPreference::Quit,false);});
   else TitleMenu::FinishEntrance(MainPanel.ToSharedRef());
   PerformancePanel=MainPanel;ShowPerformance();
  }
  TSharedPtr<SWidget> FindFocus(TSharedPtr<SWidget> Widget)
  {
   if(!Widget)return nullptr;
   if(Widget->SupportsKeyboardFocus())return Widget;
   auto* Children=Widget->GetChildren();
   for(int32 I=0;I<Children->Num();++I)if(auto Child=FindFocus(Children->GetChildAt(I)))return Child;
   return nullptr;
  }
  void FocusPanel(TSharedPtr<SWidget> Focus)
  {
   if(auto* PC=Player.Get())
   {
    Focus=FindFocus(Focus);
    FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Focus);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Mode);
    FSlateApplication::Get().SetKeyboardFocus(Focus);
   }
  }
  void ShowPerformance()
  {
   const bool Title=bFrontend&&(bMain||bIntro);
   Backdrop->SetHAlign(Title?HAlign_Fill:HAlign_Center);Backdrop->SetVAlign(Title?VAlign_Fill:VAlign_Center);
   Backdrop->SetPadding(Title?0:20);Backdrop->SetBorderBackgroundColor(Title?(bMain?FLinearColor::FromSRGBColor(FColor(171,0,0)):FLinearColor::Black):FLinearColor(.008f,.013f,.022f,1.f));
   PanelHost->SetPadding(Title?0:26);PanelHost->SetBorderBackgroundColor(Title?FLinearColor::Transparent:FLinearColor(.025f,.04f,.06f));
   PanelScale->SetStretchDirection(Title?EStretchDirection::Both:EStretchDirection::DownOnly);
   if(bStyledMenu){Backdrop->SetBorderBackgroundColor(FLinearColor(.006f,.005f,.005f));PanelHost->SetBorderBackgroundColor(MenuPresentation::Surface());}
   bPerformance=true;
   if(bStyledMenu)PanelHost->SetContent(SNew(SMenuEntrance)[PerformancePanel.ToSharedRef()]);
   else PanelHost->SetContent(PerformancePanel.ToSharedRef());
   FocusPanel(PerformancePanel);
  }
  void ShowLanguage()
  {
   bPerformance=false;
   auto Choices=SNew(SVerticalBox);
   Choices->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SMenuTextBlock).Font(GameLanguage::Font(28)).Text(FText::FromString(TEXT("语言")))];
   Choices->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(SMenuTextBlock).Text(FText::FromString(TEXT("选择后立即生效")))];
   TSharedPtr<SButton> FocusChoice;
   for(bool English:{false,true})
   {
    auto Choice=SNew(SMenuButton).ContentPadding(FMargin(22,14)).OnClicked_Lambda([English]{GameLanguage::SetEnglish(English);return FReply::Handled();})
     [SNew(SMenuTextBlock).Font(GameLanguage::Font(24)).Text_Lambda([English]{return FText::FromString(FString(GameLanguage::IsEnglish()==English?TEXT("✓  "):TEXT("    "))+(English?TEXT("English"):TEXT("中文")));})];
    if(English==GameLanguage::IsEnglish())FocusChoice=Choice;
    Choices->AddSlot().AutoHeight().Padding(0,6)[Choice];
   }
   Choices->AddSlot().AutoHeight().Padding(0,20,0,0)[SNew(SMenuButton).ContentPadding(12).OnClicked_Lambda([this]{Close();return FReply::Handled();})
    [SNew(SMenuTextBlock).Text(FText::FromString(TEXT("返回设置")))]];
   Backdrop->SetBorderBackgroundColor(FLinearColor(.006f,.005f,.005f));PanelHost->SetBorderBackgroundColor(MenuPresentation::Surface());
   PanelHost->SetContent(SNew(SMenuEntrance)[SNew(SBox).WidthOverride(400)[Choices]]);FocusPanel(FocusChoice);
  }

  void Close()
  {
   if(!bPerformance&&PerformancePanel){ShowPerformance();return;}
   if(bFrontend){if(!bMain)ShowMain();return;}
   if(bPauseMenu){if(!bPauseRoot){ShowPause();return;}bPauseMenu=false;bPauseRoot=false;}
   auto* PC=Player.Get();bOpen=false;if(!PC)return;
   if(auto* V=PC->GetWorld()->GetGameViewport())if(Root)V->RemoveViewportWidgetContent(Root.ToSharedRef());Root.Reset();PanelHost.Reset();PerformancePanel.Reset();MainPanel.Reset();Backdrop.Reset();PanelScale.Reset();
   if(bPausedByMenu){PC->SetPause(false);bPausedByMenu=false;}
   PC->SetIgnoreMoveInput(false);if(!bLookReleased)PC->SetIgnoreLookInput(false);bLookReleased=false;
   if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
   // Extraction may finish while this non-pausing menu is open. Keep the
   // settlement's newly established focus/cursor instead of restoring gameplay.
   auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
   if(Equipment&&Equipment->RecordingLibrary&&Equipment->RecordingLibrary->bRoundEnded)
   {
    // Settings opened from an existing ending must restore its UI focus as well.
    // When extraction completed behind settings, preserve the newly mounted ending instead.
    if(bPreviousUIOnly)if(auto Focus=PreviousFocus.Pin())
    {
     PC->bShowMouseCursor=true;FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Focus);PC->SetInputMode(Mode);
     FSlateApplication::Get().SetKeyboardFocus(Focus,EFocusCause::SetDirectly);
    }
    return;
   }
   PC->bShowMouseCursor=bCursor;
   if(bPreviousUIOnly){FInputModeUIOnly Mode;Mode.SetWidgetToFocus(PreviousFocus.Pin());Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Mode);}
   else PC->SetInputMode(FInputModeGameOnly());
   if(FSlateApplication::IsInitialized())
   {if(auto Focus=PreviousFocus.Pin())FSlateApplication::Get().SetKeyboardFocus(Focus);else FSlateApplication::Get().SetAllUserFocusToGameViewport();}
  }
 };
}
TSharedPtr<IInputProcessor> GameLanguage::Install(APlayerController* PC)
{
 if(!PC||!PC->IsLocalController()||!FSlateApplication::IsInitialized())return nullptr;
 auto Gate=MakeShared<FLanguageInput>(PC);FSlateApplication::Get().RegisterInputPreProcessor(Gate,0);return Gate;
}
void GameLanguage::Uninstall(TSharedPtr<IInputProcessor>& Handle)
{
 if(Handle){if(FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(Handle);Handle.Reset();}
}
