// Read-only gameplay observations. All state here belongs to audio presentation.
#include "Game_Jam_Clean_VersCharacter.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Camcorder/RecordingLibrary.h"
#include "Combat/WorldEvents.h"
#include "Combat/PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Localization/GameLanguage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

void UBackgroundMusic::StopPlayerSpeech()
{
 if(PlayerSpeech.IsValid())PlayerSpeech->Stop();
 PlayerSpeech.Reset();
 if(PlayerSubtitle.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())
  GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PlayerSubtitle.ToSharedRef());
 PlayerSubtitle.Reset();
}
void UBackgroundMusic::PlayPlayerVocalization(const TCHAR* Name,float Volume)
{
 StopPlayerSpeech();
 auto* Sound=GameAudio::Sound(Name);if(!Sound)return;
 PlayerSpeech=UGameplayStatics::CreateSound2D(this,Sound,Volume,1.f,0.f,nullptr,false,true);
 if(PlayerSpeech.IsValid()){PlayerSpeech->bIsUISound=false;PlayerSpeech->Play();}
 NextPlayerLine=GetWorld()->GetTimeSeconds()+Sound->GetDuration()+.25f;
}
bool UBackgroundMusic::SayPlayer(int32 Line)
{
 auto* PC=Cast<APlayerController>(GetOwner());
 auto* Pawn=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 if(!Pawn||Pawn->Health<=0||GetWorld()->IsPaused()||Line<1||Line>7)return false;
 const float Now=GetWorld()->GetTimeSeconds();
 if(Now<NextPlayerLine||(PlayerSpeech.IsValid()&&PlayerSpeech->IsPlaying()))return false;
 if(const float* Last=PlayerLineTimes.Find(Line);Last&&Now-*Last<30.f)return false;
 const TCHAR* Texts[]={TEXT("Did you guys see that?"),TEXT("Holy shit!"),TEXT("I gotta get closer."),TEXT("I'm getting out of here!"),TEXT("Oh, that's not good."),TEXT("Okay, we're rolling."),TEXT("This is insane.")};
 auto* Sound=GameAudio::Sound(*FString::Printf(TEXT("Voice_Player_%02d"),Line));if(!Sound)return false;
 StopPlayerSpeech();
 PlayerSpeech=UGameplayStatics::CreateSound2D(this,Sound,1.0f,1.f,0.f,nullptr,false,true);
 if(!PlayerSpeech.IsValid())return false;
 PlayerSpeech->bIsUISound=false;PlayerSpeech->Play();
 PlayerLineTimes.Add(Line,Now);NextPlayerLine=Now+FMath::Max(6.f,Sound->GetDuration()+2.f);
 if(auto* Viewport=GetWorld()->GetGameViewport()){
  PlayerSubtitle=SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(40,0,40,100)).Visibility(EVisibility::HitTestInvisible)
   [SNew(SBorder).BorderBackgroundColor(FLinearColor(0,0,0,.65f)).Padding(FMargin(16,8))
    [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(22)).ColorAndOpacity(FLinearColor::White).Text(FText::FromString(Texts[Line-1]))]];
  Viewport->AddViewportWidgetContent(PlayerSubtitle.ToSharedRef(),100);
 }
 return true;
}
void UBackgroundMusic::UpdatePlayerSpeech()
{
 if(PlayerSubtitle.IsValid()&&(!PlayerSpeech.IsValid()||!PlayerSpeech->IsPlaying()))StopPlayerSpeech();
 auto* PC=Cast<APlayerController>(GetOwner());auto* Pawn=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 if(!Pawn||Pawn->Health<=0){if(PlayerSubtitle.IsValid())StopPlayerSpeech();return;}
 if(GetWorld()->IsPaused()||PC->bShowMouseCursor||Stage!=EBackgroundMusicStage::Gameplay)return;
 if(Pawn->Health>30.f)bLowHealth=false;
 if(Pawn->Health<=30.f&&!bLowHealth&&SayPlayer(5))bLowHealth=true;
 if(auto* Library=PC->FindComponentByClass<URecordingLibrary>()){
  if(Library->bRoundEnded)return;
  if(Library->Clips.Num()>SeenClips){
   bool Valuable=false;for(int32 I=SeenClips;I<Library->Clips.Num();++I)for(const auto& E:Library->Clips[I].Events)Valuable|=E.Tier>=2&&E.Points>0;
   if(!Valuable||SayPlayer(1))SeenClips=Library->Clips.Num();
  }
 }
 auto* Events=GetWorld()->GetSubsystem<UWorldEvents>();if(!Events)return;
 auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
 auto* Camera=Equipment?Equipment->Camcorder.Get():nullptr;
 int32 Chaos=0;
 for(const auto& E:Events->Events){
  if(!E.bActive||!E.bPerforming)continue;
  const bool Combat=E.Kind==EStreetEvent::GangShootout||E.Kind==EStreetEvent::MassiveGangWar||E.Kind==EStreetEvent::GangMassacre;
  if(FVector::DistSquared(E.Location,Pawn->GetActorLocation())<=FMath::Square(2500.f)){
   if(Combat||E.Kind==EStreetEvent::CivilianEscape||E.Kind==EStreetEvent::PoliceChase)++Chaos;
   if(Combat&&!bReactedToCombat&&SayPlayer(2))bReactedToCombat=true;
  }
  if(Camera&&Camera->bRecording&&Camera->bViewfinderRaised&&Camera->Capture&&!Camera->bSelfie){
   const FVector Eye=Camera->Capture->GetComponentLocation();const FVector Delta=E.Location-Eye;
   const float MaxDistance=GetDefault<UGlobalData>()->RecordingMaxDistance;
   bool ParticipantInRange=false;
   for(const auto& P:E.Participants)if(P.IsValid()&&FVector::DistSquared(Eye,P->GetActorLocation())<=FMath::Square(MaxDistance))ParticipantInRange=true;
   const FVector Local=Camera->Capture->GetComponentTransform().InverseTransformVector(Delta);
   const float Tan=FMath::Tan(FMath::DegreesToRadians(Camera->Capture->FOVAngle*.5f));
   if(!ParticipantInRange&&Delta.Size()>MaxDistance&&Delta.Size()<MaxDistance*2&&Local.X>0&&FMath::Abs(Local.Y)<Local.X*Tan&&FMath::Abs(Local.Z)<Local.X*Tan*9.f/16.f){
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(VoiceCameraSight),true,Pawn);Q.AddIgnoredActor(Camera);
    const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Eye,E.Location,ECC_Visibility,Q);
    bool Participant=false;for(const auto& P:E.Participants)Participant|=P.Get()==Hit.GetActor();
    if(!Blocked||Participant)SayPlayer(3);
   }
  }
 }
 if(Chaos>=2)SayPlayer(7);
}
