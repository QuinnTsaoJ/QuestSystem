// 任务实例 —— 运行时数据,每玩家接取时创建
// 持有状态、时间戳、实例化的目标与奖励数据(纯结构体)
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.h"
#include "Rewards/QuestReward.h"
#include "QuestInstance.generated.h"

class UQuestDefinition;
class UQuestObjective;
class UQuestComponent;

UCLASS(BlueprintType)
class QUESTSYSTEM_API UQuestInstance : public UObject
{
	GENERATED_BODY()

public:
	// === 关联配置 ===
	/** 任务定义(接取时绑定,提供静态配置) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	TObjectPtr<UQuestDefinition> Definition;

	// === 运行时状态 ===
	/** 任务当前状态(Inactive→Active→Completed/Failed/Abandoned) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	EQuestStatus Status = EQuestStatus::Inactive;

	/** 实例化的目标列表(每玩家独立进度,接取时从 Definition.ObjectiveClasses 实例化) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	TArray<TObjectPtr<UQuestObjective>> Objectives;

	/** 奖励列表(纯数据,从 Definition 复制;任务系统只记录与通知,不生成物品,由游戏层在 OnQuestCompleted 回调中发放) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	TArray<FQuestReward> Rewards;

	// === 时间戳(绝对时刻,存档用) ===
	/** 接取时刻(存档用) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	FDateTime AcceptedTime;

	/** 完成时刻(存档用) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	FDateTime CompletedTime;

	/** 失败时刻(存档用) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	FDateTime FailedTime;

	/** 截止时刻 = AcceptedTime + TimeLimit,无时限留空(Ticks <= 0)。Tick 检查超时用 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest|Instance")
	FDateTime Deadline;

	// === 查询 ===
	/** 任务是否进行中 */
	UFUNCTION(BlueprintPure, Category="Quest|Instance")
	bool IsActive() const { return Status == EQuestStatus::Active; }

	/** 任务是否已完成 */
	UFUNCTION(BlueprintPure, Category="Quest|Instance")
	bool IsCompleted() const { return Status == EQuestStatus::Completed; }

	/** 所有目标是否全部完成(供 Component 判断是否可 CompleteQuest) */
	UFUNCTION(BlueprintPure, Category="Quest|Instance")
	bool AreAllObjectivesCompleted() const;

	/** 是否超时(Deadline 有效且当前时间已过) */
	UFUNCTION(BlueprintPure, Category="Quest|Instance")
	bool IsTimedOut() const;

	// === 内部调用(Component 在接取/放弃/失败时调用) ===
	/** 用 Definition 的 ObjectiveClasses 实例化目标,复制 Rewards(纯数据深拷贝) */
	void InitializeFromDefinition(UQuestDefinition* InDefinition, UQuestComponent* InOwningComponent);

	/** 回滚全部目标进度:重置 CurrentCount=0 + Status=NotStarted(放弃/失败时调用) */
	void RollbackObjectives();
};
