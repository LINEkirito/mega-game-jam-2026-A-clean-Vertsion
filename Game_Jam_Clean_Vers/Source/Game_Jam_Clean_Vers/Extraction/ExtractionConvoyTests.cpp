#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ExtractionPoint.h"
#include "ExtractionConvoyCar.h"
#include "Combat/PatrolEnemy.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Data/DataTransmitter.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/AutomationTest.h"

class FConvoyCheck : public IAutomationLatentCommand
{
public:
 explicit FConvoyCheck(FAutomationTestBase* T,bool V=false,bool Two=false):Visual(V),RequireTwo(Two),Test(T){}
 virtual bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  if(!W){Test->AddError(TEXT("Missing PIE world"));return true;}
  auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  if(!P){Test->AddError(TEXT("Missing player"));return true;}
  auto* S=W->GetSubsystem<UExtractionSubsystem>();
  if(Stage==0)
  {
   PC->SetPause(false);PC->bShowMouseCursor=false;P->Health=100;
   P->GetCharacterMovement()->DisableMovement();
   for(TActorIterator<APatrolEnemy> It(W);It;++It){It->SetActorTickEnabled(false);It->SetActorEnableCollision(false);}
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);bool Found=false;
   for(TActorIterator<AExtractionPoint> It(W);It&&!Found;++It)
   {
    It->SetActorTickEnabled(false);
    for(int32 Side=0;Side<8&&!Found;++Side)
    {
     FNavLocation G;if(!Nav||!Nav->ProjectPointToNavigation(It->GetActorLocation()+FRotator(0,Side*45,0).Vector()*900.f,G,FVector(250,250,500)))continue;
     P->SetActorLocation(G.Location+FVector(0,0,93),false,nullptr,ETeleportType::TeleportPhysics);
     for(int32 Yaw=0;Yaw<360&&!Found;Yaw+=45)
     {
      PC->SetControlRotation(FRotator(0,Yaw,0));P->GetFirstPersonCameraComponent()->SetWorldRotation(FRotator(0,Yaw,0));PC->PlayerCameraManager->UpdateCamera(.016f);
      Found=AExtractionConvoyCar::FindArrival(W,PC,Entry,Stop,Heading);
      if(Found&&RequireTwo)
      {
       auto* Preview=W->SpawnActor<AExtractionConvoyCar>(Entry+FVector(0,0,76),Heading);
       if(Preview)
       {
        Preview->Initialize(PC,Stop,Entry);
        FVector SecondEntry,SecondStop;FRotator SecondHeading;
        Found=AExtractionConvoyCar::FindArrival(W,PC,SecondEntry,SecondStop,SecondHeading);
        Preview->Destroy();
       }
       else Found=false;
      }
     }
     if(Found)Point=*It;
    }
   }
   if(!Test->TestTrue(TEXT("Real map has hidden road approach and front parking"),Found))return true;
   Test->TestTrue(TEXT("Car initial footprint is not visible"),AExtractionConvoyCar::IsHiddenFromPlayer(W,PC,Entry,Heading));
   Test->TestTrue(TEXT("Parking is in player's front cone"),AExtractionConvoyCar::IsInFront(PC,Stop));
   Test->TestTrue(TEXT("Approach has full car clearance"),AExtractionConvoyCar::ClearRoad(W,Entry,Stop));
   // Ejected shells/magazines must not turn a validated road into an impassable route.
   auto* Debris=W->SpawnActor<AWeaponDebris>(FMath::Lerp(Entry,Stop,.5f)+FVector(0,0,76),Heading);
   Test->TestNotNull(TEXT("Road debris fixture exists"),Debris);
   Test->TestTrue(TEXT("Small weapon debris does not block a convoy road"),AExtractionConvoyCar::ClearRoad(W,Entry,Stop));
   if(Debris)Debris->Destroy();
   Point->ExtractingPlayer=P;Point->State=EExtractionState::Extracting;Point->RemainingSeconds=600;
   S->Claim(Point.Get());
   if(RequireTwo)
   {
    // Select a seed whose next production wave draw is two; all placement and delivery code stays real.
    for(int32 Seed=0;Seed<100;++Seed){FMath::RandInit(Seed);if(FMath::RandRange(1,2)==2){FMath::RandInit(Seed);break;}}
   }
   S->UpdateResponse(0);
   if(!Test->TestTrue(TEXT("Wave contains one or two cars"),S->Cars.Num()>=1&&S->Cars.Num()<=2))return true;
   if(RequireTwo)Test->TestEqual(TEXT("Two-car encounter actually starts both cars"),S->Cars.Num(),2);
   Expected=S->Cars.Num()*4;
   TArray<UBoxComponent*> VehicleBodies;
   for(TActorIterator<AExtractionPoint> It(W);It;++It)VehicleBodies.Add(It->VehicleBody);
   for(auto C:S->Cars)if(C.IsValid())VehicleBodies.Add(C->CollisionBody);
   for(auto* Body:VehicleBodies)
   {
    Test->TestTrue(TEXT("Vehicle body blocks bullet channel"),Body->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Block);
    const FVector Half=Body->GetUnscaledBoxExtent();
    for(float Height:{-.6f,0.f,.6f})for(int32 Side=0;Side<4;++Side)
    {
     FVector Local(0,0,Half.Z*Height);
     if(Side<2)Local.X=(Side==0?1:-1)*(Half.X+3);else Local.Y=(Side==2?1:-1)*(Half.Y+3);
     const FVector RayStart=Body->GetComponentTransform().TransformPosition(Local);
     const FVector RayEnd=Body->GetComponentTransform().TransformPosition(FVector(0,0,Half.Z*Height));
     FHitResult BulletHit;FCollisionQueryParams BulletQuery(SCENE_QUERY_STAT(ConvoyBulletCheck),true);
     Test->TestTrue(TEXT("Real world bullet trace stops at vehicle"),W->LineTraceSingleByChannel(BulletHit,RayStart,RayEnd,ECC_Visibility,BulletQuery)&&BulletHit.GetActor()==Body->GetOwner());
    }
   }
   for(auto C:S->Cars)if(C.IsValid())C->SetActorTickEnabled(false);
   ++Stage;return false;
  }
  if(Visual)
  {
   const double Now=FPlatformTime::Seconds();if(Now-LastStep<.05)return false;LastStep=Now;
  }
  // Advance the real state machine in fixed steps; AI is frozen for repeatable wave accounting.
  for(int32 Step=0;Step<(Visual?1:12);++Step)
  {
   TArray<AExtractionConvoyCar*> Cars;for(TActorIterator<AExtractionConvoyCar> It(W);It;++It)Cars.Add(*It);
   for(auto* C:Cars)if(IsValid(C))C->Tick(.05f);
   for(auto N:S->Responders)if(N.IsValid())N->SetActorTickEnabled(false);
  }
  Elapsed+=Visual?.05f:.6f;
  if(Stage==1)
  {
   if(S->GetLivingResponders()<Expected&&Elapsed<45)return false;
   if(!Test->TestEqual(TEXT("Every arriving car releases four members"),S->GetLivingResponders(),Expected)){Point->Cancel();return true;}
   int32 Rifles=0,Shotguns=0;
   for(auto N:S->Responders)if(N.IsValid())
   {
    Test->TestTrue(TEXT("All passengers are gang members"),N->NPCRole==ENPCRole::GangMember);
    Test->TestTrue(TEXT("No observation prerequisite after exit"),N->CombatTarget==P&&N->WeaponCarry==ENPCWeaponCarry::Drawn);
    Rifles+=N->WeaponKind==ETacticalWeaponKind::Rifle;Shotguns+=N->WeaponKind==ETacticalWeaponKind::Shotgun;
   }
   Test->TestEqual(TEXT("Two rifles per car"),Rifles,Expected/2);Test->TestEqual(TEXT("Two shotguns per car"),Shotguns,Expected/2);
   bool Fired=false;P->Health=10000;
   const FRotator SavedView=PC->GetControlRotation();
   PC->SetControlRotation(SavedView+FRotator(0,180,0));P->GetFirstPersonCameraComponent()->SetWorldRotation(PC->GetControlRotation());PC->PlayerCameraManager->UpdateCamera(.016f);
   for(auto N:S->Responders)if(N.IsValid()&&N->HeldWeapon&&N->CanSeeActor(P,false))
   {
    const int32 Before=N->HeldWeapon->Magazine;
    for(int32 Frame=0;Frame<100&&N->HeldWeapon->Magazine==Before;++Frame)
    {N->Tick(.05f);N->GetMesh()->TickAnimation(.05f,false);N->GetMesh()->RefreshBoneTransforms();}
    Fired|=N->HeldWeapon->Magazine<Before;
   }
   Test->TestTrue(TEXT("Disembarked shooters can fire while player looks away"),Fired);
   P->Health=100;PC->SetControlRotation(SavedView);P->GetFirstPersonCameraComponent()->SetWorldRotation(SavedView);PC->PlayerCameraManager->UpdateCamera(.016f);
   auto* Victim=S->Responders[0].Get();FHitResult Hit(Victim,Victim->GetCapsuleComponent(),Victim->GetActorLocation(),FVector::UpVector);
   W->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,10000,FVector::ForwardVector,PC,P);
   const int32 Wave=S->WaveNumber;S->UpdateResponse(5);
   Test->TestEqual(TEXT("Partial kill does not start next wave"),S->WaveNumber,Wave);
   Test->TestEqual(TEXT("Partial kill is not individually replenished"),S->GetLivingResponders(),Expected-1);
   ++Stage;Elapsed=0;return false;
  }
  if(Stage==2)
  {
   // Frozen test shooters may stand in the exit lane. Clear the squad before requiring retirement;
   // the car must wait for solid actors rather than drive through them or disappear in view.
   if(!SquadCleared)
   {
    for(auto N:S->Responders)if(N.IsValid()&&N->Health>0)
    {
     FHitResult Hit(N.Get(),N->GetCapsuleComponent(),N->GetActorLocation(),FVector::UpVector);
     W->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,10000,FVector::ForwardVector,PC,P);
    }
    SquadCleared=true;
   }
   bool Any=false;for(TActorIterator<AExtractionConvoyCar> It(W);It;++It)Any=true;
   if(Any&&Elapsed<45)return false;
   Test->TestFalse(TEXT("Cars drive out of sight and retire without accumulating"),Any);
   // Re-enter after eight historical waves; there must be no obsolete eight-car ceiling.
   S->WaveNumber=8;S->SpawnRetry=0;S->UpdateResponse(1);
   Test->TestEqual(TEXT("Cleared wave permits wave nine"),S->WaveNumber,9);
   Test->TestTrue(TEXT("Next wave starts with cars, not teleporting enemies"),!S->Cars.IsEmpty()&&S->GetLivingResponders()==0);
   for(auto C:S->Cars)if(C.IsValid())C->SetActorTickEnabled(false);
   PC->SetControlRotation(PC->GetControlRotation()+FRotator(0,180,0));P->GetFirstPersonCameraComponent()->SetWorldRotation(PC->GetControlRotation());PC->PlayerCameraManager->UpdateCamera(.016f);
   ++Stage;Elapsed=0;return false;
  }
  if(Stage==3)
  {
   bool Delivering=false;for(auto C:S->Cars)if(C.IsValid()&&C->IsDelivering())Delivering=true;
   if(Delivering&&Elapsed<35)return false;
   Test->TestEqual(TEXT("Turning away prevents rear parking and unloading"),S->GetLivingResponders(),0);
   Point->Cancel();Test->TestEqual(TEXT("Cancellation clears active squad"),S->GetLivingResponders(),0);
   for(auto C:S->Cars)if(C.IsValid()&&C->Phase==EConvoyCarPhase::Departing)
   {
    C->Tick(.05f);const float DepartingTime=C->PhaseTime;C->LeaveWithoutUnloading();
    Test->TestEqual(TEXT("Repeated cancellation preserves departure progress"),C->PhaseTime,DepartingTime);
   }
   return true;
  }
  return true;
 }
private:
 bool Visual=false,RequireTwo=false,SquadCleared=false;double LastStep=0;
 FAutomationTestBase* Test;int32 Stage=0,Expected=0;float Elapsed=0;
 FVector Entry,Stop;FRotator Heading;TWeakObjectPtr<AExtractionPoint> Point;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvoyTest,"Game.Extraction.Convoy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FConvoyTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FConvoyCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvoyVisualTest,"Game.Extraction.ConvoyVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FConvoyVisualTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FConvoyCheck(this,true));AddCommand(new FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvoyTwoCarsTest,"Game.Extraction.ConvoyTwoCars",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FConvoyTwoCarsTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FConvoyCheck(this,false,true));AddCommand(new FEndPlayMapCommand());return true;}
#endif
