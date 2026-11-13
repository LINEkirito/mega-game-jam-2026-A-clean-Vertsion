#include "CharacterSecondaryMotion.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"

bool FCharacterSecondaryMotion::IsAccessoryBone(const FString& Name)
{
 return Name.StartsWith(TEXT("hair_"))||Name.StartsWith(TEXT("celeste_hair_"))||Name.StartsWith(TEXT("mech_hair_"))||Name.StartsWith(TEXT("mech_sleeve_"))||
        Name.StartsWith(TEXT("cloth_"))||Name.StartsWith(TEXT("celeste_skirt_"));
}

void FCharacterSecondaryMotion::Reset()
{
 Initialized=false;MaxAngleDegrees=0;
 for(auto& P:Particles)P.Velocity=FVector::ZeroVector;
}

void FCharacterSecondaryMotion::Configure(const USkeletalMesh* Mesh)
{
 if(ConfiguredMesh.Get()==Mesh&&Parents.Num()>0)return;
 ConfiguredMesh=Mesh;Parents.Reset();Particles.Reset();ParticleForBone.Reset();Reset();
 if(!Mesh)return;
 const auto& Ref=Mesh->GetRefSkeleton();const auto& Pose=Ref.GetRefBonePose();
 Parents.SetNum(Ref.GetNum());ParticleForBone.Init(INDEX_NONE,Ref.GetNum());
 for(int32 B=0;B<Ref.GetNum();++B)Parents[B]=Ref.GetParentIndex(B);
 for(int32 B=0;B<Ref.GetNum();++B)
 {
  const FString Name=Ref.GetBoneName(B).ToString();
  if(!IsAccessoryBone(Name))continue;
  const auto* Render=Mesh->GetResourceForRendering();
  if(Render&&Render->LODRenderData.Num()&&!Render->LODRenderData[0].ActiveBoneIndices.Contains(B))continue;
  // A rigid bun is anchored to the head. Simulate hanging hair, not the knot.
  if(Name.Contains(TEXT("bun_")))continue;
  FVector Tip=FVector::ZeroVector;
  for(int32 C=B+1;C<Ref.GetNum();++C)
   if(Parents[C]==B&&IsAccessoryBone(Ref.GetBoneName(C).ToString())){Tip=Pose[C].GetTranslation();break;}
  // Terminal bones have no independently movable geometry beyond the endpoint.
  // Their inherited parent motion is sufficient and avoids inventing an axis.
  if(Tip.SizeSquared()<.01f)continue;
  FParticle P;P.Bone=B;P.TipLocal=Tip;P.Cloth=Name.Contains(TEXT("cloth"))||Name.Contains(TEXT("skirt"))||Name.Contains(TEXT("sleeve"));
  P.ShortHair=Name.StartsWith(TEXT("hair_secondary_short_"));
  ParticleForBone[B]=Particles.Add(P);
 }
 Head=Ref.FindBoneIndex(TEXT("head"));Neck=Ref.FindBoneIndex(TEXT("neck_01"));
 Chest=Ref.FindBoneIndex(TEXT("spine_03"));Pelvis=Ref.FindBoneIndex(TEXT("pelvis"));
 ThighL=Ref.FindBoneIndex(TEXT("thigh_l"));CalfL=Ref.FindBoneIndex(TEXT("calf_l"));
 ThighR=Ref.FindBoneIndex(TEXT("thigh_r"));CalfR=Ref.FindBoneIndex(TEXT("calf_r"));
}

void FCharacterSecondaryMotion::Solve(TArray<FTransform>& Local,const FTransform& World,float DT,
                                     float Weight,const TArray<FCapsule>& External)
{
 MaxAngleDegrees=0;
 if(Particles.IsEmpty()||Local.Num()!=Parents.Num()) { Reset();return; }
 if(!FMath::IsFinite(DT)||DT<=0)return;
 if(Weight<=KINDA_SMALL_NUMBER){Reset();return;}
 Weight=FMath::Clamp(Weight,0.f,1.f);
 const bool Discontinuity=Initialized&&(DT>.15f||
  FVector::DistSquared(World.GetLocation(),PreviousWorld.GetLocation())>FMath::Square(150.f)||
  World.GetRotation().AngularDistance(PreviousWorld.GetRotation())>FMath::DegreesToRadians(75.f)||
  !World.GetScale3D().Equals(PreviousWorld.GetScale3D(),.001f));
 if(Discontinuity)Reset();
 ComponentPose.SetNum(Local.Num());AnimatedPose.SetNum(Local.Num());
 for(int32 B=0;B<Local.Num();++B)
  AnimatedPose[B]=Parents[B]<0?Local[B]*World:Local[B]*AnimatedPose[Parents[B]];
 BodyColliders.Reset();
 auto Capsule=[&](int32 A,int32 B,float R){if(AnimatedPose.IsValidIndex(A)&&AnimatedPose.IsValidIndex(B))
  BodyColliders.Add({AnimatedPose[A].GetLocation(),AnimatedPose[B].GetLocation(),static_cast<float>(R*World.GetScale3D().GetAbsMax())});};
 if(BodyCollisionEnabled)
 {
  Capsule(Head,Head,8.f);Capsule(Neck,Chest,9.f);Capsule(Chest,Pelvis,12.f);
  Capsule(ThighL,CalfL,7.f);Capsule(ThighR,CalfR,7.f);
 }
 const FVector Movement=Initialized?World.GetLocation()-PreviousWorld.GetLocation():FVector::ZeroVector;
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(DT*120.f),1,12);
 const float H=FMath::Min(DT,.1f)/Steps;
 for(int32 B=0;B<Local.Num();++B)
 {
  const int32 Parent=Parents[B];
  FTransform Current=Parent<0?Local[B]*World:Local[B]*ComponentPose[Parent];
  const int32 ParticleIndex=ParticleForBone[B];
  if(ParticleIndex!=INDEX_NONE)
  {
   auto& P=Particles[ParticleIndex];
   const FVector Origin=Current.GetLocation(),Target=Current.TransformPosition(P.TipLocal);
   const FVector Rest=Target-Origin;const float Length=Rest.Size();
   if(!Initialized){P.Position=Target;P.Velocity=FVector::ZeroVector;}
   else
   {
    // Most translation follows the actor; the remaining inertia produces sway.
    P.Position+=Movement*.8f;
    const float Spring=P.ShortHair?220.f:P.Cloth?140.f:95.f,Damping=P.ShortHair?26.f:P.Cloth?20.f:15.f;
    for(int32 Step=0;Step<Steps;++Step)
    {
     P.Velocity+=(Target-P.Position)*Spring*H;
     P.Velocity*=FMath::Exp(-Damping*H);
     P.Position+=P.Velocity*H;
    }
   }
   // Conservative contact envelopes. Rest-pose clearance limits each radius,
   // so authored contact never pushes a strand out of place at startup.
   auto Collide=[&](const FCapsule& C)
   {
    const FVector RestClosest=FMath::ClosestPointOnSegment(Target,C.A,C.B);
    const float Radius=FMath::Min(C.Radius,FMath::Max(0.f,FVector::Distance(Target,RestClosest)-.2f));
    const FVector Closest=FMath::ClosestPointOnSegment(P.Position,C.A,C.B);
    FVector Out=P.Position-Closest;const float Distance=Out.Size();
    if(Radius>0&&Distance<Radius)
    {
     Out=Distance>SMALL_NUMBER?Out/Distance:(Target-RestClosest).GetSafeNormal();
     P.Position=Closest+Out*Radius;
     const float Into=FVector::DotProduct(P.Velocity,Out);if(Into<0)P.Velocity-=Out*Into;
    }
   };
   for(const auto& C:BodyColliders)Collide(C);
   for(const auto& C:External)Collide(C);
   const FVector RestDirection=Rest.GetSafeNormal();
   FVector Direction=(P.Position-Origin).GetSafeNormal();if(Direction.IsNearlyZero())Direction=RestDirection;
   FQuat Delta=FQuat::FindBetweenNormals(RestDirection,Direction);
   const float Angle=Delta.GetAngle();
   const float Limit=FMath::DegreesToRadians(P.ShortHair?3.f:P.Cloth?10.f:18.f);
   if(Angle>Limit)Delta=FQuat::Slerp(FQuat::Identity,Delta,Limit/Angle);
   // Projection keeps authored lengths and removes radial velocity.
   Direction=Delta.RotateVector(RestDirection);
   P.Position=Origin+Direction*Length;
   P.Velocity-=Direction*FVector::DotProduct(P.Velocity,Direction);
   P.Velocity=P.Velocity.GetClampedToMaxSize(150.f);
   Delta=FQuat::Slerp(FQuat::Identity,Delta,Weight);
   MaxAngleDegrees=FMath::Max(MaxAngleDegrees,FMath::RadiansToDegrees(Delta.GetAngle()));
   Current.SetRotation((Delta*Current.GetRotation()).GetNormalized());
   // Write rotation only. In particular never stretch a skinned joint or move its root.
   Local[B].SetRotation((Parent<0?World.GetRotation().Inverse():ComponentPose[Parent].GetRotation().Inverse())*Current.GetRotation());
   Local[B].NormalizeRotation();
  }
  ComponentPose[B]=Current;
 }
 PreviousWorld=World;Initialized=true;
}

void FCharacterSecondaryMotion::Apply(UPoseableMeshComponent* Mesh,float DT,float Weight,const TArray<FCapsule>& External)
{
 if(!Mesh)return;
 Configure(Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()));
 Solve(Mesh->BoneSpaceTransforms,Mesh->GetComponentTransform(),DT,Weight,External);
 if(Weight>0&&!Particles.IsEmpty()){Mesh->MarkRefreshTransformDirty();Mesh->RefreshBoneTransforms();}
}
