#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "CamcorderEquipmentComponent.h"
#include "ShoulderCamcorder.h"
#include "BackpackInventory.h"
#include "InventoryItemPreview.h"
#include "Combat/CombatEquipmentComponent.h"
#include "Combat/TacticalWeapon.h"
#include "Combat/PatrolEnemy.h"
#include "Combat/StatueTargetComponent.h"
#include "EngineUtils.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Framework/Application/SlateApplication.h"
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FEquipmentStep,TFunction<void()>,Action);
bool FEquipmentStep::Update(){Action();return true;}
// Equipment timers use world time, so the test must also wait in world time across loading hitches.
class FEquipmentGameWait : public IAutomationLatentCommand
{
    TWeakObjectPtr<UWorld> World;float Duration,End=-1;
public:
    FEquipmentGameWait(UWorld* W,float Seconds):World(W),Duration(Seconds){}
    virtual bool Update() override {if(!World.IsValid())return true;if(End<0)End=World->GetTimeSeconds()+Duration;return World->GetTimeSeconds()>=End;}
};
class FEquipmentWeaponWait : public IAutomationLatentCommand
{
    TWeakObjectPtr<UCombatEquipmentComponent> Combat;
    ETacticalWeaponKind Kind;FAutomationTestBase* Test;double Started=0;
public:
    FEquipmentWeaponWait(UCombatEquipmentComponent* C,ETacticalWeaponKind K,FAutomationTestBase* T):Combat(C),Kind(K),Test(T){}
    virtual bool Update() override
    {
        if(!Started)Started=FPlatformTime::Seconds();
        if(auto* C=Combat.Get())if(auto* G=C->ActiveWeapon.Get())
            if(G->Kind==Kind && G->Handling==EWeaponHandling::Idle && C->EquipRemaining<=0)return true;
        if(FPlatformTime::Seconds()-Started>20){Test->AddError(TEXT("Equipment transition timed out"));return true;}
        return false;
    }
};
// At background-editor frame rates a 0.45-second window can contain only one
// weapon tick. Verify sustained input by its result, with a bounded timeout.
class FEquipmentAutoFireWait : public IAutomationLatentCommand
{
    TWeakObjectPtr<UCombatEquipmentComponent> Combat;double Started=0;
public:
    explicit FEquipmentAutoFireWait(UCombatEquipmentComponent* C):Combat(C){}
    virtual bool Update() override {
        if(!Started)Started=FPlatformTime::Seconds();
        auto* C=Combat.Get();if(!C||!C->ActiveWeapon)return true;
        return C->ActiveWeapon->Magazine<28||FPlatformTime::Seconds()-Started>10;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentIntegration,"Game.Equipment.Integration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEquipmentIntegration::RunTest(const FString&)
{
 AddCommand(new FStartPIECommand(false));AddCommand(new FWaitLatentCommand(2));
 AddCommand(new FEquipmentStep([this]{
 UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){W=Context.World();break;}
 if(!W||!W->GetFirstPlayerController()){AddError(TEXT("PIE unavailable"));return;}
 auto* PC=W->GetFirstPlayerController();auto* E=PC->FindComponentByClass<UCamcorderEquipmentComponent>();
 if(!E||!E->Camcorder||!E->Combat){AddError(TEXT("Equipment missing"));return;}
 // Keep this equipment test independent of combat AI responding to test shots.
 for(TActorIterator<APatrolEnemy> It(W);It;++It)It->SetActorTickEnabled(false);
 int32 StatueCount=0;for(TActorIterator<AActor> It(W);It;++It)if(auto* Target=It->FindComponentByClass<UStatueTargetComponent>()){++StatueCount;TestEqual(TEXT("Statue starts with 1000 HP"),Target->Health,1000.f);TestEqual(TEXT("Statue takes damage"),Target->ApplyHit(100),100.f);TestEqual(TEXT("Statue HP after hit"),Target->Health,900.f);TestEqual(TEXT("Statue lethal damage clamps"),Target->ApplyHit(2000),900.f);TestEqual(TEXT("Dead statue ignores further damage"),Target->ApplyHit(30),0.f);Target->ResetTarget();}TestEqual(TEXT("Five JieGe statue targets"),StatueCount,5);
 TWeakObjectPtr<UCamcorderEquipmentComponent> Weak(E);TWeakObjectPtr<APlayerController> WP(PC);
 auto Key=[WP](FKey K,EInputEvent V){if(auto* P=WP.Get())P->InputKey(FInputKeyEventArgs::CreateSimulated(K,V,V==IE_Pressed?1.f:0.f));};
 auto Step=[this](TFunction<void()> F){AddCommand(new FEquipmentStep(MoveTemp(F)));};auto Wait=[this,W](float S){AddCommand(new FEquipmentGameWait(W,S));};
 TestFalse(TEXT("Cannot record at hip"),E->Camcorder->StartRecording());TestEqual(TEXT("Two batteries"),E->GetBatteryCount(),2);
 Step([Key]{Key(EKeys::RightMouseButton,IE_Pressed);});Wait(.25f);
 Step([this,Weak,Key]{auto* E=Weak.Get();if(!E){AddError(TEXT("PIE ended"));return;}TestTrue(TEXT("RMB raises viewfinder"),E->Camcorder->bViewfinderRaised);Key(EKeys::LeftMouseButton,IE_Pressed);});Wait(.3f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get())TestTrue(TEXT("LMB records"),E->Camcorder->bRecording);Key(EKeys::LeftMouseButton,IE_Released);Key(EKeys::MouseScrollUp,IE_Pressed);});Wait(.15f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get())TestTrue(TEXT("Wheel zooms"),E->Camcorder->FocalLength>35);Key(EKeys::MouseScrollUp,IE_Released);Key(EKeys::RightMouseButton,IE_Released);});Wait(.3f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get()){TestFalse(TEXT("Release lowers camera"),E->Camcorder->bViewfinderRaised);TestFalse(TEXT("Lowering stops recording"),E->Camcorder->bRecording);TestTrue(TEXT("Clip saved"),E->Camcorder->SavedClipCount>0);}Key(EKeys::RightMouseButton,IE_Pressed);});Wait(.25f);
 Step([]{FSlateApplication::Get().SetAllUserFocusToGameViewport();FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));});Wait(.2f);
 Step([this,Weak,Key]{auto* E=Weak.Get();if(!E){AddError(TEXT("Escape incorrectly ended PIE"));return;}TestFalse(TEXT("Escape leaves viewfinder while RMB held"),E->Camcorder->bViewfinderRaised);Key(EKeys::RightMouseButton,IE_Released);E->StoreCamera();TestTrue(TEXT("Camera stored"),E->Camcorder->bInBackpack);TestFalse(TEXT("Stowing never shows duplicate hands"),E->Camcorder->HoldingHands->IsVisible());TestFalse(TEXT("Camera remains visible during lowering"),E->Camcorder->IsHidden());});Wait(.35f);
 Step([this,Weak]{auto* E=Weak.Get();if(!E)return;TestFalse(TEXT("Stored hands hidden after lowering"),E->Camcorder->HoldingHands->IsVisible());E->ReplaceBattery();TestEqual(TEXT("Battery consumed"),E->GetBatteryCount(),1);E->EquipCamera();TestFalse(TEXT("Equipping never shows duplicate hands"),E->Camcorder->HoldingHands->IsVisible());TestTrue(TEXT("Original body remains visible"),Cast<AGame_Jam_Clean_VersCharacter>(Cast<APlayerController>(E->GetOwner())->GetPawn())->GetFirstPersonMesh()->IsVisible());
 E->SetBackpackOpen(true);
 TestNotNull(TEXT("Opening backpack creates item inspection"),E->ItemPreview.Get());
 if(E->ItemPreview){TestTrue(TEXT("Camera preview includes purchased body and display"),E->ItemPreview->PartCount()>=2);const FTransform Before=E->Camcorder->GetActorTransform();const float Focal=E->Camcorder->FocalLength;E->ItemPreview->Rotate(80,30);E->ItemPreview->Zoom(2);TestTrue(TEXT("Inspecting does not rotate equipped camera"),Before.Equals(E->Camcorder->GetActorTransform()));TestEqual(TEXT("Inspection zoom does not change shooting focal length"),E->Camcorder->FocalLength,Focal);E->ItemPreview->ResetView();TestEqual(TEXT("Inspection reset restores zoom"),E->ItemPreview->ZoomAmount,1.f);}
 E->SelectInventoryItem(UBackpackInventory::BatteryId);if(E->ItemPreview){TestEqual(TEXT("Selection updates inspected item"),E->ItemPreview->ItemId,UBackpackInventory::BatteryId);TestTrue(TEXT("Battery includes terminals and label"),E->ItemPreview->PartCount()>3);}TestEqual(TEXT("Inspection does not consume battery"),E->GetBatteryCount(),1);
 E->HandleBackpackKey(EKeys::Two);TestEqual(TEXT("Only four quick slots"),E->Inventory->Slots.Num(),4);TestEqual(TEXT("Battery cannot register"),E->Inventory->SlotIndex(UBackpackInventory::BatteryId),INDEX_NONE);E->SetBackpackOpen(false);TestNull(TEXT("Closing backpack releases inspection actor"),E->ItemPreview.Get());
 auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Cast<APlayerController>(E->GetOwner())->GetPawn());FDamageEvent D;TestEqual(TEXT("First damage"),C->TakeDamage(20,D,nullptr,nullptr),20.f);TestEqual(TEXT("Protection ignores repeated damage"),C->TakeDamage(20,D,nullptr,nullptr),0.f);TestEqual(TEXT("Health 80"),C->Health,80.f);
 // Exercise the actual starting loadout and its registered quick slots.
 for(auto& Pair:E->Combat->Weapons){auto* Gun=Pair.Value.Get();Gun->Magazine=Gun->Spec().Capacity;Gun->bChambered=true;E->Inventory->Add(Gun->Spec().Ammo,TEXT("Ammo"),TEXT("Test"),50);}
 E->ActivateQuickSlot(1);
 TestEqual(TEXT("Carry three guns"),E->Combat->Weapons.Num(),3);TestTrue(TEXT("Camera stored for weapon"),E->Camcorder->bInBackpack);TestFalse(TEXT("Equip delay blocks firing"),E->Combat->Fire());

 });AddCommand(new FEquipmentWeaponWait(E->Combat,ETacticalWeaponKind::Pistol,this));
 Step([this,Weak]{if(auto* E=Weak.Get()){
 // EquipWeapon completes on a timer; inspect only after the equip wait above.
 if(!E->Combat->ActiveWeapon){AddError(TEXT("Pistol did not finish equipping"));return;}
 E->SetBackpackOpen(true);auto* Equipped=E->Combat->ActiveWeapon.Get();const int32 Magazine=Equipped->Magazine;
 for(auto& Pair:E->Combat->Weapons){E->SelectInventoryItem(Pair.Key);TestTrue(TEXT("Each gun has inspectable geometry"),E->ItemPreview && E->ItemPreview->PartCount()>0);}
 E->SelectInventoryItem(TEXT("Ammo556"));TestTrue(TEXT("Ammo has inspectable geometry"),E->ItemPreview && E->ItemPreview->PartCount()==1);
 TestTrue(TEXT("Browsing guns does not equip them"),Equipped==E->Combat->ActiveWeapon);TestEqual(TEXT("Browsing does not fire or reload"),Equipped->Magazine,Magazine);E->SetBackpackOpen(false);
 auto* C=E->Combat.Get();auto* PC=Cast<APlayerController>(E->GetOwner());FVector Eye;FRotator Aim;PC->GetPlayerViewPoint(Eye,Aim);auto* Target=E->GetWorld()->SpawnActor<ACombatTarget>(Eye+Aim.Vector()*150,FRotator::ZeroRotator);Target->SetActorScale3D(FVector(.5f,1.5f,1.5f));TestTrue(TEXT("Pistol fires"),C->Fire());TestTrue(TEXT("Shot applies target damage"),Target->Health<100);Target->Destroy();TestEqual(TEXT("Pistol consumes one round"),C->ActiveWeapon->Magazine,C->ActiveWeapon->Spec().Capacity-1);TestFalse(TEXT("Rate limit blocks duplicate"),C->Fire());TestTrue(TEXT("Reload begins"),C->Reload());TestEqual(TEXT("Magazine reload is active"),C->ActiveWeapon->CurrentAnimationName(),FString(TEXT("MagazineReload")));}});Wait(.7f);
 Step([this,Weak]{if(auto* E=Weak.Get()){auto* Gun=E->Combat->ActiveWeapon.Get();if(!Gun){AddError(TEXT("Missing pistol during reload"));return;}TestTrue(TEXT("Right hand keeps physical grip contact during reload"),Gun->GripError(false)<.1f);}});AddCommand(new FEquipmentWeaponWait(E->Combat,ETacticalWeaponKind::Pistol,this));
 Step([this,Weak]{if(auto* E=Weak.Get()){auto* C=E->Combat.Get();if(!C->ActiveWeapon){AddError(TEXT("Missing reloaded pistol"));return;}TestEqual(TEXT("Pistol reload full"),C->ActiveWeapon->Magazine,C->ActiveWeapon->Spec().Capacity);TestEqual(TEXT("Reserve consumes one"),C->Reserve(),49);for(auto& Pair:C->Weapons)if(Pair.Value->Kind==ETacticalWeaponKind::Shotgun){C->EquipWeapon(Pair.Key);break;}}});AddCommand(new FEquipmentWeaponWait(E->Combat,ETacticalWeaponKind::Shotgun,this));
 Step([this,Weak]{if(auto* E=Weak.Get()){auto* C=E->Combat.Get();if(!C->ActiveWeapon||C->ActiveWeapon->Kind!=ETacticalWeaponKind::Shotgun){AddError(TEXT("Missing equipped shotgun"));return;}C->ActiveWeapon->Magazine=2;TestTrue(TEXT("Shotgun reload begins"),C->Reload());TestEqual(TEXT("Shell reload starts with authored lead-in"),C->ActiveWeapon->CurrentAnimationName(),FString(TEXT("ShellStart")));}});Wait(1.4f);
 Step([this,Weak]{if(auto* E=Weak.Get()){auto* C=E->Combat.Get();if(!C->ActiveWeapon){AddError(TEXT("Missing reloading shotgun"));return;}TestEqual(TEXT("One shell loaded"),C->ActiveWeapon->Magazine,3);TestFalse(TEXT("Shotgun finishes insertion and charging before interrupt fire"),C->Fire());for(auto& Pair:C->Weapons)if(Pair.Value->Kind==ETacticalWeaponKind::Rifle){C->EquipWeapon(Pair.Key);break;}}});AddCommand(new FEquipmentWeaponWait(E->Combat,ETacticalWeaponKind::Rifle,this));
 Step([Key]{Key(EKeys::LeftMouseButton,IE_Pressed);});AddCommand(new FEquipmentAutoFireWait(E->Combat));
 Step([this,Weak,Key]{Key(EKeys::LeftMouseButton,IE_Released);if(auto* E=Weak.Get()){if(!E->Combat->ActiveWeapon){AddError(TEXT("Missing equipped rifle"));return;}TestTrue(TEXT("Rifle full auto fires multiple rounds"),E->Combat->ActiveWeapon->Magazine<28);const int32 Mag=E->Combat->ActiveWeapon->Magazine;auto* Gun=E->Combat->ActiveWeapon.Get();E->Combat->DropWeapon();TestEqual(TEXT("Dropped gun retains magazine"),Gun->Magazine,Mag);TestTrue(TEXT("Drop waits for hand release"),E->Combat->Weapons.Contains(Gun->InventoryId));E->EquipCamera();TestFalse(TEXT("Released gun is no longer owned"),E->Combat->Weapons.Contains(Gun->InventoryId));TestFalse(TEXT("Camera equip disarms"),bool(E->Combat->ActiveWeapon));}});
 Wait(.85f);
 Step([this,Weak]{if(auto* E=Weak.Get()){
 auto* PC=Cast<APlayerController>(E->GetOwner());auto* C=Cast<AGame_Jam_Clean_VersCharacter>(PC->GetPawn());FDamageEvent D;
 TestEqual(TEXT("Invalid damage is ignored"),C->TakeDamage(0,D,nullptr,nullptr),0.f);
 TestEqual(TEXT("Protection expires and invalid damage does not rearm it"),C->TakeDamage(60,D,nullptr,nullptr),60.f);TestEqual(TEXT("Low health 20"),C->Health,20.f);
 E->Camcorder->SetViewfinderRaised(true);TestTrue(TEXT("Battery test records"),E->Camcorder->StartRecording());const int32 Saved=E->Camcorder->SavedClipCount;E->Camcorder->ConsumeBattery(10000);
 TestFalse(TEXT("Zero battery powers off"),E->Camcorder->bPowered);TestFalse(TEXT("Zero battery ends recording"),E->Camcorder->bRecording);TestEqual(TEXT("Zero battery finalizes clip"),E->Camcorder->SavedClipCount,Saved+1);E->ReplaceBattery();TestEqual(TEXT("Last spare consumed"),E->GetBatteryCount(),0);TestEqual(TEXT("Fresh battery full"),E->Camcorder->BatteryPercent,100.f);TestTrue(TEXT("Fresh battery powers on"),E->Camcorder->bPowered);
 }});
 Step([Key]{Key(EKeys::W,IE_Pressed);Key(EKeys::LeftShift,IE_Pressed);});Wait(.2f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get()){auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Cast<APlayerController>(E->GetOwner())->GetPawn());TestTrue(TEXT("Forward Shift sprints"),C->bSprinting);TestEqual(TEXT("Sprint speed"),C->GetCharacterMovement()->MaxWalkSpeed,650.f);}Key(EKeys::RightMouseButton,IE_Pressed);});Wait(.25f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get()){auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Cast<APlayerController>(E->GetOwner())->GetPawn());TestFalse(TEXT("Viewfinder suspends sprint"),C->bSprinting);TestEqual(TEXT("Aim speed is 75 percent"),C->GetCharacterMovement()->MaxWalkSpeed,270.f);}Key(EKeys::RightMouseButton,IE_Released);});Wait(.2f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get()){auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Cast<APlayerController>(E->GetOwner())->GetPawn());TestTrue(TEXT("Releasing aim restores held sprint"),C->bSprinting);}Key(EKeys::W,IE_Released);Key(EKeys::LeftControl,IE_Pressed);});Wait(.2f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get()){auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Cast<APlayerController>(E->GetOwner())->GetPawn());TestTrue(TEXT("Ctrl crouches despite held Shift"),C->bIsCrouched);TestFalse(TEXT("Crouch cancels sprint"),C->bSprinting);C->ProcessEvent(C->FindFunctionChecked(TEXT("DoJumpStart")),nullptr);}Key(EKeys::LeftControl,IE_Released);Key(EKeys::LeftShift,IE_Released);});Wait(.15f);
 Step([this,Weak,Key]{if(auto* E=Weak.Get()){auto* C=Cast<AGame_Jam_Clean_VersCharacter>(Cast<APlayerController>(E->GetOwner())->GetPawn());TestFalse(TEXT("Jump uncrouches with clear ceiling"),C->bIsCrouched);TestTrue(TEXT("Jump leaves ground"),C->GetCharacterMovement()->IsFalling());TestEqual(TEXT("Single jump only"),C->JumpMaxCount,1);C->ProcessEvent(C->FindFunctionChecked(TEXT("DoJumpEnd")),nullptr);}});
 Step([Weak]{
    // Automation opens MessageLog during PIE. It can occlude the viewport and intercept synthetic pointer input.
    if(auto LogTab=FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(TEXT("MessageLog"))))LogTab->RequestCloseTab();
    if(auto* E=Weak.Get()){E->SetBackpackOpen(true);E->SelectInventoryItem(UBackpackInventory::CameraId);}
 });Wait(.2f);
 Step([this,Weak]{
    auto* E=Weak.Get();if(!E||!E->ItemPreview){AddError(TEXT("Inspection unavailable for UI test"));return;}
    auto Window=E->GetWorld()->GetGameViewport()->GetWindow();
    TFunction<TSharedPtr<SWidget>(TSharedRef<SWidget>)> FindInspection;
    FindInspection=[&FindInspection](TSharedRef<SWidget> Widget)->TSharedPtr<SWidget>{if(Widget->GetType()==TEXT("SInventoryInspection"))return Widget;auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)if(auto Found=FindInspection(Children->GetChildAt(I)))return Found;return nullptr;};
    auto View=Window.IsValid()?FindInspection(Window.ToSharedRef()):nullptr;
    if(!View){AddError(TEXT("Inspection Slate widget not found"));E->SetBackpackOpen(false);return;}
    const FVector2D P=View->GetCachedGeometry().GetAbsolutePosition()+View->GetCachedGeometry().GetAbsoluteSize()*.5;
    const FVector2D End=P+FVector2D(35,20);const TSet<FKey> Down{EKeys::LeftMouseButton};const TSet<FKey> Up;
    auto& Slate=FSlateApplication::Get();const float Focal=E->Camcorder->FocalLength;const int32 Shots=E->Combat->ShotsFired;
    const FRotator Look=Cast<APlayerController>(E->GetOwner())->GetControlRotation();
    // Position the real Slate cursor before injecting a drag; previous tests may leave it elsewhere.
    Slate.SetCursorPos(P);
    {const auto Path=Slate.LocateWindowUnderMouse(P,Slate.GetInteractiveTopLevelWindows());FString Names;for(int32 Index=0;Index<Path.Widgets.Num();++Index)Names+=Path.Widgets[Index].Widget->GetTypeAsString()+TEXT("/");AddInfo(FString::Printf(TEXT("Inspection pointer (%s), target window %s, route %s"),*P.ToString(),*Window->GetTitle().ToString(),*Names));}
    Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),FPointerEvent(0,P,P,Down,EKeys::LeftMouseButton,0,FModifierKeysState()));
    Slate.ProcessMouseMoveEvent(FPointerEvent(0,End,P,Down,EKeys::Invalid,0,FModifierKeysState()));
    Slate.ProcessMouseButtonUpEvent(FPointerEvent(0,End,End,Up,EKeys::LeftMouseButton,0,FModifierKeysState()));
    TestFalse(TEXT("Real Slate mouse drag rotates inspection"),E->ItemPreview->ViewRotation.IsNearlyZero());
    Slate.ProcessMouseWheelOrGestureEvent(FPointerEvent(0,End,End,Up,EKeys::Invalid,1,FModifierKeysState()),nullptr);
    TestTrue(TEXT("Real Slate wheel zooms inspection"),E->ItemPreview->ZoomAmount>1);
    TestEqual(TEXT("Inspection mouse does not fire"),E->Combat->ShotsFired,Shots);TestEqual(TEXT("Inspection wheel does not zoom gameplay camera"),E->Camcorder->FocalLength,Focal);
    TestTrue(TEXT("Inspection drag does not turn player"),Look.Equals(Cast<APlayerController>(E->GetOwner())->GetControlRotation()));
    Slate.ProcessMouseButtonDoubleClickEvent(Window->GetNativeWindow(),FPointerEvent(0,End,End,Down,EKeys::LeftMouseButton,0,FModifierKeysState()));
    TestEqual(TEXT("Double click resets inspection"),E->ItemPreview->ZoomAmount,1.f);
    E->SetBackpackOpen(false);
 });
 AddCommand(new FEndPlayMapCommand());
 }));return true;
}
#endif
