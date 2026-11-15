#include "AmbientTrafficCar.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

UStaticMeshComponent* AAmbientTrafficCar::AddPart(const TCHAR* Name,const TCHAR* Part,const FVector& Position,const FRotator& Rotation)
{
 auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(VisualRoot);
 C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Game/Luxury_Sedan_1_Pack/Meshes/Parts/SM_Luxury_Sedan_1_"))+Part)));
 C->SetRelativeLocationAndRotation(Position,Rotation);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
 Parts.Add(C);return C;
}
AAmbientTrafficCar::AAmbientTrafficCar()
{
 PrimaryActorTick.bCanEverTick=true;
 Body=CreateDefaultSubobject<UBoxComponent>(TEXT("TrafficBody"));SetRootComponent(Body);
 Body->SetBoxExtent(FVector(250,112,65));Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 Body->SetCanEverAffectNavigation(false);Body->SetGenerateOverlapEvents(false);
 VisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));VisualRoot->SetupAttachment(Body);VisualRoot->SetRelativeLocation(FVector(0,0,-76));
 Route=CreateDefaultSubobject<USplineComponent>(TEXT("Route"));Route->SetupAttachment(Body);Route->SetAbsolute(true,true,true);
 AddPart(TEXT("Body"),TEXT("Body"),FVector::ZeroVector);
 AddPart(TEXT("DoorFL"),TEXT("Door_FL"),FVector(90.9671,-91.7858,76.5832));
 AddPart(TEXT("DoorFR"),TEXT("Door_FR"),FVector(90.9671,91.7858,76.5832));
 AddPart(TEXT("DoorRL"),TEXT("Door_RL"),FVector(-15.8824,-91.531,79.5173));
 AddPart(TEXT("DoorRR"),TEXT("Door_RR"),FVector(-15.8824,91.531,79.5173));
 AddPart(TEXT("Hood"),TEXT("Hood"),FVector(99.6386,0,99.9992));
 AddPart(TEXT("Trunk"),TEXT("Trunk"),FVector(-128.06,0,130.233));
 AddPart(TEXT("TrunkCover"),TEXT("Trunk_Cover"),FVector(-136.659,0,103.736));
 AddPart(TEXT("Steering"),TEXT("Steering_Wheel"),FVector(49.6812,-40.918,84.8244),FRotator(65,0,0));
 Wheels.Add(AddPart(TEXT("WheelFL"),TEXT("Wheel"),FVector(158.141,-80.368,34.2088)));
 Wheels.Add(AddPart(TEXT("WheelFR"),TEXT("Wheel"),FVector(158.141,80.6993,34.2088),FRotator(0,180,0)));
 Wheels.Add(AddPart(TEXT("WheelRL"),TEXT("Wheel"),FVector(-130.519,-80.368,33.4457)));
 Wheels.Add(AddPart(TEXT("WheelRR"),TEXT("Wheel"),FVector(-129.966,80.6993,34.2088),FRotator(0,180,0)));

 Driver=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Driver"));Driver->SetupAttachment(VisualRoot);
 Driver->SetRelativeLocationAndRotation(FVector(12,-44,0),FRotator(0,-90,0));Driver->SetCollisionEnabled(ECollisionEnabled::NoCollision);Driver->SetCanEverAffectNavigation(false);
}
void AAmbientTrafficCar::BuildRoute()
{
 Route->ClearSplinePoints(false);
 for(const FVector& P:RoutePoints)Route->AddSplinePoint(P,ESplineCoordinateSpace::World,false);
 // Road-authored short segments describe rounded corners without spline overshoot onto sidewalks.
 for(int32 I=0;I<RoutePoints.Num();++I)Route->SetSplinePointType(I,ESplinePointType::Linear,false);
 Route->SetClosedLoop(true,true);
}
FVector AAmbientTrafficCar::RoutePosition(float Distance) const
{
 const float Length=Route->GetSplineLength();
 return Route->GetLocationAtDistanceAlongSpline(Length>0?FMath::Fmod(Distance+Length,Length):0,ESplineCoordinateSpace::World)+FVector(0,0,76);
}
void AAmbientTrafficCar::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);BuildRoute();
 const TCHAR* PaintPath=VehicleKind==EAmbientVehicleKind::Police?TEXT("/Game/AppearanceSkins/M_PoliceCar"):VehicleKind==EAmbientVehicleKind::Gang?TEXT("/Game/AppearanceSkins/M_GangCar"):nullptr;
 auto* Paint=PaintPath?LoadObject<UMaterialInterface>(nullptr,PaintPath):nullptr;
 for(auto C:Parts)if(C&&C->GetStaticMesh())for(int32 I=0;I<C->GetNumMaterials();++I){
  auto* Original=C->GetStaticMesh()->GetMaterial(I);
  if(Original&&Original->GetName().Contains(TEXT("Color")))C->SetMaterial(I,Paint?Paint:Original);
 }
 const TCHAR* Person=VehicleKind==EAmbientVehicleKind::Police?TEXT("NPC_Police"):VehicleKind==EAmbientVehicleKind::Gang?TEXT("NPC_GangMember"):TEXT("NPC_Civilian");
 Driver->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,*(FString(TEXT("/Game/Combat/NPC/Characters/"))+Person)));
 if(RoutePoints.Num()>2){RouteDistance=FMath::Clamp(StartFraction,0.f,.9999f)*Route->GetSplineLength();SetActorLocation(RoutePosition(RouteDistance));SetActorRotation((RoutePosition(RouteDistance+100)-RoutePosition(RouteDistance)).Rotation());}
}
void AAmbientTrafficCar::BeginPlay()
{
 Super::BeginPlay();BuildRoute();RouteDistance=FMath::Clamp(StartFraction,0.f,.9999f)*Route->GetSplineLength();
 Driver->PlayAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Extraction/Convoy/Animations/AS_Driver_Idle")),true);
}
bool AAmbientTrafficCar::IsPathClear(float FromDistance,float ToDistance) const
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(AmbientTraffic),false,this);
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
 // Sample the actual curved path, including space swept by the corners of the body.
 for(float D=FromDistance;D<ToDistance;D+=70.f){
  const FVector A=RoutePosition(D),B=RoutePosition(FMath::Min(D+70.f,ToDistance));
  FHitResult Hit;
  if(GetWorld()->SweepSingleByObjectType(Hit,A,B,(B-A).Rotation().Quaternion(),Objects,FCollisionShape::MakeBox(FVector(260,125,65)),Q))return false;
 }
 return true;
}
void AAmbientTrafficCar::Tick(float DT)
{
 Super::Tick(DT);if(RoutePoints.Num()<3||Route->GetSplineLength()<100)return;
 // Small substeps and collision sweeps prevent tunnelling on slow frames. No hit damage is generated.
 float Remaining=FMath::Min(DT,.25f);
 while(Remaining>0){const float Step=FMath::Min(Remaining,.025f);Remaining-=Step;
  const float LookAhead=160.f+Speed*Speed/(2.f*600.f);
  bWaitingForClearance=!IsPathClear(RouteDistance,RouteDistance+LookAhead);
  Speed=FMath::FInterpConstantTo(Speed,bWaitingForClearance?0.f:CruiseSpeed,Step,bWaitingForClearance?600.f:160.f);
  const float Advance=Speed*Step;
  if(Advance<=0||!IsPathClear(RouteDistance,RouteDistance+Advance+20.f)){Speed=0;continue;}
  const float Next=RouteDistance+Advance;
  const FRotator Rotation=(RoutePosition(Next+100)-RoutePosition(Next)).Rotation();
  FHitResult Hit;SetActorLocationAndRotation(RoutePosition(Next),Rotation,true,&Hit,ETeleportType::None);
  if(Hit.bBlockingHit){Speed=0;bWaitingForClearance=true;continue;}
  RouteDistance=Next;DistanceTravelled+=Advance;
  if(RouteDistance>=Route->GetSplineLength()){RouteDistance=FMath::Fmod(RouteDistance,Route->GetSplineLength());++CompletedLaps;}
  WheelAngle=FMath::Fmod(WheelAngle+Advance/34.f*180.f/PI,360.f);
  for(int32 I=0;I<Wheels.Num();++I)Wheels[I]->SetRelativeRotation(FRotator(I%2?-WheelAngle:WheelAngle,I%2?180:0,0));
 }
}
