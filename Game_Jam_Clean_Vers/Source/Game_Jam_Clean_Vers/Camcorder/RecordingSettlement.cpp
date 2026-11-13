#include "RecordingLibrary.h"
#include "RecordingReview.h"
#include "Framework/Application/SlateApplication.h"
#include "CamcorderEquipmentComponent.h"
#include "ShoulderCamcorder.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Data/DataTransmitter.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

URecordingLibrary::URecordingLibrary(){PrimaryComponentTick.bCanEverTick=true;}
void URecordingLibrary::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Fn){
 Super::TickComponent(DT,Type,Fn);if(bRoundEnded)return;
 if(auto* PC=Cast<APlayerController>(GetOwner()))if(auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn())){
  if(C->Health<=0)EndRound(true);
  else if(PC->WasInputKeyJustPressed(EKeys::BackSpace))EndRound(false);
 }
}
void URecordingLibrary::EndRound(bool bDeath){
 if(bRoundEnded)return;
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC)return;
 if(const auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()))bDeath|=C->Health<=0;
 if(auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>()){
  E->SetBackpackOpen(false);
  if(E->Camcorder){E->Camcorder->StopRecording();E->Camcorder->SetViewfinderRaised(false);}
 }
 bRoundEnded=true;bEndedByDeath=bDeath;RoundScore=0;
 for(const auto& Clip:Clips)RoundScore+=Clip.Score;
 auto Record=MakeShared<FJsonObject>();Record->SetNumberField(TEXT("rules_version"),2);Record->SetNumberField(TEXT("score"),RoundScore);Record->SetBoolField(TEXT("death"),bDeath);
 TArray<TSharedPtr<FJsonValue>> Records;for(const auto& C:Clips){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("video"),C.Path);Row->SetStringField(TEXT("score_record"),C.ScoreRecordPath);Row->SetNumberField(TEXT("score"),C.Score);Records.Add(MakeShared<FJsonValueObject>(Row));}Record->SetArrayField(TEXT("clips"),Records);
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("Recordings");IFileManager::Get().MakeDirectory(*Folder,true);
 FString Json;FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Json));const FString Path=Folder/(TEXT("Round_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".score.json"));
 if(FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))RoundRecordPath=Path;
 else UE_LOG(LogTemp,Error,TEXT("Could not persist round score: %s"),*Path);
 if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>())Bus->NotifyChange(this,TEXT("RoundScore"),RoundScore);
 PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
 if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
 PC->SetPause(true);
 if(!GetWorld()->GetGameViewport())return;
 SettlementUI=SNew(SRecordingReview).Library(this);
 GetWorld()->GetGameViewport()->AddViewportWidgetContent(SettlementUI.ToSharedRef(),1000);
 PC->bShowMouseCursor=true;FInputModeUIOnly Mode;Mode.SetWidgetToFocus(SettlementUI);PC->SetInputMode(Mode);
 FSlateApplication::Get().SetKeyboardFocus(SettlementUI,EFocusCause::SetDirectly);
}
void URecordingLibrary::EndPlay(const EEndPlayReason::Type Reason){
 if(SettlementUI&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(SettlementUI.ToSharedRef());
 SettlementUI.Reset();Super::EndPlay(Reason);
}
