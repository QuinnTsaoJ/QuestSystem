// 任务系统公共类型定义 —— 枚举、事件载荷、UI 视图数据、指引点结构
// 所有后续类的基础依赖
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "QuestTypes.generated.h"

// 任务状态机
UENUM(BlueprintType)
enum class EQuestStatus : uint8
{
	Inactive	UMETA(DisplayName = "未接取"),
	Active		UMETA(DisplayName = "进行中"),
	Completed	UMETA(DisplayName = "已完成"),
	Failed		UMETA(DisplayName = "已失败"),
	Abandoned	UMETA(DisplayName = "已放弃"),
};

// 目标状态(独立于任务状态,目标可单独失败)
UENUM(BlueprintType)
enum class EObjectiveStatus : uint8
{
	NotStarted	UMETA(DisplayName = "未开始"),
	InProgress	UMETA(DisplayName = "进行中"),
	Completed	UMETA(DisplayName = "已完成"),
	Failed		UMETA(DisplayName = "已失败"),
};

// 任务事件载荷 —— 外部系统通知任务系统时携带的数据
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestEventPayload
{
	GENERATED_BODY()

	/** 事件类型标签(如 任务插件.事件.击杀)。目标据此过滤 ListenEvents */
	UPROPERTY(BlueprintReadWrite, Category="Quest|Event")
	FGameplayTag EventTag;

	/** 事件目标标识(如 任务插件.目标.敌人.野兽)。目标据此匹配 TargetTags */
	UPROPERTY(BlueprintReadWrite, Category="Quest|Event")
	FGameplayTag TargetTag;

	/** 数量(击杀1只/拾取3个)。计数型目标据此累加 CurrentCount */
	UPROPERTY(BlueprintReadWrite, Category="Quest|Event")
	int32 Amount = 1;

	/** 关联 Actor 软引用(可选,游戏层自行解释,任务系统不强制使用) */
	UPROPERTY(BlueprintReadWrite, Category="Quest|Event")
	TSoftObjectPtr<AActor> SourceActor;

	/** 自定义扩展数据(灵活载荷,避免频繁改结构。游戏层自行解释,任务系统不读取) */
	UPROPERTY(BlueprintReadWrite, Category="Quest|Event")
	FInstancedStruct CustomData;
};

// UI 视图数据 —— 给蓝图 Widget 用的格式化展示数据
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Quest|UI")
	FName QuestID;

	UPROPERTY(BlueprintReadOnly, Category="Quest|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Quest|UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Quest|UI")
	EQuestStatus Status = EQuestStatus::Inactive;

	UPROPERTY(BlueprintReadOnly, Category="Quest|UI")
	TArray<FText> ObjectiveTexts;

	UPROPERTY(BlueprintReadOnly, Category="Quest|UI")
	TArray<FText> RewardTexts;

	UPROPERTY(BlueprintReadOnly, Category="Quest|UI")
	int32 RemainingSeconds = -1;
};

// 指引点 —— 一个目标可暴露多个指引点
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestGuidePoint
{
	GENERATED_BODY()

	// 指引点所属的目标索引(任务内第几个 Objective,Component 填充)
	UPROPERTY(BlueprintReadOnly, Category="Quest|Guide")
	int32 ObjectiveIndex = INDEX_NONE;

	// 世界位置
	UPROPERTY(BlueprintReadOnly, Category="Quest|Guide")
	FVector WorldLocation = FVector::ZeroVector;

	// 是否有效(HUD 据此决定是否显示标记)
	UPROPERTY(BlueprintReadOnly, Category="Quest|Guide")
	bool bIsValid = false;

	// 指引点标签(HUD 用不同图标)
	UPROPERTY(BlueprintReadOnly, Category="Quest|Guide")
	FGameplayTag GuideTag;
};

// 任务的全部指引点汇总
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestGuideData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Quest|Guide")
	FName QuestID;

	// 当前需要显示的所有指引点(只含进行中的 Objective)
	UPROPERTY(BlueprintReadOnly, Category="Quest|Guide")
	TArray<FQuestGuidePoint> GuidePoints;

	// 是否需要 HUD 指引箭头(任务级开关)
	UPROPERTY(BlueprintReadOnly, Category="Quest|Guide")
	bool bShowHUDArrow = true;
};
