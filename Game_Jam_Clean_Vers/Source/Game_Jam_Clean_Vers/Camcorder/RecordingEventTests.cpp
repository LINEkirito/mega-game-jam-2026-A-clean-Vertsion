#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "RecordingLibrary.h"
#include "ShoulderCamcorder.h"
#include "Combat/WorldEvents.h"
#include "Combat/PatrolEnemy.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/BoxComponent.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FFilmingEventMatrixCheck,FAutomationTestBase*,Test);
bool FFilmingEventMatrixCheck::Update(){
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}if(!W){Test->AddError(TEXT("No PIE world"));return true;}
 auto* D=W->GetSubsystem<UWorldEvents>();TGuardValue<bool> Automatic(D->bAutomatic,false);
 const FVector Origin(35000,35000,5000);FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;P.ObjectFlags|=RF_Transient;
 auto* Camera=W->SpawnActor<AShoulderCamcorder>(Origin,FRotator::ZeroRotator,P);Camera->SetActorTickEnabled(false);Camera->Body->SetSimulatePhysics(false);Camera->Capture->SetWorldLocationAndRotation(Origin,FRotator::ZeroRotator);Camera->Capture->FOVAngle=90;
 const EStreetEvent Kinds[]={EStreetEvent::CivilianEscape,EStreetEvent::GangMeeting,EStreetEvent::SecretTransaction,EStreetEvent::GangShootout,EStreetEvent::MassiveGangWar,EStreetEvent::GangMassacre};
 for(const auto Kind:Kinds){
  TArray<APatrolEnemy*> People;
  auto Spawn=[&](ENPCRole Role,float Y){FTransform T(FRotator::ZeroRotator,Origin+FVector(Y==0?1050:800,Y,0));auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);N->NPCRole=Role;N->PatrolRadius=0;N->FinishSpawning(T);N->GetCharacterMovement()->DisableMovement();N->SetActorTickEnabled(false);N->Health=10000;People.Add(N);return N;};
  const bool Escape=Kind==EStreetEvent::CivilianEscape,Meeting=Kind==EStreetEvent::GangMeeting,Trade=Kind==EStreetEvent::SecretTransaction,Massacre=Kind==EStreetEvent::GangMassacre,War=Kind==EStreetEvent::MassiveGangWar||Massacre;
  if(Escape)Spawn(ENPCRole::Civilian,0);
  else {Spawn(ENPCRole::GangMember,-240);Spawn(Meeting||Trade||War?ENPCRole::GangMember:ENPCRole::Police,0);if(Meeting||War)Spawn(Meeting?ENPCRole::GangMember:Massacre?ENPCRole::Civilian:ENPCRole::Police,240);}
  const int32 Id=D->BeginEvent(Kind,People);Test->TestTrue(TEXT("Event instance created"),Id>0);
  if(Escape)People[0]->Activity=ENPCActivity::Flee;
  else if(Meeting||Trade)for(auto* N:People){N->Activity=Meeting?ENPCActivity::Meeting:ENPCActivity::Trade;N->EventDestination=N->GetActorLocation();N->bSocialArrived=true;}
  else for(auto* N:People){auto* Target=N->IsGang()?People.Last():People[0];N->CombatTarget=Target;N->bCombat=true;N->Activity=ENPCActivity::Combat;N->SetActorRotation((Target->GetActorLocation()-N->GetActorLocation()).Rotation());}
  // Exercise the actual director's performing gate, not a fabricated bPerforming flag.
  D->Tick(0);Test->TestTrue(UWorldEvents::EventName(Kind)+TEXT(" performing gate"),D->Find(Id)&&D->Find(Id)->bPerforming);
  auto* Owner=W->SpawnActor<AActor>(P);auto* L=NewObject<URecordingLibrary>(Owner);L->RegisterComponent();
  auto Film=[&](int32 Frames){Camera->RecordingSeconds=0;L->BeginClip(Camera);for(int32 I=0;I<=Frames;++I){Camera->RecordingSeconds=I*.25f;L->Sample(Camera);}L->FinishClip(Camera,true);};
  Film(11);Test->TestEqual(TEXT("2.75s does not qualify event"),L->Clips.Last().Events.Num(),0);
  Film(12);const int32 Tier=Escape||Meeting?1:War?3:2;
  if(Test->TestEqual(UWorldEvents::EventName(Kind)+TEXT(" captured at3s"),L->Clips.Last().Events.Num(),1)){
   const auto& Event=L->Clips.Last().Events[0];Test->TestEqual(TEXT("Recorded event level"),Event.Tier,Tier);Test->TestEqual(TEXT("3s threshold plus first-kind reward"),Event.Points,5*Tier+5);
   Test->TestEqual(TEXT("Canonical display name"),Event.Name,UWorldEvents::EventName(Massacre?EStreetEvent::MassiveGangWar:Kind));
  }
  People.Last()->SetActorHiddenInGame(true);Film(16);Test->TestEqual(TEXT("Missing required participant prevents event recognition"),L->Clips.Last().Events.Num(),0);People.Last()->SetActorHiddenInGame(false);
  Camera->Capture->SetWorldRotation(FRotator(0,180,0));Film(16);Test->TestEqual(TEXT("Event outside camera does not count"),L->Clips.Last().Events.Num(),0);Camera->Capture->SetWorldRotation(FRotator::ZeroRotator);
  D->EndEvent(Id);Film(16);Test->TestEqual(TEXT("Ended event cannot continue scoring"),L->Clips.Last().Events.Num(),0);
  Owner->Destroy();for(auto* N:People)N->Destroy();
 }
 Test->TestEqual(TEXT("Civilian variant shares canonical event name"),UWorldEvents::EventName(EStreetEvent::GangMassacre),UWorldEvents::EventName(EStreetEvent::MassiveGangWar));
 Camera->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFilmingEventMatrix,"Game.Recording.FiveEventMatrix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFilmingEventMatrix::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FFilmingEventMatrixCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
