// 任务目标实现
// QuestSystem Plugin

#include "Objectives/QuestObjective.h"
#include "QuestInstance.h"
#include "QuestComponent.h"
#include "Interfaces/QuestInterface.h"

bool UQuestObjective::OnEventReceived_Implementation(const FQuestEventPayload& Payload, UObject* Owner)
{
	// 默认实现:物品检查 + 计数累加 + 达标判定(纯数据逻辑)

	// 物品检查(RequiredItems 非空时查背包,全部够数则扣除)
	if (RequiredItems.Num() > 0)
	{
		if (!Owner || !Owner->Implements<UQuestInterface>()) return false;

		bool bAllSufficient = true;
		for (const FQuestRequiredItem& Item : RequiredItems)
		{
			int32 HaveCount = IQuestInterface::Execute_GetItemCount(Owner, Item.ItemRowID);
			if (HaveCount < Item.Amount)
			{
				bAllSufficient = false;
				break;
			}
		}
		if (!bAllSufficient) return false;  // 背包不足

		if (bConsumeItems)
		{
			for (const FQuestRequiredItem& Item : RequiredItems)
			{
				IQuestInterface::Execute_RemoveItem(Owner, Item.ItemRowID, Item.Amount);
			}
		}
	}

	// 计数累加
	CurrentCount += Payload.Amount;

	// 达标判定:RequiredCount=0=即完成;>0=计数达标后完成
	if (RequiredCount == 0 || CurrentCount >= RequiredCount)
	{
		MarkCompleted();
	}
	return true;
}

FText UQuestObjective::GetProgressText_Implementation() const
{
	if (RequiredCount > 0)
	{
		return FText::FromString(FString::Printf(
			TEXT("%s %d/%d"), *DisplayName.ToString(), CurrentCount, RequiredCount));
	}
	return Status == EObjectiveStatus::Completed
		? FText::FromString(TEXT("已完成"))
		: DisplayName;
}

void UQuestObjective::MarkCompleted()
{
	if (Status == EObjectiveStatus::Completed) return;
	Status = EObjectiveStatus::Completed;

	if (OwningComponent)
	{
		OwningComponent->NotifyObjectiveCompleted(this);
	}
}

void UQuestObjective::MarkFailed()
{
	if (Status == EObjectiveStatus::Failed) return;
	Status = EObjectiveStatus::Failed;

	if (OwningComponent)
	{
		OwningComponent->NotifyObjectiveFailed(this);
	}
}
