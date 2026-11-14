#pragma once
#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "IImageWrapper.h"
// Reads this project's MJPEG AVI output using bounded per-frame allocations.
class FCamcorderPlayback {
public:
 bool Open(const FString& Path);
 void Close();
 bool ReadFrame(int32 Index,TArray<uint8>& Pixels);
 int32 NumFrames() const {return Frames.Num();}
 int32 GetWidth() const {return Width;}
 int32 GetHeight() const {return Height;}
private:
 int32 Width=0,Height=0;
 struct FFrame {int64 Offset;uint32 Size;};
 TArray<FFrame> Frames;
 TUniquePtr<FArchive> File;
 TSharedPtr<IImageWrapper> Decoder;
};
