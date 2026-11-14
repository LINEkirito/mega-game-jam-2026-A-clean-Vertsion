#include "RecordingLibrary.h"
#include "RecordingSceneRegion.h"
#include "Combat/WorldEvents.h"
#include "ShoulderCamcorder.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/CapsuleComponent.h"
#include "Combat/PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
void URecordingLibrary::BeginClip(AShoulderCamcorder* Camera) {
 if(bRoundEnded)return;
 RecordingCamera=Camera; Pending=FRecordedClip(); Scoring.Begin();
 if(Camera) Pending.Path=Camera->LastRecordingPath;
}
void URecordingLibrary::Sample(AShoulderCamcorder* Camera) {
 if(bRoundEnded || !Camera || RecordingCamera!=Camera || !Camera->Capture) return;
 const auto* Config=GetDefault<UGlobalData>();
 TArray<FFilmObservation> Observations;
 const FTransform Lens=Camera->Capture->GetComponentTransform();
 const float TanX=FMath::Tan(FMath::DegreesToRadians(Camera->Capture->FOVAngle*.5f)),TanY=TanX*9.f/16.f;
 auto* PC=Cast<APlayerController>(GetOwner());
 TSet<AActor*> VisibleSubjects;
 TMap<AActor*,FVector4> VisibleRects;
 auto ProjectBounds=[&](FVector Center,FVector Extent){
  FBox2D Rect(ForceInit);
  for(int X:{-1,1})for(int Y:{-1,1})for(int Z:{-1,1}){
   const FVector Local=Lens.InverseTransformPosition(Center+Extent*FVector(X,Y,Z));if(Local.X<10)continue;
   Rect+=FVector2D(FMath::Clamp(.5f+float(Local.Y)/(2*float(Local.X)*TanX),0.f,1.f),FMath::Clamp(.5f-float(Local.Z)/(2*float(Local.X)*TanY),0.f,1.f));
  }
  return Rect.bIsValid?FVector4(Rect.Min.X,Rect.Min.Y,Rect.Max.X,Rect.Max.Y):FVector4(0,0,0,0);
 };
 for(TActorIterator<ACharacter> It(GetWorld());It;++It) {
  ACharacter* Person=*It;
  if(Person->IsPlayerControlled() || Person->IsHidden()) continue;
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
   if(!Camera->bSelfie && PC && PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn());
   FHitResult Hit;
   if(!GetWorld()->LineTraceSingleByChannel(Hit,Lens.GetLocation(),Point,ECC_Visibility,Q) || Hit.GetActor()==Person) { Visible=true; break; }
  }
  if(!Visible) continue;
  VisibleSubjects.Add(Person);
  const auto* NPC=Cast<APatrolEnemy>(Person);
  const ENPCRole Role=NPC?NPC->IdentityRole:ENPCRole::Civilian;
  FFilmObservation O;O.Key=TEXT("Person:")+Person->GetPathName();
  const float Radius=Person->GetCapsuleComponent()->GetScaledCapsuleRadius();
  O.ScreenRect=ProjectBounds(Center,FVector(Radius,Radius,Half));O.bHasScreenRect=true;VisibleRects.Add(Person,O.ScreenRect);
  const TCHAR* Roles[]={TEXT("黑帮成员"),TEXT("黑帮头目"),TEXT("平民"),TEXT("商人"),TEXT("警察"),TEXT("伤者")};
  if(!SubjectNumbers.Contains(Person))SubjectNumbers.Add(Person,SubjectNumbers.Num()+1);
  O.Name=FString::Printf(TEXT("%s #%d"),Roles[FMath::Clamp(int32(Role),0,5)],SubjectNumbers[Person]);
  O.Kind=(Role==ENPCRole::GangMember||Role==ENPCRole::GangBoss)?EFilmSubject::Gang:Role==ENPCRole::Police?EFilmSubject::Police:EFilmSubject::Civilian;
  if(NPC){O.Behavior=StaticEnum<ENPCActivity>()->GetNameStringByValue(int64(NPC->Activity));
   const TCHAR* Labels[]={TEXT("待机"),TEXT("巡逻"),TEXT("调查"),TEXT("战斗"),TEXT("威胁"),TEXT("殴打"),TEXT("交易"),TEXT("指挥"),TEXT("逃跑"),TEXT("受伤"),TEXT("死亡"),TEXT("警告玩家"),TEXT("聚会"),TEXT("看向摄像机"),TEXT("挥手驱赶摄像机"),TEXT("躲避摄像机"),TEXT("追赶"),TEXT("反击")};
   O.BehaviorLabel=Labels[FMath::Clamp(int32(NPC->Activity),0,17)];
  }
  const FName RoleKey(*StaticEnum<ENPCRole>()->GetNameStringByValue(int64(Role)));
  if(const float* M=Config->RecordingNPCMultipliers.Find(RoleKey))O.Multiplier=*M;
  Observations.Add(O);
 }
 if(auto* Director=GetWorld()->GetSubsystem<UWorldEvents>())for(const auto& E:Director->Events){
  if(!E.bActive||!E.bPerforming)continue;
  // Chase is not one of the five approved events. Civilian warfare is a variant of MassiveGangWar.
  if(E.Kind==EStreetEvent::PoliceChase)continue;
  int32 VisibleCount=0;bool Gang=false,Police=false,Civilian=false;
  for(auto Weak:E.Participants)if(auto* N=Weak.Get();N&&N->Health>0&&VisibleSubjects.Contains(N)){
   ++VisibleCount;Gang|=N->IsGang();Police|=N->NPCRole==ENPCRole::Police;Civilian|=N->IsLawProtected();
  }
  const bool Combat=E.Kind==EStreetEvent::PoliceChase||E.Kind==EStreetEvent::GangShootout||E.Kind==EStreetEvent::MassiveGangWar||E.Kind==EStreetEvent::GangMassacre;
  const int32 Required=E.Kind==EStreetEvent::CivilianEscape?1:E.Kind==EStreetEvent::MassiveGangWar||E.Kind==EStreetEvent::GangMassacre||E.Kind==EStreetEvent::GangMeeting?3:2;
  if(VisibleCount<Required||(Combat&&(!Gang||!(E.Kind==EStreetEvent::GangMassacre?Civilian:Police))))continue;
  const EStreetEvent ScoredKind=E.Kind==EStreetEvent::GangMassacre?EStreetEvent::MassiveGangWar:E.Kind;
  FFilmObservation O;O.Key=FString::Printf(TEXT("Event:%d"),E.Id);O.Name=UWorldEvents::EventName(ScoredKind);
  O.Kind=EFilmSubject::Event;O.Type=StaticEnum<EStreetEvent>()->GetNameStringByValue(int64(ScoredKind));O.EventId=E.Id;O.Tier=E.Tier;
  FBox2D Rect(ForceInit);for(auto Weak:E.Participants)if(const auto* R=VisibleRects.Find(Weak.Get())){Rect+=FVector2D(R->X,R->Y);Rect+=FVector2D(R->Z,R->W);}
  if(Rect.bIsValid){O.ScreenRect=FVector4(Rect.Min.X,Rect.Min.Y,Rect.Max.X,Rect.Max.Y);O.bHasScreenRect=true;}
  O.Multiplier=E.Tier==3?Config->RecordingLevel3Multiplier:E.Tier==2?Config->RecordingLevel2Multiplier:Config->RecordingLevel1Multiplier;
  if(const float* M=Config->RecordingEventMultipliers.Find(FName(*O.Type)))O.Multiplier=*M;
  Observations.Add(O);
 }
 // Opt-in only: no actor is tagged or inferred to be a logo by this system.
 for(TActorIterator<AActor> It(GetWorld());It;++It){
  AActor* Logo=*It;if(!Logo->ActorHasTag(TEXT("RecordingLogo"))||Logo->IsHidden())continue;
  FVector Center,Extent;Logo->GetActorBounds(false,Center,Extent);
  if(FVector::DistSquared(Lens.GetLocation(),Center)>FMath::Square(Config->RecordingMaxDistance))continue;
  bool Visible=false;
  const FVector Points[]={Center,Center+FVector(Extent.X*.7,0,0),Center-FVector(Extent.X*.7,0,0),Center+FVector(0,Extent.Y*.7,0),Center-FVector(0,Extent.Y*.7,0),Center+FVector(0,0,Extent.Z*.7),Center-FVector(0,0,Extent.Z*.7)};
  for(const FVector& Point:Points){
   const FVector Local=Lens.InverseTransformPosition(Point);
   if(Local.X<10||FMath::Abs(Local.Y)>Local.X*TanX||FMath::Abs(Local.Z)>Local.X*TanY)continue;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(RecordingLogo),true,Camera);if(!Camera->bSelfie&&PC&&PC->GetPawn())Q.AddIgnoredActor(PC->GetPawn());
   FHitResult Hit;
   if(!GetWorld()->LineTraceSingleByChannel(Hit,Lens.GetLocation(),Point,ECC_Visibility,Q)||Hit.GetActor()==Logo){Visible=true;break;}
  }
  if(Visible){FFilmObservation O;O.Key=TEXT("Logo:")+Logo->GetPathName();O.Name=TEXT("Logo · ")+Logo->GetName();O.Kind=EFilmSubject::Logo;O.ScreenRect=ProjectBounds(Center,Extent);O.bHasScreenRect=true;Observations.Add(O);}
 }
 // Only explicitly defined areas count. One highest-priority containing region avoids overlap farming.
 ARecordingSceneRegion* Region=nullptr;
 for(TActorIterator<ARecordingSceneRegion> It(GetWorld());It;++It)if(!It->SceneId.IsNone()&&It->Contains(Lens.GetLocation())){
  if(!Region||It->Priority>Region->Priority||(It->Priority==Region->Priority&&It->GetPathName()<Region->GetPathName()))Region=*It;
 }
 if(Region){FFilmObservation Scene;Scene.Kind=EFilmSubject::Scene;Scene.Key=TEXT("Scene:")+Region->SceneId.ToString();Scene.Name=TEXT("场景 · ")+(Region->SceneName.IsEmpty()?Region->SceneId.ToString():Region->SceneName);Observations.Add(Scene);}
 Scoring.Sample(Camera->RecordingSeconds,Observations);
}
void URecordingLibrary::FinishClip(AShoulderCamcorder* Camera,bool bSaved) {
 if(!Camera || RecordingCamera!=Camera) return;
 if(bSaved) {
  // Only saved footage commits session history. Failed saves consume no thresholds.
  Pending.Seconds=Camera->RecordingSeconds;
  Scoring.FinalizeDuration(Pending.Seconds);
  TArray<TSharedPtr<FJsonValue>> Timeline;
  for(const auto& M:Scoring.Moments){
   FRecordedScoreMoment Moment;Moment.Seconds=M.Time;Moment.Points=M.Points;Moment.Subject=M.Subject.Name;Moment.Category=M.Category;Moment.Explanation=M.Explanation;Moment.ScreenRect=M.Subject.ScreenRect;Moment.bHasScreenRect=M.Subject.bHasScreenRect;Pending.ScoreMoments.Add(Moment);
   Pending.ScoreDetails.Add(FString::Printf(TEXT("[%05.1fs] %s · %s"),M.Time,*M.Subject.Name,*M.Explanation));
   auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("seconds"),M.Time);Row->SetNumberField(TEXT("points"),M.Points);Row->SetStringField(TEXT("subject"),M.Subject.Name);Row->SetStringField(TEXT("category"),M.Category);Row->SetStringField(TEXT("formula"),M.Explanation);Timeline.Add(MakeShared<FJsonValueObject>(Row));
  }
  auto Record=MakeShared<FJsonObject>();Record->SetNumberField(TEXT("rules_version"),2);Record->SetStringField(TEXT("video"),Pending.Path);Record->SetNumberField(TEXT("seconds"),Pending.Seconds);
  Record->SetArrayField(TEXT("score_timeline"),Timeline);
  TArray<TSharedPtr<FJsonValue>> Exposures;
  for(const auto& Pair:Scoring.Shot){const auto& E=Pair.Value;auto Row=MakeShared<FJsonObject>();
   const FString Level=E.Subject.Kind==EFilmSubject::Event?FString::Printf(TEXT(" · Level %d · %.0f倍"),E.Subject.Tier,E.Subject.Multiplier):TEXT("");
   Pending.RecordingEvidence.Add(FString::Printf(TEXT("%s%s：入镜%d次，累计 %.2f秒，最长连续 %.2f秒%s"),*E.Subject.Name,*Level,E.Entries,E.Total,E.Longest,
    *(E.Subject.Kind==EFilmSubject::Gang||E.Subject.Kind==EFilmSubject::Event?FString::Printf(TEXT("，有效时长 %.2f秒"),E.Valid):FString())));
   Row->SetStringField(TEXT("id"),Pair.Key);Row->SetStringField(TEXT("name"),E.Subject.Name);Row->SetStringField(TEXT("event_type"),E.Subject.Type);Row->SetNumberField(TEXT("level"),E.Subject.Tier);Row->SetNumberField(TEXT("multiplier"),E.Subject.Multiplier);
   Row->SetNumberField(TEXT("total_seconds"),E.Total);Row->SetNumberField(TEXT("longest_continuous_seconds"),E.Longest);Row->SetNumberField(TEXT("valid_seconds"),E.Valid);
   Row->SetNumberField(TEXT("entries"),E.Entries);Row->SetNumberField(TEXT("valid_runs"),E.ValidRuns);
   TArray<TSharedPtr<FJsonValue>> Behaviors;for(const auto& B:E.Behaviors)Behaviors.Add(MakeShared<FJsonValueString>(B));Row->SetArrayField(TEXT("valid_behaviors"),Behaviors);
   Exposures.Add(MakeShared<FJsonValueObject>(Row));
  }Record->SetArrayField(TEXT("exposures"),Exposures);
  TArray<TSharedPtr<FJsonValue>> ScoreRows;
  for(const auto& Award:Scoring.Commit(Pending.Seconds)){
   const auto& O=Award.Subject;Pending.Score+=Award.Points;
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),O.Key);Row->SetStringField(TEXT("name"),O.Name);Row->SetNumberField(TEXT("points"),Award.Points);ScoreRows.Add(MakeShared<FJsonValueObject>(Row));
   if(O.Kind==EFilmSubject::Event){FRecordedEvent E;E.Id=O.EventId;E.Name=O.Name;E.Tier=O.Tier;E.Points=Award.Points;Pending.Events.Add(E);}
   else if(O.Kind==EFilmSubject::Gang||O.Kind==EFilmSubject::Civilian||O.Kind==EFilmSubject::Police){FRecordedPerson P;P.Name=O.Name;P.Points=Award.Points;Pending.People.Add(P);}
  }
  Record->SetNumberField(TEXT("score"),Pending.Score);Record->SetArrayField(TEXT("awards"),ScoreRows);
  if(!Pending.Path.IsEmpty()){
   FString Json;FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Json));const FString Path=Pending.Path+TEXT(".score.json");
   if(FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))Pending.ScoreRecordPath=Path;
   else UE_LOG(LogTemp,Error,TEXT("Could not persist recording score: %s (score retained in memory)"),*Path);
  }
  Clips.Add(Pending);
  if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()) Bus->NotifyChange(this,TEXT("RecordedClips"),float(Clips.Num()));
 }
 RecordingCamera.Reset();Scoring.Begin();Pending=FRecordedClip();
}
