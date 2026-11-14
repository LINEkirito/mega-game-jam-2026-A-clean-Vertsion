#include "ExtractionConvoyCar.h"
#include "ExtractionPoint.h"
#include "Combat/PatrolEnemy.h"
#include "Combat/TacticalWeapon.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Data/GlobalData.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"

namespace
{
 const FVector CarHalf(250,112,65);
 const FVector BodyOffset(0,0,76);
 void IgnoreBodies(UWorld* W,FCollisionQueryParams& Q)
 {
  for(TActorIterator<APatrolEnemy> It(W);It;++It)if(It->Health<=0){Q.AddIgnoredActor(*It);if(It->HeldWeapon)Q.AddIgnoredActor(It->HeldWeapon);}
  for(TActorIterator<ACombatAmmoPickup> It(W);It;++It)Q.AddIgnoredActor(*It);
 }
 bool RoadGround(UWorld* W,const FVector& P,FVector& Ground,const AActor* Ignore=nullptr)
 {
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(ConvoyGround),true,Ignore);IgnoreBodies(W,Q);
  if(!W->LineTraceSingleByChannel(H,P+FVector(0,0,200),P-FVector(0,0,400),ECC_WorldStatic,Q)||H.ImpactNormal.Z<.95f)return false;
  Ground=H.ImpactPoint+FVector(0,0,2);return true;
 }
}
UStaticMeshComponent* AExtractionConvoyCar::AddPart(const TCHAR* Name,const TCHAR* Part,const FVector& Location,const FRotator& Rotation)
{
 auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(VisualRoot);
 C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Game/Luxury_Sedan_1_Pack/Meshes/Parts/SM_Luxury_Sedan_1_"))+Part)));
 C->SetRelativeLocationAndRotation(Location,Rotation);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
 if(C->GetStaticMesh())for(int32 I=0;I<C->GetNumMaterials();++I)
  if(auto* M=C->GetMaterial(I);M&&M->GetName().Contains(TEXT("Color")))C->SetMaterial(I,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Extraction/Convoy/M_ConvoyBlack")));
 return C;
}
AExtractionConvoyCar::AExtractionConvoyCar()
{
 PrimaryActorTick.bCanEverTick=true;
 CollisionBody=CreateDefaultSubobject<UBoxComponent>(TEXT("CarBody"));SetRootComponent(CollisionBody);
 CollisionBody->SetBoxExtent(CarHalf);CollisionBody->SetCollisionProfileName(TEXT("BlockAllDynamic"));CollisionBody->SetGenerateOverlapEvents(false);CollisionBody->SetCanEverAffectNavigation(false);
 VisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("CarVisual"));VisualRoot->SetupAttachment(CollisionBody);VisualRoot->SetRelativeLocation(-BodyOffset);
 AddPart(TEXT("Body"),TEXT("Body"),FVector::ZeroVector);
 Doors.Add(AddPart(TEXT("DoorFL"),TEXT("Door_FL"),FVector(90.9671,-91.7858,76.5832)));
 Doors.Add(AddPart(TEXT("DoorFR"),TEXT("Door_FR"),FVector(90.9671,91.7858,76.5832)));
 Doors.Add(AddPart(TEXT("DoorRL"),TEXT("Door_RL"),FVector(-15.8824,-91.531,79.5173)));
 Doors.Add(AddPart(TEXT("DoorRR"),TEXT("Door_RR"),FVector(-15.8824,91.531,79.5173)));
 AddPart(TEXT("Hood"),TEXT("Hood"),FVector(99.6386,0,99.9992));
 AddPart(TEXT("Trunk"),TEXT("Trunk"),FVector(-128.06,0,130.233));
 AddPart(TEXT("TrunkCover"),TEXT("Trunk_Cover"),FVector(-136.659,0,103.736));
 AddPart(TEXT("Steering"),TEXT("Steering_Wheel"),FVector(49.6812,-40.918,84.8244),FRotator(65,0,0));
 Wheels.Add(AddPart(TEXT("WheelFL"),TEXT("Wheel"),FVector(158.141,-80.368,34.2088)));
 Wheels.Add(AddPart(TEXT("WheelFR"),TEXT("Wheel"),FVector(158.141,80.6993,34.2088),FRotator(0,180,0)));
 Wheels.Add(AddPart(TEXT("WheelRL"),TEXT("Wheel"),FVector(-130.519,-80.368,33.4457)));
 Wheels.Add(AddPart(TEXT("WheelRR"),TEXT("Wheel"),FVector(-129.966,80.6993,34.2088),FRotator(0,180,0)));
 for(int32 I=0;I<5;++I)
 {
  auto* C=CreateDefaultSubobject<USkeletalMeshComponent>(*FString::Printf(TEXT("Rider%d"),I));C->SetupAttachment(VisualRoot);
  C->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Combat/NPC/Characters/NPC_GangMember")));
  C->SetRelativeLocationAndRotation(Seat(I),FRotator(0,-90,0));C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
  C->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Riders.Add(C);
 }
 DriverIdle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Extraction/Convoy/Animations/AS_Driver_Idle"));
 PassengerIdle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Extraction/Convoy/Animations/AS_Passenger_Idle"));
 ExitLeft=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Extraction/Convoy/Animations/AS_Car_GetOut_Driver"));
 ExitRight=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Extraction/Convoy/Animations/AS_Car_GetOut_Passenger"));
}
FVector AExtractionConvoyCar::Seat(int32 I) const
{
 // Four passengers plus a dedicated driver who takes the car away.
 static const FVector Seats[]={FVector(12,44,0),FVector(-83,-44,0),FVector(-83,44,0),FVector(-83,0,0),FVector(12,-44,0)};
 return Seats[FMath::Clamp(I,0,4)];
}
int32 AExtractionConvoyCar::DoorForSeat(int32 I) const {return I==0?1:I==1?2:3;}
void AExtractionConvoyCar::Initialize(APlayerController* PC,const FVector& Stop,const FVector& Start)
{
 PlayerController=PC;StopLocation=Stop+BodyOffset;EntryLocation=Start+BodyOffset;
 for(int32 I=0;I<Riders.Num();++I)Riders[I]->PlayAnimation(I==4?DriverIdle.Get():PassengerIdle.Get(),true);
}
bool AExtractionConvoyCar::IsInFront(APlayerController* PC,const FVector& L)
{
 if(!PC||!PC->GetPawn())return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 const FVector Delta=L-PC->GetPawn()->GetActorLocation();
 return FVector::DotProduct(Delta.GetSafeNormal2D(),FRotator(0,View.Yaw,0).Vector())>=.866f&&Delta.Size2D()>=900.f&&Delta.Size2D()<=1650.f;
}
bool AExtractionConvoyCar::IsHiddenFromPlayer(UWorld* W,APlayerController* PC,const FVector& L,const FRotator& R,const AActor* Ignore)
{
 if(!W||!PC||!PC->GetPawn())return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 const float Fov=PC->PlayerCameraManager?PC->PlayerCameraManager->GetFOVAngle():90.f;
 const float Cos=FMath::Cos(FMath::DegreesToRadians(Fov*.5f+10.f));
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ConvoyHidden),true,Ignore);Q.AddIgnoredActor(PC->GetPawn());
 TArray<AActor*> Attached;PC->GetPawn()->GetAttachedActors(Attached,true,true);Q.AddIgnoredActors(Attached);
 for(float X:{-250.f,0.f,250.f})for(float Y:{-112.f,0.f,112.f})for(float Z:{25.f,145.f})
 {
  const FVector P=L+R.RotateVector(FVector(X,Y,Z));const FVector Delta=P-Eye;
  if(FVector::DotProduct(Delta.GetSafeNormal2D(),FRotator(0,View.Yaw,0).Vector())<Cos)continue;
  FHitResult H;if(!W->LineTraceSingleByChannel(H,Eye,P,ECC_Visibility,Q))return false;
  if(H.Distance>=Delta.Size()-5)return false;
 }
 return true;
}
bool AExtractionConvoyCar::ClearRoad(UWorld* W,const FVector& Start,const FVector& End,const AActor* Ignore)
{
 if(!W)return false;
 const FQuat R=(End-Start).Rotation().Quaternion();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ConvoyRoad),false,Ignore);IgnoreBodies(W,Q);FHitResult H;
 if(W->SweepSingleByChannel(H,Start+BodyOffset,End+BodyOffset,R,ECC_WorldStatic,FCollisionShape::MakeBox(CarHalf+FVector(8,8,0)),Q))return false;
 const float Length=FVector::Dist2D(Start,End);
 FVector Previous=Start;
 const int32 Segments=FMath::Max(1,FMath::CeilToInt(Length/90.f));
 for(int32 Segment=0;Segment<=Segments;++Segment)
 {
  const FVector P=FMath::Lerp(Start,End,float(Segment)/Segments);
  FVector Ground;if(!RoadGround(W,P,Ground,Ignore)||FMath::Abs(Ground.Z-P.Z)>25)return false;
  // Follow the same ground height as runtime movement; a level sweep can miss a curb at a dip.
  if(W->SweepSingleByChannel(H,Previous+BodyOffset,Ground+BodyOffset,R,ECC_WorldStatic,FCollisionShape::MakeBox(CarHalf+FVector(8,8,0)),Q))return false;
  Previous=Ground;
 }
 return true;
}
bool AExtractionConvoyCar::FindArrival(UWorld* W,APlayerController* PC,FVector& Start,FVector& Stop,FRotator& Rotation)
{
 if(!W||!PC||!PC->GetPawn())return false;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);if(!Nav)return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 for(float Radius:{1200.f,1450.f,1050.f})for(float Offset:{0.f,12.f,-12.f,24.f,-24.f})
 {
  const FVector Probe=PC->GetPawn()->GetActorLocation()+FRotator(0,View.Yaw+Offset,0).Vector()*Radius;
  FVector Ground;if(!RoadGround(W,Probe,Ground)||!IsInFront(PC,Ground))continue;
  // Parking must be visible from the player, not merely on the forward side of a wall.
  FCollisionQueryParams SightQuery(SCENE_QUERY_STAT(ConvoyParkingSight),true,PC->GetPawn());
  TArray<AActor*> Attached;PC->GetPawn()->GetAttachedActors(Attached,true,true);SightQuery.AddIgnoredActors(Attached);
  FHitResult SightHit;if(W->LineTraceSingleByChannel(SightHit,Eye,Ground+FVector(0,0,110),ECC_Visibility,SightQuery))continue;
  FNavLocation NavPoint;if(!Nav->ProjectPointToNavigation(Ground,NavPoint,FVector(70,70,100)))continue;
  bool TooClose=false;for(TActorIterator<AExtractionConvoyCar> It(W);It;++It)
   if(FVector::Dist2D(Ground,It->StopLocation-BodyOffset)<560){TooClose=true;break;}
  if(TooClose)continue;
  // Most streets are axis aligned, with diagonals available for open intersections.
  for(int32 Heading=0;Heading<16;++Heading)
  {
   const FRotator Yaw(0,Heading*22.5f,0);const FVector Direction=Yaw.Vector();
   bool ExitsClear=true;FCollisionQueryParams ExitQuery(SCENE_QUERY_STAT(ConvoyExit));IgnoreBodies(W,ExitQuery);
   for(const FVector Local:{FVector(-25,170,0),FVector(-120,-170,0),FVector(-120,170,0)})
   {
    FVector ExitGround;const FVector E=Ground+Yaw.RotateVector(Local);
    if(!RoadGround(W,E,ExitGround)||FMath::Abs(ExitGround.Z-Ground.Z)>25||
     W->OverlapBlockingTestByChannel(ExitGround+FVector(0,0,95),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(45,88),ExitQuery)){ExitsClear=false;break;}
   }
   if(!ExitsClear)continue;
   for(float Distance:{2600.f,4000.f,5500.f})
   {
    FVector S;if(!RoadGround(W,Ground-Direction*Distance,S)||FMath::Abs(S.Z-Ground.Z)>25)continue;
    if(!IsHiddenFromPlayer(W,PC,S,Yaw)||!ClearRoad(W,S,Ground))continue;
    Start=S;Stop=Ground;Rotation=Yaw;return true;
   }
  }
 }
 return false;
}
bool AExtractionConvoyCar::MoveAlongRoad(float DT,const FVector& Target,bool Brake)
{
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It)if(It->Health<=0)CollisionBody->IgnoreActorWhenMoving(*It,true);
 for(TActorIterator<ACombatAmmoPickup> It(GetWorld());It;++It)CollisionBody->IgnoreActorWhenMoving(*It,true);
 const float Distance=FVector::Dist2D(GetActorLocation(),Target);
 const float MaxSpeed=GetDefault<UGlobalData>()->ConvoySpeed;
 const float Desired=Brake?FMath::Min(MaxSpeed,FMath::Sqrt(2.f*650.f*FMath::Max(0.f,Distance-5.f))):MaxSpeed;
 Speed=FMath::FInterpConstantTo(Speed,Desired,DT,Brake?900.f:450.f);
 const FVector Direction=(Target-GetActorLocation()).GetSafeNormal2D();
 const float Step=FMath::Min(Distance,Speed*DT);
 FVector Next=GetActorLocation()+Direction*Step,Ground;
 if(!RoadGround(GetWorld(),Next-BodyOffset,Ground,this)){Speed=0;BlockedTime+=DT;return false;}
 Next.Z=Ground.Z+BodyOffset.Z;
 FHitResult Hit;const FVector Old=GetActorLocation();SetActorLocation(Next,true,&Hit);
 const float Moved=FVector::Dist2D(Old,GetActorLocation());
 if(Hit.bBlockingHit){Speed=0;BlockedTime+=DT;}else BlockedTime=FMath::Max(0.f,BlockedTime-DT*.1f);
 WheelAngle=FMath::Fmod(WheelAngle+(bReverse?-1:1)*FMath::RadiansToDegrees(Moved/34.f),360.f);
 for(int32 I=0;I<Wheels.Num();++I)Wheels[I]->SetRelativeRotation(FRotator((I%2?-1:1)*WheelAngle,I%2?180:0,0));
 return FVector::Dist2D(GetActorLocation(),Target)<10.f;
}
void AExtractionConvoyCar::StartDeparture()
{
 Phase=EConvoyCarPhase::Departing;PhaseTime=0;Speed=0;BlockedTime=0;
 const FVector Here=GetActorLocation()-BodyOffset;
 FVector Ahead;
 if(RoadGround(GetWorld(),Here+GetActorForwardVector()*5500.f,Ahead,this)&&ClearRoad(GetWorld(),Here,Ahead,this))
 {DepartureTarget=Ahead+BodyOffset;bReverse=false;}
 else {DepartureTarget=EntryLocation;bReverse=true;}
}
void AExtractionConvoyCar::LeaveWithoutUnloading(){bAborted=true;if(Phase!=EConvoyCarPhase::Unloading)StartDeparture();}
void AExtractionConvoyCar::UpdatePassengers(float DT)
{
 for(int32 I=0;I<4;++I)
 {
  if(Exited[I])continue;
  const float Delay=I==3?3.8f:I*.22f;
  const float T=PhaseTime-Delay;if(T<0)continue;
  const bool Left=I==1;UAnimSequence* Clip=Left?ExitLeft.Get():ExitRight.Get();if(!Clip)continue;
  auto* Rider=Riders[I].Get();
  if(Rider->GetSingleNodeInstance()&&Rider->GetSingleNodeInstance()->GetCurrentAsset()!=Clip)Rider->PlayAnimation(Clip,false);
  Rider->SetPosition(FMath::Min(T,Clip->GetPlayLength()),false);
  // Rear centre passenger slides over only after the right rear passenger has exited.
  const float ClearDoor=FMath::SmoothStep(1.6f,3.f,T);
  Rider->SetRelativeLocation(Seat(I)+FVector(I==3?-90.f*ClearDoor:0.f,(I==3?44*FMath::Clamp(T/.65f,0.f,1.f):0.f)+(Left?-16.f:16.f)*ClearDoor,0));
  const float Open=FMath::SmoothStep(.05f,.65f,T)*(1.f-FMath::SmoothStep(2.2f,3.f,T));
  Doors[DoorForSeat(I)]->SetRelativeRotation(FRotator(0,(Left?1:-1)*65.f*Open,0));
  if(T<Clip->GetPlayLength())continue;
  auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>();
  if(bAborted||!Session||!Session->GetExtractingPlayer())
  {Rider->SetVisibility(false);Exited[I]=true;continue;}
  const FVector RootOffset=FRotator(0,-90,0).RotateVector(Clip->ExtractRootMotionFromRange(0,Clip->GetPlayLength(),FAnimExtractContext()).GetTranslation());
  const FVector Local=Rider->GetRelativeLocation()+RootOffset;
  FVector Ground;const FVector Exit=VisualRoot->GetComponentTransform().TransformPosition(Local);
  if(!RoadGround(GetWorld(),Exit,Ground,this))continue;
  const FVector Spawn=Ground+FVector(0,0,93);
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
  auto* N=GetWorld()->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),FTransform(GetActorRotation(),Spawn),nullptr,nullptr,Params.SpawnCollisionHandlingOverride);
  if(!N)continue;
  N->NPCRole=ENPCRole::GangMember;N->bRandomGangWeapon=false;N->WeaponKind=I%2?ETacticalWeaponKind::Shotgun:ETacticalWeaponKind::Rifle;N->PatrolRadius=0;
  N->FinishSpawning(FTransform(GetActorRotation(),Spawn));Session->RegisterPassenger(N);
  Rider->SetVisibility(false);Exited[I]=true;++Delivered;
 }
 if(Exited[0]&&Exited[1]&&Exited[2]&&Exited[3])StartDeparture();
}
void AExtractionConvoyCar::Tick(float DT)
{
 Super::Tick(DT);DT=FMath::Min(DT,.1f);PhaseTime+=DT;
 auto* PC=PlayerController.Get();
 if(Phase==EConvoyCarPhase::Approaching)
 {
  if(MoveAlongRoad(DT,StopLocation,true))
  {
   if(!bAborted&&IsInFront(PC,GetActorLocation())){Phase=EConvoyCarPhase::Unloading;PhaseTime=0;Speed=0;}
   else {UE_LOG(LogTemp,Display,TEXT("Convoy parking rejected: front=%d aborted=%d location=%s"),IsInFront(PC,GetActorLocation()),bAborted,*GetActorLocation().ToString());StartDeparture();} // A player who turns away never receives a parked car behind them.
  }
  else if(BlockedTime>4.f)StartDeparture();
 }
 else if(Phase==EConvoyCarPhase::Unloading)UpdatePassengers(DT);
 else
 {
  for(auto Door:Doors)Door->SetRelativeRotation(FMath::RInterpTo(Door->GetRelativeRotation(),FRotator::ZeroRotator,DT,5));
  if(PhaseTime>.65f)MoveAlongRoad(DT,DepartureTarget,false);
  const bool Hidden=IsHiddenFromPlayer(GetWorld(),PC,GetActorLocation()-BodyOffset,GetActorRotation(),this);
  HiddenSeconds=Hidden?HiddenSeconds+DT:0;
  if(HiddenSeconds>1.f&&PhaseTime>2.f&&FVector::Dist2D(GetActorLocation(),StopLocation)>900.f){Destroy();return;}
  // If the forward escape becomes blocked, use the already validated incoming lane in reverse.
  if((BlockedTime>3.f||FVector::Dist2D(GetActorLocation(),DepartureTarget)<10.f)&&!bReverse)
  {bReverse=true;DepartureTarget=EntryLocation;BlockedTime=0;}
 }
}
void AExtractionConvoyCar::EndPlay(const EEndPlayReason::Type Reason){Super::EndPlay(Reason);}
