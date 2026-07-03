// 接取条件实现
// 内置子类访问玩家数据走 ForComponent->GetOwner()
// 插件不依赖具体等级/声望系统,做最小假设
// QuestSystem Plugin

#include "Conditions/QuestConditionBase.h"
#include "QuestTags.h"
#include "QuestComponent.h"
#include "Interfaces/QuestInterface.h"
#include "GameFramework/PlayerController.h"

bool UQuestCondition_Level::Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason)
{
	// 最小假设:玩家通过 IQuestInterface 暴露等级(用特定 Tag 查询)
	// 游戏层若用其他方式存等级,自定义 Condition 子类
	UObject* Owner = ForComponent ? ForComponent->GetOwner() : nullptr;
	if (!Owner || !Owner->Implements<UQuestInterface>())
	{
		OutReason = FText::FromString(TEXT("无法获取玩家信息"));
		return false;
	}
	int32 CurLevel = IQuestInterface::Execute_GetCurrency(
		Owner, TAG_Quest_Attribute_等级);
	if (CurLevel < RequiredLevel)
	{
		OutReason = FText::FromString(FString::Printf(TEXT("等级不足,需要 %d 级"), RequiredLevel));
		return false;
	}
	return true;
}

FText UQuestCondition_Level::GetConditionText_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("需要等级:%d"), RequiredLevel));
}

bool UQuestCondition_Reputation::Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason)
{
	UObject* Owner = ForComponent ? ForComponent->GetOwner() : nullptr;
	if (!Owner || !Owner->Implements<UQuestInterface>())
	{
		OutReason = FText::FromString(TEXT("无法获取玩家信息"));
		return false;
	}
	int32 CurValue = IQuestInterface::Execute_GetCurrency(Owner, ReputationTag);
	if (CurValue < RequiredValue)
	{
		OutReason = FText::FromString(TEXT("声望不足"));
		return false;
	}
	return true;
}

bool UQuestCondition_PrerequisiteQuest::Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason)
{
	if (!ForComponent || !ForComponent->IsQuestCompleted(RequiredQuestID))
	{
		OutReason = FText::FromString(TEXT("前置任务未完成"));
		return false;
	}
	return true;
}

bool UQuestCondition_Class::Evaluate_Implementation(UQuestComponent* ForComponent, FText& OutReason)
{
	UObject* Owner = ForComponent ? ForComponent->GetOwner() : nullptr;
	APlayerController* PC = Cast<APlayerController>(Owner);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	// ActorHasTag 接收 FName,GameplayTag 用 GetTagName 转换
	if (!Pawn || !Pawn->ActorHasTag(RequiredClassTag.GetTagName()))
	{
		OutReason = FText::FromString(TEXT("职业不符合要求"));
		return false;
	}
	return true;
}
