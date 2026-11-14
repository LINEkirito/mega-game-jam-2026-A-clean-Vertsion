#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "RecordingLibrary.h"
#include "RecordingSceneRegion.h"
#include "ShoulderCamcorder.h"
#include "Combat/PatrolEnemy.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/DataTransmitter.h"
#include "Engine/StaticMesh.h"
#include "Components/SceneCaptureComponent2D.h"

namespace {
FFilmObservation Subject(EFilmSubject Kind,const TCHAR* Key=TEXT("A")) {FFilmObservation O;O.Kind=Kind;O.Key=Key;O.Type=TEXT("Trade");O.Behavior=TEXT("Idle");return O;}
void Film(FRecordingScoreRules& R,const TArray<FFilmObservation>& O,double Seconds){R.Begin();for(int32 I=0;I<=FMath::RoundToInt(Seconds*4);++I)R.Sample(I*.25,O);}
int32 SubjectScore(const TArray<FFilmAward>& A,const FString& Key){int32 S=0;for(const auto& X:A)if(X.Subject.Key==Key)S+=X.Points;return S;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingRulesTables,"Game.Recording.DesignTables",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingRulesTables::RunTest(const FString&){
 const double LongTimes[]={10,20,30,60};const int32 LongTotals[]={10,20,30,70};
 for(int I=0;I<4;++I){TestEqual(TEXT("Long-shot exact threshold"),FRecordingScoreRules::LongShot(LongTimes[I]),LongTotals[I]);TestEqual(TEXT("Long-shot below threshold"),FRecordingScoreRules::LongShot(LongTimes[I]-.001),I?LongTotals[I-1]:0);}
 const double EventTimes[]={3,10,20};const int32 EventTotals[]={5,15,30};
 for(int I=0;I<3;++I){TestEqual(TEXT("Event exact threshold"),FRecordingScoreRules::EventShot(EventTimes[I]),EventTotals[I]);TestEqual(TEXT("Event below threshold"),FRecordingScoreRules::EventShot(EventTimes[I]-.001),I?EventTotals[I-1]:0);}
 const double GangTimes[]={10,30,60,120};const int32 GangTotals[]={10,30,70,150};
 const double TypeTimes[]={10,20,40,60};const int32 TypeTotals[]={5,15,30,45};
 for(int I=0;I<4;++I){TestEqual(TEXT("NPC duration table"),FRecordingScoreRules::GangTime(GangTimes[I]),GangTotals[I]);TestEqual(TEXT("NPC below duration"),FRecordingScoreRules::GangTime(GangTimes[I]-.001),I?GangTotals[I-1]:0);TestEqual(TEXT("Event type duration table"),FRecordingScoreRules::EventTime(TypeTimes[I]),TypeTotals[I]);TestEqual(TEXT("Event type below duration"),FRecordingScoreRules::EventTime(TypeTimes[I]-.001),I?TypeTotals[I-1]:0);}
 const double PenaltyTimes[]={1,2,5,10};const int32 Civilian[]={-2,-4,-7,-12},Police[]={-4,-8,-13,-21},Logo[]={-5,-10,-15,-25};
 for(int I=0;I<4;++I)for(auto K:{EFilmSubject::Civilian,EFilmSubject::Police,EFilmSubject::Logo}){
  FRecordingScoreRules R;Film(R,{Subject(K)},PenaltyTimes[I]);TestEqual(TEXT("Per-subject penalty table"),SubjectScore(R.Commit(PenaltyTimes[I]),TEXT("A")),K==EFilmSubject::Civilian?Civilian[I]:K==EFilmSubject::Police?Police[I]:Logo[I]);
 }
 for(int Level=1;Level<=3;++Level){FRecordingScoreRules R;auto E=Subject(EFilmSubject::Event);E.Multiplier=Level;Film(R,{E},20);TestEqual(TEXT("20s event: shot30 + accumulated15, times level, plus type5"),SubjectScore(R.Commit(20),TEXT("A")),45*Level+5);}
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingRulesHistory,"Game.Recording.ContinuityAndHistory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingRulesHistory::RunTest(const FString&){
 FRecordingScoreRules R;auto Gang=Subject(EFilmSubject::Gang);Film(R,{Gang},2.75);TestEqual(TEXT("Under3 gang has no rewards"),SubjectScore(R.Commit(2.75),TEXT("A")),0);
 Film(R,{Gang},3);TestEqual(TEXT("First gang and first behavior"),SubjectScore(R.Commit(3),TEXT("A")),15);
 Film(R,{Gang},3);TestEqual(TEXT("Same NPC behavior not repeated"),SubjectScore(R.Commit(3),TEXT("A")),0);
 Film(R,{Gang},4);TestEqual(TEXT("Qualified durations accumulate to10"),SubjectScore(R.Commit(4),TEXT("A")),10);
 TestEqual(TEXT("Duplicate commit is empty"),R.Commit(4).Num(),0);
 for(int N=2;N<=11;++N){Gang.Behavior=FString::FromInt(N);Film(R,{Gang},3);const auto Awards=R.Commit(3);const int Expected=N<=5?5:N<=10?10:15;const int DurationBonus=N==8?20:0;TestEqual(TEXT("Unique behavior tier, including5/6/10/11 boundaries"),SubjectScore(Awards,TEXT("A")),Expected+DurationBonus);}
 FRecordingScoreRules Split;auto E=Subject(EFilmSubject::Event);Split.Begin();
 for(int I=0;I<=8;++I)Split.Sample(I*.25,{E});Split.Sample(2.25,{});for(int I=10;I<=18;++I)Split.Sample(I*.25,{E});
 TestEqual(TEXT("Two2s glimpses do not qualify"),SubjectScore(Split.Commit(4.5),TEXT("A")),0);
 Film(Split,{E},3);TestEqual(TEXT("Valid first event"),SubjectScore(Split.Commit(3),TEXT("A")),10);
 Film(Split,{E},3);TestEqual(TEXT("Same event may earn single-shot bonus in new shot"),SubjectScore(Split.Commit(3),TEXT("A")),5);
 Film(Split,{E},4);TestEqual(TEXT("Type total reaches10 across clips, no duplicate type reward"),SubjectScore(Split.Commit(4),TEXT("A")),10);
 E.Key=TEXT("B");Film(Split,{E},3);TestEqual(TEXT("New instance of same type does not earn type reward"),SubjectScore(Split.Commit(3),TEXT("B")),5);
 E.Type=TEXT("Chase");E.Key=TEXT("C");Film(Split,{E},3);TestEqual(TEXT("New type earns fixed5"),SubjectScore(Split.Commit(3),TEXT("C")),10);
 FRecordingScoreRules Failed;Film(Failed,{Gang},10);Failed.Begin();Film(Failed,{Gang},3);TestEqual(TEXT("Discarded shot commits no first/time/behavior state"),SubjectScore(Failed.Commit(3),TEXT("A")),15);
 FRecordingScoreRules Gap;Gap.Begin();Gap.Sample(0,{E});Gap.Sample(20,{E});TestEqual(TEXT("Missing samples cannot fabricate exposure"),SubjectScore(Gap.Commit(20),TEXT("C")),0);
 FRecordingScoreRules Pen;auto P=Subject(EFilmSubject::Police);Pen.Begin();for(int I=0;I<=4;++I)Pen.Sample(I*.25,{P,P});Pen.Sample(1.25,{});for(int I=6;I<=10;++I)Pen.Sample(I*.25,{P});TestEqual(TEXT("Penalty accumulates disconnected exposure; duplicate observations count once"),SubjectScore(Pen.Commit(2.5),TEXT("A")),-8);
 Film(Pen,{P},1);TestEqual(TEXT("Penalty thresholds reset each shot"),SubjectScore(Pen.Commit(1),TEXT("A")),-4);
 FRecordingScoreRules Scenes;auto S=Subject(EFilmSubject::Scene);Film(Scenes,{S},5);TestEqual(TEXT("Area not yet10s"),SubjectScore(Scenes.Commit(5),TEXT("A")),0);Film(Scenes,{S},5);TestEqual(TEXT("Same area across shots reaches10"),SubjectScore(Scenes.Commit(5),TEXT("A")),20);Film(Scenes,{S},10);TestEqual(TEXT("Area reward once only"),SubjectScore(Scenes.Commit(10),TEXT("A")),0);S.Key=TEXT("AreaB");Film(Scenes,{S},10);TestEqual(TEXT("Different area earns20"),SubjectScore(Scenes.Commit(10),TEXT("AreaB")),20);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingMixedScore,"Game.Recording.AllCategoriesAndTimeline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingMixedScore::RunTest(const FString&){
 FRecordingScoreRules R;auto Gang=Subject(EFilmSubject::Gang,TEXT("Gang"));auto Event=Subject(EFilmSubject::Event,TEXT("Event"));Event.Multiplier=2;
 const TArray<FFilmObservation> O{Gang,Event,Subject(EFilmSubject::Civilian,TEXT("Civilian")),Subject(EFilmSubject::Police,TEXT("Police")),Subject(EFilmSubject::Logo,TEXT("Logo")),Subject(EFilmSubject::Scene,TEXT("Scene"))};
 Film(R,O,10);int32 TimelineTotal=0;TSet<FString> Categories;for(const auto& M:R.Moments){TimelineTotal+=M.Points;Categories.Add(M.Category);TestTrue(TEXT("No reward timestamp outside clip"),M.Time>=0&&M.Time<=10);TestTrue(TEXT("Each award explains arithmetic"),M.Explanation.Contains(TEXT("×")));}
 TestEqual(TEXT("10s combined: long10 + gang25 + event45 + area20 - civilian12 - police21 - logo25"),TimelineTotal,42);
 TestEqual(TEXT("All11 scoring categories appear"),Categories.Num(),11);
 int32 Total=0;for(const auto& A:R.Commit(10))Total+=A.Points;TestEqual(TEXT("Timeline equals committed clip total"),Total,TimelineTotal);
 Film(R,O,10);Total=0;for(const auto& A:R.Commit(10))Total+=A.Points;TestEqual(TEXT("Second shot: long10 + event50 - penalties58; no first/area duplicates"),Total,2);
 FRecordingScoreRules Behaviors;Behaviors.Begin();double Time=0;
 for(int32 N=1;N<=11;++N){Gang.Behavior=FString::FromInt(N);for(int32 I=0;I<=12;++I){Behaviors.Sample(Time,{Gang});Time+=.25;}}
 int32 BehaviorTotal=0;for(const auto& M:Behaviors.Moments)if(M.Category==TEXT("NPC不同行为"))BehaviorTotal+=M.Points;
 TestEqual(TEXT("11 behaviors in one shot:5x5+5x10+1x15"),BehaviorTotal,90);
 return true;
}
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRecordingVisibilityCheck,FAutomationTestBase*,Test);
bool FRecordingVisibilityCheck::Update(){
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}if(!W){Test->AddError(TEXT("No PIE world"));return true;}
 const FVector Origin(30000,30000,8000);FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;P.ObjectFlags|=RF_Transient;
 auto* Camera=W->SpawnActor<AShoulderCamcorder>(Origin,FRotator::ZeroRotator,P);Camera->SetActorTickEnabled(false);Camera->Body->SetSimulatePhysics(false);Camera->Capture->SetWorldLocationAndRotation(Origin,FRotator::ZeroRotator);Camera->Capture->FOVAngle=90;
 auto* Owner=W->SpawnActor<AActor>(P);auto* L=NewObject<URecordingLibrary>(Owner);L->RegisterComponent();
 auto* NPC=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),FTransform(FRotator::ZeroRotator,Origin+FVector(600,0,0)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);NPC->NPCRole=ENPCRole::GangMember;NPC->FinishSpawning(NPC->GetActorTransform());NPC->SetActorTickEnabled(false);NPC->GetCharacterMovement()->DisableMovement();
 auto FilmActor=[&](double T,bool Save=true){Camera->RecordingSeconds=0;L->BeginClip(Camera);for(int I=0;I<=FMath::RoundToInt(T*4);++I){Camera->RecordingSeconds=I*.25f;L->Sample(Camera);}L->FinishClip(Camera,Save);};
 FilmActor(3,false);Test->TestEqual(TEXT("Failed save absent"),L->Clips.Num(),0);FilmActor(3);Test->TestEqual(TEXT("Visible gang first3s"),L->Clips.Last().Score,15);FilmActor(3);Test->TestEqual(TEXT("Same behavior no repeated first"),L->Clips.Last().Score,0);
 NPC->SetActorHiddenInGame(true);FilmActor(3);Test->TestEqual(TEXT("Hidden NPC absent"),L->Clips.Last().People.Num(),0);NPC->SetActorHiddenInGame(false);
 auto* Wall=W->SpawnActor<AActor>(Origin+FVector(300,0,0),FRotator::ZeroRotator,P);auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Box->SetBoxExtent(FVector(20,300,300));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Box->SetWorldLocation(Origin+FVector(300,0,0));FilmActor(3);Test->TestEqual(TEXT("Occluded NPC absent"),L->Clips.Last().People.Num(),0);Wall->Destroy();
 NPC->IdentityRole=ENPCRole::Police;FilmActor(2);Test->TestEqual(TEXT("Police cumulative2s penalty"),L->Clips.Last().Score,-8);
 NPC->IdentityRole=ENPCRole::Civilian;FilmActor(2);Test->TestEqual(TEXT("Civilian cumulative2s penalty"),L->Clips.Last().Score,-4);
 NPC->SetActorHiddenInGame(true);auto* Area=W->SpawnActor<ARecordingSceneRegion>(Origin,FRotator::ZeroRotator,P);Area->SceneId=TEXT("TestArea");Area->Bounds->SetBoxExtent(FVector(100));FilmActor(10);Test->TestEqual(TEXT("Defined area20 plus longshot10"),L->Clips.Last().Score,30);FilmActor(10);Test->TestEqual(TEXT("Same area no repeated20"),L->Clips.Last().Score,10);
 const int Count=L->Clips.Num();L->FinishClip(Camera,true);Test->TestEqual(TEXT("Duplicate FinishClip cannot duplicate saved clip"),L->Clips.Num(),Count);
 Area->Destroy();
 auto* Logo=W->SpawnActor<AActor>(P);auto* Mesh=NewObject<UStaticMeshComponent>(Logo);Logo->SetRootComponent(Mesh);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->RegisterComponent();Mesh->SetWorldLocation(Origin+FVector(500,0,0));
 FilmActor(2);Test->TestEqual(TEXT("Untagged visible prop is not a logo"),L->Clips.Last().Score,0);
 // Transient PIE fixture only; never mark a level asset as a logo.
 Logo->Tags.Add(TEXT("RecordingLogo"));FilmActor(2);Test->TestEqual(TEXT("Explicit logo2s deducts10"),L->Clips.Last().Score,-10);
 Logo->SetActorHiddenInGame(true);FilmActor(2);Test->TestEqual(TEXT("Hidden logo cannot deduct"),L->Clips.Last().Score,0);
 Logo->Destroy();
 NPC->SetActorHiddenInGame(false);NPC->IdentityRole=ENPCRole::GangMember;
 Camera->RecordingSeconds=0;L->BeginClip(Camera);
 for(int I=0;I<=4;++I){Camera->RecordingSeconds=I*.25f;L->Sample(Camera);}
 FHitResult Fatal(NPC,NPC->GetCapsuleComponent(),NPC->GetActorLocation(),FVector::UpVector);
 W->GetSubsystem<UDataTransmitter>()->TransmitHit(Fatal,10000,FVector::ForwardVector,nullptr,Owner);
 Test->TestTrue(TEXT("Fixture actually died"),NPC->Health<=0);
 for(int I=5;I<=8;++I){Camera->RecordingSeconds=I*.25f;L->Sample(Camera);}L->FinishClip(Camera,true);
 int DeathAwards=0;for(const auto& M:L->Clips.Last().ScoreMoments)if(M.Category==TEXT("死亡瞬间"))++DeathAwards;
 Test->TestEqual(TEXT("Actual NPC death produces one saved death moment"),DeathAwards,1);
 FilmActor(2);DeathAwards=0;for(const auto& M:L->Clips.Last().ScoreMoments)if(M.Category==TEXT("死亡瞬间"))++DeathAwards;
 Test->TestEqual(TEXT("Filming existing corpse cannot earn death bonus"),DeathAwards,0);
 NPC->GetMesh()->SetSimulatePhysics(false);NPC->GetMesh()->SetWorldLocation(Origin+FVector(600,3000,0));NPC->GetMesh()->UpdateBounds();
 FilmActor(2);Test->TestEqual(TEXT("Offscreen corpse is not detected at its stale capsule"),L->Clips.Last().People.Num(),0);
 NPC->Destroy();Camera->Destroy();Owner->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingVisibility,"Game.Recording.VisibilityAndSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingVisibility::RunTest(const FString&){AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FRecordingVisibilityCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
