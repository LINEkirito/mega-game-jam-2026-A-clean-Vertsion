#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CamcorderVideoWriter.h"
#include "CamcorderPlayback.h"
#include "CamcorderFrameReadback.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCamcorderVideoQualityTest,"Game.Recording.VideoQualityRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCamcorderVideoQualityTest::RunTest(const FString&)
{
    // Reuse a reader across legacy and HD clips to exercise mixed-resolution playback.
    FCamcorderPlayback Reader;
    for(const FIntPoint Size:{FIntPoint(640,360),FIntPoint(1280,720)})
    {
        const FString Path=FPaths::ProjectSavedDir()/TEXT("Automation")/(FGuid::NewGuid().ToString()+TEXT(".avi"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
        Target->RenderTargetFormat=RTF_RGBA8_SRGB;
        Target->TargetGamma=2.2f;
        Target->ClearColor=FLinearColor(.18f,.18f,.18f,1);
        Target->InitAutoFormat(Size.X,Size.Y);
        Target->UpdateResourceImmediate(true);
        TestTrue(TEXT("Capture target uses display gamma"),FMath::IsNearlyEqual(Target->GetDisplayGamma(),2.2f));
        TArray<FColor> Reference;
        FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);
        TestTrue(TEXT("Read reference GPU pixels"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Reference,Flags));
        FCamcorderVideoWriter Writer;
        if(!TestTrue(TEXT("Open video writer"),Writer.Open(Path,Size.X,Size.Y,15)))return false;
        {
            FCamcorderFrameReadback Readback;
            TestTrue(TEXT("Schedule first GPU frame"),Readback.Schedule(Target->GameThread_GetRenderTargetResource(),0));
            TestTrue(TEXT("Schedule later timestamp"),Readback.Schedule(Target->GameThread_GetRenderTargetResource(),3));
            Readback.Pump(Writer,true);
            TestFalse(TEXT("GPU readback succeeded"),Readback.HasFailed());
        }
        Writer.Finish(5);
        TestFalse(TEXT("AVI finalized"),Writer.HasFailed());
        TestEqual(TEXT("Missed frames and final duration preserved"),Writer.GetFrameCount(),5);
        if(TestTrue(TEXT("Reopen encoded AVI"),Reader.Open(Path)))
        {
            TestEqual(TEXT("Width preserved"),Reader.GetWidth(),Size.X);
            TestEqual(TEXT("Height preserved"),Reader.GetHeight(),Size.Y);
            TArray<uint8> Pixels;
            if(TestTrue(TEXT("Decode final padded frame"),Reader.ReadFrame(4,Pixels)))
            {
                TestEqual(TEXT("Frame allocation matches dimensions"),Pixels.Num(),Size.X*Size.Y*4);
                if(!Reference.IsEmpty())TestTrue(TEXT("GPU readback/JPEG preserve display brightness"),FMath::Abs(int32(Pixels[0])-int32(Reference[0].B))<=3);
            }
        }
        Reader.Close();IFileManager::Get().Delete(*Path);
    }
    return true;
}
#endif
