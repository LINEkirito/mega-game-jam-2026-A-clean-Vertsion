#include "RecordingSceneRegion.h"
#include "Components/BoxComponent.h"
ARecordingSceneRegion::ARecordingSceneRegion(){
 Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));SetRootComponent(Bounds);
 Bounds->SetBoxExtent(FVector(500,500,300));Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bounds->SetHiddenInGame(true);
}
bool ARecordingSceneRegion::Contains(FVector Point) const {
 const FVector P=Bounds->GetComponentTransform().InverseTransformPosition(Point),E=Bounds->GetUnscaledBoxExtent();
 return FMath::Abs(P.X)<=E.X&&FMath::Abs(P.Y)<=E.Y&&FMath::Abs(P.Z)<=E.Z;
}
