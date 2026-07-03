// 任务定义 —— 静态配置,设计师在编辑器配置
// UPrimaryDataAsset 支持资产管理与按需加载
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Rewards/QuestReward.h"
#include "QuestDefinition.generated.h"

class UQuestObjective;
class UQuestConditionBase;

UCLASS(BlueprintType)
class QUESTSYSTEM_API UQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// === 标识 ===
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Identity")
	FName QuestID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Identity")
	FText Description;

	// === 给予者 ===
	/** 任务给予者(NPC)的 GameplayTag。留空=任意处可接;填具体 tag 后,游戏层可据此列出该给予者可提供的任务 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Giver",
		meta=(ToolTip="任务给予者(NPC)的 GameplayTag。留空=任意处可接;填具体 tag 后,游戏层可据此列出该给予者可提供的任务"))
	FGameplayTag GiverTag;

	// === 任务链 ===
	// 主链下一个任务(空 NAME_None = 链终点)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Chain")
	FName NextQuestID;

	// 前置任务(AND 语义,全完成才能接),支持分支汇聚
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Chain")
	TArray<FName> PrerequisiteQuestIDs;

	// === 内容(接取时实例化,每玩家独立进度) ===
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Content",
		meta=(TitleProperty="Objective"))
	TArray<TSubclassOf<UQuestObjective>> ObjectiveClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Content",
		meta=(TitleProperty="ItemRowID"))
	TArray<FQuestReward> Rewards;

	// === 接取条件(等级/声望/职业等,接取时实例化检查) ===
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Conditions",
		meta=(TitleProperty="Condition"))
	TArray<TSubclassOf<UQuestConditionBase>> ConditionClasses;

	// === 失败/超时 ===
	// 限时秒数,-1 表示无时限
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Failure")
	int32 TimeLimit = -1;

	// 某目标失败即任务失败(护送 NPC 死亡等场景)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Failure")
	bool bAutoFailOnObjectiveFailure = false;

	// === 指引 ===
	// 任务级 HUD 指引总开关,false 时 Component 不生成该任务指引数据
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Guide")
	bool bShowHUDGuide = true;

	// === 资产管理(UPrimaryDataAsset 用) ===
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("QuestDefinition"), QuestID);
	}
};
