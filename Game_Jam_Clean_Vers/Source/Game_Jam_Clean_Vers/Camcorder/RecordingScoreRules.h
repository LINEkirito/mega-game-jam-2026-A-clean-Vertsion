#pragma once
#include "CoreMinimal.h"

enum class EFilmSubject : uint8 { Gang, Civilian, Police, Event, Logo, Scene };
struct FFilmObservation {
 FString Key, Name, Type, Behavior, BehaviorLabel;
 EFilmSubject Kind=EFilmSubject::Gang;
 float Multiplier=1;
 int32 EventId=0, Tier=1;
 FVector4 ScreenRect=FVector4(0,0,0,0);
 bool bHasScreenRect=false;
};
struct FFilmExposure {
 FFilmObservation Subject;
 double Total=0, Run=0, Longest=0, Valid=0;
 int32 Entries=0,ValidRuns=0;
 FString LastBehavior;
 double BehaviorRun=0;
 TSet<FString> Behaviors;
 bool bVisible=false;
 void Observe(double Delta,const FFilmObservation& Value) {
  Subject=Value;
  if(!bVisible){Run=0;BehaviorRun=0;++Entries;}
  const double Before=Run;
  Run+=Delta;Total+=Delta;Longest=FMath::Max(Longest,Run);
  if(Run+1.e-6>=3)Valid+=Before+1.e-6<3?Run:Delta;
  if(Before+1.e-6<3&&Run+1.e-6>=3)++ValidRuns;
  if(LastBehavior!=Value.Behavior)BehaviorRun=0;else BehaviorRun+=Delta;
  LastBehavior=Value.Behavior;
  if(BehaviorRun+1.e-6>=3&&!LastBehavior.IsEmpty())Behaviors.Add(LastBehavior);
  bVisible=true;
 }
 void Break(){bVisible=false;Run=0;BehaviorRun=0;}
};
struct FFilmHistory { double Seconds=0;bool bSeen=false;TSet<FString> Behaviors; };
struct FFilmAward { FFilmObservation Subject;int32 Points=0; };
struct FFilmScoreMoment {
 FFilmObservation Subject;
 FString Category,Explanation;
 double Time=0;
 int32 Points=0;
};
// Transactional per-player state: only Commit promotes working history to saved history.
struct FRecordingScoreRules {
 TMap<FString,FFilmExposure> Shot;
 TMap<FString,FFilmHistory> History,WorkingHistory;
 TMap<FString,FFilmAward> Awards;
 TArray<FFilmScoreMoment> Moments;
 TArray<FString> ObservationOrder;
 double LastTime=0,LongDuration=0;
 bool bHasSample=false;
 static int32 Steps(double Time,std::initializer_list<double> Thresholds,std::initializer_list<int32> Values){int32 Sum=0;auto V=Values.begin();for(double T:Thresholds){if(Time+1.e-6>=T)Sum+=*V;++V;}return Sum;}
 static int32 LongShot(double T){return Steps(T,{10,20,30,60},{10,10,10,40});}
 static int32 EventShot(double T){return Steps(T,{3,10,20},{5,10,15});}
 static int32 GangTime(double T){return Steps(T,{10,30,60,120},{10,20,40,80});}
 static int32 EventTime(double T){return Steps(T,{10,20,40,60},{5,10,15,15});}
 void Begin(){Shot.Reset();Awards.Reset();Moments.Reset();ObservationOrder.Reset();WorkingHistory=History;LastTime=LongDuration=0;bHasSample=false;}
 void Add(const FFilmObservation& O,const FString& Category,const FString& Reason,int32 Base,float Multiplier,double Time){
  const int32 Points=FMath::RoundToInt(Base*FMath::Max(0.f,Multiplier));
  auto& A=Awards.FindOrAdd(O.Key);A.Subject=O;A.Points+=Points;
  FFilmScoreMoment M;M.Subject=O;M.Category=Category;M.Time=Time;M.Points=Points;
  M.Explanation=FString::Printf(TEXT("%s：%d × %.2g = %+d 分"),*Reason,Base,Multiplier,Points);Moments.Add(M);
 }
 void Cross(const FFilmObservation& O,const FString& Category,double Before,double After,std::initializer_list<double> Thresholds,std::initializer_list<int32> Values,float Multiplier,double Time){
  auto V=Values.begin();for(double T:Thresholds){if(Before+1.e-6<T&&After+1.e-6>=T)Add(O,Category,FString::Printf(TEXT("%s达到 %.0f 秒"),*Category,T),*V,Multiplier,Time);++V;}
 }
 void FinalizeDuration(double Seconds){
  if(!bHasSample)return;FFilmObservation O;O.Key=TEXT("Shot");O.Name=TEXT("长镜头");O.Kind=EFilmSubject::Scene;
  Cross(O,TEXT("长镜头"),LongDuration,Seconds,{10,20,30,60},{10,10,10,40},1,Seconds);LongDuration=FMath::Max(LongDuration,Seconds);
 }
 void Sample(double Time,const TArray<FFilmObservation>& Observations){
  if(Time<0||(bHasSample&&Time<=LastTime))return;
  const double Gap=bHasSample?Time-LastTime:0;const double Delta=Gap<=.5?Gap:0;
  bHasSample=true;FinalizeDuration(Time);
  TSet<FString> Visible;
  for(const auto& O:Observations){
   if(Visible.Contains(O.Key))continue;Visible.Add(O.Key);
   if(!Shot.Contains(O.Key))ObservationOrder.Add(O.Key);
   auto& E=Shot.FindOrAdd(O.Key);const double OldTotal=E.Total,OldLongest=E.Longest,OldValid=E.Valid;
   if(Gap>.5)E.Break();E.Observe(E.bVisible?Delta:0,O);
   auto& H=WorkingHistory.FindOrAdd(O.Key);
   const float Multiplier=FMath::Max(0.f,O.Multiplier);
   if(O.Kind==EFilmSubject::Gang&&E.Valid>0){
    if(!H.bSeen){Add(O,TEXT("NPC首次入镜"),TEXT("首次连续有效入镜3秒"),10,Multiplier,Time);H.bSeen=true;}
    Cross(O,TEXT("NPC累计时长"),H.Seconds,H.Seconds+E.Valid-OldValid,{10,30,60,120},{10,20,40,80},Multiplier,Time);H.Seconds+=E.Valid-OldValid;
    if(E.Behaviors.Contains(O.Behavior)&&!H.Behaviors.Contains(O.Behavior)){
     H.Behaviors.Add(O.Behavior);const int32 N=H.Behaviors.Num();
     Add(O,TEXT("NPC不同行为"),FString::Printf(TEXT("第%d种有效行为 · %s"),N,*(O.BehaviorLabel.IsEmpty()?O.Behavior:O.BehaviorLabel)),N<=5?5:N<=10?10:15,Multiplier,Time);
    }
   }else if(O.Kind==EFilmSubject::Event&&E.Valid>0){
    Cross(O,TEXT("事件连续入镜"),OldLongest,E.Longest,{3,10,20},{5,10,15},Multiplier,Time);
    auto& Type=WorkingHistory.FindOrAdd(TEXT("EventType:")+O.Type);
    if(!Type.bSeen){Add(O,TEXT("不同事件种类"),TEXT("本局首次有效拍到该类事件"),5,1,Time);Type.bSeen=true;}
    Cross(O,TEXT("同类事件累计时长"),Type.Seconds,Type.Seconds+E.Valid-OldValid,{10,20,40,60},{5,10,15,15},Multiplier,Time);Type.Seconds+=E.Valid-OldValid;
   }else if(O.Kind==EFilmSubject::Civilian)Cross(O,TEXT("行人入镜扣分"),OldTotal,E.Total,{1,2,5,10},{-2,-2,-3,-5},1,Time);
   else if(O.Kind==EFilmSubject::Police)Cross(O,TEXT("警察入镜扣分"),OldTotal,E.Total,{1,2,5,10},{-4,-4,-5,-8},1,Time);
   else if(O.Kind==EFilmSubject::Logo)Cross(O,TEXT("Logo入镜扣分"),OldTotal,E.Total,{1,2,5,10},{-5,-5,-5,-10},1,Time);
   else if(O.Kind==EFilmSubject::Scene){Cross(O,TEXT("不同区域"),H.Seconds,H.Seconds+E.Total-OldTotal,{10},{20},1,Time);H.Seconds+=E.Total-OldTotal;}
  }
  for(auto& Pair:Shot)if(!Visible.Contains(Pair.Key))Pair.Value.Break();LastTime=Time;
 }
 TArray<FFilmAward> Commit(double Seconds){
  TArray<FFilmAward> Result;if(!bHasSample)return Result;FinalizeDuration(Seconds);
  FFilmObservation Long;Long.Key=TEXT("Shot");Long.Name=TEXT("长镜头");Long.Kind=EFilmSubject::Scene;
  Result.Add(Awards.Contains(Long.Key)?Awards[Long.Key]:FFilmAward{Long,0});
  for(const auto& Key:ObservationOrder){const auto& E=Shot[Key];if(E.Subject.Kind==EFilmSubject::Event&&E.Valid<=0)continue;Result.Add(Awards.Contains(Key)?Awards[Key]:FFilmAward{E.Subject,0});}
  History=WorkingHistory;Begin();return Result;
 }
};
