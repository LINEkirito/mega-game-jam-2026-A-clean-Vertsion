#include "ExtractionPoint.h"
#include "ExtractionConvoyCar.h"
#include "Combat/PatrolEnemy.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"

bool UExtractionSubsystem::IsResponder(const APatrolEnemy* Enemy) const
{return Responders.ContainsByPredicate([Enemy](const auto& N){return N.Get()==Enemy;});}
bool UExtractionSubsystem::AllowsPlayerTarget(const APatrolEnemy* Enemy,const AActor* Target) const
{return !GetExtractingPlayer()||Target!=GetExtractingPlayer()||IsResponder(Enemy);}
int32 UExtractionSubsystem::GetLivingResponders() const
{int32 Count=0;for(auto N:Responders)if(N.IsValid()&&N->Health>0)++Count;return Count;}
void UExtractionSubsystem::RegisterPassenger(APatrolEnemy* N)
{
 auto* Player=GetExtractingPlayer();if(!IsValid(N)||!Player){if(N)N->Destroy();return;}
 Responders.AddUnique(N);N->SetTarget(Player);N->ShotTime=0;N->MoveRetry=0;
 // All passengers attack on exit; no player-observation prerequisite.
 N->WeaponCarry=ENPCWeaponCarry::Drawn;N->SetMotion();N->UpdateWeaponPose();
}
void UExtractionSubsystem::StopResponse()
{
 for(auto Weak:Responders)if(auto* N=Weak.Get();N&&N->Health>0)N->Destroy();
 for(auto Weak:Cars)if(auto* C=Weak.Get())C->LeaveWithoutUnloading();
 Responders.Empty();SpawnRetry=0;WaveNumber=0;
 // Departing cars remain tracked, preventing a restarted extraction from stacking convoys.
}
void UExtractionSubsystem::UpdateResponse(float DT)
{
 auto* Player=GetExtractingPlayer();if(!Player||Player->Health<=0)return;
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC||!GetWorld()->GetAuthGameMode())return;
 Responders.RemoveAll([](const auto& N){return !N.IsValid()||N->Health<=0;});
 Cars.RemoveAll([](const auto& C){return !C.IsValid();});
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It)if(!IsResponder(*It)&&It->CombatTarget==Player)
 {
  It->CombatTarget=nullptr;It->PriorityOffender.Reset();It->SetActivity(ENPCActivity::Idle);
  if(auto* AI=Cast<AAIController>(It->GetController()))AI->StopMovement();
 }
 // A partial kill never replenishes this wave. Cars must also finish leaving first.
 if(!Responders.IsEmpty()||!Cars.IsEmpty())return;
 SpawnRetry-=DT;if(SpawnRetry>0)return;SpawnRetry=1.f;
 const int32 Desired=FMath::RandRange(1,2);
 for(int32 I=0;I<Desired;++I)
 {
  FVector Start,Stop;FRotator Rotation;
  if(!AExtractionConvoyCar::FindArrival(GetWorld(),PC,Start,Stop,Rotation))break;
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
  auto* Car=GetWorld()->SpawnActor<AExtractionConvoyCar>(Start+FVector(0,0,76),Rotation,Params);
  if(!Car)break;
  Car->Initialize(PC,Stop,Start);Cars.Add(Car);
 }
 if(!Cars.IsEmpty())++WaveNumber;
}
bool APatrolEnemy::TickExtractionResponse(float DT)
{
 auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>();
 auto* Player=Session?Session->GetExtractingPlayer():nullptr;
 if(!Player||Player->Health<=0||Health<=0||!Session->IsResponder(this))
 {
  bRespondingToExtraction=false;return false;
 }
 if(!bRespondingToExtraction)
 {
  ReleasePartner();AssaultTarget.Reset();RetaliationTarget.Reset();EscalationTarget.Reset();
  GangAttackLookTarget.Reset();GangAttackLookUntil=0;CameraReactionTime=0;
  bAimWarning=false;WarningRemaining=0;SpeechTime=0;Speech->SetVisibility(false);MoveRetry=0;
  bRespondingToExtraction=true;
 }
 if(CombatTarget!=Player)SetTarget(Player);
 // Keep pursuing through loss of sight; TickCombat still checks LOS before firing.
 LastKnown=Player->GetActorLocation();LostTime=0;TickCombat(DT);return true;
}
