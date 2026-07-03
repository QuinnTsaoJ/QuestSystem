// 任务组件实现
// QuestSystem Plugin

#include "QuestComponent.h"
#include "QuestManager.h"
#include "QuestDefinition.h"
#include "QuestInstance.h"
#include "QuestSaveData.h"
#include "Rewards/QuestReward.h"
#include "Objectives/QuestObjective.h"
#include "Conditions/QuestConditionBase.h"
#include "Interfaces/QuestInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

UQuestComponent::UQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UQuestComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	// 注册到 Manager
	if (UQuestManager* Manager = GetQuestManager())
	{
		Manager->RegisterQuestComponent(this);
	}
}

void UQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 从 Manager 注销
	if (UQuestManager* Manager = GetQuestManager())
	{
		Manager->UnregisterQuestComponent(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UQuestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CheckTimeouts(DeltaTime);
}

UQuestManager* UQuestComponent::GetQuestManager() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameInstance()->GetSubsystem<UQuestManager>() : nullptr;
}

// ============ 接取 ============
bool UQuestComponent::AcceptQuest(FName QuestID, FText& OutReason)
{
	UQuestManager* Manager = GetQuestManager();
	if (!Manager) return false;

	UQuestDefinition* Def = Manager->GetQuestDefinition(QuestID);
	if (!Def)
	{
		OutReason = FText::FromString(TEXT("任务定义不存在"));
		OnQuestAcceptFailed.Broadcast(QuestID, OutReason);
		return false;
	}
	if (QuestInstances.Contains(QuestID))
	{
		OutReason = FText::FromString(TEXT("任务已接取"));
		OnQuestAcceptFailed.Broadcast(QuestID, OutReason);
		return false;
	}
	if (CompletedQuestIDs.Contains(QuestID))
	{
		OutReason = FText::FromString(TEXT("任务已完成"));
		OnQuestAcceptFailed.Broadcast(QuestID, OutReason);
		return false;
	}
	if (!CheckAcceptConditions(Def, OutReason))
	{
		OnQuestAcceptFailed.Broadcast(QuestID, OutReason);
		return false;
	}

	UQuestInstance* Inst = CreateQuestInstance(Def);
	if (!Inst) return false;

	Inst->Status = EQuestStatus::Active;
	Inst->AcceptedTime = FDateTime::UtcNow();
	if (Def->TimeLimit > 0)
	{
		Inst->Deadline = Inst->AcceptedTime + FTimespan::FromSeconds(Def->TimeLimit);
	}

	// 目标状态初始化(纯数据目标,无启动虚函数)
	for (UQuestObjective* Obj : Inst->Objectives)
	{
		if (Obj) Obj->Status = EObjectiveStatus::InProgress;
	}

	QuestInstances.Add(QuestID, Inst);
	UnlockedQuestIDs.Remove(QuestID);

	// 触发式加载:接取时沿 NextQuestID 预加载后续任务链定义,使后续任务在解锁时立即可查
	Manager->LoadQuestChain(QuestID);

	OnQuestAdded.Broadcast(Inst);
	BroadcastGuideForQuest(Inst);
	return true;
}

bool UQuestComponent::AbandonQuest(FName QuestID)
{
	UQuestInstance* Inst = GetQuest(QuestID);
	if (!Inst || Inst->Status != EQuestStatus::Active) return false;

	// 回滚目标(纯数据:重置计数与状态)
	Inst->RollbackObjectives();

	Inst->Status = EQuestStatus::Abandoned;
	AbandonedQuestIDs.Add(QuestID);
	QuestInstances.Remove(QuestID);
	OnQuestAbandoned.Broadcast(QuestID);
	return true;
}

bool UQuestComponent::CompleteQuest(FName QuestID)
{
	UQuestInstance* Inst = GetQuest(QuestID);
	if (!Inst || Inst->Status != EQuestStatus::Active) return false;
	if (!Inst->AreAllObjectivesCompleted()) return false;

	// 奖励为纯数据,任务系统不生成物品;游戏层通过订阅 OnQuestCompleted 回调读取 Inst->Rewards 自行发放
	Inst->Status = EQuestStatus::Completed;
	Inst->CompletedTime = FDateTime::UtcNow();
	CompletedQuestIDs.Add(QuestID);

	OnQuestCompleted.Broadcast(Inst);
	TryUnlockNextQuest(Inst);
	return true;
}

bool UQuestComponent::FailQuest(FName QuestID)
{
	UQuestInstance* Inst = GetQuest(QuestID);
	if (!Inst || Inst->Status != EQuestStatus::Active) return false;
	FailQuestInternal(QuestID, FText::FromString(TEXT("手动失败")));
	return true;
}

void UQuestComponent::FailQuestInternal(FName QuestID, const FText& Reason)
{
	UQuestInstance* Inst = GetQuest(QuestID);
	if (!Inst || Inst->Status != EQuestStatus::Active) return;

	// 失败即回滚(与放弃同路径)
	Inst->RollbackObjectives();

	Inst->Status = EQuestStatus::Failed;
	Inst->FailedTime = FDateTime::UtcNow();
	// 不加入任何阻塞集合(允许重试)
	QuestInstances.Remove(QuestID);  // 回滚后进度已清,Instance 移除

	// 广播时 Instance 仍有效(广播后等待 GC)
	OnQuestFailed.Broadcast(Inst);
}

// ============ 查询 ============
UQuestInstance* UQuestComponent::GetQuest(FName QuestID) const
{
	const TObjectPtr<UQuestInstance>* Found = QuestInstances.Find(QuestID);
	return Found ? Found->Get() : nullptr;
}

bool UQuestComponent::IsQuestAccepted(FName QuestID) const
{
	return QuestInstances.Contains(QuestID);
}

bool UQuestComponent::IsQuestCompleted(FName QuestID) const
{
	return CompletedQuestIDs.Contains(QuestID);
}

TArray<UQuestInstance*> UQuestComponent::GetAllQuests() const
{
	TArray<UQuestInstance*> Result;
	for (const auto& Pair : QuestInstances)
	{
		if (Pair.Value) Result.Add(Pair.Value.Get());
	}
	return Result;
}

void UQuestComponent::GetQuestsByStatus(EQuestStatus Status, TArray<UQuestInstance*>& OutQuests) const
{
	OutQuests.Empty();
	for (const auto& Pair : QuestInstances)
	{
		if (Pair.Value && Pair.Value->Status == Status) OutQuests.Add(Pair.Value);
	}
}

bool UQuestComponent::ArePrerequisitesMet(FName QuestID) const
{
	UQuestManager* Manager = GetQuestManager();
	if (!Manager) return false;
	UQuestDefinition* Def = Manager->GetQuestDefinition(QuestID);
	if (!Def) return false;
	for (FName PrereqID : Def->PrerequisiteQuestIDs)
	{
		if (!CompletedQuestIDs.Contains(PrereqID)) return false;
	}
	return true;
}

TArray<UQuestDefinition*> UQuestComponent::GetAvailableQuests(FGameplayTag GiverTag) const
{
	TArray<UQuestDefinition*> Result;
	UQuestManager* Manager = GetQuestManager();
	if (!Manager) return Result;

	for (UQuestDefinition* Def : Manager->GetQuestsByGiver(GiverTag))
	{
		if (!Def) continue;
		// 已接取 / 已完成 → 不可接
		if (QuestInstances.Contains(Def->QuestID)) continue;
		if (CompletedQuestIDs.Contains(Def->QuestID)) continue;
		// 前置未满足 → 不可接
		if (!ArePrerequisitesMet(Def->QuestID)) continue;
		Result.Add(Def);
	}
	return Result;
}

// ============ UI 数据 ============
TArray<UQuestInstance*> UQuestComponent::GetActiveQuests() const
{
	TArray<UQuestInstance*> Result;
	for (const auto& Pair : QuestInstances)
	{
		if (Pair.Value && Pair.Value->Status == EQuestStatus::Active) Result.Add(Pair.Value);
	}
	return Result;
}

TArray<UQuestInstance*> UQuestComponent::GetCompletedQuests() const
{
	TArray<UQuestInstance*> Result;
	for (const auto& Pair : QuestInstances)
	{
		if (Pair.Value && Pair.Value->Status == EQuestStatus::Completed) Result.Add(Pair.Value);
	}
	return Result;
}

FQuestViewData UQuestComponent::GetQuestViewData(FName QuestID) const
{
	FQuestViewData ViewData;
	ViewData.QuestID = QuestID;
	UQuestInstance* Inst = GetQuest(QuestID);
	if (Inst && Inst->Definition)
	{
		ViewData.DisplayName = Inst->Definition->DisplayName;
		ViewData.Description = Inst->Definition->Description;
		ViewData.Status = Inst->Status;
		for (UQuestObjective* Obj : Inst->Objectives)
		{
			if (!Obj) continue;
			// 调用目标的 GetProgressText(默认实现走纯数据;子类 override 可自定义展示)
			ViewData.ObjectiveTexts.Add(Obj->GetProgressText());
		}
		for (const FQuestReward& Rew : Inst->Rewards)
		{
			ViewData.RewardTexts.Add(FText::FromString(
				FString::Printf(TEXT("%s ×%d"), *Rew.ItemRowID.ToString(), Rew.Amount)));
		}
		if (Inst->Deadline.GetTicks() > 0)
		{
			int32 Remaining = FMath::Max(0, (int32)(Inst->Deadline - FDateTime::UtcNow()).GetTotalSeconds());
			ViewData.RemainingSeconds = Remaining;
		}
	}
	return ViewData;
}

// ============ 指引 ============
TArray<FQuestGuideData> UQuestComponent::GetAllStaticGuides() const
{
	TArray<FQuestGuideData> Result;
	for (const auto& Pair : QuestInstances)
	{
		UQuestInstance* Inst = Pair.Value;
		if (!Inst || Inst->Status != EQuestStatus::Active || !Inst->Definition) continue;
		if (!Inst->Definition->bShowHUDGuide) continue;

		FQuestGuideData GuideData;
		GuideData.QuestID = Pair.Key;
		GuideData.bShowHUDArrow = Inst->Definition->bShowHUDGuide;
		// 指引点数据由游戏层自行维护(目标不再内置坐标/图标字段)
		if (GuideData.bShowHUDArrow) Result.Add(GuideData);
	}
	return Result;
}

// ============ 存档 ============
FQuestSaveData UQuestComponent::SaveToData() const
{
	FQuestSaveData Data;
	for (const auto& Pair : QuestInstances)
	{
		if (!Pair.Value || Pair.Value->Status != EQuestStatus::Active) continue;
		UQuestInstance* Inst = Pair.Value;
		FQuestInstanceSaveData& InstData = Data.ActiveQuests.AddDefaulted_GetRef();
		InstData.QuestID = Pair.Key;
		InstData.Status = static_cast<uint8>(Inst->Status);
		InstData.AcceptedTimeTicks = FString::FromInt(Inst->AcceptedTime.GetTicks());
		InstData.DeadlineTicks = Inst->Deadline.GetTicks() > 0
			? FString::FromInt(Inst->Deadline.GetTicks()) : TEXT("");

		for (UQuestObjective* Obj : Inst->Objectives)
		{
			FObjectiveSaveData& ObjData = InstData.Objectives.AddDefaulted_GetRef();
			ObjData.Status = Obj ? static_cast<uint8>(Obj->Status) : 0;
			if (Obj)
			{
				ObjData.CurrentProgress = Obj->CurrentCount;
			}
		}
	}
	Data.CompletedQuestIDs = CompletedQuestIDs.Array();
	Data.AbandonedQuestIDs = AbandonedQuestIDs.Array();
	Data.UnlockedQuestIDs = UnlockedQuestIDs.Array();
	Data.EventTagStats = EventTagStats;
	Data.TargetTagStats = TargetTagStats;
	return Data;
}

void UQuestComponent::LoadFromData(const FQuestSaveData& Data)
{
	CompletedQuestIDs = TSet<FName>(Data.CompletedQuestIDs);
	AbandonedQuestIDs = TSet<FName>(Data.AbandonedQuestIDs);
	UnlockedQuestIDs = TSet<FName>(Data.UnlockedQuestIDs);
	EventTagStats = Data.EventTagStats;
	TargetTagStats = Data.TargetTagStats;

	QuestInstances.Empty();
	UQuestManager* Manager = GetQuestManager();

	for (const FQuestInstanceSaveData& InstData : Data.ActiveQuests)
	{
		if (!Manager) continue;
		UQuestDefinition* Def = Manager->GetQuestDefinition(InstData.QuestID);
		if (!Def) continue;  // 定义已移除,跳过

		UQuestInstance* Inst = CreateQuestInstance(Def);
		if (!Inst) continue;

		Inst->Status = static_cast<EQuestStatus>(InstData.Status);
		Inst->AcceptedTime = FDateTime(FCString::Atoi64(*InstData.AcceptedTimeTicks));
		Inst->Deadline = InstData.DeadlineTicks.IsEmpty()
			? FDateTime() : FDateTime(FCString::Atoi64(*InstData.DeadlineTicks));

		// 恢复目标进度(纯数据:直接写字段)
		for (int32 i = 0; i < Inst->Objectives.Num() && i < InstData.Objectives.Num(); ++i)
		{
			UQuestObjective* Obj = Inst->Objectives[i];
			const FObjectiveSaveData& ObjData = InstData.Objectives[i];
			if (!Obj) continue;
			Obj->Status = static_cast<EObjectiveStatus>(ObjData.Status);
			Obj->CurrentCount = ObjData.CurrentProgress;
		}

		QuestInstances.Add(InstData.QuestID, Inst);
	}
}

// ============ 事件处理(统一判定) ============
void UQuestComponent::HandleEvent(const FQuestEventPayload& Payload)
{
	// 记录事件统计(供玩家查看游戏数据/成就系统用,与任务判定无关,无条件记录)
	RecordEventStats(Payload);

	for (const auto& Pair : QuestInstances)
	{
		UQuestInstance* Inst = Pair.Value;
		if (!Inst || Inst->Status != EQuestStatus::Active) continue;

		for (UQuestObjective* Obj : Inst->Objectives)
		{
			if (!Obj || Obj->Status == EObjectiveStatus::Completed) continue;

			// 纯持续型目标(bRespondToEvents=false)跳过事件处理,只靠 Tick 推进
			if (!Obj->bRespondToEvents) continue;

			if (Obj->ListenEvents.IsEmpty()) continue;
			// 1. 事件类型过滤:ListenEvents 必须包含 Payload.EventTag
			if (!Obj->ListenEvents.HasTag(Payload.EventTag)) continue;

			// 2. 目标 Tag 匹配:TargetTags 为空=通配;非空=Payload.TargetTag 命中容器任一
			if (Obj->TargetTags.IsValid() && !Obj->TargetTags.HasTag(Payload.TargetTag)) continue;

			// 3. 调用目标的判定入口(默认实现走纯数据逻辑;子类 override 可自定义复杂判定)
			UObject* Owner = GetOwner();
			if (Obj->OnEventReceived(Payload, Owner))
			{
				NotifyObjectiveProgressChanged(Obj);
			}
		}
	}
}

// ============ Objective 回调 ============
int32 UQuestComponent::FindObjectiveIndex(UQuestObjective* Objective) const
{
	for (const auto& Pair : QuestInstances)
	{
		if (!Pair.Value) continue;
		int32 Idx = Pair.Value->Objectives.IndexOfByKey(Objective);
		if (Idx != INDEX_NONE) return Idx;
	}
	return INDEX_NONE;
}

void UQuestComponent::NotifyObjectiveCompleted(UQuestObjective* Objective)
{
	if (!Objective || !Objective->OwningQuest) return;
	UQuestInstance* Inst = Objective->OwningQuest;
	int32 Idx = FindObjectiveIndex(Objective);
	OnObjectiveCompleted.Broadcast(Inst, Idx);
	OnQuestUpdated.Broadcast(Inst);

	// 全部目标完成 → 自动 CompleteQuest
	if (Inst->AreAllObjectivesCompleted())
	{
		CompleteQuest(Inst->Definition ? Inst->Definition->QuestID : NAME_None);
	}
}

void UQuestComponent::NotifyObjectiveFailed(UQuestObjective* Objective)
{
	if (!Objective || !Objective->OwningQuest) return;
	UQuestInstance* Inst = Objective->OwningQuest;
	// 按 bAutoFailOnObjectiveFailure 决定是否 FailQuest
	if (Inst->Definition && Inst->Definition->bAutoFailOnObjectiveFailure)
	{
		FailQuestInternal(Inst->Definition->QuestID, FText::FromString(TEXT("目标失败")));
	}
	else
	{
		OnQuestUpdated.Broadcast(Inst);
	}
}

void UQuestComponent::NotifyObjectiveProgressChanged(UQuestObjective* Objective)
{
	if (!Objective || !Objective->OwningQuest) return;
	OnQuestUpdated.Broadcast(Objective->OwningQuest);
}

// ============ 内部辅助 ============
UQuestInstance* UQuestComponent::CreateQuestInstance(UQuestDefinition* Definition)
{
	if (!Definition) return nullptr;
	UQuestInstance* Inst = NewObject<UQuestInstance>(this);
	Inst->InitializeFromDefinition(Definition, this);
	return Inst;
}

bool UQuestComponent::CheckAcceptConditions(UQuestDefinition* Def, FText& OutReason) const
{
	if (!Def) return false;
	// 1. 前置任务(任务系统内置)
	if (!ArePrerequisitesMet(Def->QuestID))
	{
		OutReason = FText::FromString(TEXT("前置任务未完成"));
		return false;
	}
	// 2. 自定义条件
	for (const TSubclassOf<UQuestConditionBase>& CondClass : Def->ConditionClasses)
	{
		if (!CondClass) continue;
		UQuestConditionBase* Cond = NewObject<UQuestConditionBase>(
			const_cast<UQuestComponent*>(this), CondClass);
		FText Reason;
		if (!Cond->Evaluate(const_cast<UQuestComponent*>(this), Reason))
		{
			OutReason = Reason;
			return false;
		}
	}
	return true;
}

void UQuestComponent::CheckTimeouts(float DeltaTime)
{
	FDateTime Now = FDateTime::UtcNow();
	TArray<FName> TimedOutQuests;
	for (const auto& Pair : QuestInstances)
	{
		UQuestInstance* Inst = Pair.Value;
		if (!Inst || Inst->Status != EQuestStatus::Active) continue;
		if (Inst->Deadline.GetTicks() <= 0) continue;  // 无时限
		if (Now >= Inst->Deadline)
		{
			TimedOutQuests.Add(Pair.Key);
		}
	}
	for (FName QuestID : TimedOutQuests)
	{
		FailQuestInternal(QuestID, FText::FromString(TEXT("任务超时")));
	}
}

void UQuestComponent::TryUnlockNextQuest(UQuestInstance* CompletedQuest)
{
	if (!CompletedQuest || !CompletedQuest->Definition) return;
	FName NextID = CompletedQuest->Definition->NextQuestID;
	if (NextID.IsNone()) return;
	UnlockedQuestIDs.Add(NextID);
	OnQuestUnlocked.Broadcast(NextID);
}

void UQuestComponent::BroadcastGuideForQuest(UQuestInstance* Quest)
{
	if (!Quest || !Quest->Definition) return;
	FQuestGuideData GuideData;
	GuideData.QuestID = Quest->Definition->QuestID;
	GuideData.bShowHUDArrow = Quest->Definition->bShowHUDGuide;
	// 指引点数据由游戏层自行维护(目标不再内置坐标/图标字段)
	OnQuestGuideUpdated.Broadcast(GuideData);
}

// ============ 事件统计 ============
int32 UQuestComponent::GetEventTagCount(FGameplayTag EventTag) const
{
	for (const FEventTagStat& Stat : EventTagStats)
	{
		if (Stat.EventTag == EventTag) return Stat.Count;
	}
	return 0;
}

int32 UQuestComponent::GetTargetTagCount(FGameplayTag TargetTag) const
{
	for (const FTargetTagStat& Stat : TargetTagStats)
	{
		if (Stat.TargetTag == TargetTag) return Stat.Count;
	}
	return 0;
}

const TArray<FEventTagStat>& UQuestComponent::GetAllEventTagStats() const
{
	return EventTagStats;
}

const TArray<FTargetTagStat>& UQuestComponent::GetAllTargetTagStats() const
{
	return TargetTagStats;
}

void UQuestComponent::RecordEventStats(const FQuestEventPayload& Payload)
{
	// 按 EventTag 累加 Payload.Amount(无任务监听也记录,供游戏数据/成就系统用)
	if (Payload.EventTag.IsValid())
	{
		bool bFound = false;
		for (FEventTagStat& Stat : EventTagStats)
		{
			if (Stat.EventTag == Payload.EventTag)
			{
				Stat.Count += Payload.Amount;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			FEventTagStat& NewStat = EventTagStats.AddDefaulted_GetRef();
			NewStat.EventTag = Payload.EventTag;
			NewStat.Count = Payload.Amount;
		}
	}

	// 按 TargetTag 累加 Payload.Amount
	if (Payload.TargetTag.IsValid())
	{
		bool bFound = false;
		for (FTargetTagStat& Stat : TargetTagStats)
		{
			if (Stat.TargetTag == Payload.TargetTag)
			{
				Stat.Count += Payload.Amount;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			FTargetTagStat& NewStat = TargetTagStats.AddDefaulted_GetRef();
			NewStat.TargetTag = Payload.TargetTag;
			NewStat.Count = Payload.Amount;
		}
	}
}
