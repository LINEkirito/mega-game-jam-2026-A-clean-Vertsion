#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "TacticalWeapon.h"
#include "PatrolEnemy.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Camcorder/BackpackInventory.h"
#include "EngineUtils.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAppearanceSkinCheck,FAutomationTestBase*,Test);
bool FAppearanceSkinCheck::Update()
{
 UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){W=Context.World();break;}
 auto* Player=W&&W->GetFirstPlayerController()?Cast<AGame_Jam_Clean_VersCharacter>(W->GetFirstPlayerController()->GetPawn()):nullptr;
 if(!Player){Test->AddError(TEXT("Missing PIE player"));return true;}
 auto* NPC=W->SpawnActorDeferred<APatrolEnemy>(APatrolEnemy::StaticClass(),FTransform(FVector(0,0,1000)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 NPC->NPCRole=ENPCRole::Police;NPC->FinishSpawning(FTransform(FVector(0,0,1000)));NPC->SetActorTickEnabled(false);
 for(int32 Kind=0;Kind<3;++Kind)
 {
  const FTransform Spawn(FVector(0,0,900));
  auto* Gun=W->SpawnActorDeferred<ATacticalWeapon>(ATacticalWeapon::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  Gun->Kind=ETacticalWeaponKind(Kind);Gun->FinishSpawning(Spawn);Gun->SetActorTickEnabled(false);
  auto* Mesh=Gun->PackWeaponMesh->GetSkeletalMeshAsset();
  const int32 Capacity=Gun->Spec().Capacity;
  auto Check=[&](const TCHAR* Faction)
  {
   Test->TestEqual(TEXT("Paint retains exact mesh asset"),Gun->PackWeaponMesh->GetSkeletalMeshAsset(),Mesh);
   Test->TestEqual(TEXT("Paint retains weapon capacity"),Gun->Spec().Capacity,Capacity);
   for(auto* Component:{Gun->PackWeaponMesh.Get(),Gun->PackMagazineMesh.Get()})
    for(int32 Slot=0;Slot<Component->GetNumMaterials();++Slot)
     Test->TestTrue(TEXT("Every weapon/magazine slot has its owner's faction"),Component->GetMaterial(Slot)&&Component->GetMaterial(Slot)->GetName().EndsWith(Faction));
  };
  NPC->IdentityRole=ENPCRole::Police;Gun->EquipForNPC(NPC->GetMesh());Check(TEXT("_Police"));
  NPC->IdentityRole=ENPCRole::GangMember;Gun->EquipForNPC(NPC->GetMesh());Check(TEXT("_Gang"));
  Gun->Drop(FVector(0,0,900),FRotator::ZeroRotator);Check(TEXT("_Gang"));
  Gun->Equip(Player);Gun->AdvanceHandling(10);Check(TEXT("_Director"));
  if(Kind!=1)
  {
   auto* Bag=NewObject<UBackpackInventory>(Gun);Bag->Add(Gun->Spec().Ammo,TEXT("Test"),TEXT("Test"),100);
   Gun->Magazine=0;Gun->bChambered=false;Test->TestTrue(TEXT("Painted gun reload starts"),Gun->StartReload(Bag));Gun->AdvanceHandling(20);
   bool Found=false;for(TActorIterator<AWeaponDebris> It(W);It;++It)
    if(It->SkeletalVisual&&It->SkeletalVisual->GetSkeletalMeshAsset()==Gun->PackMagazineMesh->GetSkeletalMeshAsset())
    {Found=true;Test->TestTrue(TEXT("Discarded magazine keeps paint"),It->SkeletalVisual->GetMaterial(0)->GetName().EndsWith(TEXT("_Director")));}
   Test->TestTrue(TEXT("Painted detachable magazine was discarded"),Found);
  }
  Gun->Stow();Gun->Destroy();
 }
 NPC->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAppearanceSkinTest,"Game.Appearance.FactionWeaponOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAppearanceSkinTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));AddCommand(new FAppearanceSkinCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}
#endif
