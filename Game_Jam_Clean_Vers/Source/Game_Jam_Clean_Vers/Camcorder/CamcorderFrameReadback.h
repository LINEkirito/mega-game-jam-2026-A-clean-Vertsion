#pragma once
#include "CoreMinimal.h"

class FTextureRenderTargetResource;
class FCamcorderVideoWriter;

// Three staging textures bound GPU/CPU memory. Only finalization waits for the GPU.
class FCamcorderFrameReadback
{
public:
    FCamcorderFrameReadback();
    ~FCamcorderFrameReadback();
    bool Schedule(FTextureRenderTargetResource* Resource, int32 FrameIndex);
    void Pump(FCamcorderVideoWriter& Writer, bool bFinish = false);
    bool HasFailed() const;
private:
    struct FState;
    TUniquePtr<FState> State;
};
