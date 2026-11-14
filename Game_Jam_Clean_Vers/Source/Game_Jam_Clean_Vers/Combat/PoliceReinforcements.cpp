#include "PoliceReinforcements.h"
#include "PatrolEnemy.h"
#include "Data/DataTransmitter.h"
#include "Data/GlobalData.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

bool UPoliceReinforcements::DoesSupportWorldType(EWorldType::Type Type) const {return Type==EWorldType::Game||Type==EWorldType::PIE;}
TStatId UPoliceReinforcements::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UPoliceReinforcements,STATGROUP_Tickables);}
void UPoliceReinforcements::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);Collection.InitializeDependency<UDataTransmitter>();
 GetWorld()->GetSubsystem<UDataTransmitter>()->OnViolence.AddDynamic(this,&UPoliceReinforcements::OnViolence);
}
void UPoliceReinforcements::Deinitialize()
{
 if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>())Bus->OnViolence.RemoveDynamic(this,&UPoliceReinforcements::OnViolence);
 Squad.Empty();Offender.Reset();Pending=0;Super::Deinitialize();
}
void UPoliceReinforcements::OnViolence(AActor* Victim,APawn* Attacker,bool)
{
 auto* Officer=Cast<APatrolEnemy>(Victim);
 if(!GetWorld()->GetAuthGameMode()||!Officer||Officer->IdentityRole!=ENPCRole::Police||!Attacker||!Attacker->IsPlayerControlled())return;
 // IdentityRole survives lethal and injury hits; both still call for backup.
 if(Officer->Health>0&&Officer->NPCRole==ENPCRole::Police){Officer->PriorityOffender=Attacker;Officer->SetTarget(Attacker);}
 Squad.RemoveAll([](const auto& N){return !N.IsValid()||N->Health<=0||N->NPCRole!=ENPCRole::Police;});
 if(Pending>0||Squad.Num()>0)return;
 Offender=Attacker;Pending=3;Retry=0;
}
bool UPoliceReinforcements::IsHidden(const FVector& Location,APawn* Player) const
{
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC)return false;
 FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
 // Conservative whole-body bound behind the camera plane. No FOV/aspect-ratio
 // assumptions, and a visible head/weapon cannot pop up at the screen edge.
 return FVector::DotProduct(Location-Eye,Rotation.Vector()) < -220.f;
}
bool UPoliceReinforcements::FindSpawn(APawn* Player,FVector& Location) const
{
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 const auto* Rules=GetDefault<UGlobalData>();
 const float Min=FMath::Max(300.f,Rules->PoliceReinforcementMinDistance),Max=FMath::Max(Min,Rules->PoliceReinforcementMaxDistance);
 const auto* Capsule=GetDefault<APatrolEnemy>()->GetCapsuleComponent();
 const float Radius=Capsule->GetScaledCapsuleRadius(),HalfHeight=Capsule->GetScaledCapsuleHalfHeight();
 for(int32 I=0;I<48;++I)
 {
  FNavLocation Point;if(!Nav->GetRandomReachablePointInRadius(Player->GetActorLocation(),Max,Point))continue;
  if(FVector::DistSquared2D(Point.Location,Player->GetActorLocation())<FMath::Square(Min))continue;
  const FVector Candidate=Point.Location+FVector(0,0,HalfHeight+3);
  if(!IsHidden(Candidate,Player))continue;
  if(GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius+5,HalfHeight),FCollisionQueryParams(SCENE_QUERY_STAT(PoliceSpawn))))continue;
  auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Point.Location,Player->GetActorLocation(),Player);
  if(!Path||!Path->IsValid()||Path->IsPartial())continue;
  Location=Candidate;return true;
 }
 return false;
}
void UPoliceReinforcements::Tick(float DT)
{
 auto* Player=Offender.Get();
 const auto* Character=Cast<AGame_Jam_Clean_VersCharacter>(Player);
 if(!Player||!Player->IsPlayerControlled()||(Character&&Character->Health<=0)){Pending=0;Offender.Reset();return;}
 Squad.RemoveAll([](const auto& N){return !N.IsValid()||N->Health<=0||N->NPCRole!=ENPCRole::Police;});
 // Backup keeps pursuing, rather than abandoning combat eight seconds after
 // spawning out of sight. Actual shots still require the existing LOS checks.
 for(auto& Weak:Squad)if(auto* N=Weak.Get())
 {
  N->PriorityOffender=Player;if(N->CombatTarget!=Player)N->SetTarget(Player);
  N->LastKnown=Player->GetActorLocation();N->LostTime=0;
 }
 Retry-=DT;if(Pending<=0||Retry>0)return;Retry=.5f;
 for(int32 I=Pending;I>0;--I)
 {
  FVector Location;if(!FindSpawn(Player,Location))break;
  const FTransform Transform((Player->GetActorLocation()-Location).Rotation(),Location);
  auto* N=GetWorld()->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
  if(!N)break;
  N->NPCRole=ENPCRole::Police;N->WeaponKind=ETacticalWeaponKind::Rifle;N->bRandomGangWeapon=false;
  N->FinishSpawning(Transform);
  if(!IsValid(N))continue;
  N->PriorityOffender=Player;N->SetTarget(Player);N->ShotTime=0;N->MoveRetry=0;
  Squad.Add(N);--Pending;
 }
}
