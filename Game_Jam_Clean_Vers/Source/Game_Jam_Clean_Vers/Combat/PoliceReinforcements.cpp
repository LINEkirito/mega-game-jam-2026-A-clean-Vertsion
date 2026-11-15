#include "PoliceReinforcements.h"
#include "PatrolEnemy.h"
#include "Data/DataTransmitter.h"
#include "Data/GlobalData.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Extraction/ExtractionConvoyCar.h"
#include "Camcorder/RecordingLibrary.h"
#include "EngineUtils.h"
#include "AIController.h"

namespace
{
 template<typename T>T Tier(const TArray<T>& Values,int32 Stars,T Fallback)
 {return Values.IsValidIndex(Stars-1)?Values[Stars-1]:Fallback;}
}
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
 Cars.Empty();Squad.Empty();Offender.Reset();Car.Reset();ReportedAssaults.Empty();ReportedDeaths.Empty();Super::Deinitialize();
}
void UPoliceReinforcements::SetWantedLevel(int32 Level)
{
 Level=FMath::Clamp(Level,0,5);if(Level==WantedLevel)return;
 WantedLevel=Level;GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("WantedLevel"),WantedLevel);
}
void UPoliceReinforcements::ClearWanted()
{
 auto* Player=Offender.Get();
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It)if(It->NPCRole==ENPCRole::Police)
 {
  if(Player&&It->PriorityOffender.Get()==Player)
  {
   It->PriorityOffender.Reset();
   if(It->Activity==ENPCActivity::Investigate){It->InvestigationTime=0;It->SetActivity(ENPCActivity::Idle);if(auto* AI=Cast<AAIController>(It->GetController()))AI->StopMovement();}
  }
  if(Player&&It->CombatTarget==Player)
  {It->CombatTarget=nullptr;It->SetActivity(ENPCActivity::Idle);if(auto* AI=Cast<AAIController>(It->GetController()))AI->StopMovement();}
 }
 for(auto C:Cars)if(C.IsValid())C->LeaveWithoutUnloading();
 Pending=0;Heat=0;UnseenSeconds=0;DispatchTimer=0;Retry=0;bSearching=false;Offender.Reset();SetWantedLevel(0);
 // Living officers return to the world; no visible despawning on escape.
 // Keep squad accounting so immediately recommitting a crime cannot exceed the cap.
 ReportedAssaults.Empty();ReportedDeaths.Empty();
}
void UPoliceReinforcements::OnViolence(AActor* Victim,APawn* Attacker,bool)
{
 auto* N=Cast<APatrolEnemy>(Victim);
 if(!GetWorld()->GetAuthGameMode()||!N||!Attacker||!Attacker->IsPlayerControlled())return;
 // Gang attacks are never crimes, including finishing an injured gang member.
 if(N->IsGang()||N->IdentityRole==ENPCRole::GangMember||N->IdentityRole==ENPCRole::GangBoss)return;
 const bool Police=N->IdentityRole==ENPCRole::Police;
 if(!Police&&!N->IsLawProtected())return;
 const bool Killed=N->Health<=0;
 auto& Reported=Killed?ReportedDeaths:ReportedAssaults;
 if(Reported.Contains(N))
 {if(WantedLevel>0){LastKnownLocation=Attacker->GetActorLocation();UnseenSeconds=0;bSearching=false;}return;}
 Reported.Add(N);
 // One report per victim for assault and one for death; corpses cannot farm stars.
 const int32 Points=Killed?(Police?6:3):1;
 Heat=FMath::Min(100,Heat+Points);
 const auto* Rules=GetDefault<UGlobalData>();int32 Level=0;
 for(int32 I=0;I<FMath::Min(5,Rules->WantedThresholds.Num());++I)if(Heat>=Rules->WantedThresholds[I])Level=I+1;
 Level=FMath::Max(Level,Killed?(Police?3:2):(Police?2:1));
 if(Rules->WantedThresholds.IsValidIndex(Level-1))Heat=FMath::Max(Heat,Rules->WantedThresholds[Level-1]);
 const int32 Previous=WantedLevel;SetWantedLevel(Level);
 Offender=Attacker;LastKnownLocation=Attacker->GetActorLocation();UnseenSeconds=0;bSearching=false;
 if(Police&&N->Health>0&&N->NPCRole==ENPCRole::Police){N->PriorityOffender=Attacker;N->SetTarget(Attacker);}
 if(Previous==0||Level>Previous){DispatchTimer=0;Retry=0;}
 // Dispatch reserves only seats in successfully spawned vehicles.
}
bool UPoliceReinforcements::IsWantedPlayer(const AActor* Actor) const
{return WantedLevel>0&&Offender.Get()==Actor;}
bool UPoliceReinforcements::CanDeliver() const
{
 auto* P=Offender.Get();auto* C=Cast<AGame_Jam_Clean_VersCharacter>(P);
 return WantedLevel>0&&P&&P->IsPlayerControlled()&&(!C||C->Health>0)&&Pending>0;
}
void UPoliceReinforcements::RegisterPassenger(APatrolEnemy* N)
{
 if(!IsValid(N))return;
 if(!CanDeliver()){N->Destroy();return;}
 Squad.AddUnique(N);--Pending;
 N->PriorityOffender=Offender;
 if(!bSearching||N->CanSeeActor(Offender.Get(),false))
 {bSearching=false;UnseenSeconds=0;LastKnownLocation=Offender->GetActorLocation();N->SetTarget(Offender.Get());}
 else {N->CombatTarget=nullptr;N->InvestigationLocation=LastKnownLocation;N->InvestigationTime=120;N->SetActivity(ENPCActivity::Investigate);}
 N->ShotTime=0;N->MoveRetry=0;N->WeaponCarry=ENPCWeaponCarry::Drawn;N->SetMotion();N->UpdateWeaponPose();
}
void UPoliceReinforcements::UpdateAwareness(float DT)
{
 auto* Player=Offender.Get();bool Seen=false;
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It)
  if(It->NPCRole==ENPCRole::Police&&It->Health>0&&It->CanSeeActor(Player,false)){Seen=true;break;}
 bSearching=!Seen;
 if(Seen){LastKnownLocation=Player->GetActorLocation();UnseenSeconds=0;}
 else UnseenSeconds+=DT;
 if(UnseenSeconds>=Tier(GetDefault<UGlobalData>()->WantedEscapeSeconds,WantedLevel,75.f)){ClearWanted();return;}
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It)
 {
  auto* N=*It;if(N->NPCRole!=ENPCRole::Police||N->Health<=0)continue;
  if(!Squad.Contains(N)&&N->PriorityOffender.Get()!=Player)continue;
  N->PriorityOffender=Player;
  if(Seen)
  {if(N->CombatTarget!=Player)N->SetTarget(Player);N->LastKnown=LastKnownLocation;N->LostTime=0;}
  else
  {
   N->CombatTarget=nullptr;N->InvestigationLocation=LastKnownLocation;N->InvestigationTime=120;
   if(N->Activity!=ENPCActivity::Investigate){N->SetActivity(ENPCActivity::Investigate);N->MoveRetry=0;}
  }
 }
}
void UPoliceReinforcements::Tick(float DT)
{
 if(WantedLevel<=0)return;
 auto* Player=Offender.Get();const auto* Character=Cast<AGame_Jam_Clean_VersCharacter>(Player);
 auto* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
 auto* Library=PC?PC->FindComponentByClass<URecordingLibrary>():nullptr;
 if(!Player||!Player->IsPlayerControlled()||(Character&&Character->Health<=0)||(Library&&Library->bRoundEnded)){ClearWanted();return;}
 UpdateAwareness(DT);if(WantedLevel==0)return;
 Squad.RemoveAll([](const auto& N){return !N.IsValid()||N->Health<=0||N->NPCRole!=ENPCRole::Police;});
 Cars.RemoveAll([](const auto& C){return !C.IsValid();});
 Pending=0;for(auto C:Cars)if(C.IsValid())Pending+=C->GetUndeliveredPassengers();
 DispatchTimer-=DT;Retry-=DT;if(DispatchTimer>0||Retry>0)return;Retry=2.f;
 const auto* Rules=GetDefault<UGlobalData>();
 const int32 Cap=FMath::Max(0,Tier(Rules->WantedOfficerCaps,WantedLevel,15));
 const int32 CarCap=FMath::Clamp(Tier(Rules->WantedCarCaps,WantedLevel,3),1,3);
 bool Spawned=false;
 while(Cars.Num()<CarCap&&Squad.Num()+Pending<Cap)
 {
  FVector Start,Stop;FRotator Rotation;
  if(!AExtractionConvoyCar::FindPoliceArrival(GetWorld(),PC,LastKnownLocation,Start,Stop,Rotation))break;
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
  auto* Vehicle=GetWorld()->SpawnActor<AExtractionConvoyCar>(Start+FVector(0,0,76),Rotation,Params);if(!Vehicle)break;
  const int32 Count=FMath::Min(3,Cap-Squad.Num()-Pending);
  Car=Vehicle;Cars.Add(Vehicle);Pending+=Count;Vehicle->Initialize(PC,Stop,Start,true,Count);Spawned=true;
 }
 if(Spawned)DispatchTimer=FMath::Max(1.f,Tier(Rules->WantedBackupIntervals,WantedLevel,12.f));
}
