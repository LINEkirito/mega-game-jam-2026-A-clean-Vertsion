#include "CamcorderPlayback.h"
#include "HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
void FCamcorderPlayback::Close(){File.Reset();Frames.Reset();Decoder.Reset();Width=Height=0;}
bool FCamcorderPlayback::Open(const FString& Path) {
 Close(); File.Reset(IFileManager::Get().CreateFileReader(*Path));
 if(!File || File->TotalSize()<12) {Close();return false;}
 char Header[12]; File->Serialize(Header,12);
 if(FMemory::Memcmp(Header,"RIFF",4)||FMemory::Memcmp(Header+8,"AVI ",4)){Close();return false;}
 int64 MoviEnd=0;
 while(File->Tell()+8<=File->TotalSize() && !File->IsError()) {
  char ID[4];uint32 Size=0;File->Serialize(ID,4);*File<<Size;
  const int64 Start=File->Tell(),End=Start+Size+(Size&1);
  if(End>File->TotalSize())break;
  if(!FMemory::Memcmp(ID,"LIST",4)&&Size>=4) {
   char Type[4];File->Serialize(Type,4);
   if(!FMemory::Memcmp(Type,"movi",4)){MoviEnd=Start+Size;continue;}
  }
  if(MoviEnd && Start<MoviEnd && !FMemory::Memcmp(ID,"00dc",4) && Size>0 && Size<8*1024*1024) Frames.Add({Start,Size});
  File->Seek(End);
 }
 if(Frames.IsEmpty() || File->IsError()){Close();return false;}
 Decoder=FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper").CreateImageWrapper(EImageFormat::JPEG);
 if(!Decoder){Close();return false;}
 const auto& First=Frames[0];TArray<uint8> Bytes;Bytes.SetNumUninitialized(First.Size);
 File->Seek(First.Offset);File->Serialize(Bytes.GetData(),First.Size);
 if(File->IsError() || !Decoder->SetCompressed(Bytes.GetData(),Bytes.Num())){Close();return false;}
 Width=Decoder->GetWidth();Height=Decoder->GetHeight();
 // Accept old 360p clips and new HD clips, while bounding decode allocations.
 if(Width<=0 || Height<=0 || Width>1920 || Height>1080){Close();return false;}
 return true;
}
bool FCamcorderPlayback::ReadFrame(int32 Index,TArray<uint8>& Pixels) {
 if(!File || !Decoder || !Frames.IsValidIndex(Index))return false;
 const auto& F=Frames[Index];TArray<uint8> Bytes;Bytes.SetNumUninitialized(F.Size);
 File->Seek(F.Offset);File->Serialize(Bytes.GetData(),F.Size);
 return !File->IsError() && Decoder->SetCompressed(Bytes.GetData(),Bytes.Num()) && Decoder->GetWidth()==Width && Decoder->GetHeight()==Height && Decoder->GetRaw(ERGBFormat::BGRA,8,Pixels);
}
