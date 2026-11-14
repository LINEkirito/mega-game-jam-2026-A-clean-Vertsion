#include "PoliceReinforcements.h"
#include "PatrolEnemy.h"
#include "Data/DataTransmitter.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Extraction/ExtractionConvoyCar.h"

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
 Squad.Empty();Offender.Reset();Car.Reset();Pending=0;Super::Deinitialize();
}
void UPoliceReinforcements::OnViolence(AActor* Victim,APawn* Attacker,bool)
{
 auto* Officer=Cast<APatrolEnemy>(Victim);
 if(!GetWorld()->GetAuthGameMode()||!Officer||Officer->IdentityRole!=ENPCRole::Police||!Attacker||!Attacker->IsPlayerControlled())return;
 // IdentityRole survives lethal and injury hits; both still call for backup.
 if(Officer->Health>0&&Officer->NPCRole==ENPCRole::Police){Officer->PriorityOffender=Attacker;Officer->SetTarget(Attacker);}
 Squad.RemoveAll([](const auto& N){return !N.IsValid()||N->Health<=0||N->NPCRole!=ENPCRole::Police;});
 if(Pending>0||Squad.Num()>0||Car.IsValid())return;
 Offender=Attacker;Pending=3;Retry=0;
}
bool UPoliceReinforcements::CanDeliver() const
{
 auto* P=Offender.Get();auto* C=Cast<AGame_Jam_Clean_VersCharacter>(P);
 return P&&P->IsPlayerControlled()&&(!C||C->Health>0)&&Pending>0;
}
void UPoliceReinforcements::RegisterPassenger(APatrolEnemy* N)
{
 if(!IsValid(N))return;
 if(!CanDeliver()){N->Destroy();return;}
 auto* Player=Offender.Get();Squad.AddUnique(N);--Pending;
 N->PriorityOffender=Player;N->SetTarget(Player);N->ShotTime=0;N->MoveRetry=0;
 N->WeaponCarry=ENPCWeaponCarry::Drawn;N->SetMotion();N->UpdateWeaponPose();
}
void UPoliceReinforcements::Tick(float DT)
{
 auto* Player=Offender.Get();
 const auto* Character=Cast<AGame_Jam_Clean_VersCharacter>(Player);
 if(!Player||!Player->IsPlayerControlled()||(Character&&Character->Health<=0)){Pending=0;Offender.Reset();if(Car.IsValid())Car->LeaveWithoutUnloading();return;}
 Squad.RemoveAll([](const auto& N){return !N.IsValid()||N->Health<=0||N->NPCRole!=ENPCRole::Police;});
 // Backup keeps pursuing, rather than abandoning combat eight seconds after
 // spawning out of sight. Actual shots still require the existing LOS checks.
 for(auto& Weak:Squad)if(auto* N=Weak.Get())
 {
  N->PriorityOffender=Player;if(N->CombatTarget!=Player)N->SetTarget(Player);
  N->LastKnown=Player->GetActorLocation();N->LostTime=0;
 }
 if(Car.IsValid())return;
 Retry-=DT;if(Pending<=0||Retry>0)return;Retry=1.f;
 auto* PC=Cast<APlayerController>(Player->GetController());
 FVector Start,Stop;FRotator Rotation;
 if(!AExtractionConvoyCar::FindArrival(GetWorld(),PC,Start,Stop,Rotation))return;
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
 auto* Vehicle=GetWorld()->SpawnActor<AExtractionConvoyCar>(Start+FVector(0,0,76),Rotation,Params);
 if(!Vehicle)return;
 Car=Vehicle;Vehicle->Initialize(PC,Stop,Start,true,Pending);
}
