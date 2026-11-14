#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "RecordingLibrary.h"
#include "RecordingReview.h"
#include "CamcorderEquipmentComponent.h"
#include "ShoulderCamcorder.h"
#include "CamcorderPlayback.h"
#include "Combat/PatrolEnemy.h"
#include "Combat/WorldEvents.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Data/DataTransmitter.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/CapsuleComponent.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

class FRecordingSettlementCheck : public IAutomationLatentCommand {
 FAutomationTestBase* Test;bool Death;int32 Stage=0;double Started=0;TWeakObjectPtr<APatrolEnemy> Subject;
public:
 FRecordingSettlementCheck(FAutomationTestBase* T,bool D):Test(T),Death(D){}
 bool Update()override{
  if(!Started)Started=FPlatformTime::Seconds();
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;auto* E=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
  auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  if(!Player||!E||!E->RecordingLibrary||!E->Camcorder){Test->AddError(TEXT("Settlement fixture missing player/equipment"));return true;}
  auto* Camera=E->Camcorder.Get();auto* Library=E->RecordingLibrary.Get();
  if(FPlatformTime::Seconds()-Started>30){Test->AddError(FString::Printf(TEXT("Settlement flow timed out: stage=%d, raised=%d, recording=%d, seconds=%.2f"),Stage,Camera->bViewfinderRaised,Camera->bRecording,Camera->RecordingSeconds));return true;}
  if(Stage==0){
   for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
   W->GetSubsystem<UWorldEvents>()->bAutomatic=false;
   const FVector Origin(30000,30000,8000);Player->SetActorLocation(Origin);Player->GetCharacterMovement()->DisableMovement();PC->SetControlRotation(FRotator::ZeroRotator);
   E->EquipCamera();Camera->BatteryPercent=100;Camera->bPowered=true;
   FTransform T(FRotator(0,180,0),Origin+FVector(600,0,0));auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);N->NPCRole=ENPCRole::GangMember;N->PatrolRadius=0;N->FinishSpawning(T);N->SetActorTickEnabled(false);N->GetCharacterMovement()->DisableMovement();N->Activity=ENPCActivity::Idle;Subject=N;
   PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton,IE_Pressed,1));Stage=1;return false;
  }
  if(Stage==1){Camera->SetViewfinderRaised(true);if(!Camera->bViewfinderRaised)return false;Test->TestTrue(TEXT("Real video recording starts"),Camera->StartRecording());Stage=2;return false;}
  if(Stage==2){
   if(Camera->RecordingSeconds<4)return false;
   if(Death){FHitResult Hit(Player,Player->GetCapsuleComponent(),Player->GetActorLocation(),FVector::UpVector);W->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,1000,FVector::ForwardVector,PC,PC);}
   else {FSlateApplication::Get().SetAllUserFocusToGameViewport();FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::BackSpace,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::BackSpace,FModifierKeysState(),0,false,0,0));Test->TestFalse(TEXT("Backspace cannot settle the round"),Library->bRoundEnded);Library->EndRound(false);}
   Stage=3;return false;
  }
  if(!Library->bRoundEnded)return false;
  if(Death&&!Library->bSettlementShown){
   Test->TestFalse(TEXT("World continues during death fall and message"),W->IsPaused());
   Test->TestFalse(TEXT("Review cannot cover death fall"),Library->SettlementUI.IsValid());
   if(!Player->IsDeathFallComplete())Test->TestFalse(TEXT("Death message waits for completed fall"),Library->DeathUI.IsValid());
   return false;
  }
  if(Death)Test->TestTrue(TEXT("Fall completed before settlement"),Player->IsDeathFallComplete());
  if(Stage==3){
  Test->TestEqual(TEXT("Correct settlement reason"),Library->bEndedByDeath,Death);
  Test->TestFalse(TEXT("Final recording finalized"),Camera->bRecording);Test->TestEqual(TEXT("Final clip saved exactly once"),Library->Clips.Num(),1);
  Test->TestEqual(TEXT("Visible idle gang 4s scores15"),Library->RoundScore,Death?7.5f:15.f);
  if(Library->Clips.Num()==1){const auto& Clip=Library->Clips[0];
   FCamcorderPlayback Reader;Test->TestTrue(TEXT("Final AVI reopens"),Reader.Open(Clip.Path));Test->TestTrue(TEXT("Final AVI includes recorded duration"),Reader.NumFrames()>=60);
   Test->TestEqual(TEXT("Settlement video width is HD"),Reader.GetWidth(),1280);
   Test->TestEqual(TEXT("Settlement video height is HD"),Reader.GetHeight(),720);
   TArray<uint8> FinalPixels;Test->TestTrue(TEXT("Final GPU frame decodes after settlement"),Reader.ReadFrame(Reader.NumFrames()-1,FinalPixels));
   FString Json;Test->TestTrue(TEXT("Clip score audit saved"),FFileHelper::LoadFileToString(Json,*Clip.ScoreRecordPath));TSharedPtr<FJsonObject> Object;
   if(Test->TestTrue(TEXT("Clip audit valid JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object)))Test->TestEqual(TEXT("Persisted clip score agrees"),int32(Object->GetNumberField(TEXT("score"))),15);
  }
  const FString RecordPath=Library->RoundRecordPath;Library->EndRound(!Death);
  Test->TestEqual(TEXT("Repeated settlement preserves reason"),Library->bEndedByDeath,Death);Test->TestEqual(TEXT("Repeated settlement preserves total"),Library->RoundScore,Death?7.5f:15.f);Test->TestEqual(TEXT("Repeated settlement creates no extra audit"),Library->RoundRecordPath,RecordPath);
  Test->TestFalse(TEXT("Cannot record after settlement"),Camera->StartRecording());
  FString Json;Test->TestTrue(TEXT("Round audit saved"),FFileHelper::LoadFileToString(Json,*RecordPath));
  TSharedPtr<FJsonObject> Audit;
  if(Test->TestTrue(TEXT("Round audit valid JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Audit)))
  {
   Test->TestEqual(TEXT("Audit retains original score"),Audit->GetNumberField(TEXT("raw_score")),15.0);
   Test->TestEqual(TEXT("Audit applies outcome multiplier once"),Audit->GetNumberField(TEXT("settlement_multiplier")),Death?.5:1.0);
   Test->TestEqual(TEXT("Audit final score matches UI"),Audit->GetNumberField(TEXT("score")),double(Library->RoundScore));
  }
  Stage=4;return false;
  }
  auto Review=StaticCastSharedPtr<SRecordingReview>(Library->SettlementUI);
  if(!Review){Test->AddError(TEXT("Settlement review widget missing"));return true;}
  if(Review->GetPhase()==ERecordingReviewPhase::Overview)return false;
  Test->TestEqual(TEXT("Overview decoded actual first-frame thumbnail"),Review->GetThumbnailCount(),1);
  Test->TestEqual(TEXT("Overview automatically starts first clip"),Review->GetClipIndex(),0);
  Test->TestTrue(TEXT("Playback decoded a real AVI frame"),Review->GetDecodedFrame()>=0);
  Review->SetPlaybackRate(4);Test->TestEqual(TEXT("4x selected"),Review->GetPlaybackRate(),4);
  Review->TogglePause();Test->TestFalse(TEXT("Pause works"),Review->IsPlaying());Review->Seek(.8f);Test->TestTrue(TEXT("Seeking moves decoded position"),Review->GetPlaybackTime()>2);
  Review->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Two,FModifierKeysState(),0,false,0,0));Test->TestEqual(TEXT("Keyboard2 selects2x"),Review->GetPlaybackRate(),2);
  Review->ShowSummary();Test->TestEqual(TEXT("Summary accessible"),Review->GetPhase(),ERecordingReviewPhase::Summary);Test->TestEqual(TEXT("Replay does not alter total"),Library->RoundScore,Death?7.5f:15.f);
  if(Subject.IsValid())Subject->Destroy();return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingDeathSettlement,"Game.Recording.DeathSettlement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingDeathSettlement::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FRecordingSettlementCheck(this,true));AddCommand(new FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingExtractionSettlement,"Game.Recording.ExtractionSettlement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingExtractionSettlement::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FRecordingSettlementCheck(this,false));AddCommand(new FEndPlayMapCommand());return true;}
#endif
