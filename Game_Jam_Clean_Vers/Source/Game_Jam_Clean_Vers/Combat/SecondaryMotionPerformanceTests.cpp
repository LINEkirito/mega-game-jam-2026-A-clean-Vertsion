#if WITH_DEV_AUTOMATION_TESTS
#include "CharacterSecondaryMotion.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"

// Frozen pre-optimization solver: independent oracle for swept contact behavior.
static FQuat ReferenceConstrainSwing(const FVector& Origin,const FVector& Rest,
 const FQuat& Swing,const TArray<FCharacterSecondaryMotion::FCapsule>& Colliders)
{
 if(Colliders.IsEmpty()||Rest.IsNearlyZero())return Swing;
 auto Distance=[&](const FVector& Tip,const FCharacterSecondaryMotion::FCapsule& C)
 {
  FVector OnStrand,OnBody;
  FMath::SegmentDistToSegmentSafe(Origin,Tip,C.A,C.B,OnStrand,OnBody);
  return FVector::Distance(OnStrand,OnBody);
 };
 // Calibrate contact against the whole authored segment, not just its tip.
 // This preserves intentional roots touching skin without disabling mid-strand contact.
 TArray<float,TInlineAllocator<24>> Radii;
 for(const auto& C:Colliders)Radii.Add(FMath::Min(C.Radius,FMath::Max(0.f,Distance(Origin+Rest,C)-.2f)));
 auto Clear=[&](float Alpha)
 {
  const FVector Tip=Origin+FQuat::Slerp(FQuat::Identity,Swing,Alpha).RotateVector(Rest);
  for(int32 I=0;I<Colliders.Num();++I)
   if(Radii[I]>0&&Distance(Tip,Colliders[I])<Radii[I])return false;
  return true;
 };
 // Walk the angular path as well as checking its endpoint: a fast turn must not
 // skip through a collider and emerge on the other side in one update.
 float Safe=0.f;
 for(int32 Step=1;Step<=16;++Step)
 {
  float Blocked=Step/16.f;
  if(Clear(Blocked)){Safe=Blocked;continue;}
  for(int32 Iteration=0;Iteration<12;++Iteration)
  {
   const float Mid=(Safe+Blocked)*.5f;
   if(Clear(Mid))Safe=Mid;else Blocked=Mid;
  }
  return FQuat::Slerp(FQuat::Identity,Swing,Safe).GetNormalized();
 }
 return Swing;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondaryMotionBroadPhaseTest,"Game.Characters.SecondaryMotionBroadPhase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSecondaryMotionBroadPhaseTest::RunTest(const FString&)
{
 FRandomStream Random(746219);
 int32 Mismatches=0;
 for(int32 Trial=0;Trial<5000;++Trial)
 {
  const FVector Origin=Random.VRand()*Random.FRandRange(0.f,10000.f);
  const FVector Rest=Random.VRand()*Random.FRandRange(.01f,60.f);
  const FQuat Swing(Random.VRand(),FMath::DegreesToRadians(Random.FRandRange(-179.f,179.f)));
  TArray<FCharacterSecondaryMotion::FCapsule> Colliders;
  for(int32 I=0;I<12;++I)
  {
   const FVector A=Origin+Random.VRand()*Random.FRandRange(0.f,100.f);
   Colliders.Add({A,A+(I%3?Random.VRand()*Random.FRandRange(0.f,50.f):FVector::ZeroVector),static_cast<float>(Random.FRandRange(0.f,15.f))});
  }
  const auto Expected=ReferenceConstrainSwing(Origin,Rest,Swing,Colliders);
  const auto Actual=FCharacterSecondaryMotion::ConstrainSwing(Origin,Rest,Swing,Colliders);
  if(!Actual.Equals(Expected,.000001))++Mismatches;
 }
 TestEqual(TEXT("5000 seeded swept-contact cases match original solver"),Mismatches,0);

 // A short hair strand near the head: most torso/limb capsules are out of
 // reach. Report kernel timing only; this is not a whole-game FPS benchmark.
 const FVector Origin(0,0,170),Rest(0,0,-8);
 TArray<FCharacterSecondaryMotion::FCapsule> Colliders;
 for(int32 I=0;I<8;++I)Colliders.Add({FVector(I*10,0,100),FVector(I*10,0,60),5.f});
 constexpr int32 Count=20000;
 double Checksum=0;
 auto Measure=[&](bool Optimized)
 {
  const double Start=FPlatformTime::Seconds();
  for(int32 I=0;I<Count;++I)
  {
   const FQuat Swing(FVector::YAxisVector,FMath::DegreesToRadians(float(I%18)));
   const FQuat Result=Optimized?FCharacterSecondaryMotion::ConstrainSwing(Origin,Rest,Swing,Colliders):ReferenceConstrainSwing(Origin,Rest,Swing,Colliders);
   Checksum+=Result.W;
  }
  return (FPlatformTime::Seconds()-Start)*1000;
 };
 Measure(true);Measure(false);
 const double Before=Measure(false),After=Measure(true);
 AddInfo(FString::Printf(TEXT("Contact kernel: %d calls, original %.3f ms, broad phase %.3f ms, ratio %.2fx (checksum %.3f); not whole-game FPS"),Count,Before,After,Before/FMath::Max(After,.000001),Checksum));
 return true;
}
#endif
