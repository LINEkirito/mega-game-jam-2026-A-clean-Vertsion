#include "RecordingLibrary.h"
#include "RecordingReview.h"
#include "RecordingTabletComponent.h"
#include "BackpackInventory.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Localization/GameLanguage.h"
#include "Kismet/GameplayStatics.h"
#include "RunScoreHistory.h"
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
 Super::TickComponent(DT,Type,Fn);
 if(bRoundEnded){
  if(bEndedByDeath&&!bSettlementShown){
   auto* PC=Cast<APlayerController>(GetOwner());auto* C=PC?Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()):nullptr;
   if(C&&!C->IsDeathFallComplete())return;
   if(!DeathUI&&GetWorld()->GetGameViewport()){
    DeathUI=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
     .BorderBackgroundColor_Lambda([this]{return FLinearColor(0,0,0,FMath::Clamp(DeathMessageTime/.5f,0.f,1.f)*.85f);})
     .HAlign(HAlign_Center).VAlign(VAlign_Center)
     [SNew(SLocalizedTextBlock).Text(FText::FromString(TEXT("您已死亡"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",48))
      .ColorAndOpacity_Lambda([this]{return FLinearColor(.9f,.18f,.13f,FMath::Clamp(DeathMessageTime/.35f,0.f,1.f));})];
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(DeathUI.ToSharedRef(),999);
   }
   DeathMessageTime+=DT;
   if(DeathMessageTime>=1.4f)ShowSettlement();
  }
  return;
 }
 if(auto* PC=Cast<APlayerController>(GetOwner()))if(auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn())){
  if(C->Health<=0)EndRound(true);
 }
}
void URecordingLibrary::EndRound(bool bDeath){
 if(bRoundEnded)return;
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC)return;
 if(const auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()))bDeath|=C->Health<=0;
 if(auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>()){
  E->SetBackpackOpen(false);
  if(E->Tablet)E->Tablet->SetOpen(false);
  if(E->Camcorder){E->Camcorder->StopRecording();E->Camcorder->SetViewfinderRaised(false);}
 }
 bRoundEnded=true;bEndedByDeath=bDeath;RoundScore=0;
 for(const auto& Clip:Clips)RoundScore+=Clip.Score;
 const float RawScore=RoundScore;
 if(bDeath)RoundScore*=.5f;
 auto Record=MakeShared<FJsonObject>();Record->SetNumberField(TEXT("raw_score"),RawScore);Record->SetNumberField(TEXT("settlement_multiplier"),bDeath?.5:1.0);Record->SetStringField(TEXT("outcome"),bDeath?TEXT("death"):TEXT("extracted"));Record->SetNumberField(TEXT("rules_version"),3);Record->SetNumberField(TEXT("score"),RoundScore);Record->SetBoolField(TEXT("death"),bDeath);
 TArray<TSharedPtr<FJsonValue>> Records;for(const auto& C:Clips){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("video"),C.Path);Row->SetStringField(TEXT("score_record"),C.ScoreRecordPath);Row->SetNumberField(TEXT("score"),C.Score);Records.Add(MakeShared<FJsonValueObject>(Row));}Record->SetArrayField(TEXT("clips"),Records);
 Record->SetStringField(TEXT("completed_utc"),FDateTime::UtcNow().ToIso8601());
 const FString Folder=FRunScoreHistory::Directory();IFileManager::Get().MakeDirectory(*Folder,true);
 FString Json;FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Json));const FString Path=Folder/(TEXT("Round_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".score.json"));
 if(FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))RoundRecordPath=Path;
 else UE_LOG(LogTemp,Error,TEXT("Could not persist round score: %s"),*Path);
 if(auto* Bus=GetWorld()->GetSubsystem<UDataTransmitter>())Bus->NotifyChange(this,TEXT("RoundScore"),RoundScore);
 PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
 if(PC->PlayerInput)PC->PlayerInput->FlushPressedKeys();
 if(bDeath){
  if(auto* Combat=PC->FindComponentByClass<UCombatEquipmentComponent>()){
   Combat->DropOnDeath();
  }
  if(auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>())if(E->Camcorder&&E->Camcorder->IsPresenting()){
   E->Camcorder->DropOnDeath();if(E->Inventory)E->Inventory->Remove(UBackpackInventory::CameraId);E->RefreshBackpack();
  }
  if(auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn()))C->BeginDeathFall();
  PC->bShowMouseCursor=false;PC->SetInputMode(FInputModeGameOnly());
  return;
 }
 ShowSettlement();
}
void URecordingLibrary::ShowSettlement(){
 if(bSettlementShown)return;
 bSettlementShown=true;
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC)return;
 if(DeathUI&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DeathUI.ToSharedRef());
 DeathUI.Reset();
 PC->SetPause(true);
 if(!GetWorld()->GetGameViewport())return;
 SettlementUI=SNew(SRecordingReview).Library(this);
 GetWorld()->GetGameViewport()->AddViewportWidgetContent(SettlementUI.ToSharedRef(),1000);
 PC->bShowMouseCursor=true;FInputModeUIOnly Mode;Mode.SetWidgetToFocus(SettlementUI);PC->SetInputMode(Mode);
 FSlateApplication::Get().SetKeyboardFocus(SettlementUI,EFocusCause::SetDirectly);
}
void URecordingLibrary::ReturnToTitle(){
 if(!bSettlementShown||bReturningToTitle)return;
 bReturningToTitle=true;
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC)return;
 // Reload the current map so its normal frontend boot creates a clean new run.
 PC->SetPause(false);PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();
 UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this,true)),true,TEXT("ShowTitle"));
}
void URecordingLibrary::EndPlay(const EEndPlayReason::Type Reason){
 if(SettlementUI&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(SettlementUI.ToSharedRef());
 if(DeathUI&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DeathUI.ToSharedRef());
 DeathUI.Reset();SettlementUI.Reset();Super::EndPlay(Reason);
}
