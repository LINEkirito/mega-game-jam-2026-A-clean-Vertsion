#include "ExtractionPoint.h"
#include "Audio/GameAudio.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "Camcorder/RecordingLibrary.h"
#include "Localization/GameLanguage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"

bool UExtractionSubsystem::Claim(AExtractionPoint* Point)
{
 if(!IsValid(Point)||Point->GetWorld()!=GetWorld()||ActivePoint.IsValid())return false;
 ActivePoint=Point;return true;
}
void UExtractionSubsystem::Release(AExtractionPoint* Point){if(ActivePoint.Get()==Point)ActivePoint.Reset();}
AGame_Jam_Clean_VersCharacter* UExtractionSubsystem::GetExtractingPlayer() const
{
 auto* Point=ActivePoint.Get();
 return Point&&Point->State==EExtractionState::Extracting?Point->ExtractingPlayer.Get():nullptr;
}

AExtractionPoint::AExtractionPoint()
{
 PrimaryActorTick.bCanEverTick=true;
 SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(SceneRoot);
 VehicleMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));VehicleMesh->SetupAttachment(SceneRoot);
 VehicleSkeletalMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleSkeletalMesh"));VehicleSkeletalMesh->SetupAttachment(SceneRoot);
 VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 VehicleSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 VehicleSkeletalMesh->SetSimulatePhysics(false);
 VehicleBody=CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleBody"));VehicleBody->SetupAttachment(SceneRoot);
 VehicleBody->SetBoxExtent(FVector(230,95,75));VehicleBody->SetRelativeLocation(FVector(0,0,75));
 VehicleBody->SetCollisionProfileName(TEXT("BlockAll"));VehicleBody->SetGenerateOverlapEvents(false);
}
FString AExtractionPoint::GetDisplayName() const
{
 switch(VehicleType)
 {
 case EExtractionVehicle::SUV:return GameLanguage::Display(TEXT("撤离点 · SUV"));
 case EExtractionVehicle::BoxTruck:return GameLanguage::Display(TEXT("撤离点 · 厢式货车"));
 case EExtractionVehicle::Campervan:return GameLanguage::Display(TEXT("撤离点 · 房车"));
 default:return GameLanguage::Display(TEXT("撤离点 · 轿车"));
 }
}
float AExtractionPoint::DistanceToVehicle(const FVector& Position) const
{
 // Closest point on the oriented parked-car body, not distance to its pivot.
 const FTransform Transform=VehicleBody->GetComponentTransform();
 const FVector Local=Transform.InverseTransformPosition(Position),Extent=VehicleBody->GetUnscaledBoxExtent();
 const FVector Closest(FMath::Clamp(Local.X,-Extent.X,Extent.X),FMath::Clamp(Local.Y,-Extent.Y,Extent.Y),FMath::Clamp(Local.Z,-Extent.Z,Extent.Z));
 return FVector::Distance(Position,Transform.TransformPosition(Closest));
}
bool AExtractionPoint::IsPlayerInRange(const AGame_Jam_Clean_VersCharacter* Player) const
{
 return IsValid(Player)&&Player->Health>0&&DistanceToVehicle(Player->GetActorLocation())<=GetDefault<UGlobalData>()->ExtractionRadius;
}
bool AExtractionPoint::IsLookedAtBy(APlayerController* PC) const
{
 if(!PC||!PC->GetPawn())return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(ExtractionLook),true,PC->GetPawn());
 TArray<AActor*> Attached;PC->GetPawn()->GetAttachedActors(Attached,true,true);Query.AddIgnoredActors(Attached);
 FHitResult Hit;
 // Long enough to inspect the full vehicle, while distance eligibility is checked separately.
 return GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*2000.f,ECC_Visibility,Query)&&Hit.GetActor()==this;
}
bool AExtractionPoint::TryStart(APlayerController* PC)
{
 auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 auto* Library=PC?PC->FindComponentByClass<URecordingLibrary>():nullptr;
 if(State!=EExtractionState::Idle||!PC||!PC->IsLocalController()||PC->bShowMouseCursor||PC->IsPaused()||
    !Library||Library->bRoundEnded||!IsPlayerInRange(Player)||!IsLookedAtBy(PC))return false;
 auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>();
 if(!Session||!Session->Claim(this))return false;
 ExtractingPlayer=Player;RemainingSeconds=GetDefault<UGlobalData>()->ExtractionSeconds;State=EExtractionState::Extracting;
 Notice.Empty();NoticeSeconds=0;NotifyState();return true;
}
void AExtractionPoint::NotifyState()
{
 if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>())
 {
  Bus->NotifyChange(this,TEXT("ExtractionState"),static_cast<float>(State));
  Bus->NotifyChange(this,TEXT("ExtractionRemaining"),RemainingSeconds);
 }
}
void AExtractionPoint::Cancel()
{
 if(State!=EExtractionState::Extracting)return;
 if(auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>())Session->Release(this);
 State=EExtractionState::Idle;RemainingSeconds=0;ExtractingPlayer=nullptr;
 Notice=TEXT("撤离已取消：请留在车辆 3 米内");NoticeSeconds=2.f;NotifyState();
}
void AExtractionPoint::Advance(float DeltaSeconds)
{
 if(State!=EExtractionState::Extracting)return;
 auto* PC=IsValid(ExtractingPlayer)?Cast<APlayerController>(ExtractingPlayer->GetController()):nullptr;
 auto* Library=PC?PC->FindComponentByClass<URecordingLibrary>():nullptr;
 if(!IsPlayerInRange(ExtractingPlayer)||!Library||Library->bRoundEnded){Cancel();return;}
 const int32 PreviousSecond=FMath::CeilToInt(RemainingSeconds);
 RemainingSeconds=FMath::Max(0.f,RemainingSeconds-FMath::Max(0.f,DeltaSeconds));
 if(RemainingSeconds>0&&FMath::CeilToInt(RemainingSeconds)<PreviousSecond)GameAudio::Cue(this,TEXT("ExtractionTick"),.22f);
 if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>())Bus->NotifyChange(this,TEXT("ExtractionRemaining"),RemainingSeconds);
 if(RemainingSeconds>0.f)return;
 State=EExtractionState::Completed;
 GetWorld()->GetSubsystem<UExtractionSubsystem>()->Release(this);NotifyState();RemoveOverlay();
 GameAudio::Cue(this,TEXT("Victory"),.35f);
 Library->EndRound(false);
}
FString AExtractionPoint::StatusText() const
{
 if(State==EExtractionState::Extracting)return FString::Printf(TEXT("撤离倒计时：%.1f 秒"),RemainingSeconds);
 if(NoticeSeconds>0)return Notice;
 return TEXT("[F] 开始撤离（车身 3 米内）");
}
void AExtractionPoint::UpdateOverlay(APlayerController* PC,bool Visible)
{
 if(!Visible){RemoveOverlay();return;}
 if(Overlay||!PC||!GetWorld()->GetGameViewport())return;
 TWeakObjectPtr<AExtractionPoint> WeakThis(this);
 Overlay=SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(20,100,20,0))
 [SNew(SBox).WidthOverride(560)
  [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(18).BorderBackgroundColor(FLinearColor(.015f,.025f,.03f,.94f))
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
    [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(24)).Text_Lambda([WeakThis]{return FText::FromString(WeakThis.IsValid()?WeakThis->GetDisplayName():FString());})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8).HAlign(HAlign_Center)
    [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(20)).AutoWrapText(true).Justification(ETextJustify::Center)
     .Text_Lambda([WeakThis]{return FText::FromString(WeakThis.IsValid()?WeakThis->StatusText():FString());})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,4)
    [SNew(SProgressBar).FillColorAndOpacity(FLinearColor(.15f,.8f,.55f))
     .Visibility_Lambda([WeakThis]{return WeakThis.IsValid()&&WeakThis->State==EExtractionState::Extracting?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
     .Percent_Lambda([WeakThis]()->TOptional<float>{return WeakThis.IsValid()?1.f-WeakThis->RemainingSeconds/FMath::Max(.01f,GetDefault<UGlobalData>()->ExtractionSeconds):0.f;})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,6).HAlign(HAlign_Center)
    [SNew(SLocalizedTextBlock).Font(GameLanguage::Font(16)).AutoWrapText(true).Justification(ETextJustify::Center)
     .Visibility_Lambda([WeakThis]{return WeakThis.IsValid()&&WeakThis->State==EExtractionState::Extracting?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
     .Text(FText::FromString(TEXT("敌人正在靠近！留在车辆 3 米内，可转身还击。")))]
   ]]];
 Overlay->SetVisibility(EVisibility::HitTestInvisible);
 GetWorld()->GetGameViewport()->AddViewportWidgetContent(Overlay.ToSharedRef(),45);
}
void AExtractionPoint::RemoveOverlay()
{
 if(Overlay&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
 Overlay.Reset();
}
void AExtractionPoint::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);NoticeSeconds=FMath::Max(0.f,NoticeSeconds-DeltaSeconds);
 auto* PC=UGameplayStatics::GetPlayerController(this,0);
 auto* Library=PC?PC->FindComponentByClass<URecordingLibrary>():nullptr;
 if(!PC||!Library||Library->bRoundEnded){if(State==EExtractionState::Extracting)Cancel();RemoveOverlay();return;}
 if(State==EExtractionState::Completed){RemoveOverlay();return;}
 const bool InRange=IsPlayerInRange(Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()));
 if(InRange&&!bAudioPlayerInRange&&!PC->bShowMouseCursor&&!PC->IsPaused())GameAudio::Cue(this,TEXT("ExtractionEnter"));
 bAudioPlayerInRange=InRange;
 const bool WasExtracting=State==EExtractionState::Extracting;
 if(WasExtracting)Advance(DeltaSeconds);
 // Starting is handled exclusively by the shared F interaction input.
 const auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>();
 const bool Focus=(!Session->GetActivePoint()||Session->GetActivePoint()==this)&&!PC->bShowMouseCursor&&IsPlayerInRange(Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()))&&IsLookedAtBy(PC);
 UpdateOverlay(PC,!Library->bRoundEnded&&(State==EExtractionState::Extracting||Focus||NoticeSeconds>0));
}
void AExtractionPoint::EndPlay(const EEndPlayReason::Type Reason)
{
 if(auto* Session=GetWorld()->GetSubsystem<UExtractionSubsystem>())Session->Release(this);
 RemoveOverlay();Super::EndPlay(Reason);
}
