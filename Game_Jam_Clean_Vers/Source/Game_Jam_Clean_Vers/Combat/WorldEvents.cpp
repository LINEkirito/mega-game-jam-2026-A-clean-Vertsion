#include "WorldEvents.h"
#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UWorldEvents::DoesSupportWorldType(EWorldType::Type Type) const {return Type==EWorldType::Game||Type==EWorldType::PIE;}
TStatId UWorldEvents::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldEvents,STATGROUP_Tickables);}
FString UWorldEvents::EventName(EStreetEvent Kind) {
 switch(Kind){
 case EStreetEvent::CivilianEscape:return TEXT("市民逃跑 / Civilian Escape");
 case EStreetEvent::GangMeeting:return TEXT("帮派聚会 / Gang Meeting");
 case EStreetEvent::SecretTransaction:return TEXT("秘密交易 / Secret Transaction");
 case EStreetEvent::PoliceChase:return TEXT("警察追击（旧版，不计事件分）");
 case EStreetEvent::GangShootout:return TEXT("警匪枪战 / Gang Shootout");
 case EStreetEvent::GangMassacre:
 default:return TEXT("大规模帮派战争 / Massive Gang War");}
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
 E.Tier=(Kind==EStreetEvent::MassiveGangWar||Kind==EStreetEvent::GangMassacre)?3:(Kind==EStreetEvent::CivilianEscape||Kind==EStreetEvent::GangMeeting)?1:2;
 const auto* C=GetDefault<UGlobalData>();E.Points=FMath::Max(0,E.Tier==1?C->CommonEventPoints:E.Tier==2?C->RareEventPoints:C->LegendaryEventPoints);
 const auto Duration=[](float Min,float Max){return FMath::FRandRange(FMath::Max(1.f,Min),FMath::Max(FMath::Max(1.f,Min),Max));};
 switch(Kind){
 case EStreetEvent::GangMeeting:E.Duration=Duration(C->MeetingMinDuration,C->MeetingMaxDuration);break;
 case EStreetEvent::SecretTransaction:E.Duration=Duration(C->SecretTradeMinDuration,C->SecretTradeMaxDuration);break;
 case EStreetEvent::CivilianEscape:E.Duration=Duration(C->EscapeMinDuration,C->EscapeMaxDuration);break;
 case EStreetEvent::GangShootout:E.Duration=Duration(C->PoliceGangMinDuration,C->PoliceGangMaxDuration);break;
 default:E.Duration=Duration(C->LargeEventMinDuration,C->LargeEventMaxDuration);break;
 }
 E.bScheduled=bStartingScheduled;
 for(auto* N:People){E.Participants.Add(N);E.Location+=N->GetActorLocation();N->StreetEventId=E.Id;}
 E.Location/=People.Num();Events.Add(E);
 GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("StreetEventStarted"),E.Id);
 return E.Id;
}
void UWorldEvents::EndEvent(int32 Id) {
 auto* E=Events.FindByPredicate([Id](const FStreetEventRecord& V){return V.Id==Id;});if(!E||!E->bActive)return;
 E->bActive=false;
#if WITH_EDITOR
 E->DebugEndedAt=GetWorld()->GetTimeSeconds();
 if(E->DebugEndReason.IsEmpty())E->DebugEndReason=TEXT("手动结束");
#endif
 // An accepted scheduled occurrence is retained until it has actually performed.
 if(E->bScheduled&&!E->bHasPerformed)PendingEvents.Add(E->Kind);
 const float Cooldown=FMath::Max(1.f,GetDefault<UGlobalData>()->EventCooldownSeconds);
 if(E->Kind==EStreetEvent::SecretTransaction&&E->bHasPerformed&&GetWorld()->GetTimeSeconds()>=E->ExpiresAt)
  for(auto Weak:E->Participants)if(auto* N=Weak.Get();N&&N->StreetEventId==Id&&N->Activity==ENPCActivity::Trade){
   ++N->CompletedTrades;GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(N,TEXT("Trades"),N->CompletedTrades);
  }

 for(auto Weak:E->Participants)if(auto* N=Weak.Get();N&&N->StreetEventId==Id){
  N->StreetEventId=0;N->SocialCooldown=Cooldown;
  if(E->Kind==EStreetEvent::GangShootout||E->Kind==EStreetEvent::MassiveGangWar)
  {
   N->CrossfireCooldown=Cooldown;
   auto* Target=Cast<APatrolEnemy>(N->CombatTarget);
   const bool Crossfire=Target&&((N->NPCRole==ENPCRole::Police&&Target->IsGang())||(N->IsGang()&&Target->NPCRole==ENPCRole::Police));
   if(Crossfire)
   {
    N->CombatTarget=nullptr;N->ObservedShooter.Reset();N->EscalationTarget.Reset();N->LostTime=0;
    if(auto* AI=Cast<AAIController>(N->GetController()))AI->StopMovement();
    N->GetCharacterMovement()->StopMovementImmediately();N->SetActivity(ENPCActivity::Idle);
   }
  }
  if(E->Kind==EStreetEvent::GangMassacre&&N->IsGang()&&E->Participants.Contains(Cast<APatrolEnemy>(N->CombatTarget)))
  {
   N->CombatTarget=nullptr;N->EscalationTarget.Reset();
   if(auto* AI=Cast<AAIController>(N->GetController()))AI->StopMovement();N->SetActivity(ENPCActivity::Idle);
  }
  if(E->Kind==EStreetEvent::CivilianEscape&&N->Activity==ENPCActivity::Flee)
  {
   N->PanicTime=0;if(auto* AI=Cast<AAIController>(N->GetController()))AI->StopMovement();
   N->GetCharacterMovement()->StopMovementImmediately();N->SetActivity(ENPCActivity::Idle);
  }
  if(N->Activity==ENPCActivity::Meeting||N->Activity==ENPCActivity::Trade){
   N->ReleasePartner();if(auto* AI=Cast<AAIController>(N->GetController()))AI->StopMovement();N->SetActivity(ENPCActivity::Idle);
  }
 }
 for(auto Weak:E->Participants)if(auto* N=Weak.Get())N->SocialCooldown=Cooldown;
 GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("StreetEventEnded"),Id);
}
void UWorldEvents::ObserveEscape(APatrolEnemy* N) {
 if(!GetDefault<UGlobalData>()->bEnableStreetEvents||!N||N->StreetEventId)return;
 const float Now=GetWorld()->GetTimeSeconds();if(const float* Last=EscapeCooldown.Find(N);Last&&Now-*Last<30)return;
 if(BeginEvent(EStreetEvent::CivilianEscape,{N})){EscapeCooldown.Add(N,Now);Events.Last().bPerforming=true;Events.Last().bHasPerformed=true;Events.Last().PerformingSince=Now;Events.Last().ExpiresAt=Now+Events.Last().Duration;N->PanicTime=Events.Last().Duration;}
}
void UWorldEvents::ObserveCombat(APatrolEnemy* N,APatrolEnemy* Target) {
 if(!GetDefault<UGlobalData>()->bEnableStreetEvents||!N||!Target)return;
 if(!((N->IsGang()&&Target->NPCRole==ENPCRole::Police)||(Target->IsGang()&&N->NPCRole==ENPCRole::Police)))return;
 if(N->CrossfireCooldown>0||Target->CrossfireCooldown>0)return;
 if(N->StreetEventId||Target->StreetEventId)
 {
  // Reinforcing observers belong to the same occurrence and stop with it.
  auto* E=Events.FindByPredicate([&](const FStreetEventRecord& V){return V.Id==(N->StreetEventId?N->StreetEventId:Target->StreetEventId);});
  if(E&&E->bActive&&(E->Kind==EStreetEvent::GangShootout||E->Kind==EStreetEvent::MassiveGangWar))
   for(auto* Member:{N,Target})if(!Member->StreetEventId){Member->StreetEventId=E->Id;E->Participants.Add(Member);}
  return;
 }
 BeginEvent(EStreetEvent::GangShootout,{N,Target});
}
bool UWorldEvents::PrepareScheduledCast(EStreetEvent Kind)
{
 const auto* Rules=GetDefault<UGlobalData>();
 if((Kind==EStreetEvent::MassiveGangWar||Kind==EStreetEvent::GangMassacre)&&GetWorld()->GetTimeSeconds()<Rules->LargeEventEarliestSeconds)return false;
 const int32 PreparedCount=PreparedCast.Num();
 PreparedCast.RemoveAll([](const auto& Weak){auto* N=Weak.Get();return !N||N->Health<=0||N->NPCRole==ENPCRole::Injured||N->bCombat||N->StreetEventId;});
 if(PreparedCast.Num()!=PreparedCount)PreparedCast.Reset();
 // Do not accumulate new actors on repeated failed staging attempts.
 if(!PreparedCast.IsEmpty())return false;
 auto* PC=UGameplayStatics::GetPlayerController(this,0);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if(!PC||!PC->GetPawn()||!Nav)return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 TArray<ENPCRole> Roles;
 const bool Large=Kind==EStreetEvent::MassiveGangWar||Kind==EStreetEvent::GangMassacre;
 const int32 GangCount=Large||Kind==EStreetEvent::GangMeeting?3:Kind==EStreetEvent::SecretTransaction?2:1;
 for(int32 I=0;I<GangCount;++I)Roles.Add(ENPCRole::GangMember);
 if(Kind==EStreetEvent::GangShootout)Roles.Add(ENPCRole::Police);
 if(Kind==EStreetEvent::MassiveGangWar){Roles.Add(ENPCRole::Police);Roles.Add(ENPCRole::Police);}
 if(Kind==EStreetEvent::GangMassacre)for(int32 I=0;I<3;++I)Roles.Add(ENPCRole::Civilian);
 const auto* Capsule=GetDefault<APatrolEnemy>()->GetCapsuleComponent();
 const float HalfHeight=Capsule->GetScaledCapsuleHalfHeight();
 for(int32 Attempt=0;Attempt<48;++Attempt){
  FNavLocation Center;
  if(!Nav->GetRandomReachablePointInRadius(PC->GetPawn()->GetActorLocation(),3000.f,Center))continue;
  if(FVector::DistSquared2D(Center.Location,Eye)<FMath::Square(1200.f))continue;
  TArray<FVector> Positions;bool Valid=true;
  for(int32 I=0;I<Roles.Num();++I){
   FNavLocation Ground;const FVector Offset((I%3)*(Kind==EStreetEvent::GangShootout?650.f:180.f),(I/3)*500.f,0);
   if(!Nav->ProjectPointToNavigation(Center.Location+Offset,Ground,FVector(100,100,250))){Valid=false;break;}
   const FVector P=Ground.Location+FVector(0,0,HalfHeight+4.f);
   if(FVector::DotProduct(P-Eye,View.Vector())>=-250.f){Valid=false;break;}
   const auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),PC->GetPawn()->GetActorLocation(),Ground.Location,PC->GetPawn());
   if(!Path||!Path->IsValid()||Path->IsPartial()||GetWorld()->OverlapBlockingTestByChannel(P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),HalfHeight))){Valid=false;break;}
   Positions.Add(P);
  }
  if(!Valid)continue;
  for(int32 I=0;I<Roles.Num();++I){
   const FTransform T(FRotator::ZeroRotator,Positions[I]);
   auto* N=GetWorld()->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
   if(!N){for(auto Weak:PreparedCast)if(auto* Created=Weak.Get())Created->Destroy();PreparedCast.Reset();return false;}
   N->NPCRole=Roles[I];N->FinishSpawning(T);N->SocialCooldown=0;PreparedCast.Add(N);
  }
  return true;
 }
 return false;
}
bool UWorldEvents::TryStartEvent(EStreetEvent Kind) {
 if(Kind==EStreetEvent::CivilianEscape)return false; // Only actual danger creates an escape.
 const auto* Config=GetDefault<UGlobalData>();
 if(Kind==EStreetEvent::MassiveGangWar&&GetWorld()->GetTimeSeconds()<Config->LargeEventEarliestSeconds)return false;
 if(Kind==EStreetEvent::GangMassacre&&GetWorld()->GetTimeSeconds()<Config->LargeEventEarliestSeconds)return false;
 TArray<APatrolEnemy*> Gang,Police,Civilians;
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It){auto* N=*It;
  if(bStartingScheduled&&!PreparedCast.IsEmpty()&&!PreparedCast.Contains(N))continue;
  if(N->Health<=0||N->StreetEventId||N->SocialCooldown>0||N->Partner.IsValid()||N->AssaultTarget.IsValid()||N->RetaliationTarget.IsValid()||N->NPCRole==ENPCRole::Injured||N->Activity==ENPCActivity::WarnPlayer||N->bCombat||N->CameraReactionTime>0)continue;
  if(N->IsGang())Gang.Add(N);else if(N->NPCRole==ENPCRole::Police)Police.Add(N);else if(N->NPCRole==ENPCRole::Civilian||N->NPCRole==ENPCRole::Merchant)Civilians.Add(N);
 }
 if(Gang.IsEmpty())return false;
 // Randomize the anchor, then use a local cluster rather than teleporting actors.
 APatrolEnemy* Anchor=Gang[FMath::RandHelper(Gang.Num())];const FVector Center=Anchor->GetActorLocation();
 Gang.Sort([Center](const APatrolEnemy& A,const APatrolEnemy& B){return FVector::DistSquared(A.GetActorLocation(),Center)<FVector::DistSquared(B.GetActorLocation(),Center);});
 Police.Sort([Center](const APatrolEnemy& A,const APatrolEnemy& B){return FVector::DistSquared(A.GetActorLocation(),Center)<FVector::DistSquared(B.GetActorLocation(),Center);});
 Civilians.Sort([Center](const APatrolEnemy& A,const APatrolEnemy& B){return FVector::DistSquared(A.GetActorLocation(),Center)<FVector::DistSquared(B.GetActorLocation(),Center);});
 const bool Massacre=Kind==EStreetEvent::GangMassacre;
 const bool Meeting=Kind==EStreetEvent::GangMeeting,Trade=Kind==EStreetEvent::SecretTransaction,War=Kind==EStreetEvent::MassiveGangWar;
 const int32 GangCount=Meeting||War||Massacre?3:Trade?2:1,PoliceCount=Meeting||Trade||Massacre?0:War?2:1;
 if(Gang.Num()<GangCount||Police.Num()<PoliceCount||(Massacre&&Civilians.Num()<3))return false;
 const float Radius=War||Massacre?5000.f:Meeting?1800.f:Trade?1000.f:2200.f;
 TArray<APatrolEnemy*> People;
 for(int32 I=0;I<GangCount;++I){if(FVector::Dist2D(Center,Gang[I]->GetActorLocation())>Radius)return false;People.Add(Gang[I]);}
 for(int32 I=0;I<PoliceCount;++I){if(FVector::Dist2D(Center,Police[I]->GetActorLocation())>Radius)return false;People.Add(Police[I]);}
 if(Massacre)for(int32 I=0;I<3;++I){if(FVector::Dist2D(Center,Civilians[I]->GetActorLocation())>Radius)return false;People.Add(Civilians[I]);}
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
 else if(Massacre){for(int32 I=0;I<3;++I){Gang[I]->EscalationTarget=Civilians[I];Gang[I]->SetTarget(Civilians[I]);}}
 else {
  for(int32 I=0;I<PoliceCount;++I)Police[I]->SetTarget(Gang[I%GangCount]);
  if(Kind!=EStreetEvent::PoliceChase)for(int32 I=0;I<GangCount;++I)Gang[I]->SetTarget(Police[I%PoliceCount]);

 }
 return true;
}
void UWorldEvents::Tick(float DT) {
 if(!GetWorld()||!GetWorld()->HasBegunPlay())return;
 const float Now=GetWorld()->GetTimeSeconds();int32 Active=0;
 for(auto& E:Events){if(!E.bActive)continue;if(E.Kind!=EStreetEvent::CivilianEscape)++Active;
  int32 Living=0,Ready=0,Gang=0,Police=0,Civilian=0;bool Interrupted=false;
  for(auto Weak:E.Participants){auto* N=Weak.Get();if(!N||N->Health<=0)continue;++Living;
   if(E.Kind==EStreetEvent::MassiveGangWar&&!N->IsLiving(N->CombatTarget)){
    for(auto Other:E.Participants)if(auto* Target=Other.Get();Target&&N->IsLiving(Target)&&N->IsGang()!=Target->IsGang()){N->SetTarget(Target);break;}
   }
   Gang+=N->IsGang();Police+=N->NPCRole==ENPCRole::Police;Civilian+=N->IsLawProtected();
   if(E.Kind==EStreetEvent::GangMassacre&&N->IsGang()&&!N->IsCombatTargetAlive(N->CombatTarget)){for(auto Other:E.Participants)if(auto* Victim=Other.Get();Victim&&Victim->Health>0&&Victim->IsLawProtected()){N->EscalationTarget=Victim;N->SetTarget(Victim);break;}}
   switch(E.Kind){
   case EStreetEvent::GangMeeting:Interrupted|=N->Activity!=ENPCActivity::Meeting;Ready+=N->Activity==ENPCActivity::Meeting&&FVector::Dist2D(N->GetActorLocation(),N->EventDestination)<95;break;
   case EStreetEvent::SecretTransaction:Interrupted|=N->Activity!=ENPCActivity::Trade;Ready+=N->Activity==ENPCActivity::Trade&&N->bSocialArrived;break;
   case EStreetEvent::CivilianEscape:Ready+=N->Activity==ENPCActivity::Flee;break;
   default:Ready+=N->bCombat&&N->IsCombatTargetAlive(N->CombatTarget)&&N->CanSeeActor(N->CombatTarget,false);break;
   }
  }
  const bool Social=E.Kind==EStreetEvent::GangMeeting||E.Kind==EStreetEvent::SecretTransaction;
  const int32 Required=Social?E.Participants.Num():(E.Kind==EStreetEvent::MassiveGangWar||E.Kind==EStreetEvent::GangMassacre)?3:1;
  if(Ready>=Required){
   E.bPerforming=true;
   if(!E.bHasPerformed){E.bHasPerformed=true;E.PerformingSince=Now;E.ExpiresAt=Now+E.Duration;}
  }
  // Preparation never scores. Stop social scenes when interrupted; never override self-defense.
  const bool End=Living==0||Interrupted||(Social&&Living<E.Participants.Num())||
   (E.Kind==EStreetEvent::CivilianEscape&&Ready==0)||
   (!Social&&E.Kind!=EStreetEvent::CivilianEscape&&(Gang==0||(E.Kind==EStreetEvent::GangMassacre?Civilian==0:Police==0)))||
   (!E.bHasPerformed&&Now-E.Started>FMath::Max(1.f,GetDefault<UGlobalData>()->EventPreparationSeconds))||
   (E.bHasPerformed&&Now>=E.ExpiresAt);
  if(End){
#if WITH_EDITOR
   E.DebugEndReason=E.bHasPerformed&&Now>=E.ExpiresAt?TEXT("持续时间结束"):
    Living==0?TEXT("无存活参与者"):Interrupted?TEXT("行为被打断"):
    !E.bHasPerformed&&Now-E.Started>FMath::Max(1.f,GetDefault<UGlobalData>()->EventPreparationSeconds)?TEXT("准备超时，重新安排"):
    E.Kind==EStreetEvent::CivilianEscape?TEXT("逃跑已停止"):TEXT("参与阵营不足");
#endif
   EndEvent(E.Id);
  }
  else if(!Social&&Ready<Required)E.bPerforming=false;
 }
 const auto* C=GetDefault<UGlobalData>();if(!bAutomatic||!C->bEnableStreetEvents)return;
 // Keep the selected event across retries; capacity and staging failures do not reroll it.
 if(Active>=3)return;
 if(PendingEvents.IsEmpty()){
  NextAttempt-=DT;if(NextAttempt>0)return;
  NextAttempt=FMath::Max(1.f,C->EventAttemptSeconds);
  PendingEvents.Add(SelectScheduledEvent(C->RareEventChance,FMath::FRand(),FMath::RandHelper(3)));
  RetryAttempt=0;
 }
 RetryAttempt-=DT;if(RetryAttempt>0)return;
 RetryAttempt=FMath::Max(1.f,C->EventRetrySeconds);
 const auto Kind=PendingEvents[0];
 TGuardValue<bool> Starting(bStartingScheduled,true);
 if(TryStartEvent(Kind)||(PrepareScheduledCast(Kind)&&TryStartEvent(Kind))){
  PendingEvents.RemoveAt(0);PreparedCast.Reset();NextAttempt=FMath::Max(1.f,C->EventAttemptSeconds);
 }

}
EStreetEvent UWorldEvents::SelectScheduledEvent(float RareChance,float Roll,int32 Choice)
{
 const EStreetEvent Rare[]={EStreetEvent::SecretTransaction,EStreetEvent::GangMassacre,EStreetEvent::MassiveGangWar};
 // Legacy chase is not in the approved five-event design. Keep its enum for asset compatibility only.
 const EStreetEvent Common[]={EStreetEvent::GangMeeting,EStreetEvent::GangShootout,EStreetEvent::GangShootout};
 return Roll<FMath::Clamp(RareChance,0.f,1.f)?Rare[FMath::Clamp(Choice,0,2)]:Common[FMath::Clamp(Choice,0,2)];
}
