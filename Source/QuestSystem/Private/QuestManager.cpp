// 任务管理器实现
// QuestSystem Plugin

#include "QuestManager.h"
#include "QuestDefinition.h"
#include "QuestDefinitionRow.h"
#include "QuestComponent.h"
#include "QuestTypes.h"
#include "Interfaces/QuestInterface.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"

void UQuestManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UQuestManager::Deinitialize()
{
	RegisteredComponents.Empty();
	DefinitionTable.Empty();
	QuestRowMap.Empty();
	QuestDataSource.Empty();
	Super::Deinitialize();
}

void UQuestManager::RegisterQuestDefinition(UQuestDefinition* Definition)
{
	if (!Definition || Definition->QuestID.IsNone()) return;
	DefinitionTable.Add(Definition->QuestID, Definition);
	OnQuestDefinitionRegistered.Broadcast(Definition);
}

void UQuestManager::UnregisterQuestDefinition(UQuestDefinition* Definition)
{
	if (!Definition) return;
	DefinitionTable.Remove(Definition->QuestID);
	OnQuestDefinitionUnregistered.Broadcast(Definition);
}

int32 UQuestManager::RegisterQuestDefinitionsFromDataTable(UDataTable* DataTable)
{
	if (!DataTable) return 0;

	// 行结构必须是 FQuestDefinitionRow(运行时校验)
	if (DataTable->GetRowStruct() != FQuestDefinitionRow::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("RegisterQuestDefinitionsFromDataTable: 数据表行结构非 FQuestDefinitionRow,跳过。表:%s"), *DataTable->GetName());
		return 0;
	}

	int32 IndexedCount = 0;
	TArray<FName> RowNames = DataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		// 只读 QuestID 字段建索引,不构造 UQuestDefinition(触发式加载:定义对象在接取/存档恢复时按需构造)
		FQuestDefinitionRow* Row = DataTable->FindRow<FQuestDefinitionRow>(RowName, TEXT("RegisterQuestDefinitionsFromDataTable"));
		if (!Row || Row->QuestID.IsNone()) continue;  // 跳过空行/无 ID 行

		// 跳过已索引的同 ID 任务(避免重复)
		if (QuestRowMap.Contains(Row->QuestID)) continue;

		QuestDataSource.Add(Row->QuestID, DataTable);
		QuestRowMap.Add(Row->QuestID, RowName);
		++IndexedCount;
	}

	UE_LOG(LogTemp, Log, TEXT("RegisterQuestDefinitionsFromDataTable: 从 %s 索引了 %d 个任务定义(按需构造)"), *DataTable->GetName(), IndexedCount);
	return IndexedCount;
}

UQuestDefinition* UQuestManager::LoadQuestDefinition(FName QuestID)
{
	if (QuestID.IsNone()) return nullptr;

	// 短路:已加载直接返回(热路径优化,避免重复 FindRow)
	if (const TObjectPtr<UQuestDefinition>* Found = DefinitionTable.Find(QuestID))
	{
		return Found->Get();
	}

	// 查索引定位行
	const FName* RowNamePtr = QuestRowMap.Find(QuestID);
	if (!RowNamePtr) return nullptr;  // 未注册的任务

	const TObjectPtr<UDataTable>* DataTablePtr = QuestDataSource.Find(QuestID);
	if (!DataTablePtr || !DataTablePtr->Get()) return nullptr;

	UDataTable* DataTable = DataTablePtr->Get();
	FQuestDefinitionRow* Row = DataTable->FindRow<FQuestDefinitionRow>(*RowNamePtr, TEXT("LoadQuestDefinition"));
	if (!Row) return nullptr;

	// 构造 UQuestDefinition 并拷贝行字段(与原 RegisterQuestDefinitionsFromDataTable 逻辑一致)
	UQuestDefinition* Def = NewObject<UQuestDefinition>(GetTransientPackage(), UQuestDefinition::StaticClass());
	Def->QuestID = Row->QuestID;
	Def->DisplayName = Row->DisplayName;
	Def->Description = Row->Description;
	Def->GiverTag = Row->GiverTag;
	Def->NextQuestID = Row->NextQuestID;
	Def->PrerequisiteQuestIDs = Row->PrerequisiteQuestIDs;
	Def->ObjectiveClasses = Row->ObjectiveClasses;
	Def->Rewards = Row->Rewards;
	Def->ConditionClasses = Row->ConditionClasses;
	Def->TimeLimit = Row->TimeLimit;
	Def->bAutoFailOnObjectiveFailure = Row->bAutoFailOnObjectiveFailure;
	Def->bShowHUDGuide = Row->bShowHUDGuide;

	DefinitionTable.Add(QuestID, Def);
	return Def;
}

int32 UQuestManager::LoadQuestsByGiver(FGameplayTag GiverTag)
{
	if (!GiverTag.IsValid()) return 0;

	int32 LoadedCount = 0;
	// 遍历索引找同 Giver 的任务,逐个按需构造
	for (const auto& Pair : QuestRowMap)
	{
		const FName& QuestID = Pair.Key;
		// 已加载的跳过(LoadQuestDefinition 内部也会短路,这里提前判断减少调用)
		if (DefinitionTable.Contains(QuestID)) continue;

		// 需读取行的 GiverTag 判断是否匹配,先临时加载定义再校验
		// 注:此处需 FindRow 读 GiverTag。为避免未匹配的任务也被构造,先读行判断再决定是否 LoadQuestDefinition
		const TObjectPtr<UDataTable>* DataTablePtr = QuestDataSource.Find(QuestID);
		if (!DataTablePtr || !DataTablePtr->Get()) continue;

		FQuestDefinitionRow* Row = DataTablePtr->Get()->FindRow<FQuestDefinitionRow>(Pair.Value, TEXT("LoadQuestsByGiver"));
		if (!Row) continue;

		// GiverTag 精确匹配(与 GetQuestsByGiver 语义一致)
		if (Row->GiverTag.MatchesTag(GiverTag))
		{
			if (LoadQuestDefinition(QuestID))
			{
				++LoadedCount;
			}
		}
	}
	return LoadedCount;
}

int32 UQuestManager::LoadQuestChain(FName QuestID, int32 MaxDepth)
{
	int32 LoadedCount = 0;
	TSet<FName> Visited;  // 已访问集合,防循环引用
	FName CurrentID = QuestID;

	while (!CurrentID.IsNone() && MaxDepth > 0)
	{
		if (Visited.Contains(CurrentID))
		{
			UE_LOG(LogTemp, Warning, TEXT("LoadQuestChain: 检测到循环引用,任务 %s 已访问过,停止加载。"), *CurrentID.ToString());
			break;
		}
		Visited.Add(CurrentID);

		UQuestDefinition* Def = LoadQuestDefinition(CurrentID);
		if (!Def) break;  // 链上任务未注册,停止

		++LoadedCount;
		CurrentID = Def->NextQuestID;  // 沿主链推进
		--MaxDepth;
	}

	if (MaxDepth <= 0 && !CurrentID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadQuestChain: 任务链 %s 超出最大深度 32,停止加载。可能存在超长链或循环。"), *QuestID.ToString());
	}

	return LoadedCount;
}

bool UQuestManager::TryAcceptQuest(APlayerController* Player, FName QuestID, FText& OutReason)
{
	if (!Player) return false;

	// 优先走 IQuestInterface::GetQuestComponent(语义化,支持游戏层自定义挂载位置)
	UQuestComponent* QuestComp = nullptr;
	if (Player->Implements<UQuestInterface>())
	{
		QuestComp = IQuestInterface::Execute_GetQuestComponent(Player);
	}
	// 兜底:接口未实现或返回 null 时,回退到组件搜索(保持向后兼容)
	if (!QuestComp)
	{
		QuestComp = Player->FindComponentByClass<UQuestComponent>();
	}
	if (!QuestComp)
	{
		OutReason = FText::FromString(TEXT("玩家身上未找到 QuestComponent"));
		return false;
	}
	return QuestComp->AcceptQuest(QuestID, OutReason);
}

UQuestDefinition* UQuestManager::GetQuestDefinition(FName QuestID) const
{
	// 命中热数据直接返回
	if (const TObjectPtr<UQuestDefinition>* Found = DefinitionTable.Find(QuestID))
	{
		return Found->Get();
	}
	// 兜底:未命中时按需拉取(只加载单个定义,不连带 Giver/链,避免只读查询扩散加载)
	// 注意:const 方法内触发加载,需 const_cast 去除(this 逻辑上仍可视为查询缓存填充)
	return const_cast<UQuestManager*>(this)->LoadQuestDefinition(QuestID);
}

TArray<UQuestDefinition*> UQuestManager::GetAllQuestDefinitions() const
{
	TArray<UQuestDefinition*> Result;
	for (const auto& Pair : DefinitionTable)
	{
		if (Pair.Value) Result.Add(Pair.Value.Get());
	}
	return Result;
}

TArray<UQuestDefinition*> UQuestManager::GetQuestsByGiver(FGameplayTag GiverTag) const
{
	TArray<UQuestDefinition*> Result;
	for (const auto& Pair : DefinitionTable)
	{
		if (!Pair.Value) continue;
		// GiverTag 留空时,只返回同样未绑定给予者(任意处可接)的任务;
		// GiverTag 非空时,返回精确匹配该给予者的任务
		if (GiverTag.IsValid() ? Pair.Value->GiverTag.MatchesTag(GiverTag)
		                       : !Pair.Value->GiverTag.IsValid())
		{
			Result.Add(Pair.Value.Get());
		}
	}
	return Result;
}

void UQuestManager::NotifyEvent(const FQuestEventPayload& Payload)
{
	// 遍历所有已注册 Component,转发事件
	for (const TWeakObjectPtr<UQuestComponent>& WeakComp : RegisteredComponents)
	{
		UQuestComponent* Comp = WeakComp.Get();
		if (Comp)
		{
			Comp->HandleEvent(Payload);
		}
	}
}

void UQuestManager::NotifyEventToPlayer(UQuestComponent* Component, const FQuestEventPayload& Payload)
{
	if (Component)
	{
		Component->HandleEvent(Payload);
	}
}

void UQuestManager::RegisterQuestComponent(UQuestComponent* Component)
{
	if (Component && !RegisteredComponents.Contains(Component))
	{
		RegisteredComponents.Add(Component);
	}
}

void UQuestManager::UnregisterQuestComponent(UQuestComponent* Component)
{
	if (!Component) return;
	RegisteredComponents.Remove(Component);
}
