// 奖励结构体 —— 纯数据,任务系统只记录与通知,不生成物品
// 经验/金币/声望等统一用 ItemRowID 表示,具体语义由游戏层在 OnQuestCompleted 回调中解释
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "QuestReward.generated.h"

/**
 * @brief 单条任务奖励(物品行 ID + 数量)。
 *
 * 任务系统不解释 ItemRowID,仅在任务完成时随 UQuestInstance 广播给游戏层。
 * 经验/金币/声望等也统一用 ItemRowID 表达(如 "Exp"),由游戏层按自身 DataTable 查表发放。
 */
USTRUCT(BlueprintType,meta=(TitleProperty="ItemRowID"))
struct QUESTSYSTEM_API FQuestReward
{
	GENERATED_BODY()

	/** 物品行 ID(对应游戏层 DataTable 行名,如 "Item.Herb")。经验/金币等也统一用此字段,如 "Exp"。任务系统不解释,由游戏层回调中自行查表发放。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Quest|Reward",
		meta=(ToolTip="物品行ID,对应游戏层DataTable行名;经验/金币等也统一用ItemRowID表示,如\"Exp\""))
	FName ItemRowID;

	/** 数量 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Quest|Reward")
	int32 Amount = 1;
};
