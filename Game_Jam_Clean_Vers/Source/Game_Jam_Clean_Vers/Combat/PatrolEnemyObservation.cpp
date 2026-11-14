#include "PatrolEnemy.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

bool APatrolEnemy::TickGangAttackObservation(float DT)
{
 if(!GangAttackLookTarget.IsValid())return false;
 if(!IsGang()||!IsLiving(GangAttackLookTarget.Get())||IsCombatTargetAlive(CombatTarget))
 {
  GangAttackLookTarget.Reset();GangAttackLookUntil=0.f;return false;
 }
 if(GetWorld()->GetTimeSeconds()>=GangAttackLookUntil)
 {
  GangAttackLookTarget.Reset();GangAttackLookUntil=0.f;
  // Resume the existing patrol/social/warning state on the following tick.
  // Expiring an observation is never a reason to shoot.
  return true;
 }
 if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
 GetCharacterMovement()->StopMovementImmediately();bMoving=false;
 // Turn towards the reported position first, then follow only a visible player.
 if(CanSeeActor(GangAttackLookTarget.Get(),false))GangAttackLookLocation=GangAttackLookTarget->GetActorLocation();
 Face(GangAttackLookLocation,DT);
 return true;
}
