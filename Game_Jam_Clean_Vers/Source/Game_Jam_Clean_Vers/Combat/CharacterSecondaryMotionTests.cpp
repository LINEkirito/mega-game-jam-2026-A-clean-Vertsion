#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CharacterSecondaryMotion.h"
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "CharacterSecondaryMotionComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSecondaryMotionLatePoseCheck,FAutomationTestBase*,Test);
bool FSecondaryMotionLatePoseCheck::Update()
{
 UWorld* World=nullptr;
 for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){World=C.World();break;}
 auto* PC=World?World->GetFirstPlayerController():nullptr;
 auto* Player=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
 auto* Motion=Player?Player->FindComponentByClass<UCharacterSecondaryMotionComponent>():nullptr;
 if(!Test->TestNotNull(TEXT("Runtime player physics component"),Motion))return true;
 auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
 if(!Test->TestNotNull(TEXT("Camera equipment"),Equipment))return true;
 Equipment->EquipCamera();auto* Camera=Equipment->Camcorder.Get();
 if(!Test->TestNotNull(TEXT("Selfie camera"),Camera))return true;
 // Complete the equip transition before asking to enter the viewfinder. V now
 // correctly rejects a camera that has not yet finished being equipped.
 Camera->Tick(2.f);
 Camera->SetViewfinderRaised(true);Camera->ToggleSelfie();
 Test->TestTrue(TEXT("Selfie remains enabled"),Camera->bSelfie);
 for(float Pitch:{-70.f,0.f,80.f})
 {
  PC->SetControlRotation(FRotator(Pitch,Player->GetActorRotation().Yaw,0));
  Camera->Tick(0);
  auto* Body=Player->GetFirstPersonMesh();
  for(auto* Part:{Motion->Hair.Get(),Motion->Cloth.Get()})
  {
   if(!Test->TestNotNull(TEXT("Runtime accessory"),Part))continue;
   for(const TCHAR* Bone:{TEXT("head"),TEXT("neck_01"),TEXT("spine_03"),TEXT("pelvis")})
    Test->TestTrue(FString(Part->GetName())+TEXT(" follows final selfie bone ")+Bone,Part->GetSocketTransform(Bone).Equals(Body->GetSocketTransform(Bone),.001f));
   const auto Before=Part->BoneSpaceTransforms;
   Body->RefreshBoneTransforms();
   for(int32 B=0;B<Before.Num();++B)
    if(FCharacterSecondaryMotion::IsAccessoryBone(Part->GetBoneName(B).ToString()))
     Test->TestTrue(TEXT("Late refresh preserves solved local physics without reintegration"),Before[B].Equals(Part->BoneSpaceTransforms[B],.0001f));
  }
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondaryMotionLatePoseTest,"Game.Characters.SecondaryMotionLatePose",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSecondaryMotionLatePoseTest::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FSecondaryMotionLatePoseCheck(this));AddCommand(new FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondaryMotionContactTest,"Game.Characters.SecondaryMotionContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSecondaryMotionContactTest::RunTest(const FString&)
{
 // The tip stays outside this shoulder-sized obstacle, but the middle of the
 // strand crosses it. Endpoint-only collision (the old solver) misses this.
 const FVector Origin=FVector::ZeroVector,Rest(0,0,-20);
 const FCharacterSecondaryMotion::FCapsule Obstacle{FVector(2.5f,0,-10),FVector(2.5f,0,-10),1.f};
 const FQuat Swing(FVector::YAxisVector,FMath::DegreesToRadians(-18.f));
 const FVector Unconstrained=Swing.RotateVector(Rest);
 TestTrue(TEXT("Tip alone clears obstacle"),FVector::Distance(Unconstrained,Obstacle.A)>Obstacle.Radius);
 TestTrue(TEXT("Middle of strand penetrates"),FVector::Distance(FMath::ClosestPointOnSegment(Obstacle.A,Origin,Unconstrained),Obstacle.A)<Obstacle.Radius);
 const FQuat Fixed=FCharacterSecondaryMotion::ConstrainSwing(Origin,Rest,Swing,{Obstacle});
 const FVector Tip=Fixed.RotateVector(Rest);
 TestTrue(TEXT("Whole final strand clears obstacle"),FVector::Distance(FMath::ClosestPointOnSegment(Obstacle.A,Origin,Tip),Obstacle.A)>=Obstacle.Radius-.001f);
 TestTrue(TEXT("Contact preserves bone length"),FMath::IsNearlyEqual(Tip.Size(),Rest.Size(),.0001));
 TestTrue(TEXT("Contact preserves angular limit"),Fixed.GetAngle()<=Swing.GetAngle()+.0001f);
 TestTrue(TEXT("Contact keeps some motion"),Fixed.GetAngle()>.01f);
 const FQuat Across(FVector::YAxisVector,FMath::DegreesToRadians(-40.f));
 const FQuat Swept=FCharacterSecondaryMotion::ConstrainSwing(Origin,Rest,Across,{Obstacle});
 TestTrue(TEXT("Cannot tunnel through to other side"),Swept.GetAngle()<FMath::DegreesToRadians(15.f));
 TestTrue(TEXT("No collider leaves motion intact"),FCharacterSecondaryMotion::ConstrainSwing(Origin,Rest,Swing,{}).Equals(Swing));
 return true;
}

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
  const int32 Head=Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head"));
  for(float Rate:{30.f,60.f,120.f})
  {
   Motion.Reset();bool Stable=true,Preserved=true;
   for(int32 Frame=0;Frame<180;++Frame)
   {
    Pose=Base;
    if(Head!=INDEX_NONE)Pose[Head].ConcatenateRotation(FQuat(FVector::YAxisVector,FMath::DegreesToRadians(25.f*FMath::Sin(Frame*.08f))));
    const auto Animated=Pose;
    Motion.Solve(Pose,FTransform(FRotator(0,Frame*2.f,0),FVector(Frame*2.f,0,15.f*FMath::Sin(Frame*.1f))),1.f/Rate);
    for(int32 B=0;B<Pose.Num();++B)
    {
     Stable&=!Pose[B].ContainsNaN()&&Pose[B].IsRotationNormalized();
     Preserved&=Pose[B].GetTranslation().Equals(Animated[B].GetTranslation(),.0001f)&&Pose[B].GetScale3D().Equals(Animated[B].GetScale3D(),.0001f);
     if(!FCharacterSecondaryMotion::IsAccessoryBone(Mesh->GetRefSkeleton().GetBoneName(B).ToString()))Preserved&=Pose[B].Equals(Animated[B],.0001f);
    }
   }
   TestTrue(FString::Printf(TEXT("%s head turns at %.0f FPS remain finite"),Name,Rate),Stable);
   TestTrue(FString::Printf(TEXT("%s at %.0f FPS preserves animated body and lengths"),Name,Rate),Preserved);
  }
  Pose=Base;Motion.Solve(Pose,FTransform(FVector(10000,0,0)),1.f/60);
  TestTrue(FString(Name)+TEXT(" player teleport resets"),Motion.GetMaxAngleDegrees()<.01f);
  Pose=Base;Motion.Solve(Pose,FTransform::Identity,.5f);
  TestTrue(FString(Name)+TEXT(" player hitch resets"),Motion.GetMaxAngleDegrees()<.01f);
 }
 return true;
}
#endif
