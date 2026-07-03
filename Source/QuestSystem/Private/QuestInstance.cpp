// 任务实例实现
// QuestSystem Plugin

#include "QuestInstance.h"
#include "QuestDefinition.h"
#include "Objectives/QuestObjective.h"

void UQuestInstance::InitializeFromDefinition(UQuestDefinition* InDefinition, UQuestComponent* InOwningComponent)
{
	Definition = InDefinition;
	Status = EQuestStatus::Inactive;
	Objectives.Empty();
	Rewards.Empty();

	if (!InDefinition) return;

	// 实例化目标
	for (const TSubclassOf<UQuestObjective>& ObjClass : InDefinition->ObjectiveClasses)
	{
		if (!ObjClass) continue;
		UQuestObjective* Obj = NewObject<UQuestObjective>(this, ObjClass);
		Obj->OwningQuest = this;
		Obj->OwningComponent = InOwningComponent;
		Objectives.Add(Obj);
	}

	// 奖励为纯数据,直接复制(任务系统不生成物品,由游戏层在完成回调中解释发放)
	Rewards = InDefinition->Rewards;
}

void UQuestInstance::RollbackObjectives()
{
	for (UQuestObjective* Obj : Objectives)
	{
		if (Obj)
		{
			Obj->CurrentCount = 0;
			Obj->Status = EObjectiveStatus::NotStarted;
		}
	}
}

bool UQuestInstance::AreAllObjectivesCompleted() const
{
	if (Objectives.Num() == 0) return false;
	for (const UQuestObjective* Obj : Objectives)
	{
		if (!Obj || Obj->Status != EObjectiveStatus::Completed)
		{
			return false;
		}
	}
	return true;
}

bool UQuestInstance::IsTimedOut() const
{
	if (Deadline.GetTicks() <= 0) return false;
	return FDateTime::UtcNow() >= Deadline;
}
