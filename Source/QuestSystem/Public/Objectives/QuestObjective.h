// 任务目标 —— 可组合的双轨判定(事件驱动 + 持续状态)
// 事件型:配置 bRespondToEvents=true,HandleEvent 统一判定(纯数据,常见场景无需写代码)
// 持续型:配置 bNeedsTick=true,OnObjectiveTick 每帧调用(用户 override 实现守卫/护送/生存等)
// 两者可同时启用(如"3分钟内杀10只狼":事件计数 + Tick倒计时超时)
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.h"
#include "QuestObjective.generated.h"

class UQuestInstance;
class UQuestComponent;

/** 单个提交物品需求(物品行ID + 数量) */
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestRequiredItem
{
	GENERATED_BODY()

	/** 物品行ID(对应游戏层DataTable行名,如 "Item.Herb") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Deliver",
		meta=(ToolTip="物品行ID,对应游戏层DataTable行名"))
	FName ItemRowID;

	/** 需要提交的数量 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Deliver",
		meta=(ToolTip="需要提交的数量"))
	int32 Amount = 1;
};

/**
 * 任务目标(纯数据配置)
 * - ListenEvents:监听什么事件(如 击杀/拾取/到达/对话/交互)
 * - TargetTags:匹配什么目标(空=通配,非空=Payload.TargetTag 命中容器任一即匹配)
 * - RequiredCount:需要几次(0=即完成,首次匹配事件即完成;>0=计数累加达标后完成)
 * - RequiredItems:提交物品需求列表(非空=Deliver语义,判定时查背包并扣除;支持多种物品不同数量)
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class QUESTSYSTEM_API UQuestObjective : public UObject
{
	GENERATED_BODY()

public:
	// === 展示 ===
	/** 目标显示名(如"击杀野狼"),供 UI 进度文本拼接 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective")
	FText DisplayName;

	/** 目标描述文本(如"清除威胁城镇的野兽") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective")
	FText Description;

	// === 执行模式(可组合) ===
	/** 是否响应事件(HandleEvent 中处理)。默认 true。纯持续型任务(如生存3分钟)设 false */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Mode",
		meta=(ToolTip="是否响应事件。true=参与HandleEvent判定;false=纯持续型,只靠Tick推进"))
	bool bRespondToEvents = true;

	/** 是否需要每帧Tick(OnObjectiveTick 中处理)。默认 false。持续型任务(守卫/护送/生存)设 true */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Mode",
		meta=(ToolTip="是否需要每帧Tick。true=持续型,OnObjectiveTick每帧调用;false=纯事件型"))
	bool bNeedsTick = false;

	// === 事件匹配配置 ===
	/** 监听的事件Tag(如 任务插件.事件.击杀)。空=不监听任何事件 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Match",
		meta=(ToolTip="监听的事件Tag。空=不监听任何事件"))
	FGameplayTagContainer ListenEvents;

	/** 匹配的目标Tag(空=通配任意目标;非空=Payload.TargetTag 命中容器任一即匹配) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Match",
		meta=(ToolTip="匹配的目标Tag。空=通配;非空=Payload.TargetTag命中容器任一即匹配"))
	FGameplayTagContainer TargetTags;

	/** 需要几次。0=即完成(首次匹配即完成);>0=计数累加,达到此值完成 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Match",
		meta=(ToolTip="需要几次。0=即完成;>0=计数达标后完成"))
	int32 RequiredCount = 1;

	/** 提交物品需求列表(非空时查背包检查持有量)。支持多种物品不同数量 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Match",
		meta=(ToolTip="物品需求列表。非空时查背包检查持有量;空=不涉及物品"))
	TArray<FQuestRequiredItem> RequiredItems;

	/** 是否扣除物品。true=提交(够数则扣除);false=仅检测持有(不扣除)。默认提交 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Objective|Match",
		meta=(ToolTip="是否扣除物品。true=提交(扣除);false=仅检测持有(不扣除)"))
	bool bConsumeItems = true;

	// === 运行时状态(Component 维护) ===
	/** 目标当前状态(NotStarted→InProgress→Completed/Failed,由 Component 在接取/判定/回滚时设置) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objective")
	EObjectiveStatus Status = EObjectiveStatus::NotStarted;

	/** 当前进度计数(计数型目标用,即完成型不使用)。由 Component 在 HandleEvent 中累加 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objective")
	int32 CurrentCount = 0;

	/** 所属任务实例(接取时由 InitializeFromDefinition 注入) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objective")
	TObjectPtr<UQuestInstance> OwningQuest;

	// === 可 override 的判定入口(用户继承后自定义复杂逻辑) ===

	/**
	 * 收到匹配事件时的处理入口。HandleEvent 在通过 ListenEvents + TargetTags 过滤后调用此方法。
	 * 默认实现:执行物品检查 + 计数累加 + 达标判定(纯数据逻辑)。
	 * 子类 override 可实现自定义判定(如"击杀王时持有特殊武器算双倍计数")。
	 * @param Payload 事件载荷(已通过 ListenEvents/TargetTags 匹配)
	 * @param Owner 实现了 IQuestInterface 的玩家对象(查背包/扣物品用)
	 * @return true=进度确实变化(触发 OnQuestUpdated 广播)
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Objective")
	bool OnEventReceived(const FQuestEventPayload& Payload, UObject* Owner);
	virtual bool OnEventReceived_Implementation(const FQuestEventPayload& Payload, UObject* Owner);

	/**
	 * 持续型目标的每帧更新(仅 bNeedsTick=true 时由 Component Tick 调用)。
	 * 默认实现为空。用户 override 实现持续状态逻辑(如守卫倒计时、护送追踪、生存计时)。
	 * 典型用法:累加 ElapsedTime、检查玩家/NPC 位置、超时则 MarkFailed/MarkCompleted。
	 * @param DeltaTime 帧间隔时间(秒)
	 * @param Owner 实现了 IQuestInterface 的玩家对象(查世界/玩家用)
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Objective")
	void OnObjectiveTick(float DeltaTime, UObject* Owner);
	virtual void OnObjectiveTick_Implementation(float DeltaTime, UObject* Owner) {}

	/**
	 * 获取进度文本(UI 用)。默认实现:计数型返回 "DisplayName N/M",即完成型返回状态文本。
	 * 子类 override 可自定义展示(如"已击杀 3 只王,还剩 2 只")。
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Objective")
	FText GetProgressText() const;
	virtual FText GetProgressText_Implementation() const;

	// === 内部通知方法(Component 在 HandleEvent 达标判定后调用) ===
	/** 标记目标完成:设状态并通知 Component(触发 OnObjectiveCompleted 广播,若全目标完成则自动完成任务) */
	void MarkCompleted();
	/** 标记目标失败:设状态并通知 Component(按 bAutoFailOnObjectiveFailure 决定是否触发任务级失败) */
	void MarkFailed();

protected:
	UPROPERTY()
	TObjectPtr<UQuestComponent> OwningComponent;

	friend class UQuestInstance;
	friend class UQuestComponent;
};
