// 玩家任务接口 —— 任务系统与游戏层的唯一硬契约
// 游戏层 PlayerController(或其组件)实现此接口
// 任务系统通过此接口读写玩家资源(发放/扣除物品、查询货币/物品数)并获取玩家的任务组件
// 注意:任务奖励(FQuestReward)为纯数据,任务系统不通过此接口发放,由游戏层在 OnQuestCompleted 回调中自行处理
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "QuestInterface.generated.h"

class UQuestComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UQuestInterface : public UInterface
{
	GENERATED_BODY()
};

class QUESTSYSTEM_API IQuestInterface
{
	GENERATED_BODY()

public:
	/** 发放物品。ItemRowID:物品行 ID(对应游戏层 DataTable 行名,如 "Item.Herb");Amount:数量 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Player")
	void GrantItem(FName ItemRowID, int32 Amount);

	/** 扣除物品(提交任务、放弃收集任务退还等场景)。ItemRowID:物品行 ID;Amount:数量 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Player")
	void RemoveItem(FName ItemRowID, int32 Amount);

	/** 查询当前货币数量(条件子类或 UI 用) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Player")
	int32 GetCurrency(FGameplayTag CurrencyTag) const;

	/** 查询物品持有数量(Deliver 提交前检查背包是否足够)。ItemRowID:物品行 ID;返回当前持有数量 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Player")
	int32 GetItemCount(FName ItemRowID) const;

	/** 获取此玩家控制器上的任务组件。任务系统内部用它定位 Component(如 TryAcceptQuest),游戏层按自身组件挂载方式实现 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Player", meta=(ReturnDisplayName="Quest Component"))
	UQuestComponent* GetQuestComponent() const;
};
