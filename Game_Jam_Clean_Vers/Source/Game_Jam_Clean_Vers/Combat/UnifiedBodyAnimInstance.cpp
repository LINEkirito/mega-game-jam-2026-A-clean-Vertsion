#include "UnifiedBodyAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "CombatEquipmentComponent.h"
#include "TacticalWeapon.h"
#include "Camcorder/CamcorderEquipmentComponent.h"
#include "Camcorder/ShoulderCamcorder.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camcorder/ItemInteractionComponent.h"
#include "GameFramework/PlayerController.h"

namespace UnifiedBody
{
// All game objects are sampled on the game thread. Evaluation uses only snapshots.
struct FProxy : FAnimInstanceProxy
{
    TMap<FName,FTransform> BodyLocalPose;
    TMap<FName,FTransform> AuthoredComponentPose;
    TMap<FName,FQuat> FingerRotations;
    TWeakObjectPtr<UAnimSequence> Sequence;
    TWeakObjectPtr<UAnimSequence> Unarmed;
    float UnarmedTime=0,UnarmedAlpha=0;
    FTransform Wrists[2];
    FTransform MeshWorld;
    bool Active=false;
    bool ApplyHand[2]={false,false};
    float Time=0;
    float LowAimAlpha=0;
    bool Pistol=false;
    bool StableWeaponAim=false;
    float CrouchAlpha=0,CrouchPhase=0,CrouchMoveAlpha=0,CrouchDirectionAlpha=0;
    TWeakObjectPtr<UAnimSequence> CrouchIdle,CrouchMoveA,CrouchMoveB;
    FVector BodyRight,BodyForward;
    bool SelfieLook=false;
    FVector SelfieLookTarget;
    explicit FProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance){}
    virtual void PreUpdate(UAnimInstance* Instance,float DT) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,DT);
        AuthoredComponentPose.Reset();FingerRotations.Reset();Active=false;ApplyHand[0]=ApplyHand[1]=false;Sequence.Reset();
        auto* Character=Cast<AGame_Jam_Clean_VersCharacter>(Instance->GetOwningActor());
        auto* PC=Character?Cast<APlayerController>(Character->GetController()):nullptr;
        BodyLocalPose.Reset();SelfieLook=false;
        if(Character&&Character->GetMesh())
        {
            auto* Body=Character->GetMesh();
            const auto& Local=Body->GetBoneSpaceTransforms();
            for(int32 B=0;B<Local.Num();++B)BodyLocalPose.Add(Body->GetBoneName(B),Local[B]);
        }
        if(!PC)return;
        MeshWorld=Instance->GetSkelMeshComponent()->GetComponentTransform();
        BodyRight=MeshWorld.InverseTransformVectorNoScale(Character->GetActorRightVector());
        BodyForward=MeshWorld.InverseTransformVectorNoScale(Character->GetActorForwardVector());
        LowAimAlpha=FMath::Clamp((-FMath::UnwindDegrees(PC->GetControlRotation().Pitch)-25.f)/40.f,0.f,1.f);
        auto* Combat=PC->FindComponentByClass<UCombatEquipmentComponent>();
        auto* Gun=Combat?Combat->GetPresentationWeapon():nullptr;
        Pistol=Gun&&Gun->Kind==ETacticalWeaponKind::Pistol;
        StableWeaponAim=Gun&&Gun->HasStableADSPose();
        CrouchAlpha=FMath::FInterpTo(CrouchAlpha,Character->bIsCrouched?1.f:0.f,DT,12.f);
        if(CrouchAlpha>.001f)
        {
            auto* BodyInstance=CastChecked<UUnifiedBodyAnimInstance>(Instance);
            auto GetCrouch=[&](const FString& Suffix)
            {
                const FString Path=TEXT("/Game/Characters/MechanicGirl/Animations/Body_A_Rifle_Crouch_")+Suffix;
                auto& Cached=BodyInstance->CachedAnimations.FindOrAdd(Path);
                if(!Cached)Cached=LoadObject<UAnimSequence>(nullptr,*Path);
                return Cached.Get();
            };
            const FVector Velocity=Character->GetActorTransform().InverseTransformVectorNoScale(Character->GetVelocity());
            const float Speed=Velocity.Size2D();
            CrouchMoveAlpha=FMath::Clamp(Speed/60.f,0.f,1.f);
            CrouchPhase+=DT*FMath::Max(.1f,Speed/180.f);
            const float Direction=FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y,Velocity.X))+360.f,360.f)/45.f;
            const int32 Sector=FMath::FloorToInt(Direction)%8;CrouchDirectionAlpha=FMath::Frac(Direction);
            static const TCHAR* Directions[]={TEXT("Forward"),TEXT("Forward_Right"),TEXT("Right"),TEXT("Backward_Right"),TEXT("Backward"),TEXT("Backward_Left"),TEXT("Left"),TEXT("Forward_Left")};
            CrouchIdle=GetCrouch(TEXT("Idle"));
            CrouchMoveA=GetCrouch(FString(TEXT("Walk_"))+Directions[Sector]);
            CrouchMoveB=GetCrouch(FString(TEXT("Walk_"))+Directions[(Sector+1)%8]);
        }
        USkinnedMeshComponent* Source=nullptr;
        if(Gun&&Gun->bEquipped)
        {
            if(Gun->bPackPresentation && Gun->Hands)
            {
                auto* Pack=Gun->Hands.Get();
                for(int32 B=0;B<Pack->GetNumBones();++B){const FName Name=Pack->GetBoneName(B);AuthoredComponentPose.Add(Name,Pack->GetSocketTransform(Name,RTS_Component));}
                Source=Pack;
            }
            else Source=Gun->RenderArms;
            const FString Type=Gun->Kind==ETacticalWeaponKind::Pistol?TEXT("Pistol"):Gun->Kind==ETacticalWeaponKind::Shotgun?TEXT("Shotgun"):TEXT("Rifle");
            FString Clip=TEXT("/Game/Combat/BodyAnimations/Body_A_")+Type+TEXT("_Reload");
            if(!Gun->IsReloading()||Gun->Handling==EWeaponHandling::Charge)Clip=TEXT("/Game/Characters/Mannequins/Anims/")+(Gun->Kind==ETacticalWeaponKind::Pistol?FString(TEXT("Pistol/MF_Pistol_Idle_ADS")):FString(TEXT("Rifle/MF_Rifle_Idle_ADS")));
            auto* BodyInstance=CastChecked<UUnifiedBodyAnimInstance>(Instance);
            auto& Cached=BodyInstance->CachedAnimations.FindOrAdd(Clip);
            if(!Cached)Cached=LoadObject<UAnimSequence>(nullptr,*Clip);
            Sequence=Gun->bPackPresentation?nullptr:Cached.Get();
            const float Progress=Gun->ReloadProgress();
            Time=Sequence.IsValid()?Progress*Sequence->GetPlayLength():0;
        }
        else if(auto* Equipment=PC->FindComponentByClass<UCamcorderEquipmentComponent>())
        {
            if(Equipment->Camcorder&&Equipment->Camcorder->IsPresenting())
            {
                Source=Equipment->Camcorder->HoldingHands;
                SelfieLook=Equipment->Camcorder->bSelfie;
                SelfieLookTarget=MeshWorld.InverseTransformPosition(Equipment->Camcorder->Capture->GetComponentLocation());
            }
        }
        if(Source)
        {
            Active=true;ApplyHand[0]=ApplyHand[1]=true;
            Wrists[0]=Source->GetSocketTransform(TEXT("hand_l"));
            Wrists[1]=Source->GetSocketTransform(TEXT("hand_r"));
            for(int32 B=0;B<Source->GetNumBones();++B)
            {
                const FName Name=Source->GetBoneName(B);const FString N=Name.ToString();
                if(N.StartsWith(TEXT("index_"))||N.StartsWith(TEXT("middle_"))||N.StartsWith(TEXT("ring_"))||N.StartsWith(TEXT("pinky_"))||N.StartsWith(TEXT("thumb_")))
                    FingerRotations.Add(Name,Source->GetSocketTransform(Name).GetRelativeTransform(Source->GetSocketTransform(Source->GetParentBone(Name))).GetRotation());
            }
        }
        UnarmedAlpha=FMath::FInterpTo(UnarmedAlpha,Source?0.f:1.f,DT,12.f);
        if(UnarmedAlpha>.001f)
        {
            const FString Clip=Character->GetVelocity().Size2D()>10.f?
                TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"):
                TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle");
            auto& Cached=CastChecked<UUnifiedBodyAnimInstance>(Instance)->CachedAnimations.FindOrAdd(Clip);
            if(!Cached)Cached=LoadObject<UAnimSequence>(nullptr,*Clip);
            Unarmed=Cached.Get();UnarmedTime+=DT;
        }
        if(auto* Interaction=Character->FindComponentByClass<UItemInteractionComponent>())
            if(Interaction->GetLeftWristTarget(Wrists[0])){Active=true;ApplyHand[0]=true;}
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        const auto& Bones=Output.Pose.GetBoneContainer();
        const auto& Ref=Bones.GetReferenceSkeleton();
        for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
            if(const auto* Local=BodyLocalPose.Find(Ref.GetBoneName(Bones.MakeMeshPoseIndex(B).GetInt())))Output.Pose[B]=*Local;
        if(CrouchAlpha>.001f && CrouchIdle.IsValid() && CrouchMoveA.IsValid() && CrouchMoveB.IsValid())
        {
            FPoseContext Idle(Output),MoveA(Output),MoveB(Output);
            auto Sample=[&](UAnimSequence* Clip,FPoseContext& Pose)
            {
                Pose.ResetToRefPose();FAnimationPoseData Data(Pose);
                Clip->GetAnimationPose(Data,FAnimExtractContext(FMath::Fmod(CrouchPhase,Clip->GetPlayLength()),true));
                auto& Root=Pose.Pose[FCompactPoseBoneIndex(0)];
                Root.SetTranslation(FVector(0,0,Root.GetTranslation().Z));
            };
            Sample(CrouchIdle.Get(),Idle);Sample(CrouchMoveA.Get(),MoveA);Sample(CrouchMoveB.Get(),MoveB);
            for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
            {
                FTransform Moving,Target,Blended;
                Moving.Blend(MoveA.Pose[B],MoveB.Pose[B],CrouchDirectionAlpha);
                Target.Blend(Idle.Pose[B],Moving,CrouchMoveAlpha);
                Blended.Blend(Output.Pose[B],Target,CrouchAlpha);Output.Pose[B]=Blended;
            }
        }
        if(UnarmedAlpha>.001f&&Unarmed.IsValid())
        {
            FPoseContext Sample(Output);Sample.ResetToRefPose();FAnimationPoseData Data(Sample);
            Unarmed->GetAnimationPose(Data,FAnimExtractContext(FMath::Fmod(UnarmedTime,Unarmed->GetPlayLength()),true));
            for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
            {
                const FString Name=Ref.GetBoneName(Bones.MakeMeshPoseIndex(B).GetInt()).ToString();
                if(Name.StartsWith(TEXT("clavicle"))||Name.StartsWith(TEXT("upperarm"))||Name.StartsWith(TEXT("lowerarm"))||Name.StartsWith(TEXT("hand_"))||Name.StartsWith(TEXT("index_"))||Name.StartsWith(TEXT("middle_"))||Name.StartsWith(TEXT("ring_"))||Name.StartsWith(TEXT("pinky_"))||Name.StartsWith(TEXT("thumb_")))
                {FTransform Blended;Blended.Blend(Output.Pose[B],Sample.Pose[B],UnarmedAlpha);Output.Pose[B]=Blended;}
            }
        }
        if(!AuthoredComponentPose.IsEmpty())
        {
            // Keep locomotion on pelvis, spine and legs. Preserve the pack's authored
            // arm and finger rotations, then solve wrists to the same weapon stream.
            for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
            {
                const int32 MeshIndex=Bones.MakeMeshPoseIndex(B).GetInt();
                const FName BoneName=Ref.GetBoneName(MeshIndex);
                const FString Name=BoneName.ToString();
                const bool Arm=Name.StartsWith(TEXT("upperarm"))||Name.StartsWith(TEXT("lowerarm"))||Name.StartsWith(TEXT("hand_"))||Name.StartsWith(TEXT("index_"))||Name.StartsWith(TEXT("middle_"))||Name.StartsWith(TEXT("ring_"))||Name.StartsWith(TEXT("pinky_"))||Name.StartsWith(TEXT("thumb_"));
                if(!Arm)continue;
                if(const FTransform* T=AuthoredComponentPose.Find(BoneName))
                {
                    const int32 Parent=Ref.GetParentIndex(MeshIndex);
                    const FTransform* P=Parent>=0?AuthoredComponentPose.Find(Ref.GetBoneName(Parent)):nullptr;
                    Output.Pose[B].SetRotation((P?T->GetRelativeTransform(*P):*T).GetRotation());
                }
            }
        }
        if(!Active)return true;
        auto Index=[&](FName Name){return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Ref.FindBoneIndex(Name)));};
        if(auto* Anim=Sequence.Get())
        {
            FPoseContext Sample(Output);Sample.ResetToRefPose();
            FAnimationPoseData Data(Sample);
            Anim->GetAnimationPose(Data,FAnimExtractContext(Time,false));
            // Preserve locomotion, pelvis, torso and the camera's head bone.
            for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
            {
                const FString Name=Ref.GetBoneName(Bones.MakeMeshPoseIndex(B).GetInt()).ToString();
                if(Name.StartsWith(TEXT("upperarm"))||Name.StartsWith(TEXT("lowerarm"))||Name.StartsWith(TEXT("hand_"))||Name.StartsWith(TEXT("index_"))||Name.StartsWith(TEXT("middle_"))||Name.StartsWith(TEXT("ring_"))||Name.StartsWith(TEXT("pinky_"))||Name.StartsWith(TEXT("thumb_")))Output.Pose[B]=Sample.Pose[B];
            }
        }
        for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex())
            if(const auto* Rotation=FingerRotations.Find(Ref.GetBoneName(Bones.MakeMeshPoseIndex(B).GetInt())))Output.Pose[B].SetRotation(*Rotation);
        TArray<FTransform> CS;CS.SetNum(Output.Pose.GetNumBones());
        auto Rebuild=[&](){for(FCompactPoseBoneIndex B:Output.Pose.ForEachBoneIndex()){const auto Parent=Output.Pose.GetParentBoneIndex(B);CS[B.GetInt()]=Parent.GetInt()<0?Output.Pose[B]:Output.Pose[B]*CS[Parent.GetInt()];}};
        auto SetRotation=[&](FCompactPoseBoneIndex B,const FQuat& Rotation){const auto Parent=Output.Pose.GetParentBoneIndex(B);Output.Pose[B].SetRotation((Parent.GetInt()<0?Rotation:CS[Parent.GetInt()].GetRotation().Inverse()*Rotation).GetNormalized());Rebuild();};
        Rebuild();
        if(SelfieLook)
        {
            const auto Head=Index(TEXT("head"));
            if(Head.GetInt()>=0)
            {
                // This rig maps both original eye bones onto head. Align the eye/head
                // forward axis with the actual lens, preserving the purchased skeleton.
                int32 RefIndex=Ref.FindBoneIndex(TEXT("head"));
                FTransform RefHead=Ref.GetRefBonePose()[RefIndex];
                while((RefIndex=Ref.GetParentIndex(RefIndex))>=0)RefHead=RefHead*Ref.GetRefBonePose()[RefIndex];
                const FVector LocalForward=RefHead.InverseTransformVectorNoScale(BodyForward).GetSafeNormal();
                const FVector EyeOffset=RefHead.InverseTransformVectorNoScale(BodyForward*8.f+FVector(0,0,6));
                const FTransform HeadPose=CS[Head.GetInt()];
                const FVector Eye=HeadPose.TransformPosition(EyeOffset);
                const FVector Forward=HeadPose.TransformVectorNoScale(LocalForward).GetSafeNormal();
                const FVector TowardLens=(SelfieLookTarget-Eye).GetSafeNormal();
                if(!TowardLens.IsNearlyZero())SetRotation(Head,FQuat::FindBetweenNormals(Forward,TowardLens)*HeadPose.GetRotation());
            }
        }
        if(StableWeaponAim)
        {
            // Let the torso support the stationary ADS rig instead of pulling
            // the gun into the eye. CCD over the spine preserves all bone lengths
            // and leaves pelvis/legs rooted in their locomotion pose.
            for(int32 Iteration=0;Iteration<8;++Iteration)
            for(const TCHAR* SpineName:{TEXT("spine_03"),TEXT("spine_02"),TEXT("spine_01")})
            {
                const auto Spine=Index(SpineName);
                if(Spine.GetInt()<0)continue;
                FVector ShoulderMean=FVector::ZeroVector,GoalMean=FVector::ZeroVector;
                int32 Count=0;
                for(int32 Side=0;Side<2;++Side)
                {
                    const auto Upper=Index(Side==0?TEXT("upperarm_l"):TEXT("upperarm_r"));
                    const auto Lower=Index(Side==0?TEXT("lowerarm_l"):TEXT("lowerarm_r"));
                    const auto Hand=Index(Side==0?TEXT("hand_l"):TEXT("hand_r"));
                    if(Upper.GetInt()<0||Lower.GetInt()<0||Hand.GetInt()<0)continue;
                    const FVector Shoulder=CS[Upper.GetInt()].GetLocation();
                    const FVector Target=MeshWorld.InverseTransformPosition(Wrists[Side].GetLocation());
                    const float Reach=FVector::Distance(Shoulder,CS[Lower.GetInt()].GetLocation())+FVector::Distance(CS[Lower.GetInt()].GetLocation(),CS[Hand.GetInt()].GetLocation())-.5f;
                    const FVector Delta=Target-Shoulder;
                    const float Excess=FMath::Max(0.f,static_cast<float>(Delta.Size())-Reach);
                    ShoulderMean+=Shoulder;GoalMean+=Shoulder+Delta.GetSafeNormal()*Excess;++Count;
                }
                if(!Count)continue;
                const FVector Pivot=CS[Spine.GetInt()].GetLocation();
                const FQuat Adjust=FQuat::FindBetweenNormals((ShoulderMean/Count-Pivot).GetSafeNormal(),(GoalMean/Count-Pivot).GetSafeNormal());
                SetRotation(Spine,Adjust*CS[Spine.GetInt()].GetRotation());
            }
        }
        for(int32 Side=0;Side<2;++Side)
        {
            if(!ApplyHand[Side])continue;
            const auto Upper=Index(Side==0?TEXT("upperarm_l"):TEXT("upperarm_r"));
            const auto Lower=Index(Side==0?TEXT("lowerarm_l"):TEXT("lowerarm_r"));
            const auto Hand=Index(Side==0?TEXT("hand_l"):TEXT("hand_r"));
            if(Upper.GetInt()<0||Lower.GetInt()<0||Hand.GetInt()<0)continue;
            if(StableWeaponAim)
            {
                // Raising the rifle can also need the shoulder girdle. Rotate
                // the clavicle only enough to restore reach, without stretching.
                const auto Clavicle=Index(Side==0?TEXT("clavicle_l"):TEXT("clavicle_r"));
                if(Clavicle.GetInt()>=0)
                {
                    const FVector Pivot=CS[Clavicle.GetInt()].GetLocation();
                    const FVector Shoulder=CS[Upper.GetInt()].GetLocation();
                    const FVector Target=MeshWorld.InverseTransformPosition(Wrists[Side].GetLocation());
                    const float Reach=FVector::Distance(Shoulder,CS[Lower.GetInt()].GetLocation())+FVector::Distance(CS[Lower.GetInt()].GetLocation(),CS[Hand.GetInt()].GetLocation())-.5f;
                    const float Radius=FVector::Distance(Pivot,Shoulder),Distance=FVector::Distance(Pivot,Target);
                    if(FVector::Distance(Shoulder,Target)>Reach&&Radius>1.f&&Distance>1.f)
                    {
                        const FVector Direction=(Target-Pivot).GetSafeNormal();
                        FVector Pole=Shoulder-Pivot;Pole-=Direction*FVector::DotProduct(Pole,Direction);
                        if(Pole.Normalize())
                        {
                            const float Cos=FMath::Clamp((Distance*Distance+Radius*Radius-Reach*Reach)/(2.f*Distance*Radius),-1.f,1.f);
                            const FVector Desired=Direction*Cos+Pole*FMath::Sqrt(FMath::Max(0.f,1.f-Cos*Cos));
                            SetRotation(Clavicle,FQuat::FindBetweenNormals((Shoulder-Pivot).GetSafeNormal(),Desired)*CS[Clavicle.GetInt()].GetRotation());
                        }
                    }
                }
            }
            const FVector A=CS[Upper.GetInt()].GetLocation(),B=CS[Lower.GetInt()].GetLocation(),C=CS[Hand.GetInt()].GetLocation();
            const float L1=FVector::Distance(A,B),L2=FVector::Distance(B,C);
            if(L1<1||L2<1)continue;
            const FVector Target=MeshWorld.InverseTransformPosition(Wrists[Side].GetLocation());
            const FVector Direction=(Target-A).GetSafeNormal();
            const float Distance=FMath::Clamp(FVector::Distance(Target,A),FMath::Abs(L1-L2)+.1f,L1+L2-.1f);
            const float Along=(L1*L1-L2*L2+Distance*Distance)/(2*Distance);
            // Keep the shoulder attached to the real torso and preserve anatomical lengths.
            FVector Pole=B-A;Pole-=Direction*FVector::DotProduct(Pole,Direction);
            if(!Pole.Normalize())Pole=FVector::CrossProduct(Direction,FVector::UpVector).GetSafeNormal();
            if(!AuthoredComponentPose.IsEmpty() && LowAimAlpha>0.f)
            {
                // A pistol's close two-handed grip needs the elbows in front of the
                // ribs. Pulling them behind the torso sends the forearms through it.
                // Keep the downward/outward bend and the authored wrist rotations.
                FVector LowPole=BodyRight*(Side==0?-.7f:.7f)+BodyForward*(Pistol?1.f:-.3f)+FVector(0,0,-1);
                LowPole-=Direction*FVector::DotProduct(LowPole,Direction);
                if(LowPole.Normalize())Pole=FMath::Lerp(Pole,LowPole,LowAimAlpha).GetSafeNormal();
            }
            const FVector Elbow=A+Direction*Along+Pole*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
            const FVector End=A+Direction*Distance;
            SetRotation(Upper,FQuat::FindBetweenNormals((B-A).GetSafeNormal(),(Elbow-A).GetSafeNormal())*CS[Upper.GetInt()].GetRotation());
            const FVector CurrentLower=CS[Lower.GetInt()].GetLocation(),CurrentHand=CS[Hand.GetInt()].GetLocation();
            SetRotation(Lower,FQuat::FindBetweenNormals((CurrentHand-CurrentLower).GetSafeNormal(),(End-CurrentLower).GetSafeNormal())*CS[Lower.GetInt()].GetRotation());
            SetRotation(Hand,MeshWorld.GetRotation().Inverse()*Wrists[Side].GetRotation());
        }
        return true;
    }
};
}
FAnimInstanceProxy* UUnifiedBodyAnimInstance::CreateAnimInstanceProxy(){return new UnifiedBody::FProxy(this);}
void UUnifiedBodyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
