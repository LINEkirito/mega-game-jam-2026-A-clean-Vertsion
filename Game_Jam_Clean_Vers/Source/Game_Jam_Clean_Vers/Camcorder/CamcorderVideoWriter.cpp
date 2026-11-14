#include "CamcorderVideoWriter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "Serialization/Archive.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

FCamcorderVideoWriter::FCamcorderVideoWriter() = default;
FCamcorderVideoWriter::~FCamcorderVideoWriter() { Finish(0); }
void FCamcorderVideoWriter::FourCC(const ANSICHAR* V) { File->Serialize(const_cast<ANSICHAR*>(V), 4); }
void FCamcorderVideoWriter::U32(uint32 V) { uint8 B[4] = {uint8(V),uint8(V>>8),uint8(V>>16),uint8(V>>24)}; File->Serialize(B,4); }
void FCamcorderVideoWriter::U16(uint16 V) { uint8 B[2] = {uint8(V),uint8(V>>8)}; File->Serialize(B,2); }
void FCamcorderVideoWriter::Patch32(int64 P, uint32 V) { const int64 End=File->Tell(); File->Seek(P); U32(V); File->Seek(End); }
int64 FCamcorderVideoWriter::BeginList(const ANSICHAR* Type) { FourCC("LIST"); const int64 P=File->Tell(); U32(0); FourCC(Type); return P; }
void FCamcorderVideoWriter::EndList(int64 P) { Patch32(P,uint32(File->Tell()-P-4)); }

bool FCamcorderVideoWriter::Open(const FString& Filename, int32 W, int32 H, int32 Rate)
{
    if (File || Thread || W<=0 || H<=0 || Rate<=0) return false;
    Width=W; Height=H; FPS=Rate;
    Encoder=FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper").CreateImageWrapper(EImageFormat::JPEG);
    File.Reset(IFileManager::Get().CreateFileWriter(*Filename));
    if (!File || !Encoder) { Failed=true; File.Reset(); return false; }
    Header();
    Wake=FPlatformProcess::GetSynchEventFromPool(false);
    Thread=FRunnableThread::Create(this,TEXT("CamcorderJPEGWriter"),0,TPri_BelowNormal);
    if (!Thread) { Failed=true; Finalize(); return false; }
    return true;
}
void FCamcorderVideoWriter::Header()
{
    FourCC("RIFF"); U32(0); FourCC("AVI ");
    const int64 Hdrl=BeginList("hdrl");
    FourCC("avih"); U32(56); U32(1000000/FPS); U32(Width*Height*3*FPS); U32(0); U32(0x10);
    MainFramesPos=File->Tell(); U32(0); U32(0); U32(1); U32(Width*Height*3); U32(Width); U32(Height);
    for(int I=0;I<4;++I) U32(0);
    const int64 Strl=BeginList("strl");
    FourCC("strh"); U32(56); FourCC("vids"); FourCC("MJPG"); U32(0); U16(0); U16(0);
    U32(0); U32(1); U32(FPS); U32(0); StreamFramesPos=File->Tell(); U32(0);
    U32(Width*Height*3); U32(0xffffffff); U32(0); U16(0); U16(0); U16(Width); U16(Height);
    FourCC("strf"); U32(40); U32(40); U32(Width); U32(Height); U16(1); U16(24); FourCC("MJPG");
    U32(Width*Height*3); U32(0); U32(0); U32(0); U32(0);
    EndList(Strl); EndList(Hdrl);
    MoviSizePos=BeginList("movi"); MoviTypePos=MoviSizePos+4;
}
bool FCamcorderVideoWriter::Enqueue(TArray<FColor>&& Pixels, int32 FrameIndex)
{
    if (Closing || Failed || !Thread) return false;
    if (Pixels.Num()!=Width*Height) { Failed=true; return false; }
    if (Pending.load()>=4) return false;
    FFrame F; F.Pixels=MoveTemp(Pixels); F.Index=FrameIndex;
    ++Pending; Queue.Enqueue(MoveTemp(F)); Wake->Trigger(); return true;
}
void FCamcorderVideoWriter::WriteJPEG(const TArray64<uint8>& Bytes)
{
    // Classic AVI uses 32-bit offsets. Stop safely before its size limit.
    if (File->Tell()+Bytes.Num()+16 >= 1800000000LL) { Failed=true; return; }
    FIndex Entry{uint32(File->Tell()-MoviTypePos),uint32(Bytes.Num())};
    FourCC("00dc"); U32(Entry.Size); File->Serialize(const_cast<uint8*>(Bytes.GetData()),Bytes.Num());
    if(Bytes.Num()&1) { uint8 Pad=0; File->Serialize(&Pad,1); }
    Index.Add(Entry); ++FrameCount;
    if(File->IsError()) Failed=true;
}
uint32 FCamcorderVideoWriter::Run()
{
    while(!Closing || Pending.load()>0)
    {
        FFrame F;
        if (!Queue.Dequeue(F)) { Wake->Wait(50); continue; }
        if(!Failed)
        {
            if(!Encoder->SetRaw(F.Pixels.GetData(),F.Pixels.Num()*sizeof(FColor),Width,Height,ERGBFormat::BGRA,8)) Failed=true;
            else
            {
                auto JPEG=Encoder->GetCompressed(90);
                if(JPEG.IsEmpty()) Failed=true;
                else
                {
                    // Fill missed samples using the preceding frame, preserving real clip duration.
                    while(FrameCount<F.Index && !Failed) WriteJPEG(LastJPEG.IsEmpty()?JPEG:LastJPEG);
                    if(!Failed) WriteJPEG(JPEG);
                    LastJPEG=MoveTemp(JPEG);
                }
            }
        }
        --Pending;
    }
    while(!Failed && !LastJPEG.IsEmpty() && FrameCount<FinalFrameCount.load()) WriteJPEG(LastJPEG);
    Finalize(); return 0;
}
void FCamcorderVideoWriter::Finalize()
{
    if(!File) return;
    EndList(MoviSizePos);
    FourCC("idx1"); U32(Index.Num()*16);
    for(const FIndex& E:Index) { FourCC("00dc"); U32(0x10); U32(E.Offset); U32(E.Size); }
    Patch32(MainFramesPos,FrameCount); Patch32(StreamFramesPos,FrameCount); Patch32(4,uint32(File->Tell()-8));
    File->Flush(); if(File->IsError()) Failed=true;
    if(!File->Close()) Failed=true;
    File.Reset();
}
void FCamcorderVideoWriter::Finish(int32 DesiredFrameCount)
{
    if(Thread)
    {
        FinalFrameCount=DesiredFrameCount; Closing=true; Wake->Trigger(); Thread->WaitForCompletion();
        delete Thread; Thread=nullptr;
    }
    if(Wake) { FPlatformProcess::ReturnSynchEventToPool(Wake); Wake=nullptr; }
}
