#include "WorldEvents.h"
#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UWorldEvents::DoesSupportWorldType(EWorldType::Type Type) const {return Type==EWorldType::Game||Type==EWorldType::PIE;}
TStatId UWorldEvents::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldEvents,STATGROUP_Tickables);}
FString UWorldEvents::EventName(EStreetEvent Kind) {
 switch(Kind){
 case EStreetEvent::CivilianEscape:return TEXT("市民逃跑");
 case EStreetEvent::GangMeeting:return TEXT("帮派聚会");
 case EStreetEvent::SecretTransaction:return TEXT("秘密交易");
 case EStreetEvent::PoliceChase:return TEXT("警察追击");
 case EStreetEvent::GangShootout:return TEXT("警匪枪战");
 default:return TEXT("大规模帮派战争");}
}
const FStreetEventRecord* UWorldEvents::Find(int32 Id) const {return Events.FindByPredicate([Id](const FStreetEventRecord& E){return E.Id==Id;});}
bool UWorldEvents::Claim(int32 Id) {
 auto* E=Events.FindByPredicate([Id](const FStreetEventRecord& V){return V.Id==Id;});
 if(!E||E->bClaimed)return false;E->bClaimed=true;return true;
}
int32 UWorldEvents::BeginEvent(EStreetEvent Kind,const TArray<APatrolEnemy*>& People) {
 if(People.IsEmpty())return 0;
 for(auto* N:People)if(!IsValid(N)||N->Health<=0||N->NPCRole==ENPCRole::Injured||N->StreetEventId)return 0;
 FStreetEventRecord E;E.Id=NextId++;E.Kind=Kind;E.Started=GetWorld()->GetTimeSeconds();
 E.Tier=Kind==EStreetEvent::MassiveGangWar?3:(Kind==EStreetEvent::CivilianEscape||Kind==EStreetEvent::GangMeeting)?1:2;
 const auto* C=GetDefault<UGlobalData>();E.Points=FMath::Max(0,E.Tier==1?C->CommonEventPoints:E.Tier==2?C->RareEventPoints:C->LegendaryEventPoints);
 for(auto* N:People){E.Participants.Add(N);E.Location+=N->GetActorLocation();N->StreetEventId=E.Id;}
 E.Location/=People.Num();Events.Add(E);
 GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("StreetEventStarted"),E.Id);
 return E.Id;
}
void UWorldEvents::EndEvent(int32 Id) {
 auto* E=Events.FindByPredicate([Id](const FStreetEventRecord& V){return V.Id==Id;});if(!E||!E->bActive)return;
 E->bActive=false;
 for(auto Weak:E->Participants)if(auto* N=Weak.Get();N&&N->StreetEventId==Id){
  N->StreetEventId=0;N->SocialCooldown=18;
  if(N->Activity==ENPCActivity::Meeting||N->Activity==ENPCActivity::Trade){
   N->ReleasePartner();if(auto* AI=Cast<AAIController>(N->GetController()))AI->StopMovement();N->SetActivity(ENPCActivity::Idle);
  }
 }
 GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("StreetEventEnded"),Id);
}
void UWorldEvents::ObserveEscape(APatrolEnemy* N) {
 if(!GetDefault<UGlobalData>()->bEnableStreetEvents||!N||N->StreetEventId)return;
 const float Now=GetWorld()->GetTimeSeconds();if(const float* Last=EscapeCooldown.Find(N);Last&&Now-*Last<30)return;
 if(BeginEvent(EStreetEvent::CivilianEscape,{N})){EscapeCooldown.Add(N,Now);Events.Last().bPerforming=true;Events.Last().PerformingSince=Now;}
}
void UWorldEvents::ObserveCombat(APatrolEnemy* N,APatrolEnemy* Target) {
 if(!GetDefault<UGlobalData>()->bEnableStreetEvents||!N||!Target)return;
 if(!((N->IsGang()&&Target->NPCRole==ENPCRole::Police)||(Target->IsGang()&&N->NPCRole==ENPCRole::Police)))return;
 if(N->StreetEventId||Target->StreetEventId)return;
 BeginEvent(EStreetEvent::GangShootout,{N,Target});
}
bool UWorldEvents::TryStartEvent(EStreetEvent Kind) {
 if(Kind==EStreetEvent::CivilianEscape)return false; // Only actual danger creates an escape.
 const auto* Config=GetDefault<UGlobalData>();
 if(Kind==EStreetEvent::MassiveGangWar&&(bWarStarted||GetWorld()->GetTimeSeconds()<Config->LegendaryEarliestSeconds))return false;
 TArray<APatrolEnemy*> Gang,Police;
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It){auto* N=*It;
  if(N->Health<=0||N->StreetEventId||N->SocialCooldown>0||N->Partner.IsValid()||N->IsProtectedPerson()||N->Activity==ENPCActivity::WarnPlayer||N->bCombat||N->CameraReactionTime>0)continue;
  if(N->IsGang())Gang.Add(N);else if(N->NPCRole==ENPCRole::Police)Police.Add(N);
 }
 if(Gang.IsEmpty())return false;
 // Randomize the anchor, then use a local cluster rather than teleporting actors.
 APatrolEnemy* Anchor=Gang[FMath::RandHelper(Gang.Num())];const FVector Center=Anchor->GetActorLocation();
 Gang.Sort([Center](const APatrolEnemy& A,const APatrolEnemy& B){return FVector::DistSquared(A.GetActorLocation(),Center)<FVector::DistSquared(B.GetActorLocation(),Center);});
 Police.Sort([Center](const APatrolEnemy& A,const APatrolEnemy& B){return FVector::DistSquared(A.GetActorLocation(),Center)<FVector::DistSquared(B.GetActorLocation(),Center);});
 const bool Meeting=Kind==EStreetEvent::GangMeeting,Trade=Kind==EStreetEvent::SecretTransaction,War=Kind==EStreetEvent::MassiveGangWar;
 const int32 GangCount=Meeting||War?3:Trade?2:1,PoliceCount=Meeting||Trade?0:War?2:1;
 if(Gang.Num()<GangCount||Police.Num()<PoliceCount)return false;
 const float Radius=War?5000.f:Meeting?1800.f:Trade?1000.f:2200.f;
 TArray<APatrolEnemy*> People;
 for(int32 I=0;I<GangCount;++I){if(FVector::Dist2D(Center,Gang[I]->GetActorLocation())>Radius)return false;People.Add(Gang[I]);}
 for(int32 I=0;I<PoliceCount;++I){if(FVector::Dist2D(Center,Police[I]->GetActorLocation())>Radius)return false;People.Add(Police[I]);}
 if(Kind==EStreetEvent::PoliceChase&&!Police[0]->CanSeeActor(Anchor))return false;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 TArray<FVector> Destinations;
 if(Meeting){
  if(!Nav)return false;
  for(int32 I=0;I<GangCount;++I){FNavLocation P;const float Angle=2*PI*I/GangCount;
   if(!Nav->ProjectPointToNavigation(Center+FVector(FMath::Cos(Angle)*140,FMath::Sin(Angle)*140,0),P,FVector(180,180,300)))return false;
   Destinations.Add(P.Location);
  }
 }
 const int32 Id=BeginEvent(Kind,People);if(!Id)return false;
 for(auto* N:People){N->ReleasePartner();N->MoveRetry=0;N->bMoving=false;}
 if(Meeting){for(int32 I=0;I<GangCount;++I){auto* N=Gang[I];N->EventDestination=Destinations[I];N->EventFocus=Center;N->SetActivity(ENPCActivity::Meeting);}}
 else if(Trade){Gang[0]->Partner=Gang[1];Gang[1]->Partner=Gang[0];Gang[0]->SetActivity(ENPCActivity::Trade);Gang[1]->SetActivity(ENPCActivity::Trade);}
 else {
  for(int32 I=0;I<PoliceCount;++I)Police[I]->SetTarget(Gang[I%GangCount]);
  if(Kind!=EStreetEvent::PoliceChase)for(int32 I=0;I<GangCount;++I)Gang[I]->SetTarget(Police[I%PoliceCount]);
  if(War)bWarStarted=true;
 }
 return true;
}
void UWorldEvents::Tick(float DT) {
 if(!GetWorld()||!GetWorld()->HasBegunPlay())return;
 const float Now=GetWorld()->GetTimeSeconds();int32 Active=0;
 for(auto& E:Events){if(!E.bActive)continue;if(E.Kind!=EStreetEvent::CivilianEscape)++Active;
  int32 Living=0,Ready=0,Gang=0,Police=0;bool Interrupted=false;
  for(auto Weak:E.Participants){auto* N=Weak.Get();if(!N||N->Health<=0||N->NPCRole==ENPCRole::Injured)continue;++Living;
   if(E.Kind==EStreetEvent::MassiveGangWar&&!N->IsLiving(N->CombatTarget)){
    for(auto Other:E.Participants)if(auto* Target=Other.Get();Target&&N->IsLiving(Target)&&N->IsGang()!=Target->IsGang()){N->SetTarget(Target);break;}
   }
   Gang+=N->IsGang();Police+=N->NPCRole==ENPCRole::Police;
   switch(E.Kind){
   case EStreetEvent::GangMeeting:Interrupted|=N->Activity!=ENPCActivity::Meeting;Ready+=N->Activity==ENPCActivity::Meeting&&FVector::Dist2D(N->GetActorLocation(),N->EventDestination)<95;break;
   case EStreetEvent::SecretTransaction:Interrupted|=N->Activity!=ENPCActivity::Trade;Ready+=N->Activity==ENPCActivity::Trade&&N->bSocialArrived;break;
   case EStreetEvent::CivilianEscape:Ready+=N->Activity==ENPCActivity::Flee;break;
   default:Ready+=N->bCombat&&N->IsLiving(N->CombatTarget)&&N->CanSeeActor(N->CombatTarget,false);break;
   }
  }
  const bool Social=E.Kind==EStreetEvent::GangMeeting||E.Kind==EStreetEvent::SecretTransaction;
  const int32 Required=Social?E.Participants.Num():E.Kind==EStreetEvent::MassiveGangWar?3:1;
  if(Ready>=Required&&!E.bPerforming){E.bPerforming=true;E.PerformingSince=Now;}
  // Preparation never scores. Stop social scenes when interrupted; never override self-defense.
  const bool End=Living==0||Interrupted||(Social&&Living<E.Participants.Num())||
   (E.Kind==EStreetEvent::CivilianEscape&&Ready==0)||
   (!Social&&E.Kind!=EStreetEvent::CivilianEscape&&(Gang==0||Police==0))||
   (Social&&!E.bPerforming&&Now-E.Started>30)||
   (E.Kind==EStreetEvent::GangMeeting&&E.bPerforming&&Now-E.PerformingSince>16)||
   Now-E.Started>(E.Kind==EStreetEvent::MassiveGangWar?90.f:60.f);
  if(End)EndEvent(E.Id);
  else if(!Social&&Ready<Required)E.bPerforming=false;
 }
 const auto* C=GetDefault<UGlobalData>();if(!bAutomatic||!C->bEnableStreetEvents)return;
 NextAttempt-=DT;if(NextAttempt>0||Active>=3)return;NextAttempt=FMath::Max(1.f,C->EventAttemptSeconds);
 const float CW=FMath::Max(0.f,C->CommonEventWeight),RW=FMath::Max(0.f,C->RareEventWeight),LW=FMath::Max(0.f,C->LegendaryEventWeight);
 if(CW+RW+LW<=0)return;const float Roll=FMath::FRand()*(CW+RW+LW);
 if(Roll<CW)TryStartEvent(EStreetEvent::GangMeeting);
 else if(Roll<CW+RW){const EStreetEvent Kinds[]={EStreetEvent::SecretTransaction,EStreetEvent::PoliceChase,EStreetEvent::GangShootout};TryStartEvent(Kinds[FMath::RandHelper(3)]);}
 else TryStartEvent(EStreetEvent::MassiveGangWar);
}
