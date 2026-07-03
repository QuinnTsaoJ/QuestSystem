// 任务组件 —— 玩家代理(UActorComponent,挂 PlayerController)
// 持有玩家任务实例,对外暴露生命周期/查询/UI 数据/存档接口
// 广播业务委托供 UI 模块订阅
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.h"
#include "QuestSaveData.h"
#include "QuestComponent.generated.h"

class UQuestDefinition;
class UQuestInstance;
class UQuestObjective;
class UQuestManager;

// === 业务广播委托 ===

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAdded, UQuestInstance*, Quest);           // 接取任务成功时触发。Quest:新接取的任务实例(状态=Active,目标已启动)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUpdated, UQuestInstance*, Quest);         // 任务进度变化时触发(目标推进/目标完成等)。Quest:进度变化的任务实例
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestCompleted, UQuestInstance*, Quest);       // 完成任务时触发。Quest:已完成的任务实例(状态=Completed,奖励已发放,后续链已解锁)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestFailed, UQuestInstance*, Quest);          // 任务失败时触发(超时/手动失败/目标失败)。Quest:失败的任务实例(已回滚进度,广播后即移除)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAbandoned, FName, QuestID);               // 放弃任务时触发。QuestID:被放弃的任务 ID(已回滚进度,实例已移除,可重新接取)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUnlocked, FName, QuestID);                // 任务链解锁后续任务时触发。QuestID:新解锁可接取的任务 ID
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveCompleted, UQuestInstance*, Quest, int32, ObjectiveIndex);  // 单个目标完成时触发。Quest:所属任务实例;ObjectiveIndex:完成的目标索引(从0起)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestGuideUpdated, const FQuestGuideData&, GuideData);  // 任务指引数据变化时触发。GuideData:该任务的指引点汇总(供 HUD 渲染)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestAcceptFailed, FName, QuestID, const FText&, Reason);  // 接取任务失败时触发。QuestID:接取失败的任务 ID;Reason:失败原因(等级不足/已接取/已完成/前置未满足等)

UCLASS(ClassGroup=Quest, meta=(BlueprintSpawnableComponent))
class QUESTSYSTEM_API UQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestComponent();

	// === ActorComponent 生命周期 ===
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============ 接取/放弃/完成/失败 ============
	/** 接取任务。检查定义存在/未接取/未完成/条件满足后实例化目标并启动,成功广播 OnQuestAdded,失败广播 OnQuestAcceptFailed。
	 *  @param QuestID 任务 ID
	 *  @param OutReason 失败时返回原因文本
	 *  @return 是否接取成功 */
	UFUNCTION(BlueprintCallable, Category="Quest|Lifecycle")
	bool AcceptQuest(FName QuestID, FText& OutReason);

	/** 放弃任务。回滚全部目标进度,移除实例,广播 OnQuestAbandoned,可重新接取。
	 *  @param QuestID 任务 ID
	 *  @return 是否放弃成功(任务不存在或非进行中返回 false) */
	UFUNCTION(BlueprintCallable, Category="Quest|Lifecycle")
	bool AbandonQuest(FName QuestID);

	/** 完成任务。仅当全部目标完成时生效,设置状态、记录时间戳,广播 OnQuestCompleted 并解锁后续链。
	 *  @param QuestID 任务 ID
	 *  @return 是否完成成功(目标未全完成返回 false) */
	UFUNCTION(BlueprintCallable, Category="Quest|Lifecycle")
	bool CompleteQuest(FName QuestID);

	/** 手动失败任务。回滚目标进度,广播 OnQuestFailed,实例移除(允许重试)。
	 *  @param QuestID 任务 ID
	 *  @return 是否失败成功(任务不存在或非进行中返回 false) */
	UFUNCTION(BlueprintCallable, Category="Quest|Lifecycle")
	bool FailQuest(FName QuestID);

	// ============ 查询 ============
	/** 查询指定任务实例(不限状态) */
	UFUNCTION(BlueprintPure, Category="Quest|Query")
	UQuestInstance* GetQuest(FName QuestID) const;

	/** 任务是否已接取(在当前实例中) */
	UFUNCTION(BlueprintPure, Category="Quest|Query")
	bool IsQuestAccepted(FName QuestID) const;

	/** 任务是否已完成(查历史记录) */
	UFUNCTION(BlueprintPure, Category="Quest|Query")
	bool IsQuestCompleted(FName QuestID) const;

	/** 获取玩家持有的全部任务实例(所有状态) */
	UFUNCTION(BlueprintPure, Category="Quest|Query")
	TArray<UQuestInstance*> GetAllQuests() const;

	/** 按状态筛选任务实例 */
	UFUNCTION(BlueprintPure, Category="Quest|Query")
	void GetQuestsByStatus(EQuestStatus Status, TArray<UQuestInstance*>& OutQuests) const;

	/** 检查任务前置是否全部完成(AND 语义) */
	UFUNCTION(BlueprintPure, Category="Quest|Query")
	bool ArePrerequisitesMet(FName QuestID) const;

	/** 查询指定给予者(NPC)当前可接取的任务(未接取 + 未完成 + 前置已满足)。GiverTag 留空时返回任意处可接的任务 */
	UFUNCTION(BlueprintPure, Category="Quest|Query")
	TArray<UQuestDefinition*> GetAvailableQuests(FGameplayTag GiverTag) const;

	// ============ UI 数据拉取 ============
	/** 获取所有进行中的任务实例(供任务列表 UI 初始化) */
	UFUNCTION(BlueprintCallable, Category="Quest|UI")
	TArray<UQuestInstance*> GetActiveQuests() const;

	/** 获取所有已完成的任务实例(供历史记录 UI) */
	UFUNCTION(BlueprintCallable, Category="Quest|UI")
	TArray<UQuestInstance*> GetCompletedQuests() const;

	/** 获取任务详情视图数据(含目标进度文本/奖励文本/剩余时间,供详情面板 UI) */
	UFUNCTION(BlueprintCallable, Category="Quest|UI")
	FQuestViewData GetQuestViewData(FName QuestID) const;

	// ============ 指引数据 ============
	/** 获取所有进行中任务的静态指引点汇总(供 HUD 渲染地图标记/指引箭头) */
	UFUNCTION(BlueprintCallable, Category="Quest|Guide")
	TArray<FQuestGuideData> GetAllStaticGuides() const;

	// ============ 存档 ============
	/** 序列化玩家任务状态为存档数据(游戏层负责持久化到 USaveGame) */
	UFUNCTION(BlueprintCallable, Category="Quest|Save")
	FQuestSaveData SaveToData() const;

	/** 从存档数据恢复任务状态(重建实例、恢复目标进度) */
	UFUNCTION(BlueprintCallable, Category="Quest|Save")
	void LoadFromData(const FQuestSaveData& Data);

	// ============ 事件统计(玩家游戏数据) ============
	/** 查询指定事件 Tag 的累积次数(如"总共击杀多少次")。未记录返回 0 */
	UFUNCTION(BlueprintPure, Category="Quest|Stats")
	int32 GetEventTagCount(FGameplayTag EventTag) const;

	/** 查询指定目标 Tag 的累积次数(如"杀了多少只野兽")。未记录返回 0 */
	UFUNCTION(BlueprintPure, Category="Quest|Stats")
	int32 GetTargetTagCount(FGameplayTag TargetTag) const;

	/** 获取全部事件 Tag 统计(供"游戏数据"面板/成就系统遍历) */
	UFUNCTION(BlueprintPure, Category="Quest|Stats")
	const TArray<FEventTagStat>& GetAllEventTagStats() const;

	/** 获取全部目标 Tag 统计 */
	UFUNCTION(BlueprintPure, Category="Quest|Stats")
	const TArray<FTargetTagStat>& GetAllTargetTagStats() const;

	// ============ 事件处理(Manager 调用) ============
	/** 处理外部系统上传的事件:遍历活跃任务目标,执行 ListenEvents匹配→TargetTags匹配→物品检查→计数→达标判定。
	 *  由 UQuestManager::NotifyEvent 转发调用,游戏层不直接调 */
	void HandleEvent(const FQuestEventPayload& Payload);

	// ============ 广播委托 ============
	/** 接取任务成功时触发。参数 Quest:新接取的任务实例(状态=Active,目标已启动) */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="接取任务成功时触发。Quest:新接取的任务实例(状态=Active,目标已启动)"))
	FOnQuestAdded OnQuestAdded;

	/** 任务进度变化时触发(目标推进、目标完成等)。参数 Quest:进度变化的任务实例 */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="任务进度变化时触发(目标推进/目标完成等)。Quest:进度变化的任务实例"))
	FOnQuestUpdated OnQuestUpdated;

	/** 完成任务时触发。参数 Quest:已完成的任务实例(状态=Completed,奖励已发放,后续链已解锁) */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="完成任务时触发。Quest:已完成的任务实例(状态=Completed,奖励已发放,后续链已解锁)"))
	FOnQuestCompleted OnQuestCompleted;

	/** 任务失败时触发(超时/手动失败/目标失败)。参数 Quest:失败的任务实例(已回滚进度,广播后即移除) */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="任务失败时触发(超时/手动失败/目标失败)。Quest:失败的任务实例(已回滚进度,广播后即移除)"))
	FOnQuestFailed OnQuestFailed;

	/** 放弃任务时触发。参数 QuestID:被放弃的任务 ID(已回滚进度,实例已移除,可重新接取) */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="放弃任务时触发。QuestID:被放弃的任务ID(已回滚进度,实例已移除,可重新接取)"))
	FOnQuestAbandoned OnQuestAbandoned;

	/** 任务链解锁后续任务时触发(完成当前任务解锁下一个)。参数 QuestID:新解锁可接取的任务 ID */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="任务链解锁后续任务时触发(完成当前任务解锁下一个)。QuestID:新解锁可接取的任务ID"))
	FOnQuestUnlocked OnQuestUnlocked;

	/** 单个目标完成时触发(任务未全部完成)。参数 Quest:所属任务实例;ObjectiveIndex:完成的目标索引(从0起) */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="单个目标完成时触发(任务未全部完成)。Quest:所属任务实例;ObjectiveIndex:完成的目标索引(从0起)"))
	FOnObjectiveCompleted OnObjectiveCompleted;

	/** 任务指引数据变化时触发(接取/目标位置变化等)。参数 GuideData:该任务的指引点汇总(供 HUD 渲染) */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="任务指引数据变化时触发(接取/目标位置变化等)。GuideData:该任务的指引点汇总(供HUD渲染)"))
	FOnQuestGuideUpdated OnQuestGuideUpdated;

	/** 接取任务失败时触发(条件不满足/已接取/已完成/定义不存在等)。参数 QuestID:接取失败的任务 ID;Reason:失败原因文本 */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events",
		meta=(ToolTip="接取任务失败时触发。QuestID:接取失败的任务ID;Reason:失败原因(等级不足/已接取/已完成/前置未满足等)"))
	FOnQuestAcceptFailed OnQuestAcceptFailed;

	// ============ Objective 回调(UQuestObjective 调 MarkCompleted/MarkFailed 后内部调用) ============
	/** 目标完成回调:广播 OnObjectiveCompleted + OnQuestUpdated,若全目标完成则自动 CompleteQuest */
	void NotifyObjectiveCompleted(UQuestObjective* Objective);
	/** 目标失败回调:按 bAutoFailOnObjectiveFailure 决定是否触发任务级失败 */
	void NotifyObjectiveFailed(UQuestObjective* Objective);
	/** 目标进度变化回调:广播 OnQuestUpdated(UI 刷新进度) */
	void NotifyObjectiveProgressChanged(UQuestObjective* Objective);

protected:
	// 玩家持有的全部任务实例(按状态分布,Map 保持插入序)
	UPROPERTY()
	TMap<FName, TObjectPtr<UQuestInstance>> QuestInstances;

	// 已完成任务 ID 缓存(快速查询,不保留 Instance 也行)
	UPROPERTY()
	TSet<FName> CompletedQuestIDs;

	UPROPERTY()
	TSet<FName> AbandonedQuestIDs;

	UPROPERTY()
	TSet<FName> UnlockedQuestIDs;

	// === 事件统计(HandleEvent 自动记录,供玩家查看游戏数据/成就系统用) ===
	/** 按事件 Tag 累积统计(每次事件累加 Payload.Amount) */
	UPROPERTY()
	TArray<FEventTagStat> EventTagStats;

	/** 按目标 Tag 累积统计(每次事件累加 Payload.Amount) */
	UPROPERTY()
	TArray<FTargetTagStat> TargetTagStats;

	// === 内部辅助 ===
	UQuestManager* GetQuestManager() const;
	UQuestInstance* CreateQuestInstance(UQuestDefinition* Definition);
	bool CheckAcceptConditions(UQuestDefinition* Definition, FText& OutReason) const;
	void CheckTimeouts(float DeltaTime);
	/** 遍历活跃任务中 bNeedsTick=true 的目标,调用 OnObjectiveTick(持续型目标每帧推进) */
	void TickContinuousObjectives(float DeltaTime);
	void TryUnlockNextQuest(UQuestInstance* CompletedQuest);
	void FailQuestInternal(FName QuestID, const FText& Reason);
	void BroadcastGuideForQuest(UQuestInstance* Quest);
	int32 FindObjectiveIndex(UQuestObjective* Objective) const;
	/** 记录事件统计(事件Tag+目标Tag 各累加 Payload.Amount)。HandleEvent 每次收到事件时调用 */
	void RecordEventStats(const FQuestEventPayload& Payload);
};
