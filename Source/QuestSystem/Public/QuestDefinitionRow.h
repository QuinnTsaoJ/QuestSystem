// 任务定义数据表行 —— 用于 DataTable 批量配置任务
// 字段镜像 UQuestDefinition,游戏层创建 DataTable(FQuestDefinitionRow)后,
// 调 QuestManager::RegisterQuestDefinitionsFromDataTable 批量注册。
// 与 UPrimaryDataAsset 配置方式并存,游戏层按需选择:
//   - 任务多、需表格化管理 → 用 DataTable
//   - 任务少、需资产引用 → 用 UQuestDefinition 资产
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Rewards/QuestReward.h"
#include "QuestDefinitionRow.generated.h"

class UQuestObjective;
class UQuestConditionBase;

// 任务定义数据表行(一行 = 一个任务定义)
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()	
public:
	// === 标识 ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Identity")
	FName QuestID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Identity")
	FText Description;

	// === 给予者 ===
	/** 任务给予者(NPC)的 GameplayTag。留空=任意处可接;填具体 tag 后,游戏层可据此列出该给予者可提供的任务 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Giver",
		meta=(ToolTip="任务给予者(NPC)的 GameplayTag。留空=任意处可接;填具体 tag 后,游戏层可据此列出该给予者可提供的任务"))
	FGameplayTag GiverTag;

	// === 任务链 ===
	// 主链下一个任务(空 NAME_None = 链终点)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Chain")
	FName NextQuestID;

	// 前置任务(AND 语义,全完成才能接),支持分支汇聚
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Chain")
	TArray<FName> PrerequisiteQuestIDs;

	// === 内容(接取时实例化,每玩家独立进度) ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Content",
		meta=(TitleProperty="Objective"))
	TArray<TSubclassOf<UQuestObjective>> ObjectiveClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Content",
		meta=(TitleProperty="ItemRowID"))
	TArray<FQuestReward> Rewards;

	// === 接取条件(等级/声望/职业等,接取时实例化检查) ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Conditions",
		meta=(TitleProperty="Condition"))
	TArray<TSubclassOf<UQuestConditionBase>> ConditionClasses;

	// === 失败/超时 ===
	// 限时秒数,-1 表示无时限
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Failure")
	int32 TimeLimit = -1;

	// 某目标失败即任务失败(护送 NPC 死亡等场景)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Failure")
	bool bAutoFailOnObjectiveFailure = false;

	// === 指引 ===
	// 任务级 HUD 指引总开关,false 时 Component 不生成该任务指引数据
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Guide")
	bool bShowHUDGuide = true;
};
