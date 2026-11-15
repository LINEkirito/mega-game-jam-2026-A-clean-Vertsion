#include "ExtractionPoint.h"
#include "ExtractionConvoyCar.h"
#include "Combat/PatrolEnemy.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"
#include "Data/GlobalData.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UExtractionSubsystem::IsResponder(const APatrolEnemy* Enemy) const
{return Responders.ContainsByPredicate([Enemy](const auto& N){return N.Get()==Enemy;});}
bool UExtractionSubsystem::AllowsPlayerTarget(const APatrolEnemy* Enemy,const AActor* Target) const
{return !GetExtractingPlayer()||Target!=GetExtractingPlayer()||IsResponder(Enemy)||(Enemy&&Enemy->NPCRole==ENPCRole::Police&&Enemy->PriorityOffender.Get()==Target);}
int32 UExtractionSubsystem::GetLivingResponders() const
{int32 Count=0;for(auto N:Responders)if(N.IsValid()&&N->Health>0&&N->NPCRole!=ENPCRole::Injured)++Count;return Count;}
void UExtractionSubsystem::RegisterPassenger(APatrolEnemy* N)
{
 auto* Player=GetExtractingPlayer();if(!IsValid(N)||!Player){if(N)N->Destroy();return;}
 if(GetLivingResponders()==0)
 {
  // Fixed for the whole wave: everyone searches the same flank without orbiting the car.
  ResponseOrigin=Player->GetActorLocation();
  ResponseForward=(N->GetActorLocation()-ResponseOrigin).GetSafeNormal2D();
  if(ResponseForward.IsNearlyZero())ResponseForward=FVector::ForwardVector;
  ResponseSide=FMath::RandBool()?1.f:-1.f;
 }
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
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It)if(!AllowsPlayerTarget(*It,Player)&&It->CombatTarget==Player)
 {
  It->CombatTarget=nullptr;It->PriorityOffender.Reset();It->SetActivity(ENPCActivity::Idle);
  if(auto* AI=Cast<AAIController>(It->GetController()))AI->StopMovement();
 }
 // Departing empty cars do not hold the next wave; still allow only one delivering car.
 if(GetLivingResponders()>0||Cars.ContainsByPredicate([](const auto& C){return C.IsValid()&&C->IsDelivering();}))
 {SpawnRetry=0;return;}
 SpawnRetry-=DT;if(SpawnRetry>0)return;SpawnRetry=.1f;
 const int32 Desired=1; // One car per wave keeps extraction combat manageable.
 for(int32 I=0;I<Desired;++I)
 {
  FVector Start,Stop;FRotator Rotation;
  if(!AExtractionConvoyCar::FindArrival(GetWorld(),PC,Start,Stop,Rotation))break;
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
  auto* Car=GetWorld()->SpawnActor<AExtractionConvoyCar>(Start+FVector(0,0,76),Rotation,Params);
  if(!Car)break;
  Car->Initialize(PC,Stop,Start);Cars.Add(Car);++WaveNumber;
 }
}
bool APatrolEnemy::TickExtractionResponse(float DT)
{
 auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>();
 auto* Player=Session?Session->GetExtractingPlayer():nullptr;
 if(!Player||Player->Health<=0||Health<=0||NPCRole==ENPCRole::Injured||!Session->IsResponder(this))
 {
  bRespondingToExtraction=false;bExtractionRetreating=false;return false;
 }
 if(!bRespondingToExtraction)
 {
  ReleasePartner();AssaultTarget.Reset();RetaliationTarget.Reset();EscalationTarget.Reset();
  GangAttackLookTarget.Reset();GangAttackLookUntil=0;CameraReactionTime=0;
  bAimWarning=false;WarningRemaining=0;SpeechTime=0;Speech->SetVisibility(false);MoveRetry=0;
  bRespondingToExtraction=true;
 }
 if(CombatTarget!=Player)SetTarget(Player);
 // Keep the target through lost sight; TickCombat enforces extraction standoff even when occluded.
 LastKnown=Player->GetActorLocation();LostTime=0;TickCombat(DT);return true;
}

bool UExtractionSubsystem::IsResponsePathSafe(const TArray<FVector>& Points,float MinimumDistance) const
{
 const auto* Player=GetExtractingPlayer();if(!Player||Points.IsEmpty())return false;
 const FVector Target=Player->GetActorLocation();
 const float Limit=FMath::Clamp(GetDefault<UGlobalData>()->ExtractionFiringSectorAngle,10.f,40.f)+5.f;
 FVector Previous=Points[0];
 for(const FVector& P:Points)
 {
  if(FMath::Abs(FMath::FindDeltaAngleDegrees(ResponseForward.Rotation().Yaw,(P-ResponseOrigin).Rotation().Yaw))>Limit)return false;
  const FVector Closest=FMath::ClosestPointOnSegment(FVector(Target.X,Target.Y,0),FVector(Previous.X,Previous.Y,0),FVector(P.X,P.Y,0));
  if(FVector::Dist2D(Closest,Target)<MinimumDistance)return false;
  Previous=P;
 }
 return true;
}
bool UExtractionSubsystem::FindResponseFiringAngle(APatrolEnemy* Enemy,FVector& Destination) const
{
 auto* Player=GetExtractingPlayer();
 if(!Player||!IsValid(Enemy)||!IsResponder(Enemy))return false;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 const auto* Rules=GetDefault<UGlobalData>();
 const float Limit=FMath::Clamp(Rules->ExtractionFiringSectorAngle,10.f,40.f);
 const FVector Start=Enemy->GetActorLocation(),Target=Player->GetActorLocation();
 const FVector Radial=Start-ResponseOrigin;
 const float Angle=FMath::FindDeltaAngleDegrees(ResponseForward.Rotation().Yaw,Radial.Rotation().Yaw)*ResponseSide;
 // Do not rebase on the moving player or on the last destination: the sector must never creep around behind cover.
 if(Angle>=Limit-3.f||Angle< -Limit-5.f)return false;
 const float Radius=FMath::Max(Radial.Size2D(),FMath::Max(300.f,Rules->ExtractionCombatDistance));
 const float MinDistance=FMath::Min(FMath::Max(100.f,Rules->ExtractionRetreatDistance),FVector::Dist2D(Start,Target)-30.f);
 auto SignedAngle=[this](const FVector& P)
 {return FMath::FindDeltaAngleDegrees(ResponseForward.Rotation().Yaw,(P-ResponseOrigin).Rotation().Yaw)*ResponseSide;};
 bool Found=false;
 for(float Step:{10.f,20.f,30.f})
 {
  const float Next=FMath::Min(Angle+Step,Limit);FNavLocation Ground;
  const FVector Desired=ResponseOrigin+ResponseForward.RotateAngleAxis(Next*ResponseSide,FVector::UpVector)*Radius;
  if(!Nav->ProjectPointToNavigation(Desired,Ground,FVector(70,70,250)))continue;
  const float Projected=SignedAngle(Ground.Location);
  if(Projected<Angle+2.f||Projected>Limit||FVector::Dist2D(Ground.Location,Target)<FMath::Max(300.f,Rules->ExtractionCombatDistance)-75.f)continue;
  auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),Start,Ground.Location,Enemy);
  if(!Path||!Path->IsValid()||Path->IsPartial())continue;
  bool Safe=true;FVector Previous=Start;
  for(const FVector& P:Path->PathPoints)
  {
   const float A=SignedAngle(P);
   // Check entire path segments, not just endpoints, to reject shortcuts toward the player.
   const FVector Closest=FMath::ClosestPointOnSegment(FVector(Target.X,Target.Y,0),FVector(Previous.X,Previous.Y,0),FVector(P.X,P.Y,0));
   if(A<Angle-2.f||A>Limit+1.f||FVector::Dist2D(Closest,Target)<MinDistance){Safe=false;break;}
   Previous=P;
  }
  if(!Safe)continue;
  bool Occupied=false;for(auto Other:Responders)if(Other.IsValid()&&Other.Get()!=Enemy&&FVector::Dist2D(Other->GetActorLocation(),Ground.Location)<110.f){Occupied=true;break;}
  if(Occupied)continue;
  if(!Found){Destination=Ground.Location;Found=true;}
  FCollisionQueryParams Query(SCENE_QUERY_STAT(ExtractionFiringAngle),true,Enemy);if(Enemy->HeldWeapon)Query.AddIgnoredActor(Enemy->HeldWeapon);
  FHitResult Hit;
  if(!GetWorld()->LineTraceSingleByChannel(Hit,Ground.Location+FVector(0,0,140),Target+FVector(0,0,25),ECC_Visibility,Query)||Hit.GetActor()==Player)
  {Destination=Ground.Location;return true;}
 }
 return Found;
}

void APatrolEnemy::TickExtractionMovement(bool Seen)
{
  const auto* Rules=GetDefault<UGlobalData>();
  const float HoldDistance=FMath::Max(300.f,Rules->ExtractionCombatDistance);
  const float RetreatDistance=FMath::Clamp(Rules->ExtractionRetreatDistance,100.f,HoldDistance-100.f);
  LastKnown=CombatTarget->GetActorLocation();LostTime=0;
  bool ClearShot=Seen;
  if(ClearShot&&HeldWeapon)
  {
   FCollisionQueryParams Query(SCENE_QUERY_STAT(ExtractionMuzzleSight),true,this);Query.AddIgnoredActor(HeldWeapon);
   FHitResult Hit;ClearShot=!GetWorld()->LineTraceSingleByChannel(Hit,HeldWeapon->MuzzleLocation(),LastKnown+FVector(0,0,25),ECC_Visibility,Query)||Hit.GetActor()==CombatTarget;
  }
  const float Distance=FVector::Dist2D(GetActorLocation(),LastKnown);
  GetCharacterMovement()->MaxWalkSpeed=Rules->NPCCombatAdvanceSpeed;
  if(Distance<RetreatDistance)bExtractionRetreating=true;
  // MoveTo includes the capsule radius in its acceptance; leave retreat before that stop band.
  if(Distance>=HoldDistance-100.f)bExtractionRetreating=false;
  auto Stop=[this](){if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;};
  auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>();
  if(bExtractionRetreating)
  {
   if(MoveRetry<=0)
   {
    MoveRetry=.5f;Stop();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FVector Away=(GetActorLocation()-LastKnown).GetSafeNormal2D();
    if(Away.IsNearlyZero())Away=-GetActorForwardVector();
    for(float Angle:{0.f,45.f,-45.f,90.f,-90.f})
    {
     FNavLocation Ground;
     if(!Nav||!Nav->ProjectPointToNavigation(LastKnown+Away.RotateAngleAxis(Angle,FVector::UpVector)*HoldDistance,Ground,FVector(100,100,250)))continue;
     if(FVector::Dist2D(Ground.Location,LastKnown)<HoldDistance-75.f)continue;
     auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),GetActorLocation(),Ground.Location,this);
     if(!Path||!Path->IsValid()||Path->IsPartial())continue;
     bool Safe=true;for(const FVector& Point:Path->PathPoints)
      if(FVector::Dist2D(Point,LastKnown)<Distance-30.f){Safe=false;break;}
     if(Safe&&Session&&Session->IsResponsePathSafe(Path->PathPoints,Distance-30.f)&&MoveTo(Ground.Location,40.f))break;
    }
   }
  }
  else if(Distance>HoldDistance+200.f)
  {
   if(MoveRetry<=0)
   {
    MoveRetry=.8f;Stop();FNavLocation Ground;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const FVector Goal=LastKnown+(GetActorLocation()-LastKnown).GetSafeNormal2D()*HoldDistance;
    if(Nav&&Nav->ProjectPointToNavigation(Goal,Ground,FVector(80,80,250)))
    {
     auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),GetActorLocation(),Ground.Location,this);
     if(Path&&Path->IsValid()&&!Path->IsPartial()&&Session&&Session->IsResponsePathSafe(Path->PathPoints,RetreatDistance))MoveTo(Ground.Location,35.f);
    }
   }
  }
  else if(!ClearShot)
  {
   if(MoveRetry<=0)
   {
    MoveRetry=.8f;FVector AngleDestination;
    if(!Session||!Session->FindResponseFiringAngle(this,AngleDestination)||!MoveTo(AngleDestination,35.f))Stop();
   }
  }
  else Stop();
}
