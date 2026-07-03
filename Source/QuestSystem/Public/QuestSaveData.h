// 任务存档数据结构 —— 用于 Save/Load,游戏层负责持久化
// 与 InventorySystem 的 FInventorySaveData 风格一致
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuestSaveData.generated.h"

// 单个目标的运行时进度快照
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FObjectiveSaveData
{
	GENERATED_BODY()

	// 目标状态(EObjectiveStatus 转 uint8)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	uint8 Status = 0;

	// 当前进度计数(计数型目标用)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	int32 CurrentProgress = 0;
};

// 单个任务实例的存档快照
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestInstanceSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	FName QuestID;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	uint8 Status = 0;

	// 各目标进度快照(顺序与 Definition->ObjectiveClasses 对应)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	TArray<FObjectiveSaveData> Objectives;

	// 时间戳(FDateTime.GetTicks() 转 FString,跨版本兼容)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	FString AcceptedTimeTicks;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	FString CompletedTimeTicks;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	FString FailedTimeTicks;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	FString DeadlineTicks;
};

// 单个事件 Tag 的累积统计
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FEventTagStat
{
	GENERATED_BODY()

	/** 事件 Tag(如 任务插件.事件.击杀) */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Stats")
	FGameplayTag EventTag;

	/** 累积次数(含 Amount 累加,如击杀 5 只每次 Amount=1 则=5;拾取 3 个 Amount=3 则=3) */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Stats")
	int32 Count = 0;
};

// 单个目标 Tag 的累积统计
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FTargetTagStat
{
	GENERATED_BODY()

	/** 目标 Tag(如 任务插件.目标.敌人.野兽) */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Stats")
	FGameplayTag TargetTag;

	/** 累积次数 */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Stats")
	int32 Count = 0;
};

// 完整存档(对应一个玩家的全部任务状态)
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestSaveData
{
	GENERATED_BODY()

	// 进行中任务实例快照
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	TArray<FQuestInstanceSaveData> ActiveQuests;

	// 已完成任务 ID(只存 ID,不存 Instance,省空间)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	TArray<FName> CompletedQuestIDs;

	// 已放弃任务 ID(用于判断是否曾经放弃,默认允许重接)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	TArray<FName> AbandonedQuestIDs;

	// 已解锁但未接取的任务 ID(任务链解锁状态)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Save")
	TArray<FName> UnlockedQuestIDs;

	// === 事件统计(玩家游戏数据,供查看/成就系统用) ===
	// 按事件 Tag 累积统计(如 击杀事件总计触发多少次)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Stats")
	TArray<FEventTagStat> EventTagStats;

	// 按目标 Tag 累积统计(如 杀了多少只野兽、拾取了多少草药)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest|Stats")
	TArray<FTargetTagStat> TargetTagStats;

	// 注:无 FailedQuestIDs —— 失败即回滚+移除,不持久化
};
