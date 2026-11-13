#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "TacticalWeapon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTracerBounds,"Game.Combat.TracerBounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTracerBounds::RunTest(const FString&)
{
    UWorld* World=GEditor->GetEditorWorldContext().World();
    auto* FX=World->SpawnActor<ACombatShotEffect>();
    if(!TestNotNull(TEXT("Tracer spawns"),FX))return false;
    TArray<UStaticMeshComponent*> Meshes; FX->GetComponents(Meshes);
    UStaticMeshComponent* Trail=nullptr;
    for(auto* M:Meshes)
    {
        TestEqual(TEXT("Cosmetic components cannot block bullets"),M->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
        if(M->GetName()==TEXT("Tracer"))Trail=M;
    }
    if(TestNotNull(TEXT("Tracer mesh"),Trail))
    {
        TestNotNull(TEXT("Soft material loaded"),Trail->GetMaterial(0));
        FX->Setup(FVector::ZeroVector,FVector(2000,0,0),false,false);
        const float Initial=Trail->GetComponentLocation().X;
        FX->Tick(.01f);
        TestTrue(TEXT("Short streak moves forward"),Trail->GetComponentLocation().X>Initial);
        TestTrue(TEXT("Does not span entire shot"),Trail->GetComponentScale().X*100.f<200.f);
        FX->Setup(FVector::ZeroVector,FVector(20,0,0),true,false);
        FX->Tick(.001f);
        const float Half=Trail->GetComponentScale().X*50.f;
        TestTrue(TEXT("Close hit clips head at surface"),Trail->GetComponentLocation().X+Half<=20.001f);
        TestTrue(TEXT("Close hit never extends behind muzzle"),Trail->GetComponentLocation().X-Half>=-.001f);
        FX->Tick(.2f);
        TestFalse(TEXT("Hitch cannot leave persistent trail"),Trail->IsVisible());
        FX->Setup(FVector::ZeroVector,FVector::ZeroVector,false,false);
        TestFalse(TEXT("Zero-length shot hides trail"),Trail->IsVisible());
    }
    const FVector Wall(100,0,0),Normal(-1,0,0);
    FX->Setup(FVector::ZeroVector,Wall,true,false,Normal);
    for(auto* M:Meshes)if(M!=Trail)TestFalse(TEXT("No impact before visual arrival"),M->IsVisible());
    FX->Tick(.025f);
    int32 VisibleImpacts=0;
    for(auto* M:Meshes)if(M!=Trail && M->IsVisible())
    {
        ++VisibleImpacts;
        TestNotNull(TEXT("Impact material loaded"),M->GetMaterial(0));
        TestTrue(TEXT("Impact stays outside the struck wall"),FVector::DotProduct(M->GetComponentLocation()-Wall,Normal)>0.f);
    }
    TestTrue(TEXT("Wall impact has flash, dust and sparks"),VisibleImpacts>=3);
    FX->Tick(1.f);
    for(auto* M:Meshes)TestFalse(TEXT("All impact layers expire after a hitch"),M->IsVisible());
    FX->Setup(FVector::ZeroVector,Wall,false,false,Normal);FX->Tick(.025f);
    for(auto* M:Meshes)if(M!=Trail)TestFalse(TEXT("Miss does not create wall effects"),M->IsVisible());
    FX->Setup(FVector::ZeroVector,Wall,true,true,Normal);FX->Tick(.025f);
    for(auto* M:Meshes)if(M!=Trail)TestFalse(TEXT("Flesh does not create wall effects"),M->IsVisible());
    FX->Destroy();
    return true;
}
#endif
