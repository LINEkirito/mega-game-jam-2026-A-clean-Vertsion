#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "WorldEvents.h"
#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/BoxComponent.h"
#include "Camcorder/RecordingLibrary.h"
#include "Camcorder/ShoulderCamcorder.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FStreetEventRecordsCheck,FAutomationTestBase*,Test);
bool FStreetEventRecordsCheck::Update(){
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 if(!W){Test->AddError(TEXT("No PIE world"));return true;}
 auto* D=W->GetSubsystem<UWorldEvents>();if(!Test->TestNotNull(TEXT("Per-world event subsystem exists"),D))return true;
 TGuardValue<bool> Automatic(D->bAutomatic,false);
 const FVector Origin(30000,30000,6000);TArray<APatrolEnemy*> People;
 auto Spawn=[&](ENPCRole Role,FVector Offset){FTransform T(FRotator::ZeroRotator,Origin+Offset);auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  N->NPCRole=Role;N->PatrolRadius=0;N->FinishSpawning(T);N->GetCharacterMovement()->DisableMovement();N->SetActorTickEnabled(false);People.Add(N);return N;};
 auto* A=Spawn(ENPCRole::GangMember,FVector(600,-180,0));auto* B=Spawn(ENPCRole::GangMember,FVector(600,180,0));
 const int32 Trade=D->BeginEvent(EStreetEvent::SecretTransaction,{A,B});
 Test->TestTrue(TEXT("Event reserves participants"),Trade>0&&A->StreetEventId==Trade&&B->StreetEventId==Trade);
 Test->TestEqual(TEXT("Cannot double-book participant"),D->BeginEvent(EStreetEvent::GangMeeting,{A,B}),0);
 auto* C=Spawn(ENPCRole::Civilian,FVector(1000,1500,0));C->HearGunshot(A,A->GetActorLocation(),6000);
 Test->TestTrue(TEXT("Real gunshot creates common escape event"),C->StreetEventId>0&&C->Activity==ENPCActivity::Flee);
 const int32 Escape=C->StreetEventId;D->EndEvent(Escape);D->ObserveEscape(C);
 Test->TestEqual(TEXT("Same panic cannot farm fresh events"),C->StreetEventId,0);
 Test->TestTrue(TEXT("Rare score exceeds common score"),D->Find(Trade)->Points>D->Find(Escape)->Points);
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;P.ObjectFlags|=RF_Transient;
 auto* Camera=W->SpawnActor<AShoulderCamcorder>(Origin,FRotator::ZeroRotator,P);Camera->SetActorTickEnabled(false);Camera->Body->SetSimulatePhysics(false);
 Camera->Capture->SetWorldLocationAndRotation(Origin,FRotator::ZeroRotator);Camera->Capture->FOVAngle=90;
 auto* Owner=W->SpawnActor<AActor>(P);auto* L=NewObject<URecordingLibrary>(Owner);L->RegisterComponent();
 auto Sample=[&](URecordingLibrary* Library){Library->BeginClip(Camera);for(int32 I=0;I<8;++I){Camera->RecordingSeconds=I*.25f;Library->Sample(Camera);}};
 Sample(L);L->FinishClip(Camera,true);Test->TestEqual(TEXT("Staging is not a recordable performance"),L->Clips.Last().Events.Num(),0);
 auto* Record=D->Events.FindByPredicate([Trade](const FStreetEventRecord& E){return E.Id==Trade;});Record->bPerforming=true;
 B->SetActorHiddenInGame(true);Sample(L);L->FinishClip(Camera,true);Test->TestEqual(TEXT("One trader is insufficient evidence"),L->Clips.Last().Events.Num(),0);B->SetActorHiddenInGame(false);
 Sample(L);L->FinishClip(Camera,false);Test->TestFalse(TEXT("Failed save cannot claim event"),D->Find(Trade)->bClaimed);
 auto* OtherOwner=W->SpawnActor<AActor>(P);auto* Other=NewObject<URecordingLibrary>(OtherOwner);Other->RegisterComponent();
 Sample(L);Sample(Other);L->FinishClip(Camera,true);Other->FinishClip(Camera,true);
 Test->TestEqual(TEXT("Both visible traders produce one event"),L->Clips.Last().Events.Num(),1);
 if(L->Clips.Last().Events.Num()){Test->TestEqual(TEXT("Event awards configured rare points"),L->Clips.Last().Events[0].Points,GetDefault<UGlobalData>()->RareEventPoints);}
 if(Other->Clips.Last().Events.Num())Test->TestEqual(TEXT("Concurrent recorder cannot duplicate event score"),Other->Clips.Last().Events[0].Points,0);
 else Test->AddError(TEXT("Second recorder lost event metadata"));
 D->EndEvent(Trade);Test->TestEqual(TEXT("End releases participant reservations"),A->StreetEventId,0);
 Test->TestFalse(TEXT("Unknown event cannot be claimed"),D->Claim(999999));
 const int32 NewTrade=D->BeginEvent(EStreetEvent::SecretTransaction,{A,B});Test->TestTrue(TEXT("New occurrence has distinct identity"),NewTrade>Trade);D->EndEvent(NewTrade);
 Test->TestFalse(TEXT("Director does not invent peaceful escapes"),D->TryStartEvent(EStreetEvent::CivilianEscape));
 for(auto* N:People)N->Destroy();Camera->Destroy();Owner->Destroy();OtherOwner->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetEventRecordsRules,"Game.Events.RecordingAndLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStreetEventRecordsRules::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FStreetEventRecordsCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
