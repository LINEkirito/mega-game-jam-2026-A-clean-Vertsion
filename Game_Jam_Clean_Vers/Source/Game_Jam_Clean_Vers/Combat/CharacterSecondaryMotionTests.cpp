#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CharacterSecondaryMotion.h"
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterSecondaryMotionTest,"Game.Characters.SecondaryMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterSecondaryMotionTest::RunTest(const FString&)
{
 for(const TCHAR* Role:{TEXT("Civilian"),TEXT("Merchant"),TEXT("GangMember"),TEXT("GangBoss"),TEXT("Injured"),TEXT("Police")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(FString(TEXT("/Game/Combat/NPC/Characters/NPC_"))+Role));
  if(!TestNotNull(Role,Mesh))continue;
  const auto& Ref=Mesh->GetRefSkeleton();const auto Base=Ref.GetRefBonePose();
  FCharacterSecondaryMotion Motion;Motion.Configure(Mesh);
  // The two originally rigid short hairstyles now have a dedicated, low-weight
  // tip chain; the original unused long-hair bones must remain unused.
  const bool ShortHair=FString(Role)==TEXT("Police")||FString(Role)==TEXT("GangBoss");
  TestTrue(FString(Role)+TEXT(" accessory support"),Motion.GetSimulatedBoneCount()>0);
  // Verify actual skin influence, rather than assuming a named bone moves vertices.
  TSet<int32> Weighted;
  if(auto* Model=Mesh->GetImportedModel())if(Model->LODModels.Num())
   for(const auto& Section:Model->LODModels[0].Sections)for(const auto& Vertex:Section.SoftVertices)
    for(int32 I=0;I<UE_ARRAY_COUNT(Vertex.InfluenceBones);++I)
     if(Vertex.InfluenceWeights[I]&&Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[I]))Weighted.Add(Section.BoneMap[Vertex.InfluenceBones[I]]);
  int32 WeightedAccessories=0;
  for(int32 B:Weighted)if(FCharacterSecondaryMotion::IsAccessoryBone(Ref.GetBoneName(B).ToString()))++WeightedAccessories;
  AddInfo(FString::Printf(TEXT("%s: %d simulated joints; %d accessory joints with real vertex weights"),Role,Motion.GetSimulatedBoneCount(),WeightedAccessories));
  TestTrue(FString(Role)+TEXT(" measured accessory skinning support"),WeightedAccessories>0);
  TArray<FTransform> Pose=Base;Motion.Solve(Pose,FTransform::Identity,1.f/60);
  bool InitialUnchanged=true;for(int32 B=0;B<Pose.Num();++B)InitialUnchanged&=Pose[B].Equals(Base[B],.001f);
  TestTrue(FString(Role)+TEXT(" no first-frame pop"),InitialUnchanged);
  float Peak=0;bool MainUnchanged=true,LengthsUnchanged=true,Finite=true;
  for(int32 Frame=1;Frame<=240;++Frame)
  {
   Pose=Base;const float T=Frame/60.f;
   const FTransform World(FRotator(0,25.f*FMath::Sin(T*3),0),FVector(30.f*FMath::Sin(T*4),100.f*T,0));
   Motion.Solve(Pose,World,1.f/60);Peak=FMath::Max(Peak,Motion.GetMaxAngleDegrees());
   for(int32 B=0;B<Pose.Num();++B)
   {
    Finite&=!Pose[B].ContainsNaN()&&Pose[B].IsRotationNormalized();
    LengthsUnchanged&=Pose[B].GetTranslation().Equals(Base[B].GetTranslation(),.0001f)&&Pose[B].GetScale3D().Equals(Base[B].GetScale3D(),.0001f);
    if(!FCharacterSecondaryMotion::IsAccessoryBone(Ref.GetBoneName(B).ToString()))MainUnchanged&=Pose[B].Equals(Base[B],.0001f);
   }
  }
  TestTrue(FString(Role)+TEXT(" locomotion, hands and weapon bones unchanged"),MainUnchanged);
  TestTrue(FString(Role)+TEXT(" bone lengths and scales preserved"),LengthsUnchanged);
  TestTrue(FString(Role)+TEXT(" finite normalized poses"),Finite);
  TestTrue(FString(Role)+TEXT(" bounded swing"),Peak<=(ShortHair?3.01f:18.01f));
  TestTrue(FString(Role)+TEXT(" responds to movement"),Peak>.1f);
  Pose=Base;Motion.Solve(Pose,FTransform(FVector(10000,0,0)),1.f/60);
  TestTrue(FString(Role)+TEXT(" teleport resets inertia"),Motion.GetMaxAngleDegrees()<.01f);
  Pose=Base;Motion.Solve(Pose,FTransform::Identity,.5f);
  TestTrue(FString(Role)+TEXT(" hitch resets inertia"),Motion.GetMaxAngleDegrees()<.01f);
  Pose=Base;Motion.Solve(Pose,FTransform::Identity,1.f/60,0);
  Pose=Base;Motion.Solve(Pose,FTransform(FVector(300,400,0)),1.f/60,1);
  TestTrue(FString(Role)+TEXT(" distance re-entry resets inertia"),Motion.GetMaxAngleDegrees()<.01f);
  FCharacterSecondaryMotion Other;Other.Configure(Mesh);Pose=Base;Other.Solve(Pose,FTransform::Identity,1.f/60);
  TestTrue(FString(Role)+TEXT(" instances have independent simulation"),Other.GetMaxAngleDegrees()<.01f);
 }
 for(const TCHAR* Name:{TEXT("Hair"),TEXT("Cloth")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(FString(TEXT("/Game/Characters/MechanicGirl/SK_Mechanic"))+Name+TEXT("_Secondary")));
  if(!TestNotNull(FString(TEXT("Player accessory "))+Name,Mesh))continue;
  auto* Pack=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/KINEMATION/TacticalShooterPack/Character/Operator/UE5/SKM_Operator_Mono"));
  TestTrue(TEXT("Accessory skeleton is isolated from weapon skeleton"),Pack&&Mesh->GetSkeleton()!=Pack->GetSkeleton());
  FCharacterSecondaryMotion Motion;Motion.Configure(Mesh);
  TestTrue(FString(Name)+TEXT(" supports simulation"),Motion.GetSimulatedBoneCount()>0);
  auto Base=Mesh->GetRefSkeleton().GetRefBonePose(),Pose=Base;
  Motion.Solve(Pose,FTransform::Identity,1.f/60);
  float Peak=0;
  for(int32 F=1;F<120;++F){Pose=Base;Motion.Solve(Pose,FTransform(FVector(15.f*FMath::Sin(F*.1f),0,0)),1.f/60);Peak=FMath::Max(Peak,Motion.GetMaxAngleDegrees());}
  TestTrue(FString(Name)+TEXT(" actually reacts"),Peak>.1f);
  TestTrue(FString(Name)+TEXT(" bounded swing"),Peak<=18.01f);
 }
 return true;
}
#endif
