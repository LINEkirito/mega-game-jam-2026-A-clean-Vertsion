#include "NPCVoiceLines.h"
#include "Audio/GameAudio.h"
#include "Combat/PatrolEnemy.h"
namespace NPCVoiceLines
{
 USoundBase* Select(const APatrolEnemy* NPC,const FString& Intent,FString& SpokenText,bool Filming)
 {
  struct FLine {const TCHAR* Asset;const TCHAR* Text;};
  TArray<FLine> Lines;
  if(NPC->IdentityRole==ENPCRole::Police){
   if(Intent==TEXT("PLEASE LEAVE THE AREA."))Lines={{TEXT("Voice_Police_04"),TEXT("Put the camera down!")},{TEXT("Voice_Police_06"),TEXT("Stop recording!")},{TEXT("Voice_Police_05"),TEXT("Stay behind the line!")}};
   else if(Intent==TEXT("COMBAT!"))Lines={{TEXT("Voice_Police_07"),TEXT("We need backup!")},{TEXT("Voice_Police_02"),TEXT("Hey! Back up!")}};
   else if(Intent==TEXT("MAKE WAY!"))Lines={{TEXT("Voice_Police_01"),TEXT("Get out of the way!")},{TEXT("Voice_Police_03"),TEXT("Move!")}};
  }else if(NPC->IdentityRole==ENPCRole::GangBoss){
   if(Intent==TEXT("MOVE!")||Intent==TEXT("COMBAT!"))Lines={{TEXT("Voice_Boss_01"),TEXT("Everybody, move in!")},{TEXT("Voice_Boss_03"),TEXT("You should've stayed home!")}};
   else if(Intent==TEXT("BACK OFF!")||Intent==TEXT("PAY UP!"))Lines={{TEXT("Voice_Boss_02"),TEXT("You have no idea who I am.")}};
  }else if(NPC->IdentityRole==ENPCRole::GangMember){
   if(Intent==TEXT("COMBAT!"))Lines={{TEXT("Voice_Gang_01"),TEXT("Get him!")},{TEXT("Voice_Gang_02"),TEXT("Get him!")}};
   else if(Intent==TEXT("BACK OFF!")&&Filming)Lines={{TEXT("Voice_Gang_04"),TEXT("Turn that camera off!")}};
   else if(Intent==TEXT("BACK OFF!"))Lines={{TEXT("Voice_Gang_03"),TEXT("Get lost!")},{TEXT("Voice_Gang_07"),TEXT("Yo! What you looking at?")},{TEXT("Voice_Gang_08"),TEXT("You got a problem!")}};
   else if(Intent==TEXT("PAY UP!"))Lines={{TEXT("Voice_Gang_05"),TEXT("Who sent you?")},{TEXT("Voice_Gang_06"),TEXT("Who sent you?")}};
  }
  if(Lines.IsEmpty())return nullptr;
  const auto& Line=Lines[FMath::RandRange(0,Lines.Num()-1)];SpokenText=Line.Text;return GameAudio::Sound(Line.Asset);
 }
}
