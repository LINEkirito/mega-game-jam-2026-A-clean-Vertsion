#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "PoliceReinforcements.h"
#include "Localization/GameLanguage.h"
#include "PatrolEnemy.h"
#include "Data/DataTransmitter.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Extraction/ExtractionConvoyCar.h"
#include "Extraction/ExtractionPoint.h"
#include "NavigationSystem.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
 bool PreparePoliceRoad(UWorld* W,APlayerController* PC)
 {
  auto* P=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn());
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
  if(!P||!Nav)return false;
  P->GetCharacterMovement()->DisableMovement();
  for(TActorIterator<AExtractionPoint> It(W);It;++It)for(int32 Side=0;Side<8;++Side)
  {
   FNavLocation G;if(!Nav->ProjectPointToNavigation(It->GetActorLocation()+FRotator(0,Side*45,0).Vector()*900.f,G,FVector(250,250,500)))continue;
   P->SetActorLocation(G.Location+FVector(0,0,93),false,nullptr,ETeleportType::TeleportPhysics);
   for(int32 Yaw=0;Yaw<360;Yaw+=45)
   {
    PC->SetControlRotation(FRotator(0,Yaw,0));P->GetFirstPersonCameraComponent()->SetWorldRotation(FRotator(0,Yaw,0));PC->PlayerCameraManager->UpdateCamera(.016f);
    FVector Entry,Stop;FRotator Heading;if(AExtractionConvoyCar::FindArrival(W,PC,Entry,Stop,Heading))return true;
   }
  }
  return false;
 }
}


DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FPoliceReinforcementsCheck,FAutomationTestBase*,Test);
bool FPoliceReinforcementsCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 auto* PC=W?W->GetFirstPlayerController():nullptr;
 if(!PC||!PC->GetPawn()){Test->AddError(TEXT("Missing PIE player"));return true;}
 auto* S=W->GetSubsystem<UPoliceReinforcements>();
 if(!S){Test->AddError(TEXT("Missing police subsystem (restart editor after first compile)"));return true;}
 auto* Player=PC->GetPawn().Get();auto* Bus=W->GetSubsystem<UDataTransmitter>();
 TArray<APatrolEnemy*> Created;
 auto Spawn=[&](ENPCRole Role){FTransform T(FRotator::ZeroRotator,Player->GetActorLocation()+FVector(500,0,0));auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);N->NPCRole=Role;N->FinishSpawning(T);Created.Add(N);return N;};
 auto* Officer=Spawn(ENPCRole::Police);auto* Gang=Spawn(ENPCRole::GangMember);
 auto Hit=[&](APatrolEnemy* Target,float Damage,AController* Instigator,AActor* Source){FHitResult H(Target,Target->GetMesh(),Target->GetActorLocation(),FVector::UpVector);H.ImpactPoint=Target->GetActorLocation();return Bus->TransmitHit(H,Damage,FVector::ForwardVector,Instigator,Source);};
 Hit(Officer,0,PC,Player);Test->TestEqual(TEXT("Zero damage gives no backup"),S->Pending,0);
 Hit(Officer,1,Gang->GetController(),Gang);Test->TestEqual(TEXT("Gang crossfire gives no backup"),S->Pending,0);
 Hit(Gang,1,PC,Player);Test->TestEqual(TEXT("Non-police victim gives no backup"),S->Pending,0);
 Hit(Officer,1,PC,Player);Test->TestEqual(TEXT("Police assault requests two-star response"),S->GetWantedLevel(),2);
 Test->TestEqual(TEXT("Surviving officer retaliates immediately"),Officer->CombatTarget.Get(),static_cast<AActor*>(Player));
 Hit(Officer,1,PC,Player);Test->TestEqual(TEXT("Repeated hit does not reserve duplicate seats"),S->Pending,0);
 for(TActorIterator<APatrolEnemy> It(W);It;++It){It->SetActorTickEnabled(false);It->SetActorEnableCollision(false);}
 if(!Test->TestTrue(TEXT("Map has a real parking road"),PreparePoliceRoad(W,PC)))return true;
 S->Tick(1.f);
 Test->TestTrue(TEXT("Backup arrives by car"),S->Car.IsValid());
 Test->TestEqual(TEXT("No officers pop into existence before unloading"),S->Squad.Num(),0);
 if(S->Car.IsValid())
 {
  auto* Car=S->Car.Get();Car->SetActorTickEnabled(false);
  Test->TestTrue(TEXT("Car starts outside player's sight"),AExtractionConvoyCar::IsHiddenFromPlayer(W,PC,Car->GetActorLocation()-FVector(0,0,76),Car->GetActorRotation(),Car));
  Test->TestFalse(TEXT("Fourth passenger seat empty"),Car->Riders[3]->IsVisible());
  for(auto Rider:Car->Riders)Test->TestEqual(TEXT("Seated occupants use police appearance"),Rider->GetSkeletalMeshAsset()->GetName(),FString(TEXT("NPC_Police")));
  bool HasPolicePaint=false;TInlineComponentArray<UStaticMeshComponent*> Panels(Car);
  for(auto* Panel:Panels)for(int32 Slot=0;Slot<Panel->GetNumMaterials();++Slot)
   if(auto* Mat=Panel->GetMaterial(Slot);Mat&&Mat->GetName()==TEXT("M_PoliceCar"))HasPolicePaint=true;
  Test->TestTrue(TEXT("Police livery applied at runtime"),HasPolicePaint);
  // Police must unload even when the player turns away from the approaching car.
  PC->SetControlRotation(PC->GetControlRotation()+FRotator(0,180,0));
  Player->SetActorRotation(PC->GetControlRotation());
  int32 Steps=0;
  for(int32 I=0;I<2400&&S->Pending>0;++I)
  {
   Car->Tick(.05f);++Steps;
   for(auto N:S->Squad)if(N.IsValid())N->SetActorTickEnabled(false);
  }
  Test->TestEqual(TEXT("Exactly three officers leave police car"),Car->Delivered,3);
  Test->TestTrue(TEXT("Fast police delivery finishes within 15 seconds"),Steps*.05f<15.f);
  Test->TestTrue(TEXT("Police parking is nearby"),FVector::Dist2D(Car->GetParkingLocation(),Player->GetActorLocation())<12500.f);
 }
 Test->TestEqual(TEXT("Three police passengers registered"),S->Squad.Num(),3);
 Test->TestEqual(TEXT("All requests consumed by exit animation"),S->Pending,0);
 for(const auto& Weak:S->Squad)if(auto* N=Weak.Get()){
  Test->TestEqual(TEXT("Police weapon already drawn on exit"),N->WeaponCarry,ENPCWeaponCarry::Drawn);
  Test->TestEqual(TEXT("Backup has police identity"),N->IdentityRole,ENPCRole::Police);
  if(S->IsSearching())
  {
   Test->TestNull(TEXT("Unseen player is not targeted through walls"),N->CombatTarget.Get());
   Test->TestEqual(TEXT("Unseen player triggers last-known search"),N->Activity,ENPCActivity::Investigate);
  }
  else
  {
   Test->TestEqual(TEXT("Visible player is targeted"),N->CombatTarget.Get(),static_cast<AActor*>(Player));
   Test->TestEqual(TEXT("Visible player triggers combat"),N->Activity,ENPCActivity::Combat);
  }
  Test->TestNotNull(TEXT("Backup has a weapon"),N->HeldWeapon.Get());
 }
 Hit(Officer,10000,PC,Player);Test->TestEqual(TEXT("Lethal followup does not duplicate active squad"),S->Pending,0);
 for(const auto& Weak:S->Squad)if(auto* N=Weak.Get())N->Destroy();S->Squad.Empty();S->ClearWanted();for(auto C:S->Cars)if(C.IsValid())C->Destroy();S->Cars.Empty();S->Car.Reset();
 auto* Lethal=Spawn(ENPCRole::Police);Hit(Lethal,10000,PC,Player);Test->TestEqual(TEXT("First-hit police kill requests three stars"),S->GetWantedLevel(),3);
 S->Pending=0;S->Offender.Reset();for(auto* N:Created)N->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoliceReinforcementsTest,"Game.NPC.PoliceReinforcements",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPoliceReinforcementsTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FPoliceReinforcementsCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}

// Tick the actual AI and firearm pipeline over time, not just its initial state.
class FPolicePursuitCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 double Started=0,PhotoAt=0;
 bool Fired=false;
 TWeakObjectPtr<APatrolEnemy> Victim;
public:
 explicit FPolicePursuitCheck(FAutomationTestBase* In):Test(In){}
 virtual bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  auto* S=W?W->GetSubsystem<UPoliceReinforcements>():nullptr;
  if(!P||!S){Test->AddError(TEXT("Missing pursuit world/player/subsystem"));return true;}
  const double Now=FPlatformTime::Seconds();
  if(!Started)
  {
   Started=Now;P->Health=10000;
   for(TActorIterator<APatrolEnemy> It(W);It;++It){It->SetActorTickEnabled(false);It->SetActorEnableCollision(false);}
   if(!Test->TestTrue(TEXT("Pursuit has valid road approach"),PreparePoliceRoad(W,PC)))return true;
   for(TActorIterator<APatrolEnemy> It(W);It;++It)if(It->IdentityRole==ENPCRole::Police){Victim=*It;break;}
   if(!Victim.IsValid()){Test->AddError(TEXT("Map must contain a police victim"));return true;}
   FHitResult H(Victim.Get(),Victim->GetMesh(),Victim->GetActorLocation(),FVector::UpVector);H.ImpactPoint=Victim->GetActorLocation();
   W->GetSubsystem<UDataTransmitter>()->TransmitHit(H,1,FVector::ForwardVector,PC,P);
  }
  for(const auto& Weak:S->Squad)if(auto* N=Weak.Get())
  {
   if(N->HeldWeapon&&N->HeldWeapon->Magazine<N->HeldWeapon->Spec().Capacity-1)Fired=true;
   if(!PhotoAt&&FVector::Dist2D(N->GetActorLocation(),P->GetActorLocation())<1000)
   {PC->SetControlRotation((N->GetActorLocation()-P->GetActorLocation()).Rotation());PhotoAt=Now;}
  }
  if(PhotoAt>0&&Now-PhotoAt>2)
  {FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/PoliceReinforcements-Pursuit.png"),false,false);PhotoAt=-1;}
  if(Now-Started<40)return false;
  Test->TestTrue(TEXT("Pursuit persists within two-star officer cap"),S->Squad.Num()>=3&&S->Squad.Num()<=6);
  Test->TestTrue(TEXT("Backup actually fires ammunition"),Fired);
  Test->TestTrue(TEXT("Player takes actual combat damage"),P->Health<10000);
  Test->TestTrue(TEXT("Backup reaches player and visual frame is captured"),PhotoAt<0);
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolicePursuitTest,"Game.NPC.PolicePursuit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPolicePursuitTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FPolicePursuitCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FPoliceWantedCheck,FAutomationTestBase*,Test);
bool FPoliceWantedCheck::Update()
{
 UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
 auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?PC->GetPawn().Get():nullptr;
 if(!P){Test->AddError(TEXT("Missing wanted test world/player"));return true;}
 auto* S=W->GetSubsystem<UPoliceReinforcements>();auto* Bus=W->GetSubsystem<UDataTransmitter>();S->ClearWanted();
 TArray<APatrolEnemy*> Created;
 auto Spawn=[&](ENPCRole Role,UClass* Class=nullptr)
 {
  const FTransform T(FRotator::ZeroRotator,P->GetActorLocation()+FVector(500,0,0));
  auto* N=W->SpawnActorDeferred<APatrolEnemy>(Class?Class:APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  N->NPCRole=Role;N->FinishSpawning(T);N->SetActorTickEnabled(false);Created.Add(N);return N;
 };
 auto Hit=[&](APatrolEnemy* N,float D)
 {FHitResult H(N,N->GetMesh(),N->GetActorLocation(),FVector::UpVector);Bus->TransmitHit(H,D,FVector::ForwardVector,PC,P);};
 auto* Civilian=Spawn(ENPCRole::Civilian);auto* Officer=Spawn(ENPCRole::Police);
 // Exercise real damage and police witnesses before any legitimate wanted event.
 for(ENPCRole Role:{ENPCRole::GangMember,ENPCRole::GangBoss})
 for(bool StreetBlueprint:{false,true})
 {
  UClass* Class=StreetBlueprint?LoadClass<APatrolEnemy>(nullptr,Role==ENPCRole::GangMember?TEXT("/Game/Combat/NPC/BP_GangMember.BP_GangMember_C"):TEXT("/Game/Combat/NPC/BP_GangBoss.BP_GangBoss_C")):APatrolEnemy::StaticClass();
  if(!Test->TestNotNull(TEXT("Gang test class exists"),Class))return true;
  auto* Gang=Spawn(Role,Class);
  Hit(Gang,1);
  Test->TestEqual(TEXT("Gang assault leaves zero wanted stars"),S->GetWantedLevel(),0);
  Test->TestNull(TEXT("Police do not attack the player for hurting gang"),Officer->CombatTarget.Get());
  Gang->BecomeInjured();
  Hit(Gang,1);
  Test->TestEqual(TEXT("Downed gang still has original faction"),Gang->IdentityRole,Role);
  // Even an erroneous caller-supplied protection flag cannot criminalize gang damage.
  Bus->OnViolence.Broadcast(Gang,P,true);
  Test->TestNull(TEXT("Police ignore protected flag for downed gang"),Officer->CombatTarget.Get());
  Hit(Gang,10000);
  Test->TestEqual(TEXT("Finishing gang does not raise wanted stars"),S->GetWantedLevel(),0);
  Test->TestEqual(TEXT("Gang wounds and deaths add no crime heat"),S->Heat,0);
  Test->TestNull(TEXT("Police do not target player after gang death"),Officer->CombatTarget.Get());
  Test->TestEqual(TEXT("Gang events do not reserve police reinforcements"),S->Pending,0);
 }
 Hit(Civilian,1);Test->TestEqual(TEXT("Civilian assault is one star"),S->GetWantedLevel(),1);
 for(TActorIterator<APatrolEnemy> It(W);It;++It){It->SetActorTickEnabled(false);It->SetActorEnableCollision(false);}
 if(!Test->TestTrue(TEXT("Dispatch comparison has a road"),PreparePoliceRoad(W,PC)))return true;
 S->LastKnownLocation=P->GetActorLocation();S->Tick(.01f);
 Test->TestEqual(TEXT("One star dispatches one car"),S->Cars.Num(),1);
 Test->TestEqual(TEXT("One star reserves three officers"),S->Pending,3);
 const int32 AssaultHeat=S->Heat;Hit(Civilian,1);Test->TestEqual(TEXT("Repeat wounds do not farm heat"),S->Heat,AssaultHeat);
 Hit(Civilian,10000);Test->TestEqual(TEXT("Civilian death raises two stars"),S->GetWantedLevel(),2);
 const int32 DeathHeat=S->Heat;Hit(Civilian,10000);Test->TestEqual(TEXT("Dead victim is never counted twice"),S->Heat,DeathHeat);
 Hit(Officer,10000);Test->TestTrue(TEXT("Officer death reaches at least three stars"),S->GetWantedLevel()>=3);
 for(int32 I=0;I<4;++I)Hit(Spawn(ENPCRole::Police),10000);
 Test->TestEqual(TEXT("Wanted caps at five stars"),S->GetWantedLevel(),5);
 S->Tick(.01f);
 Test->TestTrue(TEXT("Five stars dispatch more cars than one star"),S->Cars.Num()>1);
 Test->TestTrue(TEXT("Five-star car cap is respected"),S->Cars.Num()<=3);
 Test->TestTrue(TEXT("Five stars reserve more officers without exceeding cap"),S->Pending>3&&S->Pending+S->Squad.Num()<=15);
 const int32 Reserved=S->Pending;S->Tick(.01f);Test->TestEqual(TEXT("Dispatch interval prevents immediate duplicate waves"),S->Pending,Reserved);
 const int32 BeforeGang=S->Heat;Hit(Spawn(ENPCRole::GangMember),10000);Test->TestEqual(TEXT("Gang kill does not count as protected victim"),S->Heat,BeforeGang);
 const FVector Original=P->GetActorLocation();P->SetActorLocation(Original+FVector(0,0,100000),false,nullptr,ETeleportType::TeleportPhysics);
 const FVector Known=S->LastKnownLocation;S->UpdateAwareness(1);
 Test->TestTrue(TEXT("Unseen offender enters search"),S->IsSearching());
 Test->TestTrue(TEXT("Search cannot track invisible player movement"),S->LastKnownLocation.Equals(Known));
 S->UpdateAwareness(74.1f);Test->TestEqual(TEXT("Five-star escape clears after 75 seconds unseen"),S->GetWantedLevel(),0);
 P->SetActorLocation(Original,false,nullptr,ETeleportType::TeleportPhysics);
 Hit(Spawn(ENPCRole::Police),1);Test->TestEqual(TEXT("Police assault restarts at two stars"),S->GetWantedLevel(),2);
 S->ClearWanted();for(auto* N:Created)N->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoliceWantedTest,"Game.NPC.PoliceWanted",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPoliceWantedTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FPoliceWantedCheck(this));AddCommand(new FEndPlayMapCommand());return true;}

class FPoliceTrafficCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;double Began=0;FVector Initial,Direction;float InitialHealth=0;
 TWeakObjectPtr<APatrolEnemy> Pedestrian;TWeakObjectPtr<AExtractionConvoyCar> Vehicle;
 bool Yielded=false,MovedAside=false,Photo=false;
public:
 explicit FPoliceTrafficCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  if(!P){Test->AddError(TEXT("Missing traffic world"));return true;}
  auto* S=W->GetSubsystem<UPoliceReinforcements>();
  if(!Began)
  {
   PC->SetPause(false);PC->bShowMouseCursor=false;P->Health=10000;
   for(TActorIterator<APatrolEnemy> It(W);It;++It){It->SetActorTickEnabled(false);It->SetActorEnableCollision(false);}
   if(!Test->TestTrue(TEXT("Traffic fixture finds road"),PreparePoliceRoad(W,PC)))return true;
   S->Offender=P;S->LastKnownLocation=P->GetActorLocation();S->SetWantedLevel(1);S->Pending=3;S->DispatchTimer=1000;
   FVector Entry,Stop;FRotator Rotation;
   if(!Test->TestTrue(TEXT("Police finds an all-direction approach"),AExtractionConvoyCar::FindPoliceArrival(W,PC,P->GetActorLocation(),Entry,Stop,Rotation)))return true;
   Vehicle=W->SpawnActor<AExtractionConvoyCar>(Entry+FVector(0,0,76),Rotation);
   if(!Vehicle.IsValid()){Test->AddError(TEXT("Traffic car could not spawn"));return true;}
   Vehicle->Initialize(PC,Stop,Entry,true,3);S->Cars.Add(Vehicle);S->Car=Vehicle;
   Direction=(Stop-Entry).GetSafeNormal2D();Initial=Entry+Direction*FMath::Min(1300.f,FVector::Dist2D(Entry,Stop)*.6f)+FVector(0,0,93);
   const FTransform T(FRotator::ZeroRotator,Initial);
   auto* N=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
   if(!N){Test->AddError(TEXT("Traffic pedestrian could not spawn"));Vehicle->Destroy();S->ClearWanted();return true;}
   N->NPCRole=ENPCRole::Civilian;N->PatrolRadius=0;N->FinishSpawning(T);Pedestrian=N;InitialHealth=N->Health;
   PC->SetControlRotation((Initial-P->GetActorLocation()).Rotation());Began=FPlatformTime::Seconds();return false;
  }
  if(!Pedestrian.IsValid()||!Vehicle.IsValid()){Test->AddError(TEXT("Traffic fixture lost actors"));S->ClearWanted();return true;}
  auto* N=Pedestrian.Get();Yielded|=N->IsYieldingToTraffic();
  const FVector Right=FVector::CrossProduct(FVector::UpVector,Direction);
  MovedAside|=FMath::Abs(FVector::DotProduct(N->GetActorLocation()-Initial,Right))>210.f;
  if(Yielded&&!Photo){FScreenshotRequest::RequestScreenshot(TEXT("G:/TheTechfiles/ThefinalGameJam/Police-Traffic.png"),false,false);Photo=true;}
  for(auto Officer:S->Squad)if(Officer.IsValid())Officer->SetActorTickEnabled(false);
  if(FPlatformTime::Seconds()-Began<12&&Vehicle->Phase==EConvoyCarPhase::Approaching)return false;
  Test->TestTrue(TEXT("Pedestrian receives yielding request"),Yielded);
  Test->TestTrue(TEXT("Pedestrian actually moves clear of the lane"),MovedAside);
  Test->TestEqual(TEXT("Police traffic causes no pedestrian damage"),N->Health,InitialHealth);
  Test->TestTrue(TEXT("Car reaches unloading after yielding"),Vehicle->Phase!=EConvoyCarPhase::Approaching);
  N->Destroy();Vehicle->Destroy();S->ClearWanted();return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoliceTrafficTest,"Game.NPC.PoliceTraffic",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPoliceTrafficTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FPoliceTrafficCheck(this));AddCommand(new FEndPlayMapCommand());return true;}


class FPoliceHUDCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Step=0;double Next=0;bool English=false;
public:
 explicit FPoliceHUDCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now<Next)return false;
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
  auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC||!PC->GetPawn()){Test->AddError(TEXT("Missing HUD world"));return true;}
  auto* S=W->GetSubsystem<UPoliceReinforcements>();
  auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FString(TEXT("G:/TheTechfiles/ThefinalGameJam/WantedSystem/"))+Name+TEXT(".png"),true,false);};
  switch(Step)
  {
  case 0:English=GameLanguage::IsEnglish();PC->SetPause(true);S->Offender=PC->GetPawn().Get();S->SetWantedLevel(3);S->bSearching=false;GameLanguage::SetEnglish(false);break;
  case 1:Shot(TEXT("Wanted-zh"));break;
  case 2:GameLanguage::SetEnglish(true);break;
  case 3:Shot(TEXT("Wanted-en"));break;
  case 4:S->bSearching=true;GameLanguage::SetEnglish(false);break;
  case 5:Shot(TEXT("Search-zh"));break;
  case 6:GameLanguage::SetEnglish(true);break;
  case 7:Shot(TEXT("Search-en"));break;
  default:S->ClearWanted();PC->SetPause(false);GameLanguage::SetEnglish(English);return true;
  }
  ++Step;Next=Now+.4;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoliceHUDTest,"Game.NPC.PoliceHUD",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPoliceHUDTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FPoliceHUDCheck(this));AddCommand(new FEndPlayMapCommand());return true;}

#endif
