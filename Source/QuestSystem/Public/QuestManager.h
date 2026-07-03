// 任务管理器 —— 全局服务(UGameInstanceSubsystem)
// 无状态:不持有玩家任务实例,只管定义表 + 事件总线 + Component 注册
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "QuestManager.generated.h"

struct FQuestEventPayload;
class UQuestDefinition;
class UQuestComponent;
class UDataTable;
class APlayerController;

// 任务定义注册广播。Definition:新注册的任务定义(编辑器/蓝图可见)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestDefinitionRegistered, UQuestDefinition*, Definition);
// 任务定义注销广播。Definition:被注销的任务定义
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestDefinitionUnregistered, UQuestDefinition*, Definition);

UCLASS()
class QUESTSYSTEM_API UQuestManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// === Subsystem 生命周期 ===
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// === 定义表管理 ===
	/** 注册单个任务定义(资产方式)。重复 QuestID 覆盖旧定义,广播 OnQuestDefinitionRegistered */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	void RegisterQuestDefinition(UQuestDefinition* Definition);

	/** 注销任务定义(从缓存移除),广播 OnQuestDefinitionUnregistered */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	void UnregisterQuestDefinition(UQuestDefinition* Definition);

	/** 从数据表批量注册任务定义(索引化,不构造对象)。遍历每行建立 QuestID→行名 索引。
	 *  游戏层创建 DataTable(FQuestDefinitionRow)后调用此接口一次即可注册全部任务。
	 *  定义对象在接取/查询时按需构造(触发式加载)。
	 *  @param DataTable 任务定义数据表(行结构为 FQuestDefinitionRow)
	 *  @return 成功索引的任务数量(跳过 QuestID 为空或重复的行) */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	int32 RegisterQuestDefinitionsFromDataTable(UDataTable* DataTable);

	/** 查询任务定义。未命中缓存时自动按需加载(触发式)。不存在返回 nullptr */
	UFUNCTION(BlueprintPure, Category="Quest|Manager")
	UQuestDefinition* GetQuestDefinition(FName QuestID) const;

	/** 获取所有已加载的任务定义(只含被加载过的,不含未触发的) */
	UFUNCTION(BlueprintPure, Category="Quest|Manager")
	TArray<UQuestDefinition*> GetAllQuestDefinitions() const;

	/** 查询指定给予者(NPC)提供的所有任务定义。GiverTag 留空时返回 GiverTag 也为空(任意处可接)的任务 */
	UFUNCTION(BlueprintPure, Category="Quest|Manager")
	TArray<UQuestDefinition*> GetQuestsByGiver(FGameplayTag GiverTag) const;

	// === 按需加载(触发式定义构造) ===

	/** 按需加载单个任务定义。查索引→找行→构造 UQuestDefinition→入 DefinitionTable。
	 *  已加载则直接返回(短路)。未注册返回 nullptr。
	 *  注意:本方法只加载单个定义,不连带加载同 Giver 任务或后续链(避免在只读查询中扩散加载)。 */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	UQuestDefinition* LoadQuestDefinition(FName QuestID);

	/** 批量加载指定给予者(GiverTag)的所有任务定义。
	 *  遍历 QuestRowMap 找同 Giver 的行,逐个构造入 DefinitionTable。
	 *  用于接取任务时连带加载同 NPC 的全部任务,使 GetAvailableQuests 直接查内存。 */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	int32 LoadQuestsByGiver(FGameplayTag GiverTag);

	/** 沿 NextQuestID 递归预加载任务链上所有任务定义。
	 *  每个链上任务只加载自身定义,不连带其 Giver(防级联扩散)。
	 *  @param QuestID 链起点任务 ID
	 *  @param MaxDepth 最大递归深度(默认 32,防循环引用) */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	int32 LoadQuestChain(FName QuestID, int32 MaxDepth = 32);

	// === 触发便捷入口(供触发 Actor/碰撞箱等游戏层调用,封装"找 Component"胶水) ===

	/** 为指定玩家接取任务。内部查找 Player 上的 QuestComponent 并调用 AcceptQuest,省去调用方自己找组件。
	 *  @param Player 目标玩家控制器
	 *  @param QuestID 任务 ID
	 *  @param OutReason 失败时返回原因
	 *  @return 是否接取成功 */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager", meta=(AutoCreateRefTerm="OutReason"))
	bool TryAcceptQuest(APlayerController* Player, FName QuestID, FText& OutReason);

	// === 事件总线 ===
	/** 外部系统通知任务系统"发生了某事"的主入口。遍历所有已注册 Component 转发给匹配的 Objective。
	 *  @param Payload 事件载荷(EventTag=事件类型,TargetTag=目标标识,Amount=数量) */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager",
		meta=(AutoCreateRefTerm="Payload"))
	void NotifyEvent(const FQuestEventPayload& Payload);

	/** 直接通知特定玩家 Component(已拿到 Component 引用时用,省一次全量广播) */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager",
		meta=(AutoCreateRefTerm="Payload"))
	void NotifyEventToPlayer(UQuestComponent* Component, const FQuestEventPayload& Payload);

	// === Component 注册 ===
	void RegisterQuestComponent(UQuestComponent* Component);
	void UnregisterQuestComponent(UQuestComponent* Component);
	const TArray<TWeakObjectPtr<UQuestComponent>>& GetRegisteredComponents() const
	{
		return RegisteredComponents;
	}

	// === 全局广播委托 ===
	/** 任务定义注册时触发(资产方式注册或按需加载首次构造时)。Definition:新注册的定义 */
	UPROPERTY(BlueprintAssignable, Category="Quest|Manager|Events",
		meta=(ToolTip="任务定义注册时触发。Definition:新注册的任务定义"))
	FOnQuestDefinitionRegistered OnQuestDefinitionRegistered;

	/** 任务定义注销时触发。Definition:被注销的定义 */
	UPROPERTY(BlueprintAssignable, Category="Quest|Manager|Events",
		meta=(ToolTip="任务定义注销时触发。Definition:被注销的任务定义"))
	FOnQuestDefinitionUnregistered OnQuestDefinitionUnregistered;

private:
	// === 定义源索引(注册时建立,轻量,不持有行数据对象) ===
	// QuestID → 所属 DataTable(用于按行名反查行数据)
	UPROPERTY()
	TMap<FName, TObjectPtr<UDataTable>> QuestDataSource;

	// QuestID → 行名(轻量索引,不触发行数据读取)
	TMap<FName, FName> QuestRowMap;

	// === 已按需构造的定义(热数据,只含被加载过的) ===
	UPROPERTY()
	TMap<FName, TObjectPtr<UQuestDefinition>> DefinitionTable;

	// 已注册的玩家 Component(事件总线分发目标)
	TArray<TWeakObjectPtr<UQuestComponent>> RegisteredComponents;
};
