#include "GameLanguageUI.h"
#include "Audio/AudioWidgets.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameLanguage.h"
#include "Frontend/GameFrontend.h"
#include "Frontend/LocalUserSettings.h"
#include "Frontend/RunScoreHistoryPanel.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Performance/GamePerformanceSettings.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Combat/PatrolEnemy.h"
#include "Combat/StatueTargetComponent.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
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
   GameFrontend::ReleaseAudio(Player.Get());
   if(bOpen){bFrontend=false;bPerformance=true;Close();}
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
   if(E.GetKey()==EKeys::F10){if(!E.IsRepeat()){if(bOpen){if(bFrontend&&bMain)ShowSettings();else {bPerformance=true;Close();}}else Open(true);}return true;}
   if(bOpen&&E.GetKey()==EKeys::Escape){if(!E.IsRepeat())Close();bEatEscape=true;return true;}
   // Let Slate handle keyboard navigation, but stop other preprocessors from performing gameplay actions.
   if(bOpen)return E.GetKey()!=EKeys::Tab&&E.GetKey()!=EKeys::Enter&&E.GetKey()!=EKeys::SpaceBar&&E.GetKey()!=EKeys::Up&&E.GetKey()!=EKeys::Down&&E.GetKey()!=EKeys::Left&&E.GetKey()!=EKeys::Right;
   return false;
  }
  virtual bool HandleKeyUpEvent(FSlateApplication&,const FKeyEvent& E) override
  {
   if(E.GetKey()==EKeys::Escape&&bEatEscape){bEatEscape=false;return true;}
   if(!InGameWindow())return false;
   const FKey Key=E.GetKey();
   return Key==EKeys::F10||(bOpen&&Key!=EKeys::Tab&&Key!=EKeys::Enter&&Key!=EKeys::SpaceBar&&Key!=EKeys::Up&&Key!=EKeys::Down&&Key!=EKeys::Left&&Key!=EKeys::Right);
  }
  virtual void Tick(float DT,FSlateApplication&,TSharedRef<ICursor>) override
  {
   auto* PC=Player.Get();if(!PC||!PC->GetWorld())return;
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
  struct FLabel {TWeakObjectPtr<UWidgetComponent> Widget;bool bHidden=false;};
  TMap<TWeakObjectPtr<UTextRenderComponent>,FLabel> Labels;
  TWeakObjectPtr<APlayerController> Player;
  TSharedPtr<SWidget> Root,PerformancePanel;
  TSharedPtr<SBorder> PanelHost;
  bool bPerformance=false;
  bool bBoot=true,bFrontend=false,bMain=false,bPausedByMenu=false;
  TWeakPtr<SWidget> PreviousFocus;
  bool bPreviousUIOnly=false;
  bool bOpen=false,bCursor=false,bEatEscape=false;
  float ScanTime=0;
  void Open(bool Performance)
  {
   auto* PC=Player.Get();auto* V=PC?PC->GetWorld()->GetGameViewport():nullptr;if(!V)return;
   auto& App=FSlateApplication::Get();PreviousFocus=App.GetKeyboardFocusedWidget();bCursor=PC->bShowMouseCursor;
   auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
   bPreviousUIOnly=Equipment&&(Equipment->bBackpackOpen||(Equipment->Tablet&&Equipment->Tablet->bOpen)||(Equipment->RecordingLibrary&&Equipment->RecordingLibrary->bRoundEnded));
   App.DismissAllMenus();App.ReleaseAllPointerCapture();
   bPausedByMenu=!PC->IsPaused() && PC->SetPause(true);
   bOpen=true;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);PC->bShowMouseCursor=true;
   if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
   Root=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.008f,.013f,.022f,1.f)).Padding(20).HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
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
   bMain=false;PerformancePanel=GameFrontend::Settings(Player.Get(),[this]{Close();},[this]{ShowLanguage();});ShowPerformance();
  }
  void ShowMain()
  {
   bMain=true;
   PerformancePanel=GameFrontend::Main([this]{bFrontend=false;bPerformance=true;Close();FSlateApplication::Get().SetAllUserFocusToGameViewport();},[this]{ShowSettings();},[this]
   {
    bMain=false;PerformancePanel=GameFrontend::Tutorial([this]{ShowMain();});ShowPerformance();
   },[this]{bMain=false;PerformancePanel=GameFrontend::RunHistory([this]{ShowMain();});ShowPerformance();},[this]{if(auto* PC=Player.Get())UKismetSystemLibrary::QuitGame(PC,PC,EQuitPreference::Quit,false);});
   ShowPerformance();
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
   bPerformance=true;PanelHost->SetContent(PerformancePanel.ToSharedRef());FocusPanel(PerformancePanel);
  }
  void ShowLanguage()
  {
   bPerformance=false;
   auto Choices=SNew(SVerticalBox);
   Choices->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SLocalizedTextBlock).Font(GameLanguage::Font(28)).Text(FText::FromString(TEXT("语言")))];
   Choices->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("选择后立即生效")))];
   TSharedPtr<SButton> FocusChoice;
   for(bool English:{false,true})
   {
    auto Choice=SNew(SAudioButton).ContentPadding(FMargin(22,14)).OnClicked_Lambda([English]{GameLanguage::SetEnglish(English);return FReply::Handled();})
     [SNew(STextBlock).Font(GameLanguage::Font(24)).Text_Lambda([English]{return FText::FromString(FString(GameLanguage::IsEnglish()==English?TEXT("✓  "):TEXT("    "))+(English?TEXT("English"):TEXT("中文")));})];
    if(English==GameLanguage::IsEnglish())FocusChoice=Choice;
    Choices->AddSlot().AutoHeight().Padding(0,6)[Choice];
   }
   Choices->AddSlot().AutoHeight().Padding(0,20,0,0)[SNew(SAudioButton).ContentPadding(12).OnClicked_Lambda([this]{Close();return FReply::Handled();})
    [SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("返回设置")))]];
   PanelHost->SetContent(SNew(SBox).WidthOverride(400)[Choices]);FocusPanel(FocusChoice);
  }

  void Close()
  {
   if(!bPerformance&&PerformancePanel){ShowPerformance();return;}
   if(bFrontend){if(!bMain)ShowMain();return;}
   auto* PC=Player.Get();bOpen=false;if(!PC)return;
   if(auto* V=PC->GetWorld()->GetGameViewport())if(Root)V->RemoveViewportWidgetContent(Root.ToSharedRef());Root.Reset();PanelHost.Reset();PerformancePanel.Reset();
   if(bPausedByMenu){PC->SetPause(false);bPausedByMenu=false;}
   PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);PC->bShowMouseCursor=bCursor;
   if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
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
