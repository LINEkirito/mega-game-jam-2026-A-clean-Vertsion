#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "ActiveSound.h"
#include "Audio.h"
#include "UnrealClient.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/BackpackInventory.h"
#include "Data/GlobalData.h"

// Real PIE frames, real audio sources, and screenshots supplement contact metrics.
class FWeaponStanceAudioCheck final : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 int32 Case=0,Shot=0;
 double Started=0,ShotAt=0;
 uint64 AudioId=0;
 bool Pending=false,Captured=false,MovingCaptured=false;
 float MaxGrip=0;
 float MinRestSightDepth=MAX_flt;
 bool Stress=false;
 TArray<TWeakObjectPtr<UAudioComponent>> Noise;
public:
 explicit FWeaponStanceAudioCheck(FAutomationTestBase* T,bool InStress=false):Test(T),Stress(InStress){}
 bool Update() override
 {
  UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){World=C.World();break;}
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
  auto* Combat=PC?PC->FindComponentByClass<UCombatEquipmentComponent>():nullptr;
  if(!Player||!Combat){Test->AddError(TEXT("Missing PIE player"));return true;}
  const int32 Kind=Case/4,Stance=Case%4;
  const int32 ShotLimit=Kind==2&&Stance==3?36:6;
  if(Started==0)
  {
   Combat->Equipment->StoreCamera();Player->SetCanBeDamaged(false);
   Player->DisableInput(PC);PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
   // Real shots trigger NPC retaliation. Give only this transient PIE test
   // actor enough health to finish the audio sweep without changing AI.
   Player->Health=1000000.f;
   ATacticalWeapon* Gun=nullptr;for(const auto& E:Combat->Weapons)if(int32(E.Value->Kind)==Kind)Gun=E.Value;
   if(!Gun){Test->AddError(TEXT("Missing weapon"));return true;}
   if(Stance==0)Combat->Equipment->Inventory->Add(Gun->Spec().Ammo,TEXT("Test ammo"),TEXT("Test"),200);
   if(Stance==0){Combat->Unequip();Combat->ActiveWeapon=Gun;Gun->Equip(Player);Gun->AdvanceHandling(5);}
   if(Stance==1||Stance==2)Player->Crouch();else Player->UnCrouch();
   PC->SetControlRotation(FRotator(0,Player->GetActorRotation().Yaw,0));
   Combat->bADS=Stance>=2;Started=FPlatformTime::Seconds();Shot=0;Pending=false;Captured=false;MovingCaptured=false;MaxGrip=0;MinRestSightDepth=MAX_flt;
   return false;
  }
  auto* Gun=Combat->ActiveWeapon.Get();if(!Gun){Test->AddError(TEXT("Lost weapon"));return true;}
  Combat->bADS=Stance>=2;
  const double Now=FPlatformTime::Seconds(),Elapsed=Now-Started;
  if(!Stress&&(Stance==1||Stance==2)&&Elapsed>3)Player->AddMovementInput(Player->GetActorForwardVector(),1.f,true);
  if(Elapsed<1.5)return false;
  MaxGrip=FMath::Max(MaxGrip,FMath::Max(Gun->GripError(true),Gun->GripError(false)));
  if(Kind==1&&Stance>=2&&Elapsed<2.5)
   MinRestSightDepth=FMath::Min(MinRestSightDepth,static_cast<float>(FVector::DotProduct(
    Gun->PackWeaponMesh->GetSocketLocation(TEXT("AimPoint"))-Player->GetFirstPersonCameraComponent()->GetComponentLocation(),PC->GetControlRotation().Vector())));
  if(!Captured&&!Stress)
  {
   FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("G:/TheTechfiles/ThefinalGameJam/WeaponInspection/kind%d_stance%d.png"),Kind,Stance),false,false);
   Captured=true;
  }
  // Screenshot readback stalls rendering; do not measure audio in that frame.
  if(Elapsed<2.5)return false;
  if(Pending&&Now-ShotAt>.03)
  {
   auto Device=World->GetAudioDevice();bool Audible=false;int32 ActiveCount=0,SourceCount=0;
   const uint64 Id=AudioId;
   FAudioThread::RunCommandOnAudioThread([Device,Id,&Audible,&ActiveCount,&SourceCount]{
    ActiveCount=Device->GetActiveSounds().Num();SourceCount=Device->GetWaveInstanceSourceMap().Num();
    for(const auto& E:Device->GetWaveInstanceSourceMap())if(E.Key&&E.Key->ActiveSound&&E.Key->ActiveSound->GetAudioComponentID()==Id&&E.Value&&E.Value->IsPlaying())Audible=true;
   });
   FAudioCommandFence Fence;Fence.BeginFence();Fence.Wait();
   // Allow mixer scheduling/decoder startup, but keep competing voices alive
   // until the shot is actually playing. An evicted shot still fails.
   if(!Audible&&Now-ShotAt<.25)return false;
   Test->TestTrue(*FString::Printf(TEXT("Gun %d stance %d shot %d has playing mixer source (active %d sources %d)"),Kind,Stance,Shot,ActiveCount,SourceCount),Audible);
   for(auto N:Noise)if(N.IsValid())N->Stop();Noise.Reset();
   Pending=false;
  }
  if(!Pending&&Shot<ShotLimit&&World->GetTimeSeconds()>=Gun->NextFireTime+.001f&&(Shot==0||Now-ShotAt>FMath::Max(.12f,Gun->Spec().Interval+.02f)))
  {
   if(Shot==3||!Gun->bChambered)
   {
    Test->TestTrue(TEXT("Reload between shots starts"),Gun->StartReload(Combat->Equipment->Inventory));
    Gun->AdvanceHandling(20);Combat->bReloading=false;
    Test->TestTrue(TEXT("Reload restores firing"),Gun->CanFire());
   }
   if(Stress)
   {
    auto* S=LoadObject<USoundBase>(nullptr,TEXT("/Game/Combat/NPC/Audio/Help"));
    for(int32 I=0;I<40;++I)Noise.Add(UGameplayStatics::SpawnSound2D(World,S,1.f));
   }
   PC->SetControlRotation(FRotator(0,Player->GetActorRotation().Yaw,0));
   TArray<UAudioComponent*> Before;Gun->GetComponents(Before);
   Test->TestTrue(TEXT("Actual combat Fire consumes and dispatches shot"),Combat->Fire());
   TArray<UAudioComponent*> After;Gun->GetComponents(After);AudioId=0;
   for(auto* A:After)if(!Before.Contains(A)&&A->Sound)AudioId=A->GetAudioComponentID();
   Test->TestTrue(TEXT("Each muzzle flash creates gunshot audio component"),AudioId!=0);
   ShotAt=FPlatformTime::Seconds();Pending=true;++Shot;
  }
  if(!Stress&&(Stance==1||Stance==2)&&Shot==6&&!Pending&&Elapsed>4.5&&!MovingCaptured)
  {
   FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("G:/TheTechfiles/ThefinalGameJam/WeaponInspection/kind%d_stance%d_moving.png"),Kind,Stance),false,false);
   MovingCaptured=true;
  }
  if(Elapsed<5||Pending||Shot<ShotLimit)return false;
  Test->TestTrue(TEXT("Stance has correct crouch state"),Player->bIsCrouched==(Stance==1||Stance==2));
  Test->TestTrue(*FString::Printf(TEXT("Gun %d stance %d wrists contact (%.3fcm)"),Kind,Stance,MaxGrip),MaxGrip<.2f);
  if(Kind==1&&Stance>=2)Test->TestTrue(TEXT("Shotgun ADS sight retains eye clearance"),MinRestSightDepth!=MAX_flt&&MinRestSightDepth+.1f>=GetDefault<UGlobalData>()->ShotgunADSSightClearance);
  if(Stance==3)Combat->Unequip();Started=0;
  if(++Case<12)return false;
  Player->UnCrouch();return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponStanceAudioTest,"Game.Weapons.StanceAndFireAudio",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWeaponStanceAudioTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FWeaponStanceAudioCheck(this));AddCommand(new FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponAudioPressureTest,"Game.Weapons.FireAudioUnderPressure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWeaponAudioPressureTest::RunTest(const FString&)
{AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FWeaponStanceAudioCheck(this,true));AddCommand(new FEndPlayMapCommand());return true;}
#endif
