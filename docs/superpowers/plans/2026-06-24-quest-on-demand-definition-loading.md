# 触发式任务定义加�?实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** �?QuestSystem 的任务定义从启动全量构造改为按需触发加载,并在存档恢复时自动加载进行中任务的定义�?
**Architecture:** DataTable 注册时只建轻量索�?QuestID �?行名 + DataTable �?,定义对象在接�?存档恢复/查询兜底时按需构造。接取时连带加载�?Giver 任务并预加载 NextQuestID �?深度上限 32 防循�?�?
**Tech Stack:** Unreal Engine 5 C++, UBT 编译, GameplayTags, DataTable

**Spec:** `docs/specs/2026-06-24-quest-on-demand-definition-loading-design.md`

---

## 文件结构

| 文件 | 责任 | 操作 |
|---|---|---|
| `Source/QuestSystem/Public/QuestManager.h` | 声明索引字段 + 按需加载方法 | 修改 |
| `Source/QuestSystem/Private/QuestManager.cpp` | 实现索引化注�?+ 按需加载逻辑 | 修改 |
| `Source/QuestSystem/Private/QuestComponent.cpp` | AcceptQuest/LoadFromData 接入按需加载 | 修改 |

---

## Task 1: QuestManager 索引字段与按需加载方法声明

**Files:**
- Modify: `Source/QuestSystem/Public/QuestManager.h`

- [ ] **Step 1: 新增索引字段与按需加载方法声明**

�?`UQuestManager` �?`private` 区域(现有 `DefinitionTable` 之前)新增索引字段,�?`public` 区域新增按需加载方法�?
�?`QuestManager.h` �?private 区域�?

```cpp
private:
	// 定义�?ID→Definition
	UPROPERTY()
	TMap<FName, TObjectPtr<UQuestDefinition>> DefinitionTable;

	// 已注册的玩家 Component(事件总线分发目标)
	TArray<TWeakObjectPtr<UQuestComponent>> RegisteredComponents;
```

改为:

```cpp
private:
	// === 定义源索�?注册时建�?轻量,不持有行数据对象) ===
	// QuestID �?所�?DataTable(用于按行名反查行数据)
	UPROPERTY()
	TMap<FName, TObjectPtr<UDataTable>> QuestDataSource;

	// QuestID �?行名(轻量索引,不触发行数据读取)
	TMap<FName, FName> QuestRowMap;

	// === 已按需构造的定义(热数�?只含被加载过�? ===
	UPROPERTY()
	TMap<FName, TObjectPtr<UQuestDefinition>> DefinitionTable;

	// 已注册的玩家 Component(事件总线分发目标)
	TArray<TWeakObjectPtr<UQuestComponent>> RegisteredComponents;
```

�?`public` 区域,`GetQuestsByGiver` 声明之后、`NotifyEvent` 之前,新增按需加载方法:

```cpp
	// === 按需加载(触发式定义构�? ===

	/** 按需加载单个任务定义。查索引→找行→构�?UQuestDefinition→入 DefinitionTable�?	 *  已加载则直接返回(短路)。未注册返回 nullptr�?	 *  注意:本方法只加载单个定义,不连带加载同 Giver 任务或后续链(避免在只读查询中扩散加载)�?*/
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	UQuestDefinition* LoadQuestDefinition(FName QuestID);

	/** 批量加载指定给予�?GiverTag)的所有任务定义�?	 *  遍历 QuestRowMap 找同 Giver 的行,逐个构造入 DefinitionTable�?	 *  用于接取任务时连带加载同 NPC 的全部任�?�?GetAvailableQuests 直接查内存�?*/
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	int32 LoadQuestsByGiver(FGameplayTag GiverTag);

	/** �?NextQuestID 递归预加载任务链上所有任务定义�?	 *  每个链上任务只加载自身定�?不连带其 Giver(防级联扩�?�?	 *  @param QuestID 链起点任�?ID
	 *  @param MaxDepth 最大递归深度(默认 32,防循环引�? */
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	int32 LoadQuestChain(FName QuestID, int32 MaxDepth = 32);
```

- [ ] **Step 2: 确认头文件无前置声明缺失**

`UDataTable` 需要前置声明。在头文件顶部已有的前置声明�?`struct FQuestEventPayload;` 附近)确认是否已有 `class UDataTable;`。若无需新增�?
检查头文件�?12-14 行区�?

```cpp
struct FQuestEventPayload;
class UQuestDefinition;
class UQuestComponent;
```

若没�?`class UDataTable;`,�?`class UQuestComponent;` 后追�?

```cpp
class UDataTable;
```

- [ ] **Step 3: 编译头文件确认无语法错误**

Run: `"F:\EpicGame\UE_5.7\Engine\Build\BatchFiles\Build.bat" QuestSystemEditor Win64 DebugGame -Project="F:\UEProject\Inventory\Inventory.uproject" -WaitMutex -FromMsBuild`
Expected: 编译通过(实现未写,链接阶段可能�?unresolved external,此步仅验证头文件语法)

> �?此项目无独立 .uproject,使用插件所属项目的 .uproject。若编译�?unresolved external `LoadQuestDefinition` 等是预期�?实现尚在后续 Task)。只要头文件解析无错即可�?
- [ ] **Step 4: Commit**

```bash
git add Source/QuestSystem/Public/QuestManager.h
git commit -m "feat(quest): 声明触发式定义加载的索引字段与方�?
```

---

## Task 2: RegisterQuestDefinitionsFromDataTable 改为索引�?
**Files:**
- Modify: `Source/QuestSystem/Private/QuestManager.cpp:37-79`

- [ ] **Step 1: 将全量构造改为只建索�?*

�?`QuestManager.cpp` �?`RegisterQuestDefinitionsFromDataTable` 实现(�?37-79 �?�?

```cpp
int32 UQuestManager::RegisterQuestDefinitionsFromDataTable(UDataTable* DataTable)
{
	if (!DataTable) return 0;

	// 行结构必须是 FQuestDefinitionRow(运行时校�?
	if (DataTable->GetRowStruct() != FQuestDefinitionRow::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("RegisterQuestDefinitionsFromDataTable: 数据表行结构�?FQuestDefinitionRow,跳过。表:%s"), *DataTable->GetName());
		return 0;
	}

	int32 RegisteredCount = 0;
	TArray<FName> RowNames = DataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FQuestDefinitionRow* Row = DataTable->FindRow<FQuestDefinitionRow>(RowName, TEXT("RegisterQuestDefinitionsFromDataTable"));
		if (!Row || Row->QuestID.IsNone()) continue;  // 跳过空行/�?ID �?
		// 跳过已注册的�?ID 任务(避免重复)
		if (DefinitionTable.Contains(Row->QuestID)) continue;

		// 构�?UQuestDefinition 并拷贝行字段
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

		RegisterQuestDefinition(Def);
		++RegisteredCount;
	}

	UE_LOG(LogTemp, Log, TEXT("RegisterQuestDefinitionsFromDataTable: �?%s 注册�?%d 个任务定�?), *DataTable->GetName(), RegisteredCount);
	return RegisteredCount;
}
```

改为:

```cpp
int32 UQuestManager::RegisterQuestDefinitionsFromDataTable(UDataTable* DataTable)
{
	if (!DataTable) return 0;

	// 行结构必须是 FQuestDefinitionRow(运行时校�?
	if (DataTable->GetRowStruct() != FQuestDefinitionRow::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("RegisterQuestDefinitionsFromDataTable: 数据表行结构�?FQuestDefinitionRow,跳过。表:%s"), *DataTable->GetName());
		return 0;
	}

	int32 IndexedCount = 0;
	TArray<FName> RowNames = DataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		// 只读 QuestID 字段建索�?不构�?UQuestDefinition(触发式加�?定义对象在接�?存档恢复时按需构�?
		FQuestDefinitionRow* Row = DataTable->FindRow<FQuestDefinitionRow>(RowName, TEXT("RegisterQuestDefinitionsFromDataTable"));
		if (!Row || Row->QuestID.IsNone()) continue;  // 跳过空行/�?ID �?
		// 跳过已索引的�?ID 任务(避免重复)
		if (QuestRowMap.Contains(Row->QuestID)) continue;

		QuestDataSource.Add(Row->QuestID, DataTable);
		QuestRowMap.Add(Row->QuestID, RowName);
		++IndexedCount;
	}

	UE_LOG(LogTemp, Log, TEXT("RegisterQuestDefinitionsFromDataTable: �?%s 索引�?%d 个任务定�?按需构�?"), *DataTable->GetName(), IndexedCount);
	return IndexedCount;
}
```

- [ ] **Step 2: 确认 Deinitialize 清理新字�?*

`Deinitialize`(�?16-21 �?当前清空 `RegisteredComponents` �?`DefinitionTable`。追加清空新索引字段:

�?

```cpp
void UQuestManager::Deinitialize(FSubsystemCollectionBase& Collection)
{
	RegisteredComponents.Empty();
	DefinitionTable.Empty();
	Super::Deinitialize(Collection);
}
```

改为:

```cpp
void UQuestManager::Deinitialize(FSubsystemCollectionBase& Collection)
{
	RegisteredComponents.Empty();
	DefinitionTable.Empty();
	QuestRowMap.Empty();
	QuestDataSource.Empty();
	Super::Deinitialize(Collection);
}
```

- [ ] **Step 3: 编译验证**

Run: `"F:\EpicGame\UE_5.7\Engine\Build\BatchFiles\Build.bat" QuestSystemEditor Win64 DebugGame -Project="F:\UEProject\Inventory\Inventory.uproject" -WaitMutex -FromMsBuild`
Expected: 编译通过(此时按需加载方法尚未实现,�?Task 1 已提交则会有 unresolved external,属预�?�?Task 1 与本 Task 合并提交则应通过)

> 说明:若按 Task 粒度提交,Task 1 + Task 2 一起编译才不报链接错。实践中可将 Task 1+2 合并为一次提�?或接受中间态链接错�?
- [ ] **Step 4: Commit**

```bash
git add Source/QuestSystem/Private/QuestManager.cpp
git commit -m "refactor(quest): DataTable 注册改为只建索引不构造定�?
```

---

## Task 3: 实现 LoadQuestDefinition(单定义按需构�?

**Files:**
- Modify: `Source/QuestSystem/Private/QuestManager.cpp`(�?`GetQuestDefinition` 之前插入)

- [ ] **Step 1: 实现 LoadQuestDefinition**

�?`QuestManager.cpp` �?`GetQuestDefinition`(�?81 �?之前,插入实现:

```cpp
UQuestDefinition* UQuestManager::LoadQuestDefinition(FName QuestID)
{
	if (QuestID.IsNone()) return nullptr;

	// 短路:已加载直接返�?热路径优�?避免重复 FindRow)
	if (UQuestDefinition* const* Found = DefinitionTable.Find(QuestID))
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

	// 构�?UQuestDefinition 并拷贝行字段(与原 RegisterQuestDefinitionsFromDataTable 逻辑一�?
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
```

- [ ] **Step 2: 修改 GetQuestDefinition 增加兜底加载**

�?`GetQuestDefinition`(�?81-85 �?�?

```cpp
UQuestDefinition* UQuestManager::GetQuestDefinition(FName QuestID) const
{
	const TObjectPtr<UQuestDefinition>* Found = DefinitionTable.Find(QuestID);
	return Found ? Found->Get() : nullptr;
}
```

改为:

```cpp
UQuestDefinition* UQuestManager::GetQuestDefinition(FName QuestID) const
{
	// 命中热数据直接返�?	if (const TObjectPtr<UQuestDefinition>* Found = DefinitionTable.Find(QuestID))
	{
		return Found->Get();
	}
	// 兜底:未命中时按需拉取(只加载单个定�?不连�?Giver/�?避免只读查询扩散加载)
	// 注意:const 方法内触发加�?需 const_cast 去除(this 逻辑上仍可视为查询缓存填�?
	return const_cast<UQuestManager*>(this)->LoadQuestDefinition(QuestID);
}
```

- [ ] **Step 3: 编译验证**

Run: `"F:\EpicGame\UE_5.7\Engine\Build\BatchFiles\Build.bat" QuestSystemEditor Win64 DebugGame -Project="F:\UEProject\Inventory\Inventory.uproject" -WaitMutex -FromMsBuild`
Expected: 编译通过无错�?
- [ ] **Step 4: Commit**

```bash
git add Source/QuestSystem/Private/QuestManager.cpp
git commit -m "feat(quest): 实现 LoadQuestDefinition 单定义按需构造与查询兜底"
```

---

## Task 4: 实现 LoadQuestsByGiver �?LoadQuestChain

**Files:**
- Modify: `Source/QuestSystem/Private/QuestManager.cpp`(�?`LoadQuestDefinition` 之后插入)

- [ ] **Step 1: 实现 LoadQuestsByGiver**

�?`LoadQuestDefinition` 实现之后,插入:

```cpp
int32 UQuestManager::LoadQuestsByGiver(FGameplayTag GiverTag)
{
	if (!GiverTag.IsValid()) return 0;

	int32 LoadedCount = 0;
	// 遍历索引找同 Giver 的任�?逐个按需构�?	for (const auto& Pair : QuestRowMap)
	{
		const FName& QuestID = Pair.Key;
		// 已加载的跳过(LoadQuestDefinition 内部也会短路,这里提前判断减少调用)
		if (DefinitionTable.Contains(QuestID)) continue;

		// 需读取行的 GiverTag 判断是否匹配,先临时加载定义再校验
		// �?此处需 FindRow �?GiverTag。为避免未匹配的任务也被构�?先读行判断再决定是否 LoadQuestDefinition
		const TObjectPtr<UDataTable>* DataTablePtr = QuestDataSource.Find(QuestID);
		if (!DataTablePtr || !DataTablePtr->Get()) continue;

		FQuestDefinitionRow* Row = DataTablePtr->Get()->FindRow<FQuestDefinitionRow>(Pair.Value, TEXT("LoadQuestsByGiver"));
		if (!Row) continue;

		// GiverTag 精确匹配(�?GetQuestsByGiver 语义一�?
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
```

- [ ] **Step 2: 实现 LoadQuestChain**

�?`LoadQuestsByGiver` 之后,插入:

```cpp
int32 UQuestManager::LoadQuestChain(FName QuestID, int32 MaxDepth)
{
	int32 LoadedCount = 0;
	TSet<FName> Visited;  // 已访问集�?防循环引�?	FName CurrentID = QuestID;

	while (!CurrentID.IsNone() && MaxDepth > 0)
	{
		if (Visited.Contains(CurrentID))
		{
			UE_LOG(LogTemp, Warning, TEXT("LoadQuestChain: 检测到循环引用,任务 %s 已访问过,停止加载�?), *CurrentID.ToString());
			break;
		}
		Visited.Add(CurrentID);

		UQuestDefinition* Def = LoadQuestDefinition(CurrentID);
		if (!Def) break;  // 链上任务未注�?停止

		++LoadedCount;
		CurrentID = Def->NextQuestID;  // 沿主链推�?		--MaxDepth;
	}

	if (MaxDepth <= 0 && !CurrentID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadQuestChain: 任务�?%s 超出最大深�?32,停止加载。可能存在超长链或循环�?), *QuestID.ToString());
	}

	return LoadedCount;
}
```

- [ ] **Step 3: 编译验证**

Run: `"F:\EpicGame\UE_5.7\Engine\Build\BatchFiles\Build.bat" QuestSystemEditor Win64 DebugGame -Project="F:\UEProject\Inventory\Inventory.uproject" -WaitMutex -FromMsBuild`
Expected: 编译通过无错�?
- [ ] **Step 4: Commit**

```bash
git add Source/QuestSystem/Private/QuestManager.cpp
git commit -m "feat(quest): 实现 LoadQuestsByGiver �?LoadQuestChain 批量按需加载"
```

---

## Task 5: AcceptQuest 接入按需加载

**Files:**
- Modify: `Source/QuestSystem/Private/QuestComponent.cpp:60-108`(AcceptQuest 方法)

- [ ] **Step 1: �?AcceptQuest 中接入按需加载**

�?`QuestComponent.cpp` �?`AcceptQuest`(�?60-108 �?�?获取定义后的逻辑改造�?
当前代码(�?60-70 �?:

```cpp
bool UQuestComponent::AcceptQuest(FName QuestID, FText& OutReason)
{
	UQuestManager* Manager = GetQuestManager();
	if (!Manager) return false;

	UQuestDefinition* Def = Manager->GetQuestDefinition(QuestID);
	if (!Def)
	{
		OutReason = FText::FromString(TEXT("任务定义不存�?));
		return false;
	}
```

改为:

```cpp
bool UQuestComponent::AcceptQuest(FName QuestID, FText& OutReason)
{
	UQuestManager* Manager = GetQuestManager();
	if (!Manager) return false;

	// 触发式加�?获取定义(GetQuestDefinition 内部兜底按需拉取,但显式调 Load 更清�?
	UQuestDefinition* Def = Manager->GetQuestDefinition(QuestID);
	if (!Def)
	{
		OutReason = FText::FromString(TEXT("任务定义不存�?));
		return false;
	}

	// 连带加载�?Giver 的全部任�?�?GetAvailableQuests 直接查内�?
	if (Def->GiverTag.IsValid())
	{
		Manager->LoadQuestsByGiver(Def->GiverTag);
	}
	// 预加载后续任务链(�?NextQuestID 递归,只加载自身定义不连带 Giver)
	Manager->LoadQuestChain(QuestID);
```

> �?`GetQuestDefinition` 已在 Task 3 改为兜底加载,因此 `Def` 此时必定已加�?若任务已注册)。后�?`LoadQuestsByGiver` / `LoadQuestChain` 在此基础上扩散加载关联任务�?
- [ ] **Step 2: 确认 AcceptQuest 其余逻辑不变**

确认�?71 行起的状态检�?`QuestInstances.Contains` / `CompletedQuestIDs.Contains` / `CheckAcceptConditions` / `CreateQuestInstance` / 广播)均不变�?
- [ ] **Step 3: 编译验证**

Run: `"F:\EpicGame\UE_5.7\Engine\Build\BatchFiles\Build.bat" QuestSystemEditor Win64 DebugGame -Project="F:\UEProject\Inventory\Inventory.uproject" -WaitMutex -FromMsBuild`
Expected: 编译通过无错�?
- [ ] **Step 4: Commit**

```bash
git add Source/QuestSystem/Private/QuestComponent.cpp
git commit -m "feat(quest): AcceptQuest 接入触发式加�?连带 Giver + 预加载链)"
```

---

## Task 6: LoadFromData 接入按需加载

**Files:**
- Modify: `Source/QuestSystem/Private/QuestComponent.cpp:392-433`(LoadFromData 方法)

- [ ] **Step 1: �?LoadFromData 恢复实例前加载定�?*

�?`LoadFromData`(�?392-433 �?�?恢复实例的循环入口改造�?
当前代码(�?401-405 �?:

```cpp
	for (const FQuestInstanceSaveData& InstData : Data.ActiveQuests)
	{
		if (!Manager) continue;
		UQuestDefinition* Def = Manager->GetQuestDefinition(InstData.QuestID);
		if (!Def) continue;  // 定义已移�?跳过
```

改为:

```cpp
	for (const FQuestInstanceSaveData& InstData : Data.ActiveQuests)
	{
		if (!Manager) continue;
		// 触发式加�?存档恢复时确保进行中任务的定义已加载(GetQuestDefinition 兜底拉取)
		UQuestDefinition* Def = Manager->GetQuestDefinition(InstData.QuestID);
		if (!Def) continue;  // 定义已移除或未注�?跳过
```

> �?`GetQuestDefinition` �?Task 3 已改为兜底加�?此处注释更新即可,逻辑实际已由兜底覆盖。此 Step 确保注释明确反映触发式加载语义�?
- [ ] **Step 2: 编译验证**

Run: `"F:\EpicGame\UE_5.7\Engine\Build\BatchFiles\Build.bat" QuestSystemEditor Win64 DebugGame -Project="F:\UEProject\Inventory\Inventory.uproject" -WaitMutex -FromMsBuild`
Expected: 编译通过无错�?
- [ ] **Step 3: Commit**

```bash
git add Source/QuestSystem/Private/QuestComponent.cpp
git commit -m "docs(quest): LoadFromData 注释明确触发式加载语�?
```

---

## Task 7: 更新接入指南文档

**Files:**
- Modify: `docs/QuestSystem接入指南.md`(第三步注册任务定义部�?

- [ ] **Step 1: 更新第三步说�?*

�?`docs/QuestSystem接入指南.md` 第三�?方式 A:DataTable(推荐)"的表格之后、注册代码示例之�?插入触发式加载说明�?
找到第三步中:

```
Objective/Condition 子类在行里�?*蓝图�?*(TSubclassOf 下拉),子类参数(�?Kill �?TargetEnemyTags、RequiredCount)在该子类蓝图里配置�?*奖励** `Rewards` 是纯数据数组(物品�?ID + 数量),直接在行里填,经验/金币等统一�?`ItemRowID` 表示(�?`Exp`、`Item.Gold`)�?*`GiverTag`** 是可选的任务给予�?NPC)tag,留空=任意处可�?填具�?tag 后可�?`GetAvailableQuests(GiverTag)` 查询�?NPC 当前可接任务(见下�?通过给予者触发接�?)�?```

在其后追加一�?

```

> **触发式加�?*:`RegisterQuestDefinitionsFromDataTable` 注册时只建立 QuestID→行名的轻量索引,**不会立即构�?* `UQuestDefinition` 对象。定义对象在以下时机按需构�?
> - **接取任务�?*:`AcceptQuest` 自动加载该任务定�?并连带加载同 GiverTag 的全部任�?�?`GetAvailableQuests` 直接查内�?,同时预加�?`NextQuestID` 后续链�?> - **存档恢复�?*:`LoadFromData` 自动加载进行中任务的定义�?> - **查询兜底**:`GetQuestDefinition` 未命中时自动按需拉取�?>
> 这意味着任务量大�?只有玩家实际接触的任务定义才会驻留内�?降低内存占用�?```

- [ ] **Step 2: Commit**

```bash
git add docs/QuestSystem接入指南.md
git commit -m "docs(quest): 接入指南补充触发式加载说�?
```

---

## Task 8: 最终编译验�?
**Files:**
- 无文件改�?仅验�?
- [ ] **Step 1: 全量编译 QuestSystem 模块**

Run: `"F:\EpicGame\UE_5.7\Engine\Build\BatchFiles\Build.bat" QuestSystemEditor Win64 DebugGame -Project="F:\UEProject\Inventory\Inventory.uproject" -WaitMutex -FromMsBuild`
Expected: 编译通过,无错误无警告

- [ ] **Step 2: 确认无回�?*

检查编译输出无以下问题:
- unresolved external symbol(`LoadQuestDefinition` / `LoadQuestsByGiver` / `LoadQuestChain` 均已实现)
- 未使用变量警�?- 隐式转换警告

若全部通过,触发式定义加载功能实现完成�?
---

## Self-Review 记录

**Spec 覆盖检�?**
- �?数据结构(QuestDataSource / QuestRowMap)�?Task 1
- �?RegisterQuestDefinitionsFromDataTable 索引�?�?Task 2
- �?LoadQuestDefinition �?Task 3
- �?LoadQuestsByGiver �?Task 4
- �?LoadQuestChain(深度上限 32 + 循环检�?�?Task 4
- �?GetQuestDefinition 兜底 �?Task 3
- �?AcceptQuest 连带 Giver + 预加载链 �?Task 5
- �?LoadFromData 存档恢复加载 �?Task 6
- �?接入指南文档更新 �?Task 7
- �?Deinitialize 清理新字�?�?Task 2

**占位符扫�?** �?TBD/TODO,所有代码步骤含完整代码�?
**类型一致�?** `LoadQuestDefinition` 返回 `UQuestDefinition*`、`LoadQuestsByGiver`/`LoadQuestChain` 返回 `int32`,在声�?Task 1)与实�?Task 3/4)中一�?`QuestDataSource`/`QuestRowMap` 字段名在所�?Task 中一致�?