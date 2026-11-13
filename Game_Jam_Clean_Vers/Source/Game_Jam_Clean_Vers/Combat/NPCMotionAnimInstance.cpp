#include "NPCMotionAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"

namespace NPCMotion
{
struct FProxy : FAnimInstanceProxy
{
 UAnimSequence* Sequence=nullptr;
 UAnimSequence* UpperBodyPose=nullptr;
 float Time=0,BlendTime=1;
 bool CaptureBlend=false;
 bool Turning=false;
 bool WasLocomotion=false;
 TArray<FTransform> LastPose,BlendFrom;
 explicit FProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance){}
 virtual void PreUpdate(UAnimInstance* Instance,float DT) override
 {
  FAnimInstanceProxy::PreUpdate(Instance,DT);
  auto* Motion=CastChecked<UNPCMotionAnimInstance>(Instance);
  UpperBodyPose=Motion->UpperBodyPose;
  Turning=Motion->ExplicitTime>=0;
  if(Sequence!=Motion->Sequence)
  {
   const float Phase=Sequence&&Sequence->GetPlayLength()>0?Time/Sequence->GetPlayLength():0;
   Sequence=Motion->Sequence;Time=Sequence&&WasLocomotion&&Motion->bLocomotion?Phase*Sequence->GetPlayLength():0;
   BlendTime=0;CaptureBlend=true;
  }
  else if(Sequence){Time+=DT*Motion->PlaybackRate;Time=Motion->bLoop?FMath::Fmod(Time,FMath::Max(.001f,Sequence->GetPlayLength())):FMath::Min(Time,Sequence->GetPlayLength());}
  BlendTime+=DT;
  WasLocomotion=Motion->bLocomotion;
  if(Motion->ExplicitTime>=0)Time=Motion->ExplicitTime;
 }
 virtual bool Evaluate(FPoseContext& Output) override
 {
  Output.ResetToRefPose();
  if(!Sequence)return true;
  if(CaptureBlend){BlendFrom=LastPose;CaptureBlend=false;}
  FAnimationPoseData Data(Output);Sequence->GetAnimationPose(Data,FAnimExtractContext(Time,false));
  // Navigation owns world displacement and yaw. Never apply the authored root twice.
  if(Turning)Output.Pose[FCompactPoseBoneIndex(0)]=FTransform::Identity;
  else Output.Pose[FCompactPoseBoneIndex(0)].SetTranslation(FVector::ZeroVector);
  if(UpperBodyPose)
  {
   FPoseContext Upper(Output);Upper.ResetToRefPose();FAnimationPoseData UpperData(Upper);
   UpperBodyPose->GetAnimationPose(UpperData,FAnimExtractContext(0,false));
   const auto& Bones=Output.Pose.GetBoneContainer();const auto& Ref=Bones.GetReferenceSkeleton();
   const int32 Spine=Ref.FindBoneIndex(TEXT("spine_01"));
   TArray<FTransform> BodyCS,UpperCS;BodyCS.SetNum(Output.Pose.GetNumBones());UpperCS.SetNum(Output.Pose.GetNumBones());
   for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
   {
    const auto Parent=Output.Pose.GetParentBoneIndex(B);
    BodyCS[B.GetInt()]=Parent.GetInt()<0?Output.Pose[B]:Output.Pose[B]*BodyCS[Parent.GetInt()];
    UpperCS[B.GetInt()]=Parent.GetInt()<0?Upper.Pose[B]:Upper.Pose[B]*UpperCS[Parent.GetInt()];
   }
   for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
   {
    int32 Index=Bones.MakeMeshPoseIndex(B).GetInt();bool IsUpper=false;
    while(Index>=0){if(Index==Spine){IsUpper=true;break;}Index=Ref.GetParentIndex(Index);}
    if(IsUpper)
    {
     if(Bones.MakeMeshPoseIndex(B).GetInt()==Spine)
     {
      const auto Parent=Output.Pose.GetParentBoneIndex(B);
      FTransform Stable=UpperCS[B.GetInt()];
      Stable.AddToTranslation(BodyCS[Parent.GetInt()].GetLocation()-UpperCS[Parent.GetInt()].GetLocation());
      Output.Pose[B]=Stable.GetRelativeTransform(BodyCS[Parent.GetInt()]);
     }
     else Output.Pose[B]=Upper.Pose[B];
    }
   }
  }
  const float Alpha=FMath::SmoothStep(0.f,.18f,BlendTime);
  if(BlendFrom.Num()==Output.Pose.GetNumBones()&&Alpha<1)
   for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex()){FTransform T;T.Blend(BlendFrom[B.GetInt()],Output.Pose[B],Alpha);Output.Pose[B]=T;}
  LastPose.SetNum(Output.Pose.GetNumBones());for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())LastPose[B.GetInt()]=Output.Pose[B];
  return true;
 }
};
}
FAnimInstanceProxy* UNPCMotionAnimInstance::CreateAnimInstanceProxy(){return new NPCMotion::FProxy(this);}
void UNPCMotionAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
