#include "RunScoreHistoryPanel.h"
#include "Audio/AudioWidgets.h"
#include "Camcorder/RunScoreHistory.h"
#include "Localization/GameLanguage.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
namespace {
 TSharedRef<SWidget> HistoryText(const FString& Value,int32 Size=18,bool Wrap=true) {return SNew(SLocalizedTextBlock).Font(GameLanguage::Font(Size)).AutoWrapText(Wrap).Text(FText::FromString(Value));}
 TSharedRef<SWidget> HistoryButton(const TCHAR* Value,TFunction<void()> Action) {return SNew(SAudioButton).ContentPadding(FMargin(16,10)).HAlign(HAlign_Center).OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})[HistoryText(Value,18,false)];}
 class SRunHistory final:public SCompoundWidget {
 public:
  SLATE_BEGIN_ARGS(SRunHistory){} SLATE_END_ARGS()
  void Construct(const FArguments&,TFunction<void()> Close) {OnClose=MoveTemp(Close);ChildSlot[SNew(SBox).WidthOverride(780).HeightOverride(650)[SAssignNew(Host,SBorder).Padding(16)]];Refresh();}
  virtual bool SupportsKeyboardFocus() const override{return true;}
 private:
  TSharedPtr<SBorder> Host;TFunction<void()> OnClose;FRunScoreHistory Store;bool bError=false;
  void Refresh() {
   int32 Invalid=0;const auto Entries=Store.Load(Invalid);auto Rows=SNew(SVerticalBox);
   Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[HistoryText(TEXT("游玩记录"),30)];
   Rows->AddSlot().AutoHeight().Padding(0,0,0,16)[HistoryText(TEXT("成绩仅保存在本机当前用户下，不随游戏包分享。仅记录已结算的游玩。"),16)];
   if(bError)Rows->AddSlot().AutoHeight().Padding(0,8)[HistoryText(TEXT("删除失败，请检查本机文件权限后重试。"))];
   if(Invalid)Rows->AddSlot().AutoHeight().Padding(0,8)[HistoryText(TEXT("部分记录损坏，已跳过；清空记录可一并移除。"))];
   auto List=SNew(SScrollBox);
   if(Entries.IsEmpty())List->AddSlot()[HistoryText(TEXT("暂无游玩记录。完成撤离或死亡结算后，成绩会自动保存。"))];
   for(const auto& Entry:Entries) {
    List->AddSlot().Padding(0,5)[SNew(SBorder).Padding(12)[SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[HistoryText(Entry.CompletedUtc.ToString(TEXT("%Y-%m-%d  %H:%M:%S UTC")),16)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,5)[HistoryText(Entry.bDeath?TEXT("死亡结算"):TEXT("成功撤离"))]]
     +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(20,0)[HistoryText(FString::Printf(TEXT("%.1f 分"),Entry.Score),24,false)]
     +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HistoryButton(TEXT("删除此记录"),[this,Entry]{Confirm(Entry.FileName);})]]];
   }
   Rows->AddSlot().FillHeight(1)[List];
   if(Entries.Num()||Invalid)Rows->AddSlot().AutoHeight().Padding(0,12)[HistoryButton(TEXT("清空全部记录"),[this]{Confirm(FString());})];
   Rows->AddSlot().AutoHeight().Padding(0,8)[HistoryButton(TEXT("返回"),[this]{OnClose();})];Host->SetContent(Rows);
  }
  void Confirm(FString File) {
   auto Rows=SNew(SVerticalBox);
   Rows->AddSlot().AutoHeight().Padding(0,20)[HistoryText(File.IsEmpty()?TEXT("确定清空本机全部游玩记录？此操作无法撤销。"):TEXT("确定删除这条游玩记录？此操作无法撤销。"),24)];
   Rows->AddSlot().AutoHeight().Padding(0,12)[HistoryText(TEXT("只删除成绩记录，不删除录像文件。"))];
   Rows->AddSlot().AutoHeight().Padding(0,12)[HistoryButton(TEXT("确认删除"),[this,File]{bError=!(File.IsEmpty()?Store.Clear():Store.Delete(File));Refresh();})];
   Rows->AddSlot().AutoHeight()[HistoryButton(TEXT("取消"),[this]{Refresh();})];Host->SetContent(Rows);
  }
 };
}
TSharedRef<SWidget> GameFrontend::RunHistory(TFunction<void()> Close) {return SNew(SRunHistory,MoveTemp(Close));}
