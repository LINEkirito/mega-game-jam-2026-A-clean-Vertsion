#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoliceReinforcements.h"
#include "PatrolEnemy.h"
#include "Data/DataTransmitter.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "EngineUtils.h"
#include "UnrealClient.h"

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
 Hit(Officer,1,PC,Player);Test->TestEqual(TEXT("Player hit requests exactly three"),S->Pending,3);
 Test->TestEqual(TEXT("Surviving officer retaliates immediately"),Officer->CombatTarget.Get(),static_cast<AActor*>(Player));
 Hit(Officer,1,PC,Player);Test->TestEqual(TEXT("Repeated hit does not stack waves"),S->Pending,3);
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 Test->TestFalse(TEXT("In front of camera rejected"),S->IsHidden(Eye+View.Vector()*1500,Player));
 Test->TestFalse(TEXT("Camera plane edge rejected"),S->IsHidden(Eye-View.Vector()*100,Player));
 Test->TestTrue(TEXT("Whole body behind camera accepted"),S->IsHidden(Eye-View.Vector()*1500,Player));
 for(int32 I=0;I<20&&S->Pending;++I)S->Tick(.6f);
 Test->TestEqual(TEXT("Real map has three reachable hidden spawns"),S->Squad.Num(),3);
 Test->TestEqual(TEXT("All three spawn requests consumed"),S->Pending,0);
 for(const auto& Weak:S->Squad)if(auto* N=Weak.Get()){
  Test->TestTrue(TEXT("Spawn remains behind player camera"),S->IsHidden(N->GetActorLocation(),Player));
  Test->TestEqual(TEXT("Backup has police identity"),N->IdentityRole,ENPCRole::Police);
  Test->TestEqual(TEXT("Backup attacks player"),N->CombatTarget.Get(),static_cast<AActor*>(Player));
  Test->TestEqual(TEXT("Backup enters combat immediately"),N->Activity,ENPCActivity::Combat);
  Test->TestNotNull(TEXT("Backup has a weapon"),N->HeldWeapon.Get());
 }
 Hit(Officer,10000,PC,Player);Test->TestEqual(TEXT("Lethal followup does not duplicate active squad"),S->Pending,0);
 for(const auto& Weak:S->Squad)if(auto* N=Weak.Get())N->Destroy();S->Squad.Empty();S->Offender.Reset();S->Pending=0;
 auto* Lethal=Spawn(ENPCRole::Police);Hit(Lethal,10000,PC,Player);Test->TestEqual(TEXT("First-hit kill still calls three backup"),S->Pending,3);
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
  if(Now-Started<25)return false;
  Test->TestEqual(TEXT("Three officers persist through pursuit"),S->Squad.Num(),3);
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
#endif
