#include "NPCMotionAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"

namespace NPCMotion
{
struct FProxy : FAnimInstanceProxy
{
 UAnimSequence* Sequence=nullptr;
 float Time=0,BlendTime=1,BlendDuration=.18f;
 bool CaptureBlend=false;
 bool Turning=false;
 bool WasLocomotion=false;
 TArray<FTransform> LastPose,BlendFrom;
 explicit FProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance){}
 virtual void PreUpdate(UAnimInstance* Instance,float DT) override
 {
  FAnimInstanceProxy::PreUpdate(Instance,DT);
  auto* Motion=CastChecked<UNPCMotionAnimInstance>(Instance);
  Turning=Motion->ExplicitTime>=0;
  if(Sequence!=Motion->Sequence)
  {
   const float Phase=Sequence&&Sequence->GetPlayLength()>0?Time/Sequence->GetPlayLength():0;
   Sequence=Motion->Sequence;Time=Sequence&&WasLocomotion&&Motion->bLocomotion?Phase*Sequence->GetPlayLength():0;
   BlendTime=0;BlendDuration=FMath::Max(.01f,Motion->PoseBlendDuration);CaptureBlend=true;
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
  else {auto& Root=Output.Pose[FCompactPoseBoneIndex(0)];const float Height=Root.GetTranslation().Z;Root.SetTranslation(FVector(0,0,Height));}
  const float Alpha=FMath::SmoothStep(0.f,BlendDuration,BlendTime);
  if(BlendFrom.Num()==Output.Pose.GetNumBones()&&Alpha<1)
   for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex()){FTransform T;T.Blend(BlendFrom[B.GetInt()],Output.Pose[B],Alpha);Output.Pose[B]=T;}
  LastPose.SetNum(Output.Pose.GetNumBones());for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())LastPose[B.GetInt()]=Output.Pose[B];
  return true;
 }
};
}
FAnimInstanceProxy* UNPCMotionAnimInstance::CreateAnimInstanceProxy(){return new NPCMotion::FProxy(this);}
void UNPCMotionAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
