// 接取条件基类 + 内置 4 子类(等级/声望/前置任务/职业)
// UObject 多态,蓝图可派生
// 条件无状态:只在接取时 Evaluate 一次
// QuestSystem Plugin

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuestConditionBase.generated.h"

class UQuestComponent;

UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class QUESTSYSTEM_API UQuestConditionBase : public UObject
{
	GENERATED_BODY()

public:
	/** 检查玩家是否满足接取条件。返回 false 时 OutReason 填失败原因 */
	UFUNCTION(BlueprintNativeEvent, Category="Quest|Condition")
	bool Evaluate(UQuestComponent* ForComponent, FText& OutReason);
	virtual bool Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason)
	{
		OutReason = FText::FromString(TEXT("未实现条件判断"));
		return false;
	}

	/** 条件描述文本(UI 显示 "需要等级:10") */
	UFUNCTION(BlueprintNativeEvent, Category="Quest|Condition")
	FText GetConditionText() const;
	virtual FText GetConditionText_Implementation() const { return FText::GetEmpty(); }
};

// === 内置条件子类 ===

// 等级要求
UCLASS(BlueprintType)
class QUESTSYSTEM_API UQuestCondition_Level : public UQuestConditionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	int32 RequiredLevel = 1;

	virtual bool Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason) override;
	virtual FText GetConditionText_Implementation() const override;
};

// 声望要求(检查玩家某声望 Tag 的值)
UCLASS(BlueprintType)
class QUESTSYSTEM_API UQuestCondition_Reputation : public UQuestConditionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	FGameplayTag ReputationTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	int32 RequiredValue = 0;

	virtual bool Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason) override;
};

// 前置任务(检查 Component 上已完成的任务列表)
UCLASS(BlueprintType)
class QUESTSYSTEM_API UQuestCondition_PrerequisiteQuest : public UQuestConditionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	FName RequiredQuestID;

	virtual bool Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason) override;
};

// 职业要求(检查玩家 Character 上某 GameplayTag)
UCLASS(BlueprintType)
class QUESTSYSTEM_API UQuestCondition_Class : public UQuestConditionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	FGameplayTag RequiredClassTag;

	virtual bool Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason) override;
};
