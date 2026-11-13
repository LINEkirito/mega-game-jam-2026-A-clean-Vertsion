#include "PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "TacticalWeapon.h"
#include "CombatEquipmentComponent.h"
#include "NPCMotionAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"

APatrolEnemy::APatrolEnemy()
{
 const auto* Config=GetDefault<UGlobalData>();Health=Config->EnemyHealth;SightAngle=Config->EnemySightAngle;SightRange=Config->EnemySightRange;PatrolRadius=Config->NPCPatrolRadius;
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;AIControllerClass=AAIController::StaticClass();
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
 GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);GetMesh()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 GetMesh()->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")));
 GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,-90,0));
 GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 NPCBody=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("NPCWeaponBody"));NPCBody->SetupAttachment(GetMesh());NPCBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);NPCBody->SetVisibility(false);NPCBody->SetCanEverAffectNavigation(false);
 GetCharacterMovement()->MaxWalkSpeed=Config->NPCWalkSpeed;GetCharacterMovement()->MaxAcceleration=450;GetCharacterMovement()->BrakingDecelerationWalking=600;bUseControllerRotationYaw=false;
 Rifle=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EnemyRifle"));Rifle->SetupAttachment(GetMesh(),TEXT("HandGrip_R"));Rifle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Rifle->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle")));
 TradeParcel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TradeParcel"));TradeParcel->SetupAttachment(GetMesh(),TEXT("hand_r"));TradeParcel->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));TradeParcel->SetRelativeScale3D(FVector(.12f,.08f,.04f));TradeParcel->SetRelativeLocation(FVector(-6,0,0));TradeParcel->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Camcorder/Materials/M_CamcorderColor")));TradeParcel->SetDefaultCustomPrimitiveDataVector4(0,FVector4(.25f,.12f,.04f,1));TradeParcel->SetCollisionEnabled(ECollisionEnabled::NoCollision);TradeParcel->SetVisibility(false);
 Speech=CreateDefaultSubobject<UTextRenderComponent>(TEXT("NPCSpeech"));Speech->SetupAttachment(GetRootComponent());Speech->SetRelativeLocation(FVector(0,0,125));Speech->SetWorldSize(22);Speech->SetHorizontalAlignment(EHTA_Center);Speech->SetVisibility(false);
 Voice=CreateDefaultSubobject<UAudioComponent>(TEXT("NPCVoice"));Voice->SetupAttachment(GetRootComponent());Voice->SetAutoActivate(false);Voice->bOverrideAttenuation=true;Voice->AttenuationOverrides.bAttenuate=true;Voice->AttenuationOverrides.bSpatialize=true;Voice->AttenuationOverrides.AttenuationShapeExtents=FVector(150,0,0);Voice->AttenuationOverrides.FalloffDistance=2200;
 Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"));
 Walk=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"));
 const TCHAR* AppearanceNames[]={TEXT("GangMember"),TEXT("GangBoss"),TEXT("Civilian"),TEXT("Merchant"),TEXT("Police"),TEXT("Injured")};
 for(int32 I=0;I<UE_ARRAY_COUNT(AppearanceNames);++I)
  if(auto* AppearanceMesh=LoadObject<USkeletalMesh>(nullptr,*(FString(TEXT("/Game/Combat/NPC/Characters/NPC_"))+AppearanceNames[I])))RoleAppearances.Add(ENPCRole(I),AppearanceMesh);
}
void APatrolEnemy::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);ApplyAppearance();
}
void APatrolEnemy::ApplyAppearance()
{
 USkeletalMesh* Appearance=AppearanceOverride;
 if(!Appearance)if(const auto* Entry=RoleAppearances.Find(NPCRole))Appearance=Entry->Get();
 if(!Appearance)return;
 if(GetMesh()->GetSkeletalMeshAsset()==Appearance&&NPCBody->GetSkinnedAsset()==Appearance)return;
 GetMesh()->SetSkeletalMeshAsset(Appearance);
 CurrentAnimation=nullptr;
 // Clear old mannequin/instance overrides so both rendering paths use this skin.
 GetMesh()->EmptyOverrideMaterials();NPCBody->EmptyOverrideMaterials();
 NPCBody->SetSkinnedAssetAndUpdate(Appearance);
}
void APatrolEnemy::BeginPlay()
{
 ApplyAppearance();
 Super::BeginPlay();Home=GetActorLocation();ScanYaw=GetActorRotation().Yaw;DecisionTime=FMath::FRandRange(0,.25f);SocialCooldown=FMath::FRandRange(3.f,7.f);MoveRetry=FMath::FRandRange(1.f,3.f);
 UnarmedIdle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"));
 UnarmedWalk=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"));
 if(IsProtectedPerson()){Idle=UnarmedIdle;Walk=UnarmedWalk;}
 if(NPCRole==ENPCRole::GangBoss)WeaponKind=ETacticalWeaponKind::Pistol;
 else if(NPCRole==ENPCRole::GangMember&&bRandomGangWeapon)WeaponKind=ETacticalWeaponKind(FMath::RandRange(0,2));
 if(!IsProtectedPerson()&&WeaponKind==ETacticalWeaponKind::Pistol){
  Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS"));
  Walk=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Fwd"));

 }
 if(!IsProtectedPerson())
 {
  const FString Type=WeaponKind==ETacticalWeaponKind::Pistol?TEXT("Pistol"):TEXT("Rifle");
  const FString Root=TEXT("/Game/Combat/NPC/RogueBox/NPC_A_")+Type;
  if(auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("_Walk_Forward"))))Walk=Clip;
  AimWalk=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("_Aim_Walk_Forward")));
  const TCHAR* Directions[]={TEXT("Forward"),TEXT("Forward_Right"),TEXT("Right"),TEXT("Backward_Right"),TEXT("Backward"),TEXT("Backward_Left"),TEXT("Left"),TEXT("Forward_Left")};
  // Use the verified rifle lower-body set for directional steps; arms retain each weapon's own pose.
  for(const TCHAR* Direction:Directions)AimWalkDirections.Add(LoadObject<UAnimSequence>(nullptr,*(FString(TEXT("/Game/Combat/NPC/RogueBox/NPC_A_Rifle_Aim_Walk_"))+Direction)));
  Run=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("_Sprint")));

 }
 TurnLeft=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Combat/NPC/RogueBox/NPC_TurnLeft90"));
 TurnRight=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Combat/NPC/RogueBox/NPC_TurnRight90"));
 TurnLeft180=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Combat/NPC/RogueBox/NPC_TurnLeft180"));
 TurnRight180=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Combat/NPC/RogueBox/NPC_TurnRight180"));
 GetMesh()->SetAnimInstanceClass(UNPCMotionAnimInstance::StaticClass());
 Rifle->SetVisibility(false);NPCBody->SetSkinnedAssetAndUpdate(GetMesh()->GetSkeletalMeshAsset());
 if(!IsProtectedPerson()){
  SetMotion();const FTransform Transform=GetActorTransform();HeldWeapon=GetWorld()->SpawnActorDeferred<ATacticalWeapon>(ATacticalWeapon::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(HeldWeapon){HeldWeapon->Kind=WeaponKind;HeldWeapon->FinishSpawning(Transform);HeldWeapon->EquipForNPC(GetMesh());}
 }
 const TPair<ENPCActivity,const TCHAR*> Defaults[]={
  {ENPCActivity::Threaten,TEXT("AS_Taunt_Fist_01")},{ENPCActivity::Beat,TEXT("Manny_A_Melee")},{ENPCActivity::Trade,TEXT("AS_Greetings_FormalHandShake")},
  {ENPCActivity::Command,TEXT("AS_Gestures_Pointing_Forward")},{ENPCActivity::Injured,TEXT("Manny_SK_Martial_Anim")},{ENPCActivity::Flee,TEXT("Manny_A_UnarmedSprint")}};
 for(const auto& Pair:Defaults)if(!ActivityAnimations.Contains(Pair.Key))if(auto* Anim=LoadObject<UAnimSequence>(nullptr,*(FString(TEXT("/Game/Combat/NPC/Authored/"))+Pair.Value)))ActivityAnimations.Add(Pair.Key,Anim);
 if(IsProtectedPerson())if(auto* Cower=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Combat/NPC/Authored/AS_Defeat_Beg_01")))ActivityAnimations.Add(ENPCActivity::Threaten,Cower);
 if(!ScreamSound)ScreamSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/NPC/Audio/Scream"));
 if(!HelpSound)HelpSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/NPC/Audio/Help"));
 if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()){Bus->OnGunshot.AddDynamic(this,&APatrolEnemy::HearGunshot);Bus->OnViolence.AddDynamic(this,&APatrolEnemy::WitnessViolence);}
 if(NPCRole==ENPCRole::Injured)BecomeInjured();else SetMotion();UpdateWeaponPose();
}
void APatrolEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
 ReleasePartner();if(IsValid(HeldWeapon))HeldWeapon->Destroy();if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()){Bus->OnGunshot.RemoveDynamic(this,&APatrolEnemy::HearGunshot);Bus->OnViolence.RemoveDynamic(this,&APatrolEnemy::WitnessViolence);}Super::EndPlay(Reason);
}
bool APatrolEnemy::IsLiving(AActor* Actor)const
{
 if(!IsValid(Actor))return false;
 if(const auto* NPC=Cast<APatrolEnemy>(Actor))return NPC->Health>0&&NPC->NPCRole!=ENPCRole::Injured;
 if(const auto* Player=Cast<AGame_Jam_Clean_VersCharacter>(Actor))return Player->Health>0;
 return false;
}
bool APatrolEnemy::CanSeeActor(AActor* Target,bool CheckAngle)const
{
 if(!IsValid(Target)||Health<=0)return false;
 const FVector Eye=GetActorLocation()+FVector(0,0,65),Aim=Target->GetActorLocation()+FVector(0,0,35),Delta=Aim-Eye;
 if(Delta.SizeSquared()>FMath::Square(SightRange))return false;
 if(CheckAngle&&FVector::DotProduct(GetActorForwardVector(),Delta.GetSafeNormal())<FMath::Cos(FMath::DegreesToRadians(SightAngle*.5f)))return false;
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCSight),true,this);
 return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Aim,ECC_Visibility,Query)||Hit.GetActor()==Target;
}
bool APatrolEnemy::CanSeePlayer()const {auto* P=UGameplayStatics::GetPlayerPawn(this,0);return IsLiving(P)&&CanSeeActor(P);}
float APatrolEnemy::PlayerWarningRange()const
{
 const auto* Config=GetDefault<UGlobalData>();auto* PC=UGameplayStatics::GetPlayerController(this,0);
 auto* Equipment=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
 auto* Camera=Equipment?Equipment->Camcorder.Get():nullptr;
 if(Camera&&Camera->bHeld&&Camera->bPowered&&(Camera->bViewfinderRaised||Camera->bRecording)){
  FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
  if(FVector::DotProduct(View.Vector(),(GetActorLocation()-Eye).GetSafeNormal())>.85f)return Config->GangFilmingWarningDistance;
 }
 return Config->GangWarningDistance;
}
void APatrolEnemy::Say(const FString& Text,USoundBase* Sound)
{
 Speech->SetText(FText::FromString(Text));Speech->SetVisibility(true);SpeechTime=2.5f;
 if(Sound){Voice->SetSound(Sound);Voice->SetVolumeMultiplier(.7f);Voice->Play();}
}
void APatrolEnemy::SetActivity(ENPCActivity Next)
{
 if(Next!=ENPCActivity::Idle&&Next!=ENPCActivity::Patrol&&bPatrolPhaseActive){ExitPatrolPhase(PatrolPhase);bPatrolPhaseActive=false;}
 if(Activity==Next)return;Activity=Next;StateTime=0;CurrentAnimation=nullptr;bCombat=Next==ENPCActivity::Combat;bSocialArrived=false;
 bTurning=false;
 Rifle->SetVisibility(false);
 TradeParcel->SetVisibility(Next==ENPCActivity::Trade);
 OnActivityChanged(Next);GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("NPCActivity"),float(Next));
}
void APatrolEnemy::ReleasePartner()
{
 if(auto* Other=Partner.Get())if(Other->Partner.Get()==this){Other->Partner.Reset();Other->SocialCooldown=10;if(Other->Activity==ENPCActivity::Trade||Other->Activity==ENPCActivity::Threaten){Other->SetActivity(ENPCActivity::Idle);}}
 Partner.Reset();
}
void APatrolEnemy::SetTarget(AActor* Target)
{
 if(!IsLiving(Target)||IsProtectedPerson())return;
 if(CombatTarget!=Target){ReleasePartner();ShotTime=.7f;LostTime=0;}
 CombatTarget=Target;LastKnown=Target->GetActorLocation();SetActivity(ENPCActivity::Combat);
}
void APatrolEnemy::FleeFrom(FVector Location)
{
 if(NPCRole!=ENPCRole::Civilian&&NPCRole!=ENPCRole::Merchant)return;
 const bool NewPanic=Activity!=ENPCActivity::Flee;const bool ChangedDanger=FVector::DistSquared(Location,DangerLocation)>FMath::Square(400.f);
 ReleasePartner();DangerLocation=Location;PanicTime=GetDefault<UGlobalData>()->NPCPanicSeconds;
 SetActivity(ENPCActivity::Flee);GetCharacterMovement()->MaxWalkSpeed=430;
 if(NewPanic)Say(TEXT("AAAH!"),ScreamSound);
 if(NewPanic||ChangedDanger){if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;MoveRetry=0;}
}
void APatrolEnemy::HearGunshot(AActor* Shooter,FVector Location,float Range)
{
 if(Health<=0||Shooter==this||NPCRole==ENPCRole::Injured)return;
 const float Hearing=FMath::Min(Range,GetDefault<UGlobalData>()->NPCHearingRange);
 const bool Heard=FVector::DistSquared(GetActorLocation(),Location)<=FMath::Square(Hearing);
 const bool Seen=CanSeeActor(Shooter);
 if(!Heard&&!Seen)return;
 if(IsGang())if(auto* Officer=Cast<APatrolEnemy>(Shooter);Officer&&Officer->NPCRole==ENPCRole::Police&&Officer->CombatTarget==this&&Seen){
  auto* Player=UGameplayStatics::GetPlayerPawn(this,0);if(CombatTarget!=Player||!CanSeePlayer())SetTarget(Officer);
 }
 if(NPCRole==ENPCRole::Civilian||NPCRole==ENPCRole::Merchant){FleeFrom(Location);return;}
 if(NPCRole==ENPCRole::Police){
  if(auto* Gang=Cast<APatrolEnemy>(Shooter);Gang&&Gang->IsGang()&&Seen){ObservedShooter=Gang;if(!IsLiving(PriorityOffender.Get()))SetTarget(Gang);}
  if(!IsLiving(CombatTarget)){InvestigationLocation=Location;InvestigationTime=15;MoveRetry=0;SetActivity(ENPCActivity::Investigate);}
 }
}
void APatrolEnemy::WitnessViolence(AActor* Victim,APawn* Attacker,bool ProtectedVictim)
{
 if(Health<=0||!IsValid(Attacker)||NPCRole==ENPCRole::Injured)return;
 if((NPCRole==ENPCRole::Civilian||NPCRole==ENPCRole::Merchant)&&(Victim==this||CanSeeActor(Victim))){FleeFrom(Attacker->GetActorLocation());return;}
 if(NPCRole!=ENPCRole::Police)return;
 // Nearby officers react to the victim's distress, while distant officers require sight.
 if(Victim!=this&&FVector::DistSquared(GetActorLocation(),Victim->GetActorLocation())>FMath::Square(SightRange)&&!CanSeeActor(Victim))return;
 if(Attacker->IsPlayerControlled()&&(ProtectedVictim||Victim==this)){PriorityOffender=Attacker;SetTarget(Attacker);return;}
 if(auto* Gang=Cast<APatrolEnemy>(Attacker);Gang&&Gang->IsGang()&&(ProtectedVictim||Cast<AGame_Jam_Clean_VersCharacter>(Victim)||Victim==this)){
  ObservedShooter=Gang;if(!IsLiving(PriorityOffender.Get()))SetTarget(Gang);
 }
}
void APatrolEnemy::BecomeInjured()
{
 if(Health<=0)return;ReleasePartner();NPCRole=ENPCRole::Injured;CombatTarget=nullptr;PriorityOffender.Reset();
 if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();GetCharacterMovement()->DisableMovement();
 SetActivity(ENPCActivity::Injured);Rifle->SetVisibility(false);Speech->SetRelativeLocation(FVector(0,0,-15));Say(TEXT("HELP!"),HelpSound);SetMotion();
}
float APatrolEnemy::TakeDamage(float Damage,const FDamageEvent&,AController* EventInstigator,AActor* Causer)
{
 if(Health<=0||Damage<=0)return 0;
 auto* Attacker=EventInstigator?EventInstigator->GetPawn().Get():Cast<APawn>(Causer);

 const float Applied=FMath::Min(Damage,Health);Health-=Applied;
 if(Health<=0){ReleasePartner();CombatTarget=nullptr;SetActivity(ENPCActivity::Dead);Speech->SetVisibility(false);Voice->Stop();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();GetCharacterMovement()->DisableMovement();GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);NPCBody->SetVisibility(false);GetMesh()->SetVisibility(true);if(HeldWeapon)HeldWeapon->SetActorHiddenInGame(true);GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));GetMesh()->SetSimulatePhysics(true);Rifle->SetVisibility(false);SetLifeSpan(30);}
 else if(NPCRole!=ENPCRole::Injured&&Health<=GetDefault<UGlobalData>()->NPCInjuryHealth)BecomeInjured();
 else if(IsGang()&&Attacker){
  // Police support must not repeatedly reset an already-expired player warning.
  auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
  if(Attacker==Player||CombatTarget!=Player||!CanSeePlayer())SetTarget(Attacker);
 }
 else if(IsProtectedPerson()&&Attacker)FleeFrom(Attacker->GetActorLocation());
 else if(NPCRole==ENPCRole::Police&&Attacker&&Attacker->IsPlayerControlled()){PriorityOffender=Attacker;SetTarget(Attacker);}
 GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("Health"),Health);return Applied;
}
void APatrolEnemy::Face(FVector Point,float DT)
{
 const float Delta=FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,(Point-GetActorLocation()).Rotation().Yaw);TurnDelta=Delta;
 if(GetVelocity().Size2D()>10)
 {
  bTurning=false;
  SetActorRotation(FRotator(0,GetActorRotation().Yaw+FMath::Clamp(Delta,-130*DT,130*DT),0));return;
 }
 if(!bTurning&&FMath::Abs(Delta)>8)
 {
  TurnAngle=Delta;TurnStartYaw=GetActorRotation().Yaw;TurnTime=0;
  TurnNominalAngle=FMath::Abs(Delta)>135?180:90;
  ActiveTurn=Delta>0?(TurnNominalAngle>90?TurnRight180:TurnRight):(TurnNominalAngle>90?TurnLeft180:TurnLeft);
  TurnRate=Activity==ENPCActivity::Combat||Activity==ENPCActivity::WarnPlayer?3.5f:1.75f;
  bTurning=ActiveTurn!=nullptr;
 }
 if(bTurning)
 {
  TurnTime=FMath::Min(TurnTime+DT*TurnRate,ActiveTurn->GetPlayLength());
  const float RootYaw=ActiveTurn->ExtractRootMotionFromRange(0,TurnTime,FAnimExtractContext(TurnTime,false)).Rotator().Yaw;
  const float Fraction=FMath::Clamp(FMath::Abs(RootYaw)/TurnNominalAngle,0.f,1.f);
  SetActorRotation(FRotator(0,TurnStartYaw+TurnAngle*Fraction,0));
  if(TurnTime>=ActiveTurn->GetPlayLength()){SetActorRotation(FRotator(0,TurnStartYaw+TurnAngle,0));bTurning=false;}
 }
}
bool APatrolEnemy::MoveTo(FVector Point,float Acceptance)
{
 auto* AI=Cast<AAIController>(GetController());if(!AI)return false;
 const auto Result=AI->MoveToLocation(Point,Acceptance,true,true,true,false,nullptr,false);
 bMoving=Result==EPathFollowingRequestResult::RequestSuccessful;Destination=Point;MoveTimeout=12;return Result!=EPathFollowingRequestResult::Failed;
}
bool APatrolEnemy::ChooseDestination(bool Flee)
{
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 if(!Flee&&PatrolPoints.Num()){
  FVector Point=PatrolPoints[RouteIndex++%PatrolPoints.Num()];auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(this,GetActorLocation(),Point,this);
  return Path&&Path->IsValid()&&!Path->IsPartial()&&MoveTo(Point);
 }
 FVector Best=FVector::ZeroVector;float BestScore=-BIG_NUMBER;bool Found=false;
 for(int32 I=0;I<10;++I){FNavLocation Candidate;
  const FVector Center=Flee?GetActorLocation():Home;const float Radius=Flee?1100:PatrolRadius;
  if(!Nav->GetRandomReachablePointInRadius(Center,Radius,Candidate))continue;
  if(FVector::Dist2D(Candidate.Location,GetActorLocation())<220)continue;
  auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(this,GetActorLocation(),Candidate.Location,this);
  if(!Path||!Path->IsValid()||Path->IsPartial())continue;
  const float Score=Flee?FVector::DistSquared2D(Candidate.Location,DangerLocation):FMath::FRand();
  if(Flee&&Score<FVector::DistSquared2D(GetActorLocation(),DangerLocation)+10000)continue;
  if(Score>BestScore){Best=Candidate.Location;BestScore=Score;Found=true;}
 }
 return Found&&MoveTo(Best);
}
void APatrolEnemy::Think()
{
 if(IsGang()){
  auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
  if(Activity==ENPCActivity::WarnPlayer)return;
  if(CombatTarget==Player&&IsLiving(Player))return;
  const float Range=PlayerWarningRange();
  if(IsLiving(Player)&&FVector::DistSquared(GetActorLocation(),Player->GetActorLocation())<=FMath::Square(Range)&&CanSeeActor(Player,false)){
   ReleasePartner();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;
   WarningRadius=Range;WarningRemaining=GetDefault<UGlobalData>()->GangWarningSeconds;SetActivity(ENPCActivity::WarnPlayer);Say(TEXT("BACK OFF!"));return;
  }
 }
 if(NPCRole==ENPCRole::Police&&IsLiving(PriorityOffender.Get())){if(CombatTarget!=PriorityOffender.Get())SetTarget(PriorityOffender.Get());return;}
 if(IsLiving(CombatTarget)||Activity==ENPCActivity::Flee||Activity==ENPCActivity::Investigate||Partner.IsValid()||SocialCooldown>0||!IsGang())return;
 if(NPCRole==ENPCRole::GangBoss){if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();SetActivity(ENPCActivity::Command);SocialCooldown=14;Say(TEXT("MOVE!"));return;}
 APatrolEnemy* Best=nullptr;float Dist=FMath::Square(650.f);
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It){auto* Other=*It;
  if(Other==this||!IsLiving(Other)||Other->Partner.IsValid()||Other->bCombat||Other->Activity==ENPCActivity::Flee||Other->NPCRole==ENPCRole::Police||Other->SocialCooldown>0)continue;
  const float D=FVector::DistSquared(GetActorLocation(),Other->GetActorLocation());if(D<Dist&&CanSeeActor(Other)){Best=Other;Dist=D;}
 }
 // A failed search is not an interaction cooldown: otherwise two staggered
 // gang members can indefinitely keep excluding each other from trading.
 if(!Best)return;
 Partner=Best;Best->Partner=this;
 const bool Trade=Best->IsGang();SetActivity(Trade?ENPCActivity::Trade:ENPCActivity::Threaten);Best->SetActivity(Trade?ENPCActivity::Trade:ENPCActivity::Threaten);
 if(auto* AI=Cast<AAIController>(Best->GetController()))AI->StopMovement();MoveRetry=0;bMeleeApplied=false;
}
void APatrolEnemy::TickWarning(float DT)
{
 if(bAimWarning)
 {
  if(!IsPlayerAimingAtMe())
  {
   bAimWarning=false;WarningRemaining=0;Speech->SetVisibility(false);
   SetActivity(IsLiving(CombatTarget)?ENPCActivity::Combat:ENPCActivity::Idle);DecisionTime=.25f;return;
  }
  auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
  Face(Player->GetActorLocation(),DT);
  WarningRemaining=FMath::Max(0.f,WarningRemaining-DT);
  if(WarningRemaining<=0){bAimWarning=false;SetTarget(Player);ShotTime=0;TickCombat(DT);}
  return;
 }
 auto* Player=UGameplayStatics::GetPlayerPawn(this,0);WarningRadius=FMath::Max(WarningRadius,PlayerWarningRange());
 if(!IsLiving(Player)||FVector::DistSquared(GetActorLocation(),Player->GetActorLocation())>FMath::Square(WarningRadius)||!CanSeeActor(Player,false)){
  WarningRemaining=0;Speech->SetVisibility(false);SetActivity(IsLiving(CombatTarget)?ENPCActivity::Combat:ENPCActivity::Idle);MoveRetry=1;return;
 }
 Face(Player->GetActorLocation(),DT);WarningRemaining=FMath::Max(0.f,WarningRemaining-DT);
 if(WarningRemaining<=0){SetTarget(Player);ShotTime=0;}
 else if(SpeechTime<=0)Say(TEXT("BACK OFF!"));
}
void APatrolEnemy::TickSocial(float DT)
{
 if(Activity==ENPCActivity::Command){if(StateTime>FMath::Max(2.5f,CurrentAnimation?CurrentAnimation->GetPlayLength():0.f))SetActivity(ENPCActivity::Idle);return;}
 // The victim can release the pair immediately on impact; finish the punch recovery anyway.
 if(Activity==ENPCActivity::Beat&&bMeleeApplied){
  if(StateTime>=FMath::Max(.8f,CurrentAnimation?CurrentAnimation->GetPlayLength():0.f)){ReleasePartner();SetActivity(ENPCActivity::Idle);}
  return;
 }
 auto* Other=Partner.Get();if(!Other||!IsLiving(Other)){ReleasePartner();SetActivity(ENPCActivity::Idle);return;}
 Face(Other->GetActorLocation(),DT);const float Distance=FVector::Dist2D(GetActorLocation(),Other->GetActorLocation());
 if(Distance>110){
  if(!IsGang())return;
  if(MoveRetry<=0){MoveRetry=1;MoveTo(Other->GetActorLocation(),45);}
  if(StateTime>10){ReleasePartner();SocialCooldown=6;SetActivity(ENPCActivity::Idle);}return;
 }
 if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
 if(!bSocialArrived){bSocialArrived=true;StateTime=0;}
 if(Activity==ENPCActivity::Trade){
  if(StateTime>FMath::Max(3.f,CurrentAnimation?CurrentAnimation->GetPlayLength():0.f)){++CompletedTrades;++Other->CompletedTrades;Say(TEXT("DEAL."));Other->Say(TEXT("DEAL."));GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("Trades"),CompletedTrades);ReleasePartner();SocialCooldown=12;SetActivity(ENPCActivity::Idle);}
 }else if(IsGang()){
  if(Activity==ENPCActivity::Threaten){if(SpeechTime<=0)Say(TEXT("PAY UP!"));if(StateTime>FMath::Max(2.f,CurrentAnimation?CurrentAnimation->GetPlayLength():0.f)){SetActivity(ENPCActivity::Beat);bMeleeApplied=false;}}
  else if(Activity==ENPCActivity::Beat&&StateTime>.55f&&!bMeleeApplied){
   bMeleeApplied=true;
   if(Distance<175&&CanSeeActor(Other,false)){FHitResult Hit(Other,Other->GetMesh(),Other->GetActorLocation(),FVector::UpVector);Hit.ImpactPoint=Other->GetActorLocation();GetWorld()->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,GetDefault<UGlobalData>()->NPCMeleeDamage,(Other->GetActorLocation()-GetActorLocation()).GetSafeNormal(),GetController(),this);}
   ReleasePartner();SocialCooldown=15;
  }
 }
}
void APatrolEnemy::TickCombat(float DT)
{
 if(!IsLiving(CombatTarget)){CombatTarget=nullptr;SetActivity(ENPCActivity::Idle);return;}
 const bool Seen=CanSeeActor(CombatTarget,false);
 if(Seen)
 {
  LastKnown=CombatTarget->GetActorLocation();LostTime=0;
  const auto* MovementConfig=GetDefault<UGlobalData>();
  GetCharacterMovement()->MaxWalkSpeed=MovementConfig->NPCCombatAdvanceSpeed;
  if(FVector::Dist2D(GetActorLocation(),LastKnown)>MovementConfig->NPCCombatStopDistance+35)
  {if(MoveRetry<=0){MoveRetry=.35f;MoveTo(LastKnown,MovementConfig->NPCCombatStopDistance);}}
  else {if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;}
 }
 else {GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCSearchSpeed;LostTime+=DT;if(MoveRetry<=0){MoveRetry=1;MoveTo(LastKnown);}if(LostTime>8){CombatTarget=nullptr;PriorityOffender.Reset();SetActivity(ENPCActivity::Idle);return;}}
 Face(LastKnown,DT);
 if(!Seen||ShotTime>0||FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,(LastKnown-GetActorLocation()).Rotation().Yaw))>8)return;
 const auto* Config=GetDefault<UGlobalData>();const bool Police=NPCRole==ENPCRole::Police;
 const bool Shotgun=WeaponKind==ETacticalWeaponKind::Shotgun;ShotTime=Police?FMath::Max(.5f,Config->PoliceShotInterval):Shotgun?1.3f:WeaponKind==ETacticalWeaponKind::Pistol?.9f:.65f;
 FVector Start=HeldWeapon?HeldWeapon->MuzzleLocation():GetActorLocation()+FVector(0,0,50)+GetActorForwardVector()*45;
 FVector Aim=CombatTarget->GetActorLocation()+FVector(0,0,25);
 const auto* TargetNPC=Cast<APatrolEnemy>(CombatTarget);const bool FightingPolice=IsGang()&&TargetNPC&&TargetNPC->NPCRole==ENPCRole::Police;
 const int32 Pellets=Shotgun?8:1;
 auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>();Bus->ReportGunshot(this,Start);
 for(int32 Pellet=0;Pellet<Pellets;++Pellet){
  const float Spread=Police?Config->PoliceSpreadDegrees:FightingPolice?7.f:Shotgun?5.f:2.f;
  const FVector Dir=FMath::VRandCone((Aim-Start).GetSafeNormal(),FMath::DegreesToRadians(Spread));
  FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCFire),true,this);if(HeldWeapon)Query.AddIgnoredActor(HeldWeapon);
  const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Dir*SightRange,ECC_Visibility,Query);bool Hurt=false;
  if(Blocked){auto* HitNPC=Cast<APatrolEnemy>(Hit.GetActor());
   const bool Allowed=Police?Hit.GetActor()==CombatTarget:(Hit.GetActor()==CombatTarget||(HitNPC&&!HitNPC->IsGang()));
   const float Damage=Police?Config->PoliceShotDamage:(HitNPC&&HitNPC->NPCRole==ENPCRole::Police?Config->GangVsPoliceDamage:Shotgun?16.f:WeaponKind==ETacticalWeaponKind::Pistol?6.f:8.f);
   if(Allowed)Hurt=Bus->TransmitHit(Hit,Damage/Pellets,Dir,GetController(),this)>0;
  }
  if(auto* FX=GetWorld()->SpawnActor<ACombatShotEffect>())FX->Setup(Start,Blocked?Hit.ImpactPoint:Start+Dir*SightRange,Blocked,Hurt);
 }
 if(HeldWeapon)HeldWeapon->Flash();
 // NPC firing deliberately keeps the aim pose; Flash supplies audio/VFX only.

}
void APatrolEnemy::ChangePatrolPhase(EPatrolPhase Next)
{
 if(bPatrolPhaseActive)ExitPatrolPhase(PatrolPhase);
 PatrolPhase=Next;PatrolPhaseTime=0;bPatrolPhaseActive=true;EnterPatrolPhase(Next);
}
void APatrolEnemy::EnterPatrolPhase(EPatrolPhase Phase)
{
 if(Phase==EPatrolPhase::Walking)
 {
  GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCWalkSpeed;
  PatrolLastPosition=GetActorLocation();PatrolBlockedTime=0;
  MoveTo(Destination);SetActivity(ENPCActivity::Patrol);
 }
 else
 {
  if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;
  SetActivity(ENPCActivity::Idle);
  if(Phase==EPatrolPhase::Turning)
  {
   // Preserve navigation/path validation while adopting RogueBox's wait-turn-walk phases.
   const bool Found=(PatrolRadius>0||PatrolPoints.Num())&&ChooseDestination(false);
   if(!Found)Destination=GetActorLocation();
   if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;
   PatrolDirection=Found?(Destination-GetActorLocation()).GetSafeNormal2D():FRotator(0,GetActorRotation().Yaw+FMath::FRandRange(-100.f,100.f),0).Vector();
   bTurning=false;
  }
 }
}
void APatrolEnemy::ExitPatrolPhase(EPatrolPhase Phase)
{
 if(Phase==EPatrolPhase::Turning)bTurning=false;
}
void APatrolEnemy::UpdatePatrolWalking(float DT)
{
 auto* AI=Cast<AAIController>(GetController());
 if(FVector::Dist2D(GetActorLocation(),PatrolLastPosition)<1.f)PatrolBlockedTime+=DT;
 else {PatrolBlockedTime=0;PatrolLastPosition=GetActorLocation();}
 if(!AI||AI->GetMoveStatus()!=EPathFollowingStatus::Moving||MoveTimeout<=0||PatrolBlockedTime>1.5f)
 {ChangePatrolPhase(EPatrolPhase::WaitingBeforeTurn);return;}
 if(GetVelocity().Size2D()>10)Face(GetActorLocation()+GetVelocity(),DT);
}
void APatrolEnemy::UpdatePatrolWaiting(float DT)
{
 if(PatrolPhaseTime<(PatrolPhase==EPatrolPhase::WaitingBeforeTurn?2.f:1.f))return;
 if(PatrolPhase==EPatrolPhase::WaitingBeforeTurn)ChangePatrolPhase(EPatrolPhase::Turning);
 else ChangePatrolPhase((PatrolRadius>0||PatrolPoints.Num())?EPatrolPhase::Walking:EPatrolPhase::WaitingBeforeTurn);
}
void APatrolEnemy::UpdatePatrolTurning(float DT)
{
 Face(GetActorLocation()+PatrolDirection*100,DT);
 if(!bTurning)ChangePatrolPhase(EPatrolPhase::WaitingAfterTurn);
}
void APatrolEnemy::TickPatrol(float DT)
{
 auto* AI=Cast<AAIController>(GetController());
 if(Activity==ENPCActivity::Investigate){
  GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCWalkSpeed;
  InvestigationTime-=DT;
  if(InvestigationTime<=0){if(AI)AI->StopMovement();SetActivity(ENPCActivity::Idle);MoveRetry=2;return;}
  if(FVector::Dist2D(GetActorLocation(),InvestigationLocation)>150){if(MoveRetry<=0){MoveRetry=2;MoveTo(InvestigationLocation,110);}if(GetVelocity().Size2D()>10)Face(GetActorLocation()+GetVelocity(),DT);}
  else {if(AI)AI->StopMovement();if(!bTurning&&NextLookTime<=0){ScanYaw=GetActorRotation().Yaw+90;NextLookTime=3;}NextLookTime-=DT;Face(GetActorLocation()+FRotator(0,ScanYaw,0).Vector()*100,DT);}
  return;
 }
 if(Activity==ENPCActivity::Flee)
 {
  PanicTime-=DT;
  if(PanicTime<=0){if(AI)AI->StopMovement();bMoving=false;GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCWalkSpeed;SetActivity(ENPCActivity::Idle);return;}
  if(bMoving&&(!AI||AI->GetMoveStatus()!=EPathFollowingStatus::Moving||MoveTimeout<=0)){if(AI)AI->StopMovement();bMoving=false;MoveRetry=.25f;}
  if(!bMoving&&MoveRetry<=0){MoveRetry=1;ChooseDestination(true);}
  if(GetVelocity().Size2D()>10)Face(GetActorLocation()+GetVelocity(),DT);
  return;
 }
 if(!bPatrolPhaseActive)ChangePatrolPhase(EPatrolPhase::WaitingBeforeTurn);
 PatrolPhaseTime+=DT;
 switch(PatrolPhase)
 {
  case EPatrolPhase::Walking:UpdatePatrolWalking(DT);break;
  case EPatrolPhase::Turning:UpdatePatrolTurning(DT);break;
  default:UpdatePatrolWaiting(DT);break;
 }
}
void APatrolEnemy::SetMotion()
{
 auto* Motion=Cast<UNPCMotionAnimInstance>(GetMesh()->GetAnimInstance());if(!Motion)return;
 const float Speed=GetVelocity().Size2D();const bool Moving=Speed>5;
 const bool Armed=!IsProtectedPerson()&&Activity!=ENPCActivity::Threaten&&Activity!=ENPCActivity::Beat&&Activity!=ENPCActivity::Trade&&Activity!=ENPCActivity::Command;
 UAnimSequence* Anim=nullptr;float ReferenceSpeed=300.f;bool Loop=true;
 if(bTurning&&!Moving){Anim=ActiveTurn;Loop=false;}
 else if(Activity==ENPCActivity::Flee){if(Moving){if(auto* Found=ActivityAnimations.Find(Activity))Anim=*Found;ReferenceSpeed=461.07f;}else Anim=UnarmedIdle;}
 else if(!Moving)
 {
  const bool Social=Activity==ENPCActivity::Trade||Activity==ENPCActivity::Threaten;
  if(!Social||bSocialArrived)if(auto* Found=ActivityAnimations.Find(Activity))Anim=*Found;
 }
 if(!Anim)
 {
  if(Armed)
  {
   if(Moving&&Speed>180&&Run){Anim=Run;ReferenceSpeed=428.6f;}
   else if(Moving&&Activity==ENPCActivity::Combat&&AimWalk)
   {
    const float Angle=FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(GetVelocity(),GetActorRightVector()),FVector::DotProduct(GetVelocity(),GetActorForwardVector())));
    const int32 Direction=(FMath::RoundToInt(Angle/45.f)+8)%8;
    static const float Speeds[]={110.2f,168.28f,157.91f,178.43f,173.67f,186.36f,161.37f,177.5f};
    Anim=AimWalkDirections.IsValidIndex(Direction)&&AimWalkDirections[Direction]?AimWalkDirections[Direction].Get():AimWalk.Get();ReferenceSpeed=Speeds[Direction];
   }
   else {Anim=Moving?Walk.Get():Idle.Get();ReferenceSpeed=WeaponKind==ETacticalWeaponKind::Pistol?110.2f:111.4f;}
  }
  else Anim=Moving?UnarmedWalk.Get():UnarmedIdle.Get();
 }
 CurrentAnimation=Anim;Motion->Sequence=Anim;Motion->UpperBodyPose=Armed?Idle.Get():nullptr;
 Motion->ExplicitTime=bTurning&&!Moving?TurnTime:-1.f;
 Motion->bLoop=Loop;Motion->bLocomotion=Moving;Motion->PlaybackRate=Moving?FMath::Clamp(Speed/ReferenceSpeed,.05f,2.f):1.f;
}
void APatrolEnemy::Tick(float DT)
{
 Super::Tick(DT);if(Health<=0)return;TurnDelta=0;StateTime+=DT;DecisionTime-=DT;MoveRetry-=DT;MoveTimeout-=DT;ShotTime-=DT;SocialCooldown-=DT;SpeechTime-=DT;
 if(SpeechTime<=0)Speech->SetVisibility(false);
 if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))Speech->SetWorldRotation((P->GetActorLocation()-Speech->GetComponentLocation()).Rotation());
 if(NPCRole==ENPCRole::Injured){if(StateTime>7){StateTime=0;Say(TEXT("HELP!"),HelpSound);}SetMotion();UpdateWeaponPose(DT);return;}
 if(!IsProtectedPerson() && !bAimWarning && CombatTarget!=UGameplayStatics::GetPlayerPawn(this,0) && IsPlayerAimingAtMe())
 {
  ReleasePartner();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;
  bAimWarning=true;WarningRemaining=GetDefault<UGlobalData>()->AimedThreatSeconds;
  SetActivity(ENPCActivity::WarnPlayer);Say(TEXT("LOWER YOUR WEAPON!"));
 }
 if(DecisionTime<=0){DecisionTime=.25f;Think();}
 if(Activity==ENPCActivity::WarnPlayer)TickWarning(DT);
 else if(Activity==ENPCActivity::Combat)TickCombat(DT);
 else if(Activity==ENPCActivity::Threaten||Activity==ENPCActivity::Beat||Activity==ENPCActivity::Trade||Activity==ENPCActivity::Command)TickSocial(DT);
 else TickPatrol(DT);
 SetMotion();UpdateWeaponPose(DT);
}

bool APatrolEnemy::IsPlayerAimingAtMe() const
{
 auto* Player=Cast<AGame_Jam_Clean_VersCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
 auto* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
 auto* Equipment=PC?PC->FindComponentByClass<UCombatEquipmentComponent>():nullptr;
 if(!Player||Player->Health<=0||!Equipment||!Equipment->ActiveWeapon||!Equipment->bADS)return false;
 const FVector Start=Player->GetFirstPersonCameraComponent()->GetComponentLocation();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(AimedThreat),true,Player);Query.AddIgnoredActor(Equipment->ActiveWeapon);
 FHitResult Hit;
 return GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+PC->GetControlRotation().Vector()*SightRange,ECC_Visibility,Query)&&
  (Hit.GetActor()==this||(HeldWeapon&&Hit.GetActor()==HeldWeapon));
}




void APatrolEnemy::UpdateWeaponPose(float DT)
{
 const bool Armed=Health>0&&!IsProtectedPerson()&&HeldWeapon&&Activity!=ENPCActivity::Threaten&&Activity!=ENPCActivity::Beat&&Activity!=ENPCActivity::Trade&&Activity!=ENPCActivity::Command;
 if(Armed!=bPreviousArmed&&!LastPresentedPose.IsEmpty()){PresentationBlendFrom=LastPresentedPose;PresentationBlendTime=0;}
 bPreviousArmed=Armed;PresentationBlendTime+=DT;
 if(HeldWeapon)HeldWeapon->SetActorHiddenInGame(!Armed||PresentationBlendTime<.18f);
 Rifle->SetVisibility(false);NPCBody->SetVisibility(true);GetMesh()->SetVisibility(false,false);
 WeaponReadyAlpha=FMath::FInterpConstantTo(WeaponReadyAlpha,Activity==ENPCActivity::WarnPlayer||Activity==ENPCActivity::Combat?1.f:0.f,DT,4.f);
 if(Armed)HeldWeapon->UpdateNPCGripPose(GetMesh(),NPCBody,WeaponReadyAlpha);
 else NPCBody->CopyPoseFromSkeletalComponent(GetMesh());
 // Blend the actually rendered grip pose when entering/leaving social states.
 // Blending only the source animation misses the procedural arm lowering and IK.
 const float Alpha=FMath::SmoothStep(0.f,.18f,PresentationBlendTime);
 if(Alpha<1&&PresentationBlendFrom.Num()==NPCBody->BoneSpaceTransforms.Num())
  for(int32 B=0;B<NPCBody->BoneSpaceTransforms.Num();++B){FTransform Pose;Pose.Blend(PresentationBlendFrom[B],NPCBody->BoneSpaceTransforms[B],Alpha);NPCBody->BoneSpaceTransforms[B]=Pose;}
 LastPresentedPose=NPCBody->BoneSpaceTransforms;
 NPCBody->MarkRefreshTransformDirty();NPCBody->RefreshBoneTransforms();
}

