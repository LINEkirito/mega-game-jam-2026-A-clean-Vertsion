#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "RecordingLibrary.h"
#include "RecordedPersonComponent.h"
#include "ShoulderCamcorder.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRecordingUniquePeopleCheck,FAutomationTestBase*,Test);
bool FRecordingUniquePeopleCheck::Update() {
 UWorld* World=nullptr;
 for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}
 if(!World){Test->AddError(TEXT("No PIE world"));return true;}
 const FVector Origin(15000,15000,5000);
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 Params.ObjectFlags|=RF_Transient;
 auto* Camera=World->SpawnActor<AShoulderCamcorder>(Origin,FRotator::ZeroRotator,Params);
 Camera->SetActorTickEnabled(false);Camera->Body->SetSimulatePhysics(false);
 Camera->Capture->SetWorldLocationAndRotation(Origin,FRotator::ZeroRotator);Camera->Capture->FOVAngle=90;
 auto* Owner=World->SpawnActor<AActor>(Params);
 auto* Library=NewObject<URecordingLibrary>(Owner);Library->RegisterComponent();
 auto SpawnPerson=[&](float Y){
  auto* Person=World->SpawnActor<ACharacter>(Origin+FVector(600,Y,0),FRotator::ZeroRotator,Params);
  Person->GetCharacterMovement()->DisableMovement();return Person;
 };
 auto* First=SpawnPerson(-150);auto* Second=SpawnPerson(150);Second->SetActorHiddenInGame(true);
 auto Sample=[&](URecordingLibrary* L){Camera->RecordingSeconds=0;L->BeginClip(Camera);L->Sample(Camera);};
 Sample(Library);Camera->RecordingSeconds=1;Library->Sample(Camera);Library->FinishClip(Camera,true);
 Test->TestEqual(TEXT("First clip saved"),Library->Clips.Num(),1);
 Test->TestEqual(TEXT("Same person sampled twice counts once"),Library->Clips.Last().People.Num(),1);
 Test->TestTrue(TEXT("First sighting earns points"),Library->Clips.Last().Score>0);
 auto* Marker=First->FindComponentByClass<URecordedPersonComponent>();
 Test->TestTrue(TEXT("Successful save marks the person instance"),Marker && Marker->bRecorded);
 Test->TestNull(TEXT("Hidden person is not marked"),Second->FindComponentByClass<URecordedPersonComponent>());
 const int32 OriginalScore=Library->Clips[0].Score;
 Sample(Library);Library->FinishClip(Camera,true);
 Test->TestEqual(TEXT("Same person in another clip earns zero"),Library->Clips.Last().Score,0);
 Test->TestEqual(TEXT("Repeated person remains listed"),Library->Clips.Last().People.Num(),1);
 if(Library->Clips.Last().People.Num()==1)Test->TestTrue(TEXT("Repeated entry shows already-recorded marker"),Library->Clips.Last().People[0].bAlreadyRecorded);
 Test->TestEqual(TEXT("Original score remains unchanged"),Library->Clips[0].Score,OriginalScore);
 Second->SetActorHiddenInGame(false);
 Sample(Library);Library->FinishClip(Camera,false);
 Test->TestEqual(TEXT("Failed recording is not listed"),Library->Clips.Num(),2);
 Test->TestNull(TEXT("Failed save does not consume new person's opportunity"),Second->FindComponentByClass<URecordedPersonComponent>());
 Sample(Library);Library->FinishClip(Camera,true);
 Test->TestEqual(TEXT("Mixed clip lists both people"),Library->Clips.Last().People.Num(),2);
 int32 NewCount=0,RepeatCount=0,Score=0;
 for(const auto& Entry:Library->Clips.Last().People){if(Entry.bAlreadyRecorded){++RepeatCount;Test->TestEqual(TEXT("Repeat contributes no points"),Entry.Points,0);}else ++NewCount;Score+=Entry.Points;}
 Test->TestEqual(TEXT("Exactly one new person"),NewCount,1);Test->TestEqual(TEXT("Exactly one repeated person"),RepeatCount,1);
 Test->TestTrue(TEXT("New person earns points after failed attempt"),Score>0);Test->TestEqual(TEXT("Mixed clip score"),Library->Clips.Last().Score,Score);
 auto* OtherOwner=World->SpawnActor<AActor>(Params);
 auto* OtherLibrary=NewObject<URecordingLibrary>(OtherOwner);OtherLibrary->RegisterComponent();
 Sample(OtherLibrary);OtherLibrary->FinishClip(Camera,true);
 Test->TestEqual(TEXT("Replacing equipment/library cannot reset subject markers"),OtherLibrary->Clips.Last().Score,0);
 auto* Third=SpawnPerson(0);
 Sample(Library);Sample(OtherLibrary);Library->FinishClip(Camera,true);OtherLibrary->FinishClip(Camera,true);
 Test->TestTrue(TEXT("First concurrent save claims new subject"),Library->Clips.Last().Score>0);
 Test->TestEqual(TEXT("Second concurrent save cannot double-score"),OtherLibrary->Clips.Last().Score,0);
 // A distinct actor of the same class must not inherit another person's state.
 const auto* ThirdMarker=Third->FindComponentByClass<URecordedPersonComponent>();
 Test->TestTrue(TEXT("Distinct person still receives their first score"),ThirdMarker && ThirdMarker->bRecorded);
 First->Destroy();Second->Destroy();Third->Destroy();Camera->Destroy();Owner->Destroy();OtherOwner->Destroy();
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordingUniquePeople,"Game.Camcorder.UniquePeople",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecordingUniquePeople::RunTest(const FString&) {
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FRecordingUniquePeopleCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
