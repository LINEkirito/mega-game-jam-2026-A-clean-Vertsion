#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ExtractionPoint.h"
#include "ExtractionConvoyCar.h"
#include "Combat/PatrolEnemy.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

class FExtractionSpacingCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<AExtractionPoint> ActivePoint;
 TWeakObjectPtr<APatrolEnemy> Responder;
 double StartTime=0;
 bool Captured=false;
 bool SearchChecked=false;
 FVector SearchStart=FVector::ZeroVector;
public:
 explicit FExtractionSpacingCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  if(!W){Test->AddError(TEXT("Missing PIE world"));return true;}
  auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  if(!P){Test->AddError(TEXT("Missing player"));return true;}
  if(Responder.IsValid())
  {
   const double Elapsed=FPlatformTime::Seconds()-StartTime;
   if(Elapsed<7)return false;
   if(!Captured)
   {
    const float Distance=FVector::Dist2D(Responder->GetActorLocation(),P->GetActorLocation());
    Test->AddInfo(FString::Printf(TEXT("Actual retreat settled distance: %.1f cm"),Distance));
    Test->TestTrue(TEXT("Live movement retreats and settles beyond eight metres"),Distance>=800&&Distance<1100);
    SearchStart=Responder->GetActorLocation();Responder->SightRange=1;Responder->MoveRetry=0;
    Captured=true;return false;
   }
   if(Elapsed<14)return false;
   if(!SearchChecked)
   {
    auto* S=W->GetSubsystem<UExtractionSubsystem>();
    const FVector Side=FVector::CrossProduct(FVector::UpVector,S->ResponseForward)*S->ResponseSide;
    const float Travel=FVector::DotProduct(Responder->GetActorLocation()-SearchStart,Side);
    const float Angle=FMath::FindDeltaAngleDegrees(S->ResponseForward.Rotation().Yaw,(Responder->GetActorLocation()-S->ResponseOrigin).Rotation().Yaw)*S->ResponseSide;
    Test->AddInfo(FString::Printf(TEXT("Live same-side search: %.1f cm lateral, %.1f degrees"),Travel,Angle));
    Test->TestTrue(TEXT("Live blocked responder moves laterally along shared side"),Travel>30.f);
    Test->TestTrue(TEXT("Live search stays within cover-preserving sector"),Angle>=-1.f&&Angle<=36.f);
    FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/ExtractionSpacingCollision/AngleSearch.png"),false,false);
    SearchChecked=true;return false;
   }
   if(Elapsed<15)return false;
   ActivePoint->Cancel();ActivePoint->Destroy();return true;
  }
  PC->SetPause(false);P->Health=10000;
  for(TActorIterator<APatrolEnemy> It(W);It;++It){It->SetActorTickEnabled(false);It->SetActorEnableCollision(false);}
  for(TActorIterator<AExtractionPoint> It(W);It;++It)It->SetActorTickEnabled(false);
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
  FVector Base,Direction;bool Found=false;
  for(TActorIterator<AExtractionPoint> It(W);It&&!Found;++It)
   for(int32 Angle=0;Angle<360&&!Found;Angle+=30)
   {
    const FVector D=FRotator(0,Angle,0).Vector();FNavLocation A,B;
    if(!Nav||!Nav->ProjectPointToNavigation(It->GetActorLocation()+D*700,A,FVector(200,200,300))||
       !Nav->ProjectPointToNavigation(A.Location+D*1400,B,FVector(50,50,100)))continue;
    auto* Path=Nav->FindPathToLocationSynchronously(W,A.Location,B.Location);
    if(Path&&Path->IsValid()&&!Path->IsPartial()&&Path->GetPathLength()<1500){Base=A.Location+FVector(0,0,95);Direction=D;Found=true;}
   }
  if(!Test->TestTrue(TEXT("Navigable standoff fixture exists"),Found))return true;
  P->GetCharacterMovement()->DisableMovement();P->SetActorLocation(Base,false,nullptr,ETeleportType::TeleportPhysics);
  auto* Point=W->SpawnActor<AExtractionPoint>();Point->SetActorTickEnabled(false);
  Point->State=EExtractionState::Extracting;Point->ExtractingPlayer=P;
  auto* Session=W->GetSubsystem<UExtractionSubsystem>();Session->Claim(Point);
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* N=W->SpawnActor<APatrolEnemy>(Base+Direction*900,FRotator::ZeroRotator,Params);
  N->SetActorTickEnabled(false);Session->RegisterPassenger(N);
  auto* AI=Cast<AAIController>(N->GetController());
  FVector SearchDestination;
  if(!Session->FindResponseFiringAngle(N,SearchDestination))Session->ResponseSide*=-1.f;
  Test->TestTrue(TEXT("Blocked responder has a lateral search destination"),Session->FindResponseFiringAngle(N,SearchDestination));
  const FVector SharedSide=FVector::CrossProduct(FVector::UpVector,Session->ResponseForward)*Session->ResponseSide;
  int32 Searchers=0;
  for(int32 I=0;I<4;++I)
  {
   auto* Member=W->SpawnActor<APatrolEnemy>(Base+Direction*(900+I*50),FRotator::ZeroRotator,Params);
   Member->SetActorTickEnabled(false);Member->SetActorEnableCollision(false);Session->RegisterPassenger(Member);
   FVector Goal;
   if(Session->FindResponseFiringAngle(Member,Goal))
   {
    ++Searchers;
    Test->TestTrue(TEXT("Every squad member searches the same side"),FVector::DotProduct(Goal-Member->GetActorLocation(),SharedSide)>0);
    Test->TestTrue(TEXT("Angle search keeps nine metre standoff"),FVector::Dist2D(Goal,Base)>=825);
   }
   Member->Destroy();
  }
  Test->TestTrue(TEXT("Multiple members can seek the shared flank"),Searchers>=2);
  const FVector SavedPosition=N->GetActorLocation();
  N->SetActorLocation(Base+Direction.RotateAngleAxis(35.f*Session->ResponseSide,FVector::UpVector)*900,false,nullptr,ETeleportType::TeleportPhysics);
  Test->TestFalse(TEXT("Search stops at sector edge instead of circling cover"),Session->FindResponseFiringAngle(N,SearchDestination));
  P->SetActorLocation(Base+SharedSide*150,false,nullptr,ETeleportType::TeleportPhysics);
  Test->TestFalse(TEXT("Moving player does not rotate the sector around the car"),Session->FindResponseFiringAngle(N,SearchDestination));
  P->SetActorLocation(Base,false,nullptr,ETeleportType::TeleportPhysics);N->SetActorLocation(SavedPosition,false,nullptr,ETeleportType::TeleportPhysics);
  auto* Ambient=W->SpawnActor<APatrolEnemy>(Base+Direction*900,FRotator::ZeroRotator,Params);Ambient->SetActorTickEnabled(false);
  Test->TestFalse(TEXT("Non extraction gang cannot enter the angle search"),Session->FindResponseFiringAngle(Ambient,SearchDestination));Ambient->Destroy();
  if(Test->TestNotNull(TEXT("Responder AI controller"),AI))
  {
   N->MoveTo(Base,40);N->MoveRetry=0;N->TickExtractionResponse(.016f);
   Test->TestTrue(TEXT("Stops an existing chase at nine metres"),AI->GetMoveStatus()==EPathFollowingStatus::Idle);
   N->SightRange=1;N->MoveTo(Base,40);N->MoveRetry=0;N->TickExtractionResponse(.016f);
   const auto SearchPath=AI->GetPathFollowingComponent()->GetPath();
   Test->TestTrue(TEXT("Losing sight never resumes close pursuit"),AI->GetMoveStatus()==EPathFollowingStatus::Idle||(SearchPath.IsValid()&&FVector::Dist2D(SearchPath->GetEndLocation(),Base)>=825));
   N->SetActorLocation(Base+Direction*400,false,nullptr,ETeleportType::TeleportPhysics);N->MoveRetry=0;N->TickExtractionResponse(.016f);
   Test->TestTrue(TEXT("Close player triggers retreat"),N->bExtractionRetreating);
   const auto Path=AI->GetPathFollowingComponent()->GetPath();
   Test->TestTrue(TEXT("Retreat path ends away from the player"),Path.IsValid()&&FVector::Dist2D(Path->GetEndLocation(),Base)>=825);
   N->SightRange=2200;
   N->SetActorLocation(Base+Direction*900,false,nullptr,ETeleportType::TeleportPhysics);N->MoveRetry=0;N->TickExtractionResponse(.016f);
   Test->TestTrue(TEXT("Retreat settles instead of oscillating"),!N->bExtractionRetreating&&AI->GetMoveStatus()==EPathFollowingStatus::Idle);
  }
  N->SetActorLocation(Base+Direction*400,false,nullptr,ETeleportType::TeleportPhysics);
  N->MoveRetry=0;N->ShotTime=10000;N->SetActorTickEnabled(true);
  PC->SetControlRotation(Direction.Rotation());P->GetFirstPersonCameraComponent()->SetWorldRotation(Direction.Rotation());PC->PlayerCameraManager->UpdateCamera(.016f);
  ActivePoint=Point;Responder=N;StartTime=FPlatformTime::Seconds();return false;
 }
};

class FVehicleSurfaceCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
public:
 explicit FVehicleSurfaceCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  if(!W){Test->AddError(TEXT("Missing PIE world"));return true;}
  const FVector Origin(35000,35000,3000);
  auto* Car=W->SpawnActor<AExtractionConvoyCar>(Origin+FVector(0,0,76),FRotator::ZeroRotator);Car->SetActorTickEnabled(false);
  TArray<AActor*> Fixtures;Fixtures.Add(Car);
  for(const TCHAR* Name:{TEXT("Sedan"),TEXT("SUV"),TEXT("BoxTruck"),TEXT("Campervan")})
  {
   auto* Class=LoadClass<AExtractionPoint>(nullptr,*(FString(TEXT("/Game/Extraction/BP_Extraction_"))+Name+TEXT(".BP_Extraction_")+Name+TEXT("_C")));
   auto* Point=Class?W->SpawnActor<AExtractionPoint>(Class,Origin+FVector(0,Fixtures.Num()*2000,0),FRotator::ZeroRotator):nullptr;
   if(Test->TestNotNull(Name,Point)){Point->SetActorTickEnabled(false);Fixtures.Add(Point);}
  }
  for(auto* Actor:Fixtures)
  {
   TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);int32 Hits=0,Gaps=0,Mismatches=0;
   const FVector O=Actor==Car?Origin:Actor->GetActorLocation();
   // Scan across the vehicle silhouette, including empty space inside the old bounding box.
   for(int32 X=-220;X<=220;X+=20)for(int32 Z=15;Z<=135;Z+=10)
   {
    const FVector A=O+FVector(X,-350,Z),B=O+FVector(X,350,Z);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(VehicleSurfaceRegression),true);bool Surface=false;
    for(auto* Mesh:Meshes)if(Mesh->GetStaticMesh())
    {FHitResult H;if(Mesh->LineTraceComponent(H,A,B,Q)){Surface=true;break;}}
    FHitResult Hit;const bool Blocked=W->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Q)&&Hit.GetActor()==Actor;
    Mismatches+=Surface!=Blocked;Hits+=Surface;Gaps+=!Surface;
   }
   Test->TestEqual(*(Actor->GetName()+TEXT(" bullets match mesh surface")),Mismatches,0);
   Test->TestTrue(*(Actor->GetName()+TEXT(" solid panels block bullets")),Hits>10);
   Test->TestTrue(*(Actor->GetName()+TEXT(" empty silhouette gaps pass bullets")),Gaps>10);
  }
  for(auto* Actor:Fixtures)Actor->Destroy();return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExtractionSpacingTest,"Game.Extraction.Spacing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExtractionSpacingTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FExtractionSpacingCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleSurfaceTest,"Game.Extraction.VehicleSurface",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVehicleSurfaceTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FVehicleSurfaceCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
#endif
