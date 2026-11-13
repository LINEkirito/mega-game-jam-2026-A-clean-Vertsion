#include "EquipmentAssetTools.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "SkeletalMeshAttributes.h"
#include "MeshDescription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Editor.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#endif

USkeletalMesh* UEquipmentAssetTools::MakeArmsMesh(USkeletalMesh* Source,const FString& Destination,bool ForearmsOnly)
{
#if WITH_EDITOR
    if(!Source||!Destination.StartsWith(TEXT("/Game/"))||FindObject<USkeletalMesh>(nullptr,*(Destination+TEXT(".")+FPackageName::GetLongPackageAssetName(Destination))))return nullptr;
    auto* Result=DuplicateObject<USkeletalMesh>(Source,CreatePackage(*Destination),*FPackageName::GetLongPackageAssetName(Destination));
    if(!Result)return nullptr;
    Result->SetFlags(RF_Public|RF_Standalone);
    Result->PreEditChange(nullptr);
    const auto& Skeleton=Result->GetRefSkeleton();
    TSet<int32> ArmBones;
    for(int32 I=0;I<Skeleton.GetNum();++I)
    {
        for(int32 B=I;B!=INDEX_NONE;B=Skeleton.GetParentIndex(B))
        {
            const FName N=Skeleton.GetBoneName(B);
            if(N.ToString().StartsWith(ForearmsOnly?TEXT("lowerarm"):TEXT("upperarm"))||N==TEXT("hand_l")||N==TEXT("hand_r")){ArmBones.Add(I);break;}
        }
    }
    for(int32 LOD=0;LOD<Result->GetLODNum();++LOD)
    {
        auto* M=Result->GetMeshDescription(LOD);if(!M)continue;
        const auto Weights=FSkeletalMeshAttributes(*M).GetVertexSkinWeights();
        TArray<FPolygonID> Remove;
        for(const auto Polygon:M->Polygons().GetElementIDs())
        {
            bool Keep=true;
            for(const auto V:M->GetPolygonVertices(Polygon))
            {
                float ArmWeight=0;for(const auto W:Weights.Get(V))if(ArmBones.Contains(W.GetBoneIndex()))ArmWeight+=W.GetWeight();
                if(ArmWeight<.5f){Keep=false;break;}
            }
            if(!Keep)Remove.Add(Polygon);
        }
        M->DeletePolygons(Remove);FElementIDRemappings Remap;M->Compact(Remap);
        Result->CommitMeshDescription(LOD);
    }
    Result->PostEditChange();Result->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Result);
    return Result;
#else
    return nullptr;
#endif
}

UStaticMesh* UEquipmentAssetTools::ImportReferenceGeometry(const FString& JsonFile,const FString& Destination)
{
#if WITH_EDITOR
    if(!Destination.StartsWith(TEXT("/Game/Combat/Reference/")))return nullptr;
    FString Text;TSharedPtr<FJsonObject> Data;
    if(!FFileHelper::LoadFileToString(Text,*JsonFile)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data)||!Data)return nullptr;
    const FString OpticPath=TEXT("/Game/Combat/Reference/Materials/M_OpticView");
    if(!LoadObject<UMaterial>(nullptr,*(OpticPath+TEXT(".M_OpticView"))))
    {
        auto* M=NewObject<UMaterial>(CreatePackage(*OpticPath),TEXT("M_OpticView"),RF_Public|RF_Standalone);M->BlendMode=BLEND_Masked;M->SetShadingModel(MSM_Unlit);M->TwoSided=true;
        auto* Sample=NewObject<UMaterialExpressionTextureSampleParameter2D>(M);Sample->ParameterName=TEXT("LensView");Sample->Texture=LoadObject<UTexture>(nullptr,TEXT("/Engine/EngineResources/WhiteSquareTexture"));M->GetExpressionCollection().AddExpression(Sample);
        auto* UV=NewObject<UMaterialExpressionTextureCoordinate>(M);M->GetExpressionCollection().AddExpression(UV);
        auto* Center=NewObject<UMaterialExpressionConstant2Vector>(M);Center->R=.5f;Center->G=.5f;M->GetExpressionCollection().AddExpression(Center);
        auto* Mask=NewObject<UMaterialExpressionSphereMask>(M);Mask->A.Connect(0,UV);Mask->B.Connect(0,Center);Mask->AttenuationRadius=.5f;Mask->HardnessPercent=100;M->GetExpressionCollection().AddExpression(Mask);M->GetEditorOnlyData()->OpacityMask.Connect(0,Mask);
        auto* Dark=NewObject<UMaterialExpressionConstant3Vector>(M);Dark->Constant=FLinearColor(.003f,.009f,.015f);M->GetExpressionCollection().AddExpression(Dark);
        auto* Active=NewObject<UMaterialExpressionScalarParameter>(M);Active->ParameterName=TEXT("ViewActive");Active->DefaultValue=0;M->GetExpressionCollection().AddExpression(Active);
        auto* Mix=NewObject<UMaterialExpressionLinearInterpolate>(M);Mix->A.Connect(0,Dark);Mix->B.Connect(0,Sample);Mix->Alpha.Connect(0,Active);M->GetExpressionCollection().AddExpression(Mix);M->GetEditorOnlyData()->EmissiveColor.Connect(0,Mix);
        M->PostEditChange();M->MarkPackageDirty();FAssetRegistryModule::AssetCreated(M);
    }
    const auto& Vertices=Data->GetArrayField(TEXT("vertices"));const auto& Normals=Data->GetArrayField(TEXT("normals"));
    const auto& UVs=Data->GetArrayField(TEXT("uv"));const auto& Faces=Data->GetArrayField(TEXT("triangles"));const auto& MatIDs=Data->GetArrayField(TEXT("materials"));
    if(Vertices.Num()!=Normals.Num()||Vertices.Num()!=UVs.Num()||Faces.Num()!=MatIDs.Num())return nullptr;
    const FString Name=FPackageName::GetLongPackageAssetName(Destination);
    auto* Result=LoadObject<UStaticMesh>(nullptr,*(Destination+TEXT(".")+Name));const bool bNew=!Result;
    if(!Result)Result=NewObject<UStaticMesh>(CreatePackage(*Destination),*Name,RF_Public|RF_Standalone);
    Result->PreEditChange(nullptr);Result->GetStaticMaterials().Reset();
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    TArray<FPolygonGroupID> Groups;
    for(const auto& P:Data->GetArrayField(TEXT("palette")))
    {
        const auto& A=P->AsArray();const FString MatName=TEXT("M_")+A[0]->AsString();const FString Path=TEXT("/Game/Combat/Reference/Materials/")+MatName;
        auto* Material=LoadObject<UMaterial>(nullptr,*(Path+TEXT(".")+MatName));
        if(!Material){Material=NewObject<UMaterial>(CreatePackage(*Path),*MatName,RF_Public|RF_Standalone);
            auto* Color=NewObject<UMaterialExpressionConstant3Vector>(Material);const auto& RGB=A[1]->AsArray();Color->Constant=FLinearColor(RGB[0]->AsNumber(),RGB[1]->AsNumber(),RGB[2]->AsNumber());
            Material->GetExpressionCollection().AddExpression(Color);Material->GetEditorOnlyData()->BaseColor.Connect(0,Color);
            auto* Metal=NewObject<UMaterialExpressionConstant>(Material);Metal->R=A[2]->AsNumber();Material->GetExpressionCollection().AddExpression(Metal);Material->GetEditorOnlyData()->Metallic.Connect(0,Metal);
            auto* Rough=NewObject<UMaterialExpressionConstant>(Material);Rough->R=A[3]->AsNumber();Material->GetExpressionCollection().AddExpression(Rough);Material->GetEditorOnlyData()->Roughness.Connect(0,Rough);
            Material->PostEditChange();Material->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Material);
        }
        Result->GetStaticMaterials().Add(FStaticMaterial(Material,*MatName,*MatName));const auto Group=Description.CreatePolygonGroup();Attributes.GetPolygonGroupMaterialSlotNames()[Group]=*MatName;Groups.Add(Group);
    }
    TArray<FVertexID> V;
    for(const auto& Value:Vertices){const auto& A=Value->AsArray();auto ID=Description.CreateVertex();Attributes.GetVertexPositions()[ID]=FVector3f(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());V.Add(ID);}
    for(int32 I=0;I<Faces.Num();++I){TArray<FVertexInstanceID,TInlineAllocator<3>> Triangle;
        for(const auto& Index:Faces[I]->AsArray()){const int32 K=Index->AsNumber();if(!V.IsValidIndex(K))return nullptr;const auto VI=Description.CreateVertexInstance(V[K]);const auto& N=Normals[K]->AsArray();const auto& UV=UVs[K]->AsArray();
            const FVector3f Normal(N[0]->AsNumber(),N[1]->AsNumber(),N[2]->AsNumber());Attributes.GetVertexInstanceNormals()[VI]=Normal;
            FVector3f Tangent=FVector3f::CrossProduct(Normal,FMath::Abs(Normal.Z)<.9f?FVector3f(0,0,1):FVector3f(0,1,0)).GetSafeNormal();Attributes.GetVertexInstanceTangents()[VI]=Tangent;Attributes.GetVertexInstanceBinormalSigns()[VI]=1;Attributes.GetVertexInstanceUVs().Set(VI,0,FVector2f(UV[0]->AsNumber(),UV[1]->AsNumber()));Triangle.Add(VI);
        }const int32 M=MatIDs[I]->AsNumber();if(!Groups.IsValidIndex(M))return nullptr;Description.CreatePolygon(Groups[M],Triangle);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bFastBuild=false;Params.bBuildSimpleCollision=true;Params.bAllowCpuAccess=true;
    if(!Result->BuildFromMeshDescriptions({&Description},Params))return nullptr;
    Result->PostEditChange();Result->MarkPackageDirty();if(bNew)FAssetRegistryModule::AssetCreated(Result);return Result;
#else
    return nullptr;
#endif
}

bool UEquipmentAssetTools::PreviewInput(const FString& Key,bool Pressed)
{
#if WITH_EDITOR
    if(GEditor&&GEditor->PlayWorld)if(auto* PC=GEditor->PlayWorld->GetFirstPlayerController())
        return PC->InputKey(FInputKeyEventArgs::CreateSimulated(FKey(*Key),Pressed?IE_Pressed:IE_Released,Pressed?1.f:0.f));
#endif
    return false;
}

bool UEquipmentAssetTools::LockUpperBodyPose(UAnimSequence* Animation,UAnimSequence* Pose)
{
#if WITH_EDITOR
 if(!Animation||!Pose||Animation->GetSkeleton()!=Pose->GetSkeleton())return false;
 auto& Controller=Animation->GetController();Controller.OpenBracket(FText::FromString(TEXT("Preserve rifle upper body")),false);
 const auto& Ref=Animation->GetSkeleton()->GetReferenceSkeleton();TArray<FName> Names;Pose->GetDataModel()->GetBoneTrackNames(Names);
 for(FName Name:Names){int32 Bone=Ref.FindBoneIndex(Name);bool Upper=false;for(int32 B=Bone;B!=INDEX_NONE;B=Ref.GetParentIndex(B))if(Ref.GetBoneName(B)==TEXT("spine_01")){Upper=true;break;}if(!Upper)continue;
 TArray<FTransform> Values;Pose->GetDataModel()->GetBoneTrackTransforms(Name,Values);if(Values.IsEmpty())continue;
 const FTransform& T=Values[0];const int32 Count=Animation->GetNumberOfSampledKeys();TArray<FVector3f> P,S;TArray<FQuat4f> R;P.Init(FVector3f(T.GetLocation()),Count);R.Init(FQuat4f(T.GetRotation()),Count);S.Init(FVector3f(T.GetScale3D()),Count);Controller.SetBoneTrackKeys(Name,P,R,S,false);
 }
 Controller.CloseBracket(false);Animation->MarkPackageDirty();return true;
#else
 return false;
#endif
}
