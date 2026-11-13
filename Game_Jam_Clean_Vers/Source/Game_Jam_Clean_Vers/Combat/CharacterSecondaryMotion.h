#pragma once
#include "CoreMinimal.h"

class USkeletalMesh;
class UPoseableMeshComponent;

// Per-character secondary animation. Only accessory rotations are written;
// animation, locomotion, hands and weapon mechanics retain ownership of their pose.
// Configure on the game thread. Solve consumes snapshots and is animation-worker safe.
class FCharacterSecondaryMotion
{
public:
 struct FCapsule { FVector A, B; float Radius=0; };
 void Configure(const USkeletalMesh* Mesh);
 void Reset();
 void SetBodyCollisionEnabled(bool Enabled) { BodyCollisionEnabled=Enabled; }
 void Solve(TArray<FTransform>& LocalPose,const FTransform& MeshWorld,float DeltaTime,
            float Weight=1.f,const TArray<FCapsule>& ExternalColliders={});
 void Apply(UPoseableMeshComponent* Mesh,float DeltaTime,float Weight=1.f,
            const TArray<FCapsule>& ExternalColliders={});
 int32 GetSimulatedBoneCount() const { return Particles.Num(); }
 float GetMaxAngleDegrees() const { return MaxAngleDegrees; }
 static bool IsAccessoryBone(const FString& Name);
private:
 struct FParticle
 {
  int32 Bone=INDEX_NONE;
  FVector TipLocal=FVector::ZeroVector,Position=FVector::ZeroVector,Velocity=FVector::ZeroVector;
  bool Cloth=false,ShortHair=false;
 };
 TWeakObjectPtr<const USkeletalMesh> ConfiguredMesh;
 TArray<int32> Parents,ParticleForBone;
 TArray<FParticle> Particles;
 TArray<FTransform> ComponentPose,AnimatedPose;
 TArray<FCapsule> BodyColliders;
 int32 Head=INDEX_NONE,Neck=INDEX_NONE,Chest=INDEX_NONE,Pelvis=INDEX_NONE;
 int32 ThighL=INDEX_NONE,CalfL=INDEX_NONE,ThighR=INDEX_NONE,CalfR=INDEX_NONE;
 FTransform PreviousWorld=FTransform::Identity;
 bool Initialized=false;
 bool BodyCollisionEnabled=true;
 float MaxAngleDegrees=0;
};
