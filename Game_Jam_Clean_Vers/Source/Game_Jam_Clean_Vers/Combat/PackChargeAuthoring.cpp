#include "EquipmentAssetTools.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#endif

bool UEquipmentAssetTools::BuildPackChargeAnimations()
{
#if WITH_EDITOR
    const FString Root=TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/");
    for(const FString Type:{TEXT("TR15"),TEXT("WK-11_Viper"),TEXT("Herrington_11-87")})
    {
        const bool Rifle=Type==TEXT("TR15"),Pistol=Type==TEXT("WK-11_Viper");
        if(Rifle)continue; // Preserve the already approved rifle action assets.
        const FString Idle=Rifle?TEXT("Idle_Pose_Non_Grip"):Pistol?TEXT("Idle_Pose"):TEXT("Idle");
        for(const FString Entry:{TEXT("Character"),TEXT("Weapon"),TEXT("CharacterReload"),TEXT("WeaponReload")})
        {
            const bool Reload=Entry.EndsWith(TEXT("Reload"));
            const FString Group=Entry.StartsWith(TEXT("Character"))?TEXT("Character"):TEXT("Weapon");
            const bool Character=Group==TEXT("Character");
            auto* Source=LoadObject<UAnimSequence>(nullptr,*(Root+Type+TEXT("/")+Group+(Character?TEXT("/A_FP_"):TEXT("/A_W_"))+Type+TEXT("_")+(Character?Idle:TEXT("Idle"))));
            if(!Source||!Source->GetSkeleton())return false;
            const FString Path=TEXT("/Game/Combat/Viewmodel/PackActions/A_")+Type+TEXT("_")+Group+(Reload?TEXT("_ReloadCharge"):TEXT("_Charge"));
            auto* Anim=LoadObject<UAnimSequence>(nullptr,*Path);
            const bool New=Anim==nullptr;
            if(New)Anim=DuplicateObject<UAnimSequence>(Source,CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path));
            if(!Anim)return false;
            Anim->Modify();Anim->SetFlags(RF_Public|RF_Standalone);
            const auto& Ref=Source->GetSkeleton()->GetReferenceSkeleton();
            const int32 Count=Ref.GetNum();TArray<FTransform> Base=Ref.GetRefBonePose();TArray<FName> Names;
            Source->GetDataModel()->GetBoneTrackNames(Names);
            for(int32 B=0;B<Count;++B)if(Names.Contains(Ref.GetBoneName(B))){TArray<FTransform> Track;Source->GetDataModel()->GetBoneTrackTransforms(Ref.GetBoneName(B),Track);if(!Track.IsEmpty())Base[B]=Track[0];}
            TArray<FTransform> Initial=Base;
            if(Reload)
            {
                const FString Suffix=(Rifle||Pistol)?TEXT("Reload_Empty"):TEXT("Reload_Loop");
                auto* Lead=LoadObject<UAnimSequence>(nullptr,*(Root+Type+TEXT("/")+Group+(Character?TEXT("/A_FP_"):TEXT("/A_W_"))+Type+TEXT("_")+Suffix));
                if(Lead){TArray<FName> Tracks;Lead->GetDataModel()->GetBoneTrackNames(Tracks);for(int32 B=0;B<Count;++B)if(Tracks.Contains(Ref.GetBoneName(B))){TArray<FTransform> V;Lead->GetDataModel()->GetBoneTrackTransforms(Ref.GetBoneName(B),V);const float Time=Rifle?3.f:Pistol?2.6f:Lead->GetPlayLength();if(V.Num())Initial[B]=V[FMath::Clamp(FMath::RoundToInt(Time/Lead->GetPlayLength()*(V.Num()-1)),0,V.Num()-1)];}}
            }
            const int32 LastFrame=Reload?50:42;
            TArray<TArray<FTransform>> Keys;Keys.SetNum(Count);
            auto Smooth=[](float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3-2*X);};
            for(int32 F=0;F<=LastFrame;++F)
            {
                const float T=F/30.f-(Reload?.266667f:0.f);
                const float Reach=Smooth(T/.4f)*(1-Smooth((T-.95f)/.45f));
                const float Pull=Smooth((T-.48f)/.3f)*(1-Smooth((T-.84f)/.1f));
                const float Travel=Rifle?8.f:Pistol?4.2f:9.3f;
                TArray<FTransform> Local=Base,CS;CS.SetNum(Count);
                auto Rebuild=[&](){for(int32 B=0;B<Count;++B){int32 Parent=Ref.GetParentIndex(B);CS[B]=Parent<0?Local[B]:Local[B]*CS[Parent];}};
                auto SetRotation=[&](int32 B,FQuat Q){int32 Parent=Ref.GetParentIndex(B);Local[B].SetRotation((Parent<0?Q:CS[Parent].GetRotation().Inverse()*Q).GetNormalized());Rebuild();};
                if(Reload)for(int32 B=0;B<Count;++B)Local[B].Blend(Initial[B],Base[B],Smooth((F/30.f)/.266667f));
                Rebuild();
                if(Character)
                {
                    const TCHAR* Side=Pistol?TEXT("l"):TEXT("r");
                    int32 U=Ref.FindBoneIndex(*FString::Printf(TEXT("upperarm_%s"),Side)),L=Ref.FindBoneIndex(*FString::Printf(TEXT("lowerarm_%s"),Side)),H=Ref.FindBoneIndex(*FString::Printf(TEXT("hand_%s"),Side));
                    int32 G=Ref.FindBoneIndex(TEXT("ik_hand_gun")),Finger=Ref.FindBoneIndex(*FString::Printf(TEXT("index_02_%s"),Side));
                    if(U<0||L<0||H<0||G<0||Finger<0)return false;
                    const FTransform Gun=CS[G],Hand=CS[H];
                    const FVector FingerOffset=Hand.InverseTransformPosition(CS[Finger].GetLocation());
                    const FQuat Turn=FQuat(Gun.GetRotation().RotateVector(FVector::YAxisVector),FMath::DegreesToRadians(Rifle?90.f:Pistol?-90.f:0.f));
                    FQuat Grip=(Turn*Hand.GetRotation()).GetNormalized();
                    if(Pistol)
                    {
                        const int32 Middle=Ref.FindBoneIndex(TEXT("middle_01_l")),Index=Ref.FindBoneIndex(TEXT("index_01_l")),Pinky=Ref.FindBoneIndex(TEXT("pinky_01_l"));
                        const FQuat Palm=FRotationMatrix::MakeFromXY(CS[Middle].GetLocation()-Hand.GetLocation(),CS[Pinky].GetLocation()-CS[Index].GetLocation()).ToQuat();
                        const FQuat Overhand=FRotationMatrix::MakeFromXY(Gun.GetRotation().RotateVector(-FVector::XAxisVector),Gun.GetRotation().RotateVector(FVector::YAxisVector)).ToQuat();
                        Grip=(Overhand*Palm.Inverse()*Hand.GetRotation()).GetNormalized();
                    }
                    const FVector Contact=Rifle?FVector(1,-13.7f-Travel*Pull,2.8f):Pistol?FVector(-1.4f,-2.5f-Travel*Pull,3.f):FVector(-4.2f,-9.3f-Travel*Pull,.5f);
                    const FVector Destination=Gun.TransformPosition(Contact)-Grip.RotateVector(FingerOffset);
                    FVector Target=FMath::Lerp(Hand.GetLocation(),Destination,Reach);
                    // The free hand approaches above the receiver instead of passing through it.
                    Target+=Gun.GetRotation().RotateVector(FVector(0,0,8*FMath::Sin(Reach*PI)));
                    FVector A=CS[U].GetLocation(),B=CS[L].GetLocation(),C=CS[H].GetLocation();
                    float L1=(B-A).Size(),L2=(C-B).Size(),D=FMath::Clamp((Target-A).Size(),FMath::Abs(L1-L2)+.1f,L1+L2-.1f);
                    FVector Direction=(Target-A).GetSafeNormal(),Pole=B-A;Pole-=Direction*FVector::DotProduct(Pole,Direction);Pole.Normalize();
                    const float Along=(L1*L1-L2*L2+D*D)/(2*D);
                    const FVector Elbow=A+Direction*Along+Pole*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
                    SetRotation(U,FQuat::FindBetweenNormals((B-A).GetSafeNormal(),(Elbow-A).GetSafeNormal())*CS[U].GetRotation());
                    SetRotation(L,FQuat::FindBetweenNormals((CS[H].GetLocation()-CS[L].GetLocation()).GetSafeNormal(),(A+Direction*D-CS[L].GetLocation()).GetSafeNormal())*CS[L].GetRotation());
                    SetRotation(H,FQuat::Slerp(Hand.GetRotation(),Grip,Reach));
                }
                else
                {
                    for(const FName Bone:{FName(TEXT("Bolt")),FName(TEXT("Charger"))})
                    {
                        int32 B=Ref.FindBoneIndex(Bone);if(B<0)continue;
                        FTransform Moved=CS[B];Moved.AddToTranslation(FVector(0,-Travel*Pull,0));int32 Parent=Ref.GetParentIndex(B);Local[B]=Parent<0?Moved:Moved.GetRelativeTransform(CS[Parent]);
                    }
                }
                for(int32 B=0;B<Count;++B)Keys[B].Add(Local[B]);
            }
            auto& Controller=Anim->GetController();Controller.OpenBracket(FText::FromString(TEXT("Matched charging action")),false);Controller.SetFrameRate(FFrameRate(30,1),false);Controller.SetNumberOfFrames(FFrameNumber(LastFrame),false);
            TArray<FName> Existing;Anim->GetDataModel()->GetBoneTrackNames(Existing);
            for(int32 B=0;B<Count;++B){FName Name=Ref.GetBoneName(B);if(!Existing.Contains(Name))Controller.AddBoneCurve(Name,false);TArray<FVector3f> P,S;TArray<FQuat4f> Q;for(const auto& T:Keys[B]){P.Add(FVector3f(T.GetLocation()));S.Add(FVector3f(T.GetScale3D()));Q.Add(FQuat4f(T.GetRotation()));}Controller.SetBoneTrackKeys(Name,P,Q,S,false);}
            Controller.CloseBracket(false);Anim->Notifies.Empty();Anim->PostEditChange();Anim->MarkPackageDirty();if(New)FAssetRegistryModule::AssetCreated(Anim);
        }
    }
    return true;
#else
    return false;
#endif
}
