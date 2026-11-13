#include "RecordingLibrary.h"
#include "ShoulderCamcorder.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/CapsuleComponent.h"
#include "Combat/PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
void URecordingLibrary::BeginClip(AShoulderCamcorder* Camera) {
 RecordingCamera=Camera; Pending=FRecordedClip(); Seen.Reset(); NextSample=0;
 if(Camera) Pending.Path=Camera->LastRecordingPath;
}
void URecordingLibrary::Sample(AShoulderCamcorder* Camera) {
 if(!Camera || RecordingCamera!=Camera || Camera->RecordingSeconds<NextSample || !Camera->Capture) return;
 NextSample=Camera->RecordingSeconds+.2f;
 const auto* Config=GetDefault<UGlobalData>();
 const FTransform Lens=Camera->Capture->GetComponentTransform();
 const float TanX=FMath::Tan(FMath::DegreesToRadians(Camera->Capture->FOVAngle*.5f)),TanY=TanX*9.f/16.f;
 auto* PC=Cast<APlayerController>(GetOwner());
 for(TActorIterator<ACharacter> It(GetWorld());It;++It) {
  ACharacter* Person=*It;
  if(Person->IsPlayerControlled() || Person->IsHidden() || Seen.Contains(Person)) continue;
  const FVector Center=Person->GetActorLocation();
  if(FVector::DistSquared(Lens.GetLocation(),Center)>FMath::Square(Config->RecordingMaxDistance)) continue;
  // Multiple body samples: a subject must actually intersect the recorded lens and have clear LOS.
  bool Visible=false;
  const float Half=Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  for(float Height:{0.f,.65f,-.45f}) {
   const FVector Point=Center+FVector(0,0,Half*Height);
   const FVector Local=Lens.InverseTransformPosition(Point);
   if(Local.X<10 || FMath::Abs(Local.Y)>Local.X*TanX || FMath::Abs(Local.Z)>Local.X*TanY) continue;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(RecordingSubject),true,Camera);
   if(PC && PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn());
   FHitResult Hit;
   if(!GetWorld()->LineTraceSingleByChannel(Hit,Lens.GetLocation(),Point,ECC_Visibility,Q) || Hit.GetActor()==Person) { Visible=true; break; }
  }
  if(!Visible) continue;
  FRecordedPerson* Value=PersonValues.Find(Person);
  if(!Value) {
   FRecordedPerson New;
   const auto* NPC=Cast<APatrolEnemy>(Person);
   const TCHAR* Roles[]={TEXT("黑帮成员"),TEXT("黑帮头目"),TEXT("平民"),TEXT("商人"),TEXT("警察"),TEXT("伤者")};
   New.Name=FString::Printf(TEXT("%s #%d"),NPC?Roles[FMath::Clamp(int32(NPC->NPCRole),0,5)]:TEXT("人物"),PersonValues.Num()+1);
   New.Points=FMath::RandRange(FMath::Max(0,Config->RecordingPersonMinPoints),FMath::Max(Config->RecordingPersonMinPoints,Config->RecordingPersonMaxPoints));
   Value=&PersonValues.Add(Person,New);
  }
  Seen.Add(Person); Pending.People.Add(*Value); Pending.Score+=Value->Points;
 }
}
void URecordingLibrary::FinishClip(AShoulderCamcorder* Camera,bool bSaved) {
 if(!Camera || RecordingCamera!=Camera) return;
 if(bSaved) {
  Pending.Seconds=Camera->RecordingSeconds; Clips.Add(Pending);
  if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()) Bus->NotifyChange(this,TEXT("RecordedClips"),float(Clips.Num()));
 }
 RecordingCamera.Reset(); Seen.Reset(); Pending=FRecordedClip();
}
