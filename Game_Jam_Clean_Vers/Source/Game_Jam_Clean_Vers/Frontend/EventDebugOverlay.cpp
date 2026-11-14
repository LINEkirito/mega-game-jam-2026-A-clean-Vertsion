#if WITH_EDITOR
#include "EventDebugOverlay.h"
#include "Combat/WorldEvents.h"
#include "Combat/PatrolEnemy.h"
#include "Data/GlobalData.h"
#include "Localization/GameLanguage.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

namespace
{
 FString DebugEventName(EStreetEvent Kind)
 {
  switch(Kind){
  case EStreetEvent::GangMeeting:return GameLanguage::Display(TEXT("帮派聚会"));
  case EStreetEvent::SecretTransaction:return GameLanguage::Display(TEXT("秘密交易"));
  case EStreetEvent::CivilianEscape:return GameLanguage::Display(TEXT("市民逃跑"));
  case EStreetEvent::GangShootout:return GameLanguage::Display(TEXT("警匪枪战"));
  case EStreetEvent::MassiveGangWar:return GameLanguage::Display(TEXT("大规模帮派战争"));
  case EStreetEvent::GangMassacre:return GameLanguage::Display(TEXT("帮派屠杀"));
  default:return GameLanguage::Display(TEXT("旧版警察追击"));
  }
 }
 class SEventDebugOverlay final:public SCompoundWidget
 {
 public:
  SLATE_BEGIN_ARGS(SEventDebugOverlay){} SLATE_END_ARGS()
  void Construct(const FArguments&,APlayerController* PC)
  {
   Player=PC;SetVisibility(EVisibility::HitTestInvisible);
   ChildSlot[SNew(SBox).WidthOverride(370).HeightOverride_Lambda([this]{
    int32 X=1280,Y=720;if(auto* P=Player.Get())P->GetViewportSize(X,Y);return FMath::Clamp(Y*.85f,240.f,760.f);
   })[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly).VAlign(VAlign_Top)
    [SNew(SBox).WidthOverride(370)[SNew(SBorder).Padding(12).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
     .BorderBackgroundColor(FLinearColor(.012f,.02f,.028f,.72f))[SAssignNew(Rows,SVerticalBox)]]]]];
   Refresh();
  }
  virtual void Tick(const FGeometry& Geometry,double Time,float DT) override
  {
   SCompoundWidget::Tick(Geometry,Time,DT);UntilRefresh-=DT;
   if(UntilRefresh<=0||English!=GameLanguage::IsEnglish()){UntilRefresh=.25f;Refresh();}
  }
 private:
  TWeakObjectPtr<APlayerController> Player;
  TSharedPtr<SVerticalBox> Rows;
  float UntilRefresh=0;bool English=false;
  void Line(const FString& Value,int32 Size=13,FLinearColor Color=FLinearColor(.82f,.88f,.92f))
  {
   Rows->AddSlot().AutoHeight().Padding(0,2)[SNew(SLocalizedTextBlock).Font(GameLanguage::Font(Size)).ColorAndOpacity(Color)
    .AutoWrapText(false).WrapTextAt(344).Text(FText::FromString(Value))];
  }
  void Refresh()
  {
   English=GameLanguage::IsEnglish();Rows->ClearChildren();
   Line(TEXT("事件调试 · F2 隐藏"),17,FLinearColor(.35f,.95f,.78f));
   Line(TEXT("仅编辑器 · 实时运行 · 鼠标穿透"),11);
   const auto* PC=Player.Get();auto* W=PC?PC->GetWorld():nullptr;auto* D=W?W->GetSubsystem<UWorldEvents>():nullptr;
   if(!D){Line(TEXT("等待游戏场景"));return;}
   const float Now=W->GetTimeSeconds();const auto* C=GetDefault<UGlobalData>();
   int32 Active=0,Ended=0,Living=0,Combat=0;
   for(const auto& E:D->Events){Active+=E.bActive;Ended+=!E.bActive;}
   for(TActorIterator<APatrolEnemy> It(W);It;++It){Living+=It->Health>0;Combat+=It->Health>0&&It->bCombat;}
   Line(FString::Printf(TEXT("场景 %s · 游戏时间 %.0f 秒"),*W->GetMapName(),Now),12);
   Line(FString::Printf(TEXT("进行 %d | 待安排 %d | 已结束 %d"),Active,D->DebugPendingEvents().Num(),Ended));
   Line(FString::Printf(TEXT("存活 NPC %d | 交战 NPC %d"),Living,Combat));
   Line(FString::Printf(TEXT("自动调度 %s | 下次抽取 %.1f 秒"),*GameLanguage::Display(D->bAutomatic&&C->bEnableStreetEvents?TEXT("开启"):TEXT("关闭")),FMath::Max(0.f,D->DebugNextAttempt())),12);
   Line(FString::Printf(TEXT("抽取间隔 %.0f 秒 | 冷却 %.0f 秒 | 上限 3"),C->EventAttemptSeconds,C->EventCooldownSeconds),12);
   Line(FString::Printf(TEXT("稀有池 %.0f%% | 普通池 %.0f%%"),C->RareEventChance*100,(1-C->RareEventChance)*100),12);
   Line(FString::Printf(TEXT("警匪命中 %.0f%% | 伤害倍率 %.2f | 单弹上限 %.1f"),C->PoliceGangHitChance*100,C->PoliceGangDamageScale,C->PoliceGangMaxHitDamage),12);
   Line(TEXT("当前事件"),15,FLinearColor(.35f,.95f,.78f));
   int32 Shown=0;
   for(const auto& E:D->Events){if(!E.bActive)continue;if(Shown++>=4)break;
    const FString State=GameLanguage::Display(!E.bHasPerformed?TEXT("准备中"):E.bPerforming?TEXT("执行中"):TEXT("执行中，暂不可录像"));
    Line(FString::Printf(TEXT("#%d %s · %s"),E.Id,*DebugEventName(E.Kind),*State),14,FLinearColor::White);
    Line(FString::Printf(TEXT("来源 %s | 等级 %d | 配置分值 %d"),*GameLanguage::Display(E.bScheduled?TEXT("概率调度"):TEXT("现场触发")),E.Tier,E.Points),11);
    if(E.bHasPerformed)Line(FString::Printf(TEXT("已执行 %.0f 秒 / %.0f 秒 | 剩余 %.0f 秒"),Now-E.PerformingSince,E.Duration,FMath::Max(0.f,E.ExpiresAt-Now)),12);
    else Line(FString::Printf(TEXT("已准备 %.0f 秒 | 准备上限 %.0f 秒 | 将持续 %.0f 秒"),Now-E.Started,C->EventPreparationSeconds,E.Duration),12);
    int32 Alive=0,Fighting=0;float Low=MAX_flt,High=0;int32 Ammo=0;
    for(auto Weak:E.Participants)if(auto* N=Weak.Get();N&&N->Health>0){++Alive;Fighting+=N->bCombat;Low=FMath::Min(Low,N->Health);High=FMath::Max(High,N->Health);if(N->HeldWeapon)Ammo+=N->HeldWeapon->Magazine+(N->HeldWeapon->bChambered?1:0);}
    Line(FString::Printf(TEXT("参与 %d | 存活 %d | 交战 %d | 枪内弹药 %d"),E.Participants.Num(),Alive,Fighting,Ammo),11);
    Line(FString::Printf(TEXT("生命 %.1f～%.1f | 距玩家 %.0f 米"),Alive?Low:0,High,PC->GetPawn()?FVector::Dist(PC->GetPawn()->GetActorLocation(),E.Location)/100:0),11);
   }
   if(!Shown)Line(TEXT("暂无进行中的事件"),12);
   if(Active>4)Line(FString::Printf(TEXT("另有 %d 个事件，优先显示前 4 个"),Active-4),11);
   if(!D->DebugPendingEvents().IsEmpty()){
    Line(TEXT("待安排事件"),15,FLinearColor(1.f,.8f,.35f));
    const auto Kind=D->DebugPendingEvents()[0];
    Line(FString::Printf(TEXT("%s | %.1f 秒后重试"),*DebugEventName(Kind),FMath::Max(0.f,D->DebugRetryAttempt())),12);
    Line((Kind==EStreetEvent::MassiveGangWar||Kind==EStreetEvent::GangMassacre)&&Now<C->LargeEventEarliestSeconds?TEXT("等待大型事件开局门槛"):TEXT("等待可用角色或可达的视野外位置"),11);
   }
   Line(TEXT("最近结束"),15,FLinearColor(.65f,.73f,.8f));
   int32 Recent=0;
   for(int32 I=D->Events.Num()-1;I>=0&&Recent<2;--I){const auto& E=D->Events[I];if(E.bActive)continue;++Recent;
    Line(FString::Printf(TEXT("#%d %s · %s"),E.Id,*DebugEventName(E.Kind),*GameLanguage::Display(E.DebugEndReason)),12);
    Line(FString::Printf(TEXT("%.0f 秒前结束 | 实际执行 %.0f 秒"),FMath::Max(0.f,Now-E.DebugEndedAt),E.bHasPerformed?FMath::Max(0.f,E.DebugEndedAt-E.PerformingSince):0),11);
   }
   if(!Recent)Line(TEXT("暂无结束记录"),12);
  }
 };
}
TSharedRef<SWidget> EventDebug::MakeOverlay(APlayerController* Player){return SNew(SEventDebugOverlay,Player);}
#endif
