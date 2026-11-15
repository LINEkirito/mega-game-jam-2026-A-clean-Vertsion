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
  if(!NPC->UsesMaleVoice()){
   if(Intent==TEXT("HELP!"))Lines={{TEXT("Voice_FemaleHelp_01"),TEXT("Call an ambulance!")} ,{TEXT("Voice_FemaleHelp_02"),TEXT("Help me!")} ,{TEXT("Voice_FemaleHelp_03"),TEXT("I can't get up!")} ,{TEXT("Voice_FemaleHelp_04"),TEXT("I can't move!")} ,{TEXT("Voice_FemaleHelp_05"),TEXT("It hurts!")} ,{TEXT("Voice_FemaleHelp_06"),TEXT("Please, don't leave me!")} ,{TEXT("Voice_FemaleHelp_07"),TEXT("Please... help…")} ,{TEXT("Voice_FemaleHelp_08"),TEXT("Somebody help!")} };
   else if(Intent==TEXT("AAAH!")&&NPC->IdentityRole==ENPCRole::Merchant)Lines={{TEXT("Voice_Merchant_04"),TEXT("Somebody help me!")}};
   else if(Intent==TEXT("AAAH!"))Lines={{TEXT("Voice_CivilianRun_01"),TEXT("Everybody run!")} ,{TEXT("Voice_CivilianRun_02"),TEXT("Get out of here!")} ,{TEXT("Voice_CivilianRun_03"),TEXT("Go! Go! Go!")} ,{TEXT("Voice_CivilianRun_04"),TEXT("Run!")} ,{TEXT("Voice_CivilianRun_05"),TEXT("They're coming!")} ,{TEXT("Voice_Civilian_04"),TEXT("Oh my God!")} ,{TEXT("Voice_Civilian_05"),TEXT("Somebody call the cops!")} ,{TEXT("Voice_Civilian_06"),TEXT("This is crazy!")} ,{TEXT("Voice_Civilian_07"),TEXT("What the hell is going on?")} ,{TEXT("Voice_Civilian_08"),TEXT("What's happening?")} };
   else if(NPC->IdentityRole==ENPCRole::Merchant){
    if(Intent==TEXT("PLEASE, NO CAMERA."))Lines={{TEXT("Voice_Merchant_01"),TEXT("Are you serious?")} ,{TEXT("Voice_Merchant_02"),TEXT("Excuse me?")} ,{TEXT("Voice_Merchant_06"),TEXT("Why are you filming me?")} };
    else if(Intent==TEXT("LEAVE ME ALONE!"))Lines={{TEXT("Voice_Merchant_03"),TEXT("Leave me alone!")} ,{TEXT("Voice_Merchant_04"),TEXT("Somebody help me!")} ,{TEXT("Voice_Merchant_05"),TEXT("What's wrong with you?")} };
   }else{
    if(Intent==TEXT("PLEASE, NO CAMERA."))Lines={{TEXT("Voice_Civilian_01"),TEXT("Are you filming this?")} ,{TEXT("Voice_Civilian_03"),TEXT("I didn't see anything!")} };
    else if(Intent==TEXT("LEAVE ME ALONE!"))Lines={{TEXT("Voice_Civilian_02"),TEXT("Get away from me!")} };
   }
  }
  if(Lines.IsEmpty())return nullptr;
  const auto& Line=Lines[FMath::RandRange(0,Lines.Num()-1)];SpokenText=Line.Text;return GameAudio::Sound(Line.Asset);
 }
}
