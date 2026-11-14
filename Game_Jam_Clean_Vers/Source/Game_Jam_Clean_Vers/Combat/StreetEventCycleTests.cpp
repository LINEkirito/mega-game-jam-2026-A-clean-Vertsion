#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "WorldEvents.h"
#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FStreetEventCycleCheck,FAutomationTestBase*,Test);
bool FStreetEventCycleCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 if(!W){Test->AddError(TEXT("Missing PIE world"));return true;}
 auto* D=W->GetSubsystem<UWorldEvents>();auto* Rules=GetMutableDefault<UGlobalData>();
 TGuardValue<bool> Automatic(D->bAutomatic,false),Enabled(Rules->bEnableStreetEvents,true);
 for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SocialCooldown=10000;
 TArray<APatrolEnemy*> People;
 auto Spawn=[&](ENPCRole Role){
  const FTransform T(FRotator::ZeroRotator,FVector(55000+People.Num()*150,55000,5000));
  auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  N->NPCRole=Role;N->PatrolRadius=0;N->FinishSpawning(T);N->SetActorTickEnabled(false);N->GetCharacterMovement()->DisableMovement();People.Add(N);return N;
 };
 for(auto Kind:{EStreetEvent::GangMeeting,EStreetEvent::SecretTransaction,EStreetEvent::CivilianEscape,EStreetEvent::GangShootout,EStreetEvent::MassiveGangWar,EStreetEvent::GangMassacre}){
  auto* A=Spawn(Kind==EStreetEvent::CivilianEscape?ENPCRole::Civilian:ENPCRole::GangMember);
  auto* B=Spawn(Kind==EStreetEvent::GangShootout||Kind==EStreetEvent::MassiveGangWar?ENPCRole::Police:Kind==EStreetEvent::GangMassacre?ENPCRole::Civilian:ENPCRole::GangMember);
  const int32 Id=D->BeginEvent(Kind,{A,B});
  auto* E=D->Events.FindByPredicate([Id](const auto& R){return R.Id==Id;});
  if(!Test->TestNotNull(TEXT("Each kind creates an occurrence"),E))continue;
  const float Min=Kind==EStreetEvent::GangMeeting?30:Kind==EStreetEvent::GangShootout?120:Kind==EStreetEvent::MassiveGangWar||Kind==EStreetEvent::GangMassacre?180:20;
  const float Max=Kind==EStreetEvent::GangMeeting?45:Kind==EStreetEvent::GangShootout?180:Kind==EStreetEvent::MassiveGangWar||Kind==EStreetEvent::GangMassacre?240:30;
  Test->TestTrue(TEXT("Duration matches selected type"),E->Duration>=Min&&E->Duration<=Max);
  if(Kind==EStreetEvent::GangMeeting){for(auto* N:{A,B}){N->EventDestination=N->GetActorLocation();N->SetActivity(ENPCActivity::Meeting);}}
  else if(Kind==EStreetEvent::SecretTransaction){A->Partner=B;B->Partner=A;A->SetActivity(ENPCActivity::Trade);B->SetActivity(ENPCActivity::Trade);A->bSocialArrived=B->bSocialArrived=true;}
  else if(Kind==EStreetEvent::CivilianEscape){A->SetActivity(ENPCActivity::Flee);B->SetActivity(ENPCActivity::Flee);}
  else {A->CombatTarget=B;B->CombatTarget=A;A->SetActivity(ENPCActivity::Combat);B->SetActivity(ENPCActivity::Combat);}
  E->bHasPerformed=true;E->bPerforming=true;E->ExpiresAt=W->GetTimeSeconds()+10;
  // Staging time never consumes the performance duration or imposes the old 60/90s cap.
  E->Started=W->GetTimeSeconds()-500;D->Tick(0);
  Test->TestTrue(TEXT("Old staging timestamp cannot end a running event"),D->Find(Id)->bActive);
  E->ExpiresAt=W->GetTimeSeconds()-1;D->Tick(0);
  Test->TestFalse(TEXT("Every kind ends at its deadline"),D->Find(Id)->bActive);
  Test->TestEqual(TEXT("Participants released"),A->StreetEventId,0);
  Test->TestEqual(TEXT("Full cooldown applied"),A->SocialCooldown,Rules->EventCooldownSeconds);
  Test->TestNull(TEXT("Event combat target cleared"),A->CombatTarget.Get());
  if(Kind==EStreetEvent::SecretTransaction){Test->TestEqual(TEXT("First trader counted once"),A->CompletedTrades,1);Test->TestEqual(TEXT("Second trader counted once"),B->CompletedTrades,1);D->EndEvent(Id);Test->TestEqual(TEXT("Ending twice cannot count twice"),B->CompletedTrades,1);}
  A->Destroy();B->Destroy();
 }
 // A temporarily unavailable selected event must survive multiple scheduling attempts.
 D->PendingEvents.Reset();D->PendingEvents.Add(EStreetEvent::MassiveGangWar);D->RetryAttempt=0;
 {
  TGuardValue<float> Earliest(Rules->LargeEventEarliestSeconds,100000.f);TGuardValue<bool> Run(D->bAutomatic,true);
  for(int32 I=0;I<4;++I)D->Tick(20);
  Test->TestEqual(TEXT("Unavailable draw remains pending"),D->PendingEvents.Num(),1);
  Test->TestEqual(TEXT("Retries never replace the selected type"),D->PendingEvents[0],EStreetEvent::MassiveGangWar);
 }
 D->PendingEvents.Reset();
 auto* A=Spawn(ENPCRole::GangMember);auto* B=Spawn(ENPCRole::GangMember);
 const int32 Id=D->BeginEvent(EStreetEvent::SecretTransaction,{A,B});
 auto* E=D->Events.FindByPredicate([Id](const auto& R){return R.Id==Id;});E->bScheduled=true;
 D->EndEvent(Id);Test->TestEqual(TEXT("Interrupted preparation requeues accepted occurrence"),D->PendingEvents.Num(),1);
 D->PendingEvents.Reset();
 auto* Officer=Spawn(ENPCRole::Police);auto* Gang=Spawn(ENPCRole::GangMember);
 const int32 Fight=D->BeginEvent(EStreetEvent::GangShootout,{Officer,Gang});
 auto* Player=W->GetFirstPlayerController()->GetPawn().Get();Officer->CombatTarget=Player;Officer->SetActivity(ENPCActivity::Combat);
 D->EndEvent(Fight);Test->TestEqual(TEXT("Event cleanup never steals police pursuit of player"),Officer->CombatTarget.Get(),static_cast<AActor*>(Player));
 for(auto* N:People)if(IsValid(N))N->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetEventCycleTest,"Game.Events.TimedCycles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStreetEventCycleTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FStreetEventCycleCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "WorldEvents.h"
#include "PatrolEnemy.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Localization/GameLanguage.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

class FStreetEventVisualCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 int32 Phase=0,Id=0;
 double Began=0,PhotoTime=0;
 bool Fired=false;
 TArray<TWeakObjectPtr<APatrolEnemy>> Cast;
 TWeakObjectPtr<ACameraActor> Camera;
public:
 explicit FStreetEventVisualCheck(FAutomationTestBase* In):Test(In){}
 virtual bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  if(!W){Test->AddError(TEXT("Missing visual test world"));return true;}
  auto* D=W->GetSubsystem<UWorldEvents>();auto* PC=W->GetFirstPlayerController();
  const EStreetEvent Kinds[]={EStreetEvent::GangShootout,EStreetEvent::SecretTransaction,EStreetEvent::GangMeeting};
  const double Now=FPlatformTime::Seconds();D->bAutomatic=false;
  if(!Id){
   // Real reachable, off-screen staging, with the normal animations and AI ticks.
   for(TActorIterator<APatrolEnemy> It(W);It;++It){It->SetActorTickEnabled(false);It->SocialCooldown=10000;}
   D->PreparedCast.Reset();
   if(!Began)Began=Now;
   if(!D->PrepareScheduledCast(Kinds[Phase])){if(Now-Began>30){Test->AddError(TEXT("Could not stage reachable hidden event cast"));return true;}return false;}
   Cast=D->PreparedCast;
   TGuardValue<bool> Starting(D->bStartingScheduled,true);
   if(!D->TryStartEvent(Kinds[Phase])){Test->AddError(TEXT("Prepared event did not start"));return true;}
   Id=Cast[0]->StreetEventId;D->PreparedCast.Reset();Began=Now;PhotoTime=0;Fired=false;
   FVector Center=FVector::ZeroVector;for(auto Weak:Cast)Center+=Weak->GetActorLocation();Center/=Cast.Num();
   const FVector Position=Center+FVector(0,-650,260);
   Camera=W->SpawnActor<ACameraActor>(Position,(Center-Position).Rotation());PC->SetViewTarget(Camera.Get());
   return false;
  }
  for(auto Weak:Cast)if(auto* N=Weak.Get();N&&N->HeldWeapon&&N->HeldWeapon->Magazine<N->HeldWeapon->Spec().Capacity-1)Fired=true;
  const auto* E=D->Find(Id);
  if(E&&E->bHasPerformed&&!PhotoTime&&W->GetTimeSeconds()-E->PerformingSince>8){
   Test->TestTrue(TEXT("Event is still performing after initial animation"),E->bActive&&E->bPerforming);
   if(Phase==0)Test->TestTrue(TEXT("Both-faction fixture actually fires"),Fired);
   FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("G:/TheTechfiles/ThefinalGameJam/Events/Cycle-%d.png"),Phase),false,false);
   PhotoTime=Now;
   auto* Mutable=D->Events.FindByPredicate([this](const auto& R){return R.Id==Id;});Mutable->ExpiresAt=W->GetTimeSeconds()+3;
  }
  if(PhotoTime&&E&&!E->bActive){
   Test->TestTrue(TEXT("Real-time deadline expires after photo"),Now>PhotoTime);
   for(auto Weak:Cast)if(auto* N=Weak.Get()){
    Test->TestEqual(TEXT("Live participants leave event"),N->StreetEventId,0);
    if(Phase==0)Test->TestFalse(TEXT("Live crossfire stops at deadline"),N->bCombat);
    if(Phase==1)Test->TestEqual(TEXT("Live trade counts once per participant"),N->CompletedTrades,1);
    N->Destroy();
   }
   if(Camera.IsValid())Camera->Destroy();PC->SetViewTarget(PC->GetPawn());Cast.Reset();Id=0;Began=0;++Phase;
   return Phase==3;
  }
  if(Now-Began>45){Test->AddError(FString::Printf(TEXT("Event phase %d failed to perform and finish within 45 seconds"),Phase));return true;}
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetEventVisualTest,"Game.Events.LiveCycles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStreetEventVisualTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FStreetEventVisualCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}

class FEventDebugOverlayCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Step=0;double Since=0;bool PreviousLanguage=false,Cursor=false,Move=false,Look=false;
 TSharedPtr<SWidget> FindOverlay(TSharedRef<SWidget> Widget){
  if(Widget->GetTypeAsString()==TEXT("SEventDebugOverlay"))return Widget;
  auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)if(auto Found=FindOverlay(Children->GetChildAt(I)))return Found;return nullptr;
 }
 void Key(FKey Value){auto& App=FSlateApplication::Get();App.ProcessKeyDownEvent(FKeyEvent(Value,FModifierKeysState(),0,false,0,0));App.ProcessKeyUpEvent(FKeyEvent(Value,FModifierKeysState(),0,false,0,0));}
public:
 explicit FEventDebugOverlayCheck(FAutomationTestBase* In):Test(In){}
 virtual bool Update() override {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC){Test->AddError(TEXT("Missing debug overlay player"));return true;}
  auto& App=FSlateApplication::Get();auto Window=App.FindWidgetWindow(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
  const double Now=FPlatformTime::Seconds();if(Since&&Now-Since<3.0)return false;Since=Now;
  if(Step==0){
   PreviousLanguage=GameLanguage::IsEnglish();GameLanguage::SetEnglish(false);Cursor=PC->bShowMouseCursor;Move=PC->IsMoveInputIgnored();Look=PC->IsLookInputIgnored();
   Window->BringToFront(true);App.SetAllUserFocusToGameViewport();Key(EKeys::F2);++Step;return false;
  }
  if(Step==1){
   auto Overlay=FindOverlay(Window->GetContent());Test->TestTrue(TEXT("F2 opens the in-game overlay"),Overlay.IsValid());
   if(Overlay){Test->TestEqual(TEXT("Overlay is click-through"),Overlay->GetVisibility(),EVisibility::HitTestInvisible);Test->TestTrue(TEXT("Overlay stays narrow"),Overlay->GetCachedGeometry().GetLocalSize().X<=380);}
   Test->TestFalse(TEXT("Debug view never pauses simulation"),PC->IsPaused());
   Test->TestEqual(TEXT("Cursor unchanged"),PC->bShowMouseCursor,Cursor);Test->TestEqual(TEXT("Movement unchanged"),PC->IsMoveInputIgnored(),Move);Test->TestEqual(TEXT("Look unchanged"),PC->IsLookInputIgnored(),Look);
   FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/Events/Debug-zh.png"),true,false);++Step;return false;
  }
  if(Step==2){GameLanguage::SetEnglish(true);++Step;return false;}
  if(Step==3){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/Events/Debug-en.png"),true,false);++Step;return false;}
  if(Step==4){Key(EKeys::F10);++Step;return false;}
  if(Step==5){Key(EKeys::Escape);++Step;return false;}
  Test->TestTrue(TEXT("Overlay survives returning from settings"),FindOverlay(Window->GetContent()).IsValid());
  Test->TestFalse(TEXT("Returning from settings restores gameplay"),PC->IsPaused()||PC->IsMoveInputIgnored()||PC->IsLookInputIgnored());
  Key(EKeys::F2);Test->TestFalse(TEXT("F2 closes overlay"),FindOverlay(Window->GetContent()).IsValid());
  Key(EKeys::F2);Test->TestTrue(TEXT("F2 reopens without a password"),FindOverlay(Window->GetContent()).IsValid());Key(EKeys::F2);
  GameLanguage::SetEnglish(PreviousLanguage);return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEventDebugOverlayTest,"Game.Events.DebugOverlay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEventDebugOverlayTest::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FEventDebugOverlayCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
