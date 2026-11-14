#include "ExtractionPoint.h"
#include "Combat/PatrolEnemy.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/GlobalData.h"
#include "Engine/World.h"

bool APatrolEnemy::TickExtractionResponse(float DT)
{
 auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>();
 auto* Player=Session?Session->GetExtractingPlayer():nullptr;
 if(!Player||Player->Health<=0||Health<=0||IsProtectedPerson()||(!IsGang()&&NPCRole!=ENPCRole::Police))
 {
  // An enemy that actually acquired the player keeps its normal combat memory.
  // Unaware responders return to ordinary AI when the extraction is cancelled.
  if(bRespondingToExtraction&&Activity==ENPCActivity::Investigate){InvestigationTime=0;SetActivity(ENPCActivity::Idle);MoveRetry=0;}
  bRespondingToExtraction=false;return false;
 }
 if(!bRespondingToExtraction)
 {
  ReleasePartner();AssaultTarget.Reset();RetaliationTarget.Reset();EscalationTarget.Reset();
  GangAttackLookTarget.Reset();GangAttackLookUntil=0;CameraReactionTime=0;
  bAimWarning=false;WarningRemaining=0;SpeechTime=0;Speech->SetVisibility(false);MoveRetry=0;
  bRespondingToExtraction=true;
 }
 const bool Acquired=CombatTarget==Player;
 if(CanSeeActor(Player,!Acquired))
 {
  if(!Acquired||Activity!=ENPCActivity::Combat){SetTarget(Player);ShotTime=0;}
  TickCombat(DT);return true;
 }
 // Respond to the extraction signal, but never fire through cover or bypass sight range.
 CombatTarget=nullptr;InvestigationLocation=Player->GetActorLocation();InvestigationTime=1.f;
 SetActivity(ENPCActivity::Investigate);
 GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCSearchSpeed;
 if(MoveRetry<=0){MoveRetry=.35f;MoveTo(InvestigationLocation,90.f);}
 Face(InvestigationLocation,DT);return true;
}
