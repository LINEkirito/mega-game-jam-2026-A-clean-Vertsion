#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"
#include <atomic>

class IImageWrapper;
class FArchive;
class FRunnableThread;
class FEvent;

// A bounded background JPEG encoder and AVI muxer. No external process or codec installation.
class FCamcorderVideoWriter final : public FRunnable
{
public:
    FCamcorderVideoWriter();
    virtual ~FCamcorderVideoWriter() override;
    bool Open(const FString& Filename, int32 InWidth, int32 InHeight, int32 InFPS);
    bool Enqueue(TArray<FColor>&& Pixels, int32 FrameIndex);
    void Finish(int32 DesiredFrameCount);
    bool HasFailed() const { return Failed.load(); }
    int32 GetFrameCount() const { return FrameCount; } // Read after Finish.
    virtual uint32 Run() override;
private:
    struct FFrame { TArray<FColor> Pixels; int32 Index = 0; };
    struct FIndex { uint32 Offset; uint32 Size; };
    TQueue<FFrame, EQueueMode::Spsc> Queue;
    std::atomic<int32> Pending{0};
    std::atomic<bool> Closing{false}, Failed{false};
    std::atomic<int32> FinalFrameCount{0};
    TUniquePtr<FArchive> File;
    TSharedPtr<IImageWrapper> Encoder;
    FRunnableThread* Thread = nullptr;
    FEvent* Wake = nullptr;
    int32 Width = 0, Height = 0, FPS = 15, FrameCount = 0;
    int64 MainFramesPos = 0, StreamFramesPos = 0, MoviSizePos = 0, MoviTypePos = 0;
    TArray<FIndex> Index;
    TArray64<uint8> LastJPEG;
    void FourCC(const ANSICHAR* Value);
    void U32(uint32 Value);
    void U16(uint16 Value);
    void Patch32(int64 Position, uint32 Value);
    int64 BeginList(const ANSICHAR* Type);
    void EndList(int64 SizePosition);
    void Header();
    void WriteJPEG(const TArray64<uint8>& Bytes);
    void Finalize();
};
