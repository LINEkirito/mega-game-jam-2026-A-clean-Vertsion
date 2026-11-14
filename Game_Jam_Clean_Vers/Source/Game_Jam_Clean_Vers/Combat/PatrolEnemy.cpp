#include "PatrolEnemy.h"
#include "Audio/GameAudio.h"
#include "Audio/CharacterAudio.h"
#include "WorldEvents.h"
#include "Data/GlobalData.h"
#include "Data/DataTransmitter.h"
#include "TacticalWeapon.h"
#include "CombatEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
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

namespace
{
 bool IsActivePoliceGangCrossfire(const APatrolEnemy* Shooter,const AActor* Target)
 {
  const auto* Other=Cast<APatrolEnemy>(Target);
  return IsValid(Other)&&Other->Health>0&&
   ((Shooter->NPCRole==ENPCRole::Police&&Other->IsGang())||
    (Shooter->IsGang()&&Other->NPCRole==ENPCRole::Police));
 }
}

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
 Voice=CreateDefaultSubobject<UAudioComponent>(TEXT("NPCVoice"));Voice->SetupAttachment(GetRootComponent());Voice->SetAutoActivate(false);Voice->bOverrideAttenuation=true;Voice->AttenuationOverrides=CharacterAudio::Settings(ECharacterSound::Voice);Voice->SetRelativeLocation(FVector(0,0,65));
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
 Super::BeginPlay();IdentityRole=NPCRole;Home=GetActorLocation();ScanYaw=GetActorRotation().Yaw;DecisionTime=FMath::FRandRange(0,.25f);SocialCooldown=FMath::FRandRange(3.f,7.f);MoveRetry=FMath::FRandRange(1.f,3.f);
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
  const FString Root=TEXT("/Game/Combat/NPC/FullBody/NPC_A_")+Type;
  if(auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("_Aim_Idle"))))Idle=Clip;
  if(auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("_Walk_Forward"))))Walk=Clip;
  AimWalk=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("_Aim_Walk_Forward")));
  const TCHAR* Directions[]={TEXT("Forward"),TEXT("Forward_Right"),TEXT("Right"),TEXT("Backward_Right"),TEXT("Backward"),TEXT("Backward_Left"),TEXT("Left"),TEXT("Forward_Left")};
  // Keep each weapon's complete authored pose, including the torso and both arms.
  for(const TCHAR* Direction:Directions)AimWalkDirections.Add(LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("_Aim_Walk_")+Direction)));
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
  if(HeldWeapon){HeldWeapon->Kind=WeaponKind;HeldWeapon->FinishSpawning(Transform);HeldWeapon->EquipForNPC(GetMesh());
   const FString ReloadType=WeaponKind==ETacticalWeaponKind::Pistol?TEXT("Pistol"):WeaponKind==ETacticalWeaponKind::Shotgun?TEXT("Shotgun"):TEXT("Rifle");
   ReloadAnimation=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Combat/NPC/FullBody/NPC_A_")+ReloadType+TEXT("_Reload")));
   HeldWeapon->NPCReloadDuration=ReloadAnimation?ReloadAnimation->GetPlayLength():0;
   AmmoReserve=NewObject<UBackpackInventory>(this);
   AmmoReserve->Add(HeldWeapon->Spec().Ammo,TEXT("NPC ammunition"),TEXT("Personal reserve"),FMath::Max(0,GetDefault<UGlobalData>()->NPCSpareMagazines)*HeldWeapon->Spec().Capacity);}
 }
 const TPair<ENPCActivity,const TCHAR*> Defaults[]={
  {ENPCActivity::Threaten,TEXT("AS_Taunt_Fist_01")},{ENPCActivity::Beat,TEXT("Manny_A_Melee")},{ENPCActivity::Trade,TEXT("AS_Greetings_FormalHandShake")},
  {ENPCActivity::Counterattack,TEXT("Manny_A_Melee")},{ENPCActivity::Meeting,TEXT("AS_Standing_Talking_01")},{ENPCActivity::CameraDismiss,TEXT("AS_Gestures_Pointing_Forward")},{ENPCActivity::Command,TEXT("AS_Gestures_Pointing_Forward")},{ENPCActivity::Injured,TEXT("Manny_SK_Martial_Anim")},{ENPCActivity::Flee,TEXT("Manny_A_UnarmedSprint")}};
 for(const auto& Pair:Defaults)if(!ActivityAnimations.Contains(Pair.Key))if(auto* Anim=LoadObject<UAnimSequence>(nullptr,*(FString(TEXT("/Game/Combat/NPC/Authored/"))+Pair.Value)))ActivityAnimations.Add(Pair.Key,Anim);
 // The old retargeted unarmed sprint carries a tilted, ~55 degree root offset.
 // Use the existing Manny-compatible running cycle; preserve explicit custom clips.
 if(auto* FleeClip=ActivityAnimations.Find(ENPCActivity::Flee);FleeClip&&*FleeClip&&(*FleeClip)->GetName()==TEXT("Manny_A_UnarmedSprint"))
  if(auto* Jog=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd")))
   *FleeClip=Jog;
 if(IsProtectedPerson())if(auto* Cower=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Combat/NPC/Authored/AS_Defeat_Beg_01")))ActivityAnimations.Add(ENPCActivity::Threaten,Cower);
 if(!ScreamSound)ScreamSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/NPC/Audio/Scream"));
 if(!HelpSound)HelpSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/NPC/Audio/Help"));
 if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>()){Bus->OnGunshot.AddDynamic(this,&APatrolEnemy::HearGunshot);Bus->OnViolence.AddDynamic(this,&APatrolEnemy::WitnessViolence);}
 InitializeWeaponCarry();
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
 const float ViewAngle=IsGang()?GetDefault<UGlobalData>()->GangSightAngle:SightAngle;
 if(CheckAngle&&FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(),Delta.GetSafeNormal2D())<FMath::Cos(FMath::DegreesToRadians(ViewAngle*.5f))-KINDA_SMALL_NUMBER)return false;
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
 if(Sound){Voice->bAllowSpatialization=true;Voice->bOverrideAttenuation=true;Voice->AdjustAttenuation(CharacterAudio::Settings(ECharacterSound::Voice));Voice->SetSound(Sound);Voice->SetVolumeMultiplier(.7f);Voice->Play();}
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
 if(!IsCombatTargetAlive(Target)||IsProtectedPerson())return;
 if(IsActivePoliceGangCrossfire(this,Target)&&(CrossfireCooldown>0||CastChecked<APatrolEnemy>(Target)->CrossfireCooldown>0))return;
 AssaultTarget.Reset();RetaliationTarget.Reset();if(EscalationTarget.Get()!=Target)EscalationTarget.Reset();
 if(CombatTarget!=Target){ReleasePartner();ShotTime=.7f;LostTime=0;}
 CombatTarget=Target;LastKnown=Target->GetActorLocation();SetActivity(ENPCActivity::Combat);
 if(auto* Events=GetWorld()->GetSubsystem<UWorldEvents>())Events->ObserveCombat(this,Cast<APatrolEnemy>(Target));
}
void APatrolEnemy::FleeFrom(FVector Location)
{
 if(NPCRole!=ENPCRole::Civilian&&NPCRole!=ENPCRole::Merchant)return;
 const bool NewPanic=Activity!=ENPCActivity::Flee;const bool ChangedDanger=FVector::DistSquared(Location,DangerLocation)>FMath::Square(400.f);
 RetaliationTarget.Reset();ReleasePartner();DangerLocation=Location;PanicTime=GetDefault<UGlobalData>()->NPCPanicSeconds;
 SetActivity(ENPCActivity::Flee);GetCharacterMovement()->MaxWalkSpeed=430;
 if(NewPanic){Say(TEXT("AAAH!"),ScreamSound);if(auto* Events=GetWorld()->GetSubsystem<UWorldEvents>())Events->ObserveEscape(this);}
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
  auto* Player=UGameplayStatics::GetPlayerPawn(this,0);if(!EscalationTarget.IsValid()&&(CombatTarget!=Player||!CanSeePlayer()))SetTarget(Officer);
 }
 if(NPCRole==ENPCRole::Civilian||NPCRole==ENPCRole::Merchant){FleeFrom(Location);return;}
 if(NPCRole==ENPCRole::Police){
  if(auto* Gang=Cast<APatrolEnemy>(Shooter);Gang&&Gang->IsGang()&&Seen){ObservedShooter=Gang;if(!IsLiving(PriorityOffender.Get()))SetTarget(Gang);}
  if(!IsLiving(CombatTarget)){InvestigationLocation=Location;InvestigationTime=15;MoveRetry=0;SetActivity(ENPCActivity::Investigate);}
 }
}
void APatrolEnemy::WitnessViolence(AActor* Victim,APawn* Attacker,bool ProtectedVictim)
{
 if(Health<=0||!IsValid(Victim)||!IsValid(Attacker)||NPCRole==ENPCRole::Injured)return;
 if(IsGang())
 {
  const auto* GangVictim=Cast<APatrolEnemy>(Victim);
  // Damage may already have killed or injured the victim before this world event.
  // Use its original identity. Sight witnesses retaliate; proximity alone only
  // makes an uninvolved ally look towards the attacker for a short time.
  if(GangVictim&&Attacker->IsPlayerControlled()&&
     (GangVictim->IdentityRole==ENPCRole::GangMember||GangVictim->IdentityRole==ENPCRole::GangBoss))
  {
   if(Victim==this||CanSeeActor(Victim))
   {
    GangAttackLookTarget.Reset();GangAttackLookUntil=0.f;SetTarget(Attacker);
   }
   else if(!IsCombatTargetAlive(CombatTarget)&&
       FVector::DistSquared(GetActorLocation(),Victim->GetActorLocation())<=FMath::Square(FMath::Max(0.f,GetDefault<UGlobalData>()->GangRetaliationRadius)))
   {
    GangAttackLookTarget=Attacker;GangAttackLookLocation=Attacker->GetActorLocation();
    GangAttackLookUntil=GetWorld()->GetTimeSeconds()+FMath::Max(0.f,GetDefault<UGlobalData>()->GangAttackObservationSeconds);
    if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();bMoving=false;
   }
  }
  return;
 }
 if(NPCRole==ENPCRole::Civilian||NPCRole==ENPCRole::Merchant){
  if(Victim!=this&&!RetaliationTarget.IsValid()&&CanSeeActor(Victim))FleeFrom(Attacker->GetActorLocation());
  return;
 }
 if(NPCRole!=ENPCRole::Police)return;
 // Nearby officers react to the victim's distress, while distant officers require sight.
 if(Victim!=this&&FVector::DistSquared(GetActorLocation(),Victim->GetActorLocation())>FMath::Square(SightRange)&&!CanSeeActor(Victim))return;
 if(Attacker->IsPlayerControlled()&&ProtectedVictim){PriorityOffender=Attacker;SetTarget(Attacker);return;}
 if(auto* Gang=Cast<APatrolEnemy>(Attacker);Gang&&Gang->IsGang()&&(ProtectedVictim||Cast<AGame_Jam_Clean_VersCharacter>(Victim)||Victim==this)){
  ObservedShooter=Gang;if(!IsLiving(PriorityOffender.Get()))SetTarget(Gang);
 }
}
void APatrolEnemy::BecomeInjured()
{
 if(Health<=0)return;AssaultTarget.Reset();RetaliationTarget.Reset();ReleasePartner();NPCRole=ENPCRole::Injured;CombatTarget=nullptr;PriorityOffender.Reset();
 if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();GetCharacterMovement()->DisableMovement();
 SetActivity(ENPCActivity::Injured);Rifle->SetVisibility(false);Speech->SetRelativeLocation(FVector(0,0,-15));Say(TEXT("HELP!"),HelpSound);SetMotion();
}
float APatrolEnemy::TakeDamage(float Damage,const FDamageEvent&,AController* EventInstigator,AActor* Causer)
{
 if(Health<=0||Damage<=0)return 0;
 auto* Attacker=EventInstigator?EventInstigator->GetPawn().Get():Cast<APawn>(Causer);
 if(!Attacker&&Causer)Attacker=Causer->GetInstigator();
 // Police/gang crossfire cannot cause either injury or death, including boss NPCs.
 // Identity survives BecomeInjured so an opponent cannot finish a downed NPC.
 if(const auto* NPC=Cast<APatrolEnemy>(Attacker))
 {
  const auto IsGangRole=[](ENPCRole Faction){return Faction==ENPCRole::GangMember||Faction==ENPCRole::GangBoss;};
  const bool Crossfire=(IdentityRole==ENPCRole::Police&&IsGangRole(NPC->IdentityRole))
   ||(IsGangRole(IdentityRole)&&NPC->IdentityRole==ENPCRole::Police);
  if(Crossfire)
  {
   // Never revive a player-downed NPC. Living combatants recover a small buffer
   // before a low-damage hit so every later hit still reaches the event bus.
   if(NPCRole==ENPCRole::Injured)return 0;
   const auto* Rules=GetDefault<UGlobalData>();
   const float HealthFloor=FMath::Max(0.f,Rules->NPCInjuryHealth)+1.f;
   Damage=FMath::Min(Damage*FMath::Clamp(Rules->PoliceGangDamageScale,.001f,1.f),FMath::Max(.01f,Rules->PoliceGangMaxHitDamage));
   if(Health-Damage<HealthFloor)
   {
    Health=HealthFloor+FMath::Max(Damage,FMath::Max(1.f,Rules->PoliceGangRecoveryHealth));
    GetWorld()->GetSubsystem<UDataTransmitter>()->NotifyChange(this,TEXT("Health"),Health);
   }
  }
 }
 const float Applied=FMath::Min(Damage,Health);Health-=Applied;
 if(Health>0&&AudioHurtCooldown<=0){CharacterAudio::Attached(GameAudio::Sound(TEXT("NPC_Hurt")),GetRootComponent(),ECharacterSound::Voice,.55f);AudioHurtCooldown=.65f;}
 if(Health<=0){AssaultTarget.Reset();RetaliationTarget.Reset();ReleasePartner();CombatTarget=nullptr;SetActivity(ENPCActivity::Dead);Speech->SetVisibility(false);Voice->Stop();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();GetCharacterMovement()->DisableMovement();GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);DropDeathAmmo();NPCBody->SetVisibility(false);GetMesh()->SetVisibility(true);if(HeldWeapon){HeldWeapon->CancelHandling();HeldWeapon->SetActorHiddenInGame(true);}GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));GetMesh()->SetSimulatePhysics(true);Rifle->SetVisibility(false);SetLifeSpan(30);}
 else if(NPCRole!=ENPCRole::Injured&&Health<=GetDefault<UGlobalData>()->NPCInjuryHealth)BecomeInjured();
 else if(IsGang()&&Attacker){
  // Police support must not repeatedly reset an already-expired player warning.
  auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
  if(auto* Civilian=Cast<APatrolEnemy>(Attacker);Civilian&&Civilian->IsLawProtected()){
   if(!EscalationTarget.IsValid()){EscalationTarget=Civilian;SetTarget(Civilian);}
  }else if(Attacker==Player||(!EscalationTarget.IsValid()&&(CombatTarget!=Player||!CanSeePlayer())))SetTarget(Attacker);
 }
 else if(IsProtectedPerson()&&Attacker){
  if(auto* Gang=Cast<APatrolEnemy>(Attacker);Gang&&Gang->IsGang()&&Gang->AssaultTarget.Get()==this)RespondToAssault(Gang);
  else FleeFrom(Attacker->GetActorLocation());
 }
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
 if(AssaultTarget.IsValid()||RetaliationTarget.IsValid()||(StreetEventId&&(Activity==ENPCActivity::Meeting||Activity==ENPCActivity::Trade)))return;
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
 if(IsCombatTargetAlive(CombatTarget)||Activity==ENPCActivity::Flee||Activity==ENPCActivity::Investigate||Partner.IsValid()||SocialCooldown>0||!IsGang())return;
 // Everyday harassment runs independently of the rare-event director.
 APatrolEnemy* Victim=nullptr;float Nearest=FMath::Square(GetDefault<UGlobalData>()->GangAssaultSearchRadius);
 for(TActorIterator<APatrolEnemy> It(GetWorld());It;++It){auto* N=*It;
  if(!IsLiving(N)||(N->NPCRole!=ENPCRole::Civilian&&N->NPCRole!=ENPCRole::Merchant)||N->RetaliationTarget.IsValid())continue;
  const float D=FVector::DistSquared(GetActorLocation(),N->GetActorLocation());
  if(D<Nearest&&CanSeeActor(N)){Nearest=D;Victim=N;}
 }
 if(Victim){SocialCooldown=GetDefault<UGlobalData>()->NPCInteractionRetrySeconds;BeginAssault(Victim);return;}
 if(NPCRole==ENPCRole::GangBoss){if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();SetActivity(ENPCActivity::Command);SocialCooldown=14;Say(TEXT("MOVE!"));return;}
 if(GetDefault<UGlobalData>()->bEnableStreetEvents)return; // Paired scenes are selected by the world director.
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
  if(!StreetEventId&&StateTime>10.f){ReleasePartner();SocialCooldown=6;SetActivity(ENPCActivity::Idle);}return;
 }
 if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
 // Start the paired clip only once both characters face each other.
 if(Activity==ENPCActivity::Trade&&!bSocialArrived){
  const FVector Toward=(Other->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
  if(FVector::DotProduct(GetActorForwardVector(),Toward)<.966f||FVector::DotProduct(Other->GetActorForwardVector(),-Toward)<.966f)return;
 }
 if(!bSocialArrived){bSocialArrived=true;StateTime=0;}
 if(Activity==ENPCActivity::Trade){
  // Scheduled trading continues until the director's deadline; count it once there.
  if(StreetEventId)return;
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
float APatrolEnemy::ShotHitChance(const AActor* Target) const
{
 const auto* Rules=GetDefault<UGlobalData>();
 const auto* TargetNPC=Cast<APatrolEnemy>(Target);
 const bool Police=NPCRole==ENPCRole::Police;
 if(TargetNPC&&((Police&&TargetNPC->IsGang())||(IsGang()&&TargetNPC->NPCRole==ENPCRole::Police)))
  return FMath::Clamp(Rules->PoliceGangHitChance,0.f,1.f);
 return Police?1.f:FMath::Clamp(Rules->GangHitChance,0.f,1.f);
}
void APatrolEnemy::TickCombat(float DT)
{
 if(!IsCombatTargetAlive(CombatTarget)){CombatTarget=nullptr;EscalationTarget.Reset();SetActivity(ENPCActivity::Idle);return;}
 const bool Crossfire=IsActivePoliceGangCrossfire(this,CombatTarget);
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
 else {GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCSearchSpeed;LostTime+=DT;if(MoveRetry<=0){MoveRetry=1;MoveTo(LastKnown);}if(LostTime>8&&!Crossfire){CombatTarget=nullptr;EscalationTarget.Reset();PriorityOffender.Reset();SetActivity(ENPCActivity::Idle);return;}}
 // Keep the authored holding pose during combat turns. Its barrel may be
 // offset from the capsule's forward axis, especially for one-handed pistols.
 bTurning=false;
 if(HeldWeapon){
  UpdateWeaponPose();
  const float BarrelOffset=FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,HeldWeapon->MuzzleDirection().Rotation().Yaw);
  const float TargetYaw=(LastKnown-HeldWeapon->MuzzleLocation()).Rotation().Yaw;
  const float Delta=FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,TargetYaw-BarrelOffset);
  SetActorRotation(FRotator(0,GetActorRotation().Yaw+FMath::Clamp(Delta,-130.f*DT,130.f*DT),0));
 }
 if(!Seen||ShotTime>0)return;
 const bool Police=NPCRole==ENPCRole::Police;
 // Refresh the prop after this frame's actor turn before inspecting its barrel.
 UpdateWeaponPose();
 if(!HeldWeapon||WeaponCarry!=ENPCWeaponCarry::Drawn)return;
 const FVector Start=HeldWeapon->MuzzleLocation();
 const FVector Aim=CombatTarget->GetActorLocation()+FVector(0,0,25);
 const FVector Barrel=HeldWeapon->MuzzleDirection();
 if(FVector::DotProduct(Barrel,(Aim-Start).GetSafeNormal())<FMath::Cos(FMath::DegreesToRadians(12.f)))return;
 if(!HeldWeapon->ConsumeShot())return;
 const auto Spec=HeldWeapon->Spec();ShotTime=Spec.Interval;
 // Roll once per trigger pull (including shotguns), according to both combatants.
 const bool AccurateShot=FMath::FRand()<ShotHitChance(CombatTarget);
 const FVector AimDirection=(Aim-Start).GetSafeNormal();
 FVector ShotAim=Aim;
 if(!AccurateShot){
  FVector Side=FVector::CrossProduct(AimDirection,FVector::UpVector).GetSafeNormal();
  if(Side.IsNearlyZero())Side=FVector::RightVector;
  const auto* TargetCharacter=Cast<ACharacter>(CombatTarget);
  const float Radius=TargetCharacter?TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius():60.f;
  ShotAim+=Side*(Radius+(Crossfire?FMath::FRandRange(200.f,450.f):100.f))*(FMath::RandBool()?1.f:-1.f);
  if(Crossfire)ShotAim.Z+=FMath::FRandRange(-120.f,180.f);
 }
 const int32 Pellets=Spec.Pellets;
 auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>();Bus->ReportGunshot(this,Start);
 for(int32 Pellet=0;Pellet<Pellets;++Pellet){
  const FVector Dir=(ShotAim-Start).GetSafeNormal();
  FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCFire),true,this);if(HeldWeapon)Query.AddIgnoredActor(HeldWeapon);
  // A deliberate miss cannot clip a moving limb; walls and other actors still block it.
  if(!AccurateShot)Query.AddIgnoredActor(CombatTarget);
  const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Dir*Spec.Range,ECC_Visibility,Query);bool Hurt=false;
  if(Blocked){auto* HitNPC=Cast<APatrolEnemy>(Hit.GetActor());
   const bool Allowed=Police?Hit.GetActor()==CombatTarget:(Hit.GetActor()==CombatTarget||(HitNPC&&!HitNPC->IsGang()));
   const float Damage=Spec.DamageAtDistance(FVector::Distance(Start,Hit.ImpactPoint));
   if(Allowed)Hurt=Bus->TransmitHit(Hit,Damage,Dir,GetController(),this)>0;
  }
  if(auto* FX=GetWorld()->SpawnActor<ACombatShotEffect>())FX->Setup(Start,Blocked?Hit.ImpactPoint:Start+Dir*Spec.Range,Blocked,Hurt || (Blocked && Cast<APawn>(Hit.GetActor())),Blocked?Hit.ImpactNormal:FVector::ZeroVector);
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
  const auto* Escape=StreetEventId?GetWorld()->GetSubsystem<UWorldEvents>()->Find(StreetEventId):nullptr;
  if(Escape&&Escape->bActive&&Escape->Kind==EStreetEvent::CivilianEscape)
   PanicTime=FMath::Max(.1f,Escape->ExpiresAt-GetWorld()->GetTimeSeconds());
  else PanicTime-=DT;
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
 // The back reach is shared, but the pistol's compact two-hand idle is much
 // farther from its end pose. Give only those clip boundaries more blend time.
 Motion->PoseBlendDuration=HeldWeapon&&HeldWeapon->Kind==ETacticalWeaponKind::Pistol&&
  (IsWeaponTransitioning()||CurrentAnimation==DrawWeaponAnimation||CurrentAnimation==HolsterWeaponAnimation)?.35f:.18f;
 const float Speed=GetVelocity().Size2D();
 // Hysteresis prevents avoidance/braking jitter from repeatedly restarting the gait.
 const bool Moving=Speed>(Motion->bLocomotion?15.f:25.f);
 Motion->StrideIdle=nullptr;Motion->StrideWeight=1.f;
 const bool Armed=HeldWeapon&&WeaponCarry==ENPCWeaponCarry::Drawn;
 if(IsWeaponTransitioning())
 {
  Motion->Sequence=WeaponCarry==ENPCWeaponCarry::Drawing?DrawWeaponAnimation:HolsterWeaponAnimation;
  Motion->ExplicitTime=FMath::Clamp(WeaponCarryTime-(WeaponCarry==ENPCWeaponCarry::Drawing?.2f:0.f),0.f,Motion->Sequence?Motion->Sequence->GetPlayLength():1.f);
  Motion->bLoop=false;Motion->bLocomotion=false;Motion->PlaybackRate=1.f;Motion->UpperBodyPose=nullptr;CurrentAnimation=Motion->Sequence;
  return;
 }
 UAnimSequence* Anim=nullptr;float ReferenceSpeed=300.f;bool Loop=true;
 const bool Reloading=HeldWeapon&&HeldWeapon->IsReloading()&&ReloadAnimation;
 if(Reloading){Anim=ReloadAnimation;Loop=WeaponKind==ETacticalWeaponKind::Shotgun;}
 else if(bTurning&&!Moving){Anim=ActiveTurn;Loop=false;}
 else if(Activity==ENPCActivity::Flee||Activity==ENPCActivity::PursueMelee)
 {
  if(!Moving)Anim=UnarmedIdle;
  else if(Speed>(CurrentAnimation==ActivityAnimations.FindRef(ENPCActivity::Flee)?280.f:330.f))
  {if(auto* Found=ActivityAnimations.Find(ENPCActivity::Flee))Anim=*Found;ReferenceSpeed=Anim&&Anim->GetName()==TEXT("MF_Unarmed_Jog_Fwd")?600.34f:461.07f;}
  else Anim=UnarmedWalk;
 }
 else if(!Moving)
 {
  const bool Social=Activity==ENPCActivity::Trade||Activity==ENPCActivity::Threaten;
  if(!Social||bSocialArrived)if(auto* Found=ActivityAnimations.Find(Activity))Anim=*Found;
 }
 if(!Anim)
 {
  if(Armed)
  {
   if(Moving&&Speed>(CurrentAnimation==Run?380.f:430.f)&&Run){Anim=Run;ReferenceSpeed=687.48f;}
   else if(Moving&&Activity==ENPCActivity::Combat&&AimWalk)
   {
    const float Angle=FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(GetVelocity(),GetActorRightVector()),FVector::DotProduct(GetVelocity(),GetActorForwardVector())));
    const int32 Direction=(FMath::RoundToInt(Angle/45.f)+8)%8;
    static const float Speeds[]={183.33f,183.33f,183.33f,183.33f,183.33f,183.33f,183.33f,183.33f};
    static const float PistolSpeeds[]={183.33f,183.33f,147.66f,183.33f,183.33f,183.33f,183.33f,183.33f};
    Anim=AimWalkDirections.IsValidIndex(Direction)&&AimWalkDirections[Direction]?AimWalkDirections[Direction].Get():AimWalk.Get();ReferenceSpeed=WeaponKind==ETacticalWeaponKind::Pistol?PistolSpeeds[Direction]:Speeds[Direction];
   }
   else {Anim=Moving?(AimWalk?AimWalk.Get():Walk.Get()):Idle.Get();ReferenceSpeed=183.33f;}
  }
  else Anim=Moving?UnarmedWalk.Get():UnarmedIdle.Get();
 }
 CurrentAnimation=Anim;Motion->Sequence=Anim;Motion->UpperBodyPose=nullptr;
 Motion->ExplicitTime=bTurning&&!Moving?TurnTime:-1.f;
 Motion->bLoop=Loop;Motion->bLocomotion=Moving&&!Reloading;
 Motion->PlaybackRate=Motion->bLocomotion?FMath::Clamp(Speed/ReferenceSpeed,.75f,2.f):1.f;
 if(Motion->bLocomotion)
 {
  Motion->StrideIdle=Armed?Idle.Get():UnarmedIdle.Get();
  Motion->StrideWeight=FMath::Clamp(Speed/(ReferenceSpeed*Motion->PlaybackRate),0.f,1.f);
 }
}
void APatrolEnemy::Tick(float DT)
{
 Super::Tick(DT);if(Health<=0)return;TurnDelta=0;StateTime+=DT;DecisionTime-=DT;MoveRetry-=DT;MoveTimeout-=DT;ShotTime-=DT;SocialCooldown-=DT;SpeechTime-=DT;CivilianResponseCooldown-=DT;
 AudioHurtCooldown=FMath::Max(0.f,AudioHurtCooldown-DT);
 CrossfireCooldown=FMath::Max(0.f,CrossfireCooldown-DT);
 const FVector AudioPosition=GetActorLocation();
 const float Travel=bAudioPositionReady?FVector::Dist2D(AudioPosition,AudioLastPosition):0.f;
 AudioLastPosition=AudioPosition;bAudioPositionReady=true;
 if(GetCharacterMovement()->IsMovingOnGround()&&GetVelocity().Size2D()>20.f&&Travel<150.f)
 {
  AudioFootstepDistance+=Travel;
  const float Stride=GetVelocity().Size2D()>300.f?185.f:135.f;
  if(AudioFootstepDistance>=Stride)
  {
   AudioFootstepDistance=FMath::Fmod(AudioFootstepDistance,Stride);
   CharacterAudio::At(this,GameAudio::Sound(*FString::Printf(TEXT("NPC_Footstep%d"),1+AudioFootstepIndex++%4)),AudioPosition-FVector(0,0,85),ECharacterSound::Footstep,.5f,FMath::FRandRange(.96f,1.04f));
  }
 }else AudioFootstepDistance=0;
 TickWeaponCarry(DT);
 if(SpeechTime<=0)Speech->SetVisibility(false);
 if(HeldWeapon){
  HeldWeapon->AdvanceHandling(DT);
  // Sustain faction firefights while retaining the normal reload/chamber cycle.
  if(Activity==ENPCActivity::Combat&&IsActivePoliceGangCrossfire(this,CombatTarget)&&AmmoReserve&&!HeldWeapon->bChambered)
  {
   const auto Spec=HeldWeapon->Spec();
   if(AmmoReserve->Count(Spec.Ammo)==0)AmmoReserve->Add(Spec.Ammo,TEXT("NPC ammunition"),TEXT("Personal reserve"),Spec.Capacity);
  }
  if(WeaponCarry==ENPCWeaponCarry::Drawn&&!AssaultTarget.IsValid()&&!HeldWeapon->bChambered&&HeldWeapon->Handling==EWeaponHandling::Idle&&ReloadAnimation)HeldWeapon->StartReload(AmmoReserve);
  if(HeldWeapon->IsReloading()){
   bTurning=false;bMoving=false;if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
   GetCharacterMovement()->StopMovementImmediately();SetMotion();UpdateWeaponPose(DT);return;
  }
 }
 if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))Speech->SetWorldRotation((P->GetActorLocation()-Speech->GetComponentLocation()).Rotation());
 if(NPCRole==ENPCRole::Injured){if(StateTime>7){StateTime=0;Say(TEXT("HELP!"),HelpSound);}SetMotion();UpdateWeaponPose(DT);return;}
 if(TickExtractionResponse(DT)){TickWeaponCarry(0);SetMotion();UpdateWeaponPose(DT);return;}
 if(TickGangAttackObservation(DT)){SetMotion();UpdateWeaponPose(DT);return;}
 if(IsGang() && !bAimWarning && CombatTarget!=UGameplayStatics::GetPlayerPawn(this,0) && IsPlayerAimingAtMe())
 {
  ReleasePartner();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;
  bAimWarning=false;WarningRemaining=0;Speech->SetVisibility(false);
  SetTarget(UGameplayStatics::GetPlayerPawn(this,0));ShotTime=0;
 }
 if(TickCameraReaction(DT)){SetMotion();UpdateWeaponPose(DT);return;}
 if(DecisionTime<=0){DecisionTime=.25f;Think();}
 if(AssaultTarget.IsValid())TickAssault(DT);
 else if(Activity==ENPCActivity::Counterattack)TickCounterattack(DT);
 else if(Activity==ENPCActivity::Meeting)TickMeeting(DT);
 else if(Activity==ENPCActivity::WarnPlayer)TickWarning(DT);
 else if(Activity==ENPCActivity::Combat)TickCombat(DT);
 else if(Activity==ENPCActivity::Threaten||Activity==ENPCActivity::Beat||Activity==ENPCActivity::Trade||Activity==ENPCActivity::Command)TickSocial(DT);
 else TickPatrol(DT);
 TickWeaponCarry(0); // Apply this frame's decisions without advancing the animation twice.
 SetMotion();UpdateWeaponPose(DT);
}

bool APatrolEnemy::IsPlayerAimingAtMe() const
{
 auto* Player=Cast<AGame_Jam_Clean_VersCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
 auto* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
 auto* Equipment=PC?PC->FindComponentByClass<UCombatEquipmentComponent>():nullptr;
 if(!Player||Player->Health<=0||!Equipment||!Equipment->ActiveWeapon||!Equipment->bADS)return false;
 if(IsGang()&&!CanSeeActor(Player,true))return false;
 const FVector Start=Player->GetFirstPersonCameraComponent()->GetComponentLocation();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(AimedThreat),true,Player);Query.AddIgnoredActor(Equipment->ActiveWeapon);
 FHitResult Hit;
 return GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+PC->GetControlRotation().Vector()*SightRange,ECC_Visibility,Query)&&
  (Hit.GetActor()==this||(HeldWeapon&&Hit.GetActor()==HeldWeapon));
}




void APatrolEnemy::UpdateWeaponPose(float DT)
{
 const bool InHand=HeldWeapon&&IsWeaponInHand();
 const bool Armed=InHand&&WeaponCarry==ENPCWeaponCarry::Drawn;
 if(IsWeaponTransitioning()){GetMesh()->TickAnimation(0,false);GetMesh()->RefreshBoneTransforms();}
 if(HeldWeapon)HeldWeapon->SetActorHiddenInGame(Health<=0);
 Rifle->SetVisibility(false);NPCBody->SetVisibility(true);GetMesh()->SetVisibility(false,false);
 // Moving clips already contain their own carry/aim posture; do not lower that arm twice.
 WeaponReadyAlpha=FMath::FInterpConstantTo(WeaponReadyAlpha,GetVelocity().Size2D()>5||Activity==ENPCActivity::WarnPlayer||Activity==ENPCActivity::Combat?1.f:0.f,DT,4.f);
 if(InHand||IsWeaponTransitioning())
 {
  if(InHand&&(HeldWeapon->GetAttachParentActor()!=this||HeldWeapon->GetRootComponent()->GetAttachParent()!=GetMesh()))
   HeldWeapon->AttachToComponent(GetMesh(),FAttachmentTransformRules::KeepWorldTransform,TEXT("HandGrip_R"));
  HeldWeapon->UpdateNPCGripPose(GetMesh(),NPCBody,WeaponReadyAlpha,IsWeaponTransitioning());
 }
 else NPCBody->CopyPoseFromSkeletalComponent(GetMesh());
 const FTransform AuthoredGun=HeldWeapon?HeldWeapon->GetActorTransform():FTransform::Identity;
 NPCBody->MarkRefreshTransformDirty();NPCBody->RefreshBoneTransforms();
 if(HeldWeapon&&!InHand)
 {
  HeldWeapon->SetActorTransform(BackWeaponRelative*NPCBody->GetBoneTransformByName(WeaponCarrySocket(),EBoneSpaces::WorldSpace));
  if(HeldWeapon->GetRootComponent()->GetAttachParent()!=NPCBody||HeldWeapon->GetRootComponent()->GetAttachSocketName()!=WeaponCarrySocket())
   HeldWeapon->AttachToComponent(NPCBody,FAttachmentTransformRules::KeepWorldTransform,WeaponCarrySocket());
 }
 ApplyBackCarryPose(AuthoredGun);
 // The authored holding clip has a fixed elevation. Turn the upper body and
 // its held gun together so close/crouched/elevated targets can pass the real
 // muzzle-angle firing gate without breaking either hand's contact.
 const bool AimAtTarget=Armed&&Activity==ENPCActivity::Combat&&IsCombatTargetAlive(CombatTarget)&&!HeldWeapon->IsReloading();
 const FName AimBone(TEXT("spine_03"));
 if(Armed&&!HeldWeapon->IsReloading()&&NPCBody->GetBoneIndex(AimBone)!=INDEX_NONE){
  const FVector Barrel=HeldWeapon->MuzzleDirection();
  const float WantedPitch=AimAtTarget?FMath::Clamp(FMath::FindDeltaAngleDegrees(Barrel.Rotation().Pitch,(CombatTarget->GetActorLocation()+FVector(0,0,25)-HeldWeapon->MuzzleLocation()).Rotation().Pitch),-60.f,60.f):0.f;
  CombatAimPitch=FMath::FInterpConstantTo(CombatAimPitch,WantedPitch,DT,90.f);
  const FVector Axis=FVector::CrossProduct(FVector::UpVector,Barrel.GetSafeNormal2D()).GetSafeNormal();
  if(!Axis.IsNearlyZero()){
   const FQuat Correction(Axis,-FMath::DegreesToRadians(CombatAimPitch));
   FTransform Spine=NPCBody->GetBoneTransformByName(AimBone,EBoneSpaces::WorldSpace);
   const FVector Pivot=Spine.GetLocation();Spine.SetRotation(Correction*Spine.GetRotation());
   NPCBody->SetBoneTransformByName(AimBone,Spine,EBoneSpaces::WorldSpace);
   const FTransform Gun=HeldWeapon->GetActorTransform();
   HeldWeapon->SetActorLocationAndRotation(Pivot+Correction.RotateVector(Gun.GetLocation()-Pivot),Correction*Gun.GetRotation());
   NPCBody->RefreshBoneTransforms();
  }
 }else CombatAimPitch=0;
 // Run on the final rendered pose, after both authored motion and weapon IK.
 // Keep the pre-simulation presentation snapshot above free of physics feedback.
 float SecondaryWeight=1.f;
 if(auto* PC=UGameplayStatics::GetPlayerController(this,0))
 {
  FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
  SecondaryWeight=FMath::Clamp((2200.f-FVector::Distance(Eye,GetActorLocation()))/700.f,0.f,1.f);
 }
 TArray<FCharacterSecondaryMotion::FCapsule> HairGuards;
 if(HeldWeapon)HairGuards.Add({HeldWeapon->GetActorLocation(),HeldWeapon->MuzzleLocation(),4.f});
 SecondaryMotion.Apply(NPCBody,DT,SecondaryWeight,HairGuards);
}


void APatrolEnemy::TickMeeting(float DT)
{
 GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCWalkSpeed;
 if(FVector::Dist2D(GetActorLocation(),EventDestination)>85){
  if(MoveRetry<=0){MoveRetry=1;MoveTo(EventDestination,45);}
  if(GetVelocity().Size2D()>5)Face(GetActorLocation()+GetVelocity(),DT);
 }else{
  if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
  Face(EventFocus,DT);if(SpeechTime<=0)Say(TEXT("LET'S TALK."));
 }
}
bool APatrolEnemy::IsBeingFilmed() const
{
 auto* PC=UGameplayStatics::GetPlayerController(this,0);
 auto* Equipment=PC?PC->FindComponentByClass<UCamcorderEquipmentComponent>():nullptr;
 auto* Camera=Equipment?Equipment->Camcorder.Get():nullptr;
 auto* Player=PC?PC->GetPawn().Get():nullptr;
 if(!Camera||!Player||!Camera->bHeld||!Camera->bPowered||(!Camera->bViewfinderRaised&&!Camera->bRecording)||!CanSeeActor(Player,true))return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 const FVector Delta=GetActorLocation()+FVector(0,0,40)-Eye;
 return Delta.SizeSquared()<FMath::Square(1000.f)&&FVector::DotProduct(View.Vector(),Delta.GetSafeNormal())>.9f;
}
bool APatrolEnemy::TickCameraReaction(float DT)
{
 CameraReactionCooldown=FMath::Max(0.f,CameraReactionCooldown-DT);
 // Danger always interrupts camera etiquette, including police intervention.
 if(bCombat||AssaultTarget.IsValid()||RetaliationTarget.IsValid()||Activity==ENPCActivity::Flee||Activity==ENPCActivity::WarnPlayer){CameraReactionTime=0;return false;}
 auto* Player=UGameplayStatics::GetPlayerPawn(this,0);if(!Player)return false;
 if(CameraReactionTime<=0&&CameraReactionCooldown<=0&&!StreetEventId&&!Partner.IsValid()&&IsBeingFilmed()){
  if(NPCRole==ENPCRole::Civilian||NPCRole==ENPCRole::Merchant){SetActivity(ENPCActivity::CameraLook);Say(TEXT("PLEASE, NO CAMERA."));}
  else if(NPCRole==ENPCRole::Police){SetActivity(ENPCActivity::CameraDismiss);Say(TEXT("PLEASE LEAVE THE AREA."));}
  else if(NPCRole==ENPCRole::GangBoss){
   SetActivity(ENPCActivity::CameraAvoid);DangerLocation=Player->GetActorLocation();bMoving=false;MoveRetry=0;
  }else return false;
  if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();CameraReactionTime=5;CameraReactionCooldown=12;
 }
 if(CameraReactionTime<=0)return false;
 CameraReactionTime-=DT;
 if(CameraReactionTime<=0){if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();SetActivity(ENPCActivity::Idle);return false;}
 if(Activity==ENPCActivity::CameraAvoid){
  GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCWalkSpeed;
  if(!bMoving&&MoveRetry<=0){MoveRetry=1;ChooseDestination(true);}
  Face(GetActorLocation()+(GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D()*200,DT);
 }else {if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();Face(Player->GetActorLocation(),DT);}
 return true;
}

// Decisions are sampled once per encounter/cooldown, never every frame.
bool APatrolEnemy::IsCombatTargetAlive(AActor* Target) const
{
 if(auto* N=Cast<APatrolEnemy>(Target);IsValid(N)&&EscalationTarget.Get()==N)return N->Health>0;
 return IsLiving(Target);
}
void APatrolEnemy::BeginAssault(APatrolEnemy* Target)
{
 if(!IsGang()||!IsLiving(Target))return;
 ReleasePartner();AssaultTarget=Target;CombatTarget=nullptr;EscalationTarget.Reset();
 AssaultLostTime=0;bMeleeApplied=false;LastKnown=Target->GetActorLocation();MoveRetry=0;
 if(HeldWeapon)HeldWeapon->CancelHandling();SetActivity(ENPCActivity::Threaten);Say(TEXT("PAY UP!"));
}
void APatrolEnemy::RespondToAssault(APatrolEnemy* Gang)
{
 if(NPCRole!=ENPCRole::Civilian&&NPCRole!=ENPCRole::Merchant)return;
 if(CivilianAggressor.Get()!=Gang||CivilianResponseCooldown<=0){
  CivilianAggressor=Gang;CivilianResponseCooldown=GetDefault<UGlobalData>()->NPCInteractionRetrySeconds;
  bCivilianWillFight=FMath::FRand()<GetDefault<UGlobalData>()->CivilianFightBackChance;
 }
 if(bCivilianWillFight){
  ReleasePartner();RetaliationTarget=Gang;SetActivity(ENPCActivity::Counterattack);bMeleeApplied=false;MoveRetry=0;
  if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();Say(TEXT("LEAVE ME ALONE!"));
 }else FleeFrom(Gang->GetActorLocation());
}
void APatrolEnemy::TickAssault(float DT)
{
 auto* Target=AssaultTarget.Get();
 auto Stop=[&]{AssaultTarget.Reset();SocialCooldown=GetDefault<UGlobalData>()->NPCInteractionRetrySeconds;if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();SetActivity(ENPCActivity::Idle);};
 if(!IsLiving(Target)){Stop();return;}
 const bool Seen=CanSeeActor(Target,false);
 if(Seen){LastKnown=Target->GetActorLocation();AssaultLostTime=0;}else AssaultLostTime+=DT;
 if(AssaultLostTime>12){Stop();return;}
 const float Distance=FVector::Dist2D(GetActorLocation(),Target->GetActorLocation());
 if(!Seen||Distance>145){
  SetActivity(ENPCActivity::PursueMelee);GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->GangMeleeChaseSpeed;
  if(MoveRetry<=0){MoveRetry=.4f;MoveTo(LastKnown,40);}Face(LastKnown,DT);return;
 }
 if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();bMoving=false;Face(Target->GetActorLocation(),DT);
 if(Activity==ENPCActivity::Threaten){bSocialArrived=true;if(StateTime<2)return;}
 if(Activity!=ENPCActivity::Beat){SetActivity(ENPCActivity::Beat);bMeleeApplied=false;}
 if(StateTime>.55f&&!bMeleeApplied){
  bMeleeApplied=true;
  FHitResult Hit(Target,Target->GetMesh(),Target->GetActorLocation(),FVector::UpVector);Hit.ImpactPoint=Target->GetActorLocation();
  GetWorld()->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,GetDefault<UGlobalData>()->NPCMeleeDamage,(Target->GetActorLocation()-GetActorLocation()).GetSafeNormal(),GetController(),this);
 }
 // Damage delegates can synchronously interrupt this encounter (police/firearm retaliation).
 if(!AssaultTarget.IsValid()||Activity!=ENPCActivity::Beat)return;
 if(StateTime>FMath::Max(1.5f,CurrentAnimation?CurrentAnimation->GetPlayLength():0.f)){StateTime=0;bMeleeApplied=false;CurrentAnimation=nullptr;}
}
void APatrolEnemy::TickCounterattack(float DT)
{
 auto* Gang=RetaliationTarget.Get();
 if(!IsLiving(Gang)){RetaliationTarget.Reset();SetActivity(ENPCActivity::Idle);return;}
 if(Gang->bCombat){FleeFrom(Gang->GetActorLocation());return;}
 Face(Gang->GetActorLocation(),DT);
 if(FVector::Dist2D(GetActorLocation(),Gang->GetActorLocation())>160){
  GetCharacterMovement()->MaxWalkSpeed=GetDefault<UGlobalData>()->NPCWalkSpeed;
  if(MoveRetry<=0){MoveRetry=.5f;MoveTo(Gang->GetActorLocation(),40);}if(StateTime>5)FleeFrom(Gang->GetActorLocation());return;
 }
 if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
 if(StateTime>.55f&&!bMeleeApplied&&CanSeeActor(Gang,false)){
  bMeleeApplied=true;FHitResult Hit(Gang,Gang->GetMesh(),Gang->GetActorLocation(),FVector::UpVector);Hit.ImpactPoint=Gang->GetActorLocation();
  GetWorld()->GetSubsystem<UDataTransmitter>()->TransmitHit(Hit,GetDefault<UGlobalData>()->NPCMeleeDamage,(Gang->GetActorLocation()-GetActorLocation()).GetSafeNormal(),GetController(),this);
  RetaliationTarget.Reset();FleeFrom(Gang->GetActorLocation());
 }
}
