#include "CamcorderFrameReadback.h"
#include "CamcorderVideoWriter.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "HAL/PlatformProcess.h"
#include <atomic>

struct FCamcorderFrameReadback::FState
{
    struct FSlot
    {
        TUniquePtr<FRHIGPUTextureReadback> Readback;
        std::atomic<int32> Status{0}; // free, copy queued, GPU pending, CPU-ready
        int32 Index=0, Width=0, Height=0;
        TArray<FColor> Pixels;
    };
    FSlot Slots[3];
    std::atomic<bool> Failed{false};
    std::atomic<bool> PollQueued{false};

    void Poll(FRHICommandListImmediate& Cmd, bool bFinish)
    {
        bool bPending=false;
        for(auto& S:Slots) bPending|=S.Status.load()==2;
        if(bFinish && bPending) Cmd.BlockUntilGPUIdle();
        for(auto& S:Slots)
        {
            if(S.Status.load()!=2 || !S.Readback || (!bFinish && !S.Readback->IsReady())) continue;
            int32 Pitch=0, Height=0;
            const auto* Data=static_cast<const FColor*>(S.Readback->Lock(Pitch,&Height));
            if(!Data || Pitch<S.Width || Height<S.Height) Failed=true;
            else
            {
                S.Pixels.SetNumUninitialized(S.Width*S.Height);
                for(int32 Y=0;Y<S.Height;++Y)
                    FMemory::Memcpy(S.Pixels.GetData()+Y*S.Width,Data+Y*Pitch,S.Width*sizeof(FColor));
                // FinalColorLDR's alpha is not video opacity.
                for(auto& Pixel:S.Pixels) Pixel.A=255;
            }
            if(Data) S.Readback->Unlock();
            S.Status=3;
        }
    }
};

FCamcorderFrameReadback::FCamcorderFrameReadback():State(MakeUnique<FState>()){}
FCamcorderFrameReadback::~FCamcorderFrameReadback()
{
    auto* S=State.Get();
    ENQUEUE_RENDER_COMMAND(ReleaseCamcorderReadback)([S](FRHICommandListImmediate& Cmd)
    {
        S->Poll(Cmd,true);
        for(auto& Slot:S->Slots) Slot.Readback.Reset();
    });
    FlushRenderingCommands();
}
bool FCamcorderFrameReadback::HasFailed() const {return State->Failed.load();}
bool FCamcorderFrameReadback::Schedule(FTextureRenderTargetResource* Resource,int32 Index)
{
    if(!Resource || HasFailed()) return false;
    for(auto& Slot:State->Slots)
    {
        if(Slot.Status.load()!=0) continue;
        Slot.Index=Index;
        Slot.Status=1;
        auto* S=&Slot;
        ENQUEUE_RENDER_COMMAND(CopyCamcorderFrame)([S,Resource](FRHICommandListImmediate& Cmd)
        {
            // ResizeTarget is queued too: inspect dimensions after it has run.
            S->Width=Resource->GetSizeX(); S->Height=Resource->GetSizeY();
            if(!S->Readback) S->Readback=MakeUnique<FRHIGPUTextureReadback>(TEXT("CamcorderFrame"));
            S->Readback->EnqueueCopy(Cmd,Resource->GetRenderTargetTexture());
            S->Status=2;
        });
        return true;
    }
    return false;
}
void FCamcorderFrameReadback::Pump(FCamcorderVideoWriter& Writer,bool bFinish)
{
    auto* S=State.Get();
    if(bFinish || !S->PollQueued.exchange(true))
    {
        ENQUEUE_RENDER_COMMAND(PollCamcorderFrames)([S,bFinish](FRHICommandListImmediate& Cmd)
        {
            S->Poll(Cmd,bFinish); S->PollQueued=false;
        });
    }
    if(bFinish) FlushRenderingCommands();
    // Preserve timestamp order even when a lower-numbered slot has been reused.
    for(;;)
    {
        FState::FSlot* Oldest=nullptr;
        for(auto& Slot:S->Slots)
            if(Slot.Status.load()!=0 && (!Oldest || Slot.Index<Oldest->Index)) Oldest=&Slot;
        if(!Oldest || Oldest->Status.load()!=3 || HasFailed() || Writer.HasFailed()) break;
        if(!Writer.Enqueue(MoveTemp(Oldest->Pixels),Oldest->Index))
        {
            if(!bFinish) break;
            FPlatformProcess::SleepNoStats(.001f);
            continue;
        }
        Oldest->Status=0;
    }
}
