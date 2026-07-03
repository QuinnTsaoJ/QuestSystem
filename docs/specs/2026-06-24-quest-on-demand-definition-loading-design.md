# 触发式任务定义加载设计

## 背景与动机

当前 `UQuestManager::RegisterQuestDefinitionsFromDataTable` 在游戏启动时(GameMode BeginPlay)一次性遍历整张 DataTable,为每行 `NewObject<UQuestDefinition>` 并全部塞入 `DefinitionTable` 常驻内存。任务量大时(几十至上百个),包括玩家永远不会触发的后续链、接不到的高等级任务在内的所有定义均被构造并常驻,且挂在 `GetTransientPackage()` 下不会被 GC 回收。

目标:将定义构造从"启动全量"改为"按需触发",降低内存占用;同时在系统启动(存档恢复)时自动加载玩家进行中的任务定义,保证存档恢复路径可用。

## 目标

- DataTable 注册时只建轻量索引(QuestID → 行名),不构造 `UQuestDefinition` 对象。
- 定义对象在以下三个时机按需构造并进入 `DefinitionTable`:
  1. **接取任务时** —— `AcceptQuest` 发现定义未加载,按需构造;同时连带加载同 Giver 的全部任务定义。
  2. **存档恢复时** —— `LoadFromData` 对每个 `ActiveQuests` 中的 QuestID 按需构造定义,再恢复实例。
  3. **查询兜底** —— `GetQuestDefinition` 未命中时自动按需拉取,防止遗漏调用点。
- 接取任务时递归预加载 `NextQuestID` 链(仅加载链上任务自身定义,不连带其 Giver)。
- 复用现有 `FQuestSaveData` 结构,不新增存档字段;插件不自管持久化,仍由游戏层通过 `SaveToData/LoadFromData` 持久化。

## 非目标

- 不改动 `UQuestInstance` 创建逻辑(本就按需)。
- 不改动 6 种 Objective / 4 种 Condition。
- 不改动 `FQuestSaveData` 结构。
- 不引入插件自管的配置文件/磁盘持久化。
- 不实现已加载定义的自动卸载(本次仅做按需加载,卸载留待后续)。
- 不改动 `UPrimaryDataAsset` 的 Asset Manager 按需加载机制(本次聚焦 DataTable 源)。

## 数据结构

### `UQuestManager` 新增字段

```cpp
// 已注册的 DataTable 源(QuestID → 所属 DataTable,用于按行名反查行数据)
UPROPERTY()
TMap<FName, TObjectPtr<UDataTable>> QuestDataSource;

// QuestID → 行名索引(注册时建立,轻量,不持有行数据对象)
TMap<FName, FName> QuestRowMap;

// 已按需构造的定义(热数据,只含被加载过的)
// 原有字段,语义从"全量"变为"已加载子集"
TMap<FName, TObjectPtr<UQuestDefinition>> DefinitionTable;
```

`QuestDataSource` 与 `QuestRowMap` 的关系:同一个 QuestID 在 `QuestDataSource` 记录其 DataTable,在 `QuestRowMap` 记录其行名,两者配合可定位到具体行数据。

## 改动清单

### 修改

| 文件 | 改动 |
|---|---|
| `Public/QuestManager.h` | 新增 `QuestDataSource`、`QuestRowMap` 字段;新增 `LoadQuestDefinition(QuestID)`、`LoadQuestsByGiver(GiverTag)`、`LoadQuestChain(QuestID, int32 MaxDepth)` 声明;`RegisterQuestDefinitionsFromDataTable` 返回值语义改为"已索引的任务数量" |
| `Private/QuestManager.cpp` | `RegisterQuestDefinitionsFromDataTable` 改为只建索引不构造对象;实现 `LoadQuestDefinition`(查索引→找行→构造→入表);实现 `LoadQuestsByGiver`(遍历 `QuestRowMap` 找同 Giver 行批量构造);实现 `LoadQuestChain`(沿 `NextQuestID` 递归,深度上限 32 防失控);`GetQuestDefinition` 未命中时自动调 `LoadQuestDefinition` 兜底;`RegisterQuestDefinition`(单资产注册)保持直接入表(资产方式本就是显式加载,不索引化) |
| `Private/QuestComponent.cpp` | `AcceptQuest` 开头:定义未加载时调 `LoadQuestDefinition`;加载成功后若 `GiverTag` 有效则调 `LoadQuestsByGiver` 连带加载;再调 `LoadQuestChain` 预加载后续链;`LoadFromData` 恢复实例前,对每个 `ActiveQuests` 的 QuestID 调 `LoadQuestDefinition` 确保定义在内存 |

### 保留不动

| 项 | 理由 |
|---|---|
| `RegisterQuestDefinition(UQuestDefinition*)` 单资产注册 | 资产方式本就是显式加载,无需索引化 |
| `FQuestSaveData` 结构 | 复用,不新增字段 |
| `UQuestInstance` 创建逻辑 | 本就按需 |
| Objective / Condition 子类 | 与加载机制无关 |
| `SaveToData` | 不变,仍序列化进行中实例 |
| 8 个广播委托 | 不变 |

## 数据流

### 接取任务(主路径)

```
玩家 AcceptQuest("Quest.KillWolves")
  → GetQuestDefinition("Quest.KillWolves")
     ├ 命中 DefinitionTable? → 直接返回
     └ 未命中 → LoadQuestDefinition:
          ├ 查 QuestRowMap 得行名
          ├ 查 QuestDataSource 得 DataTable
          ├ FindRow → 构造 UQuestDefinition → 入 DefinitionTable
          └ 返回
  → 定义存在,检查条件通过
  → 连带加载:若 Def->GiverTag 有效
     → LoadQuestsByGiver(GiverTag):遍历 QuestRowMap 同 Giver 行批量构造入表
  → 预加载链:LoadQuestChain(QuestID, MaxDepth=32)
     → 沿 NextQuestID 递归,每个只 LoadQuestDefinition(不连带 Giver)
  → CreateQuestInstance → 启动目标 → 广播 OnQuestAdded
```

### 存档恢复(启动路径)

```
游戏启动 → 游戏层调 RegisterQuestDefinitionsFromDataTable(建索引,不构造)
  → 游戏层调 LoadFromData(SaveData)
     → 遍历 SaveData.ActiveQuests:
        → LoadQuestDefinition(QuestID) 确保定义在内存
     → 走原有实例恢复逻辑(CreateQuestInstance + 恢复目标进度)
     → 恢复 CompletedQuestIDs / AbandonedQuestIDs / UnlockedQuestIDs
```

### 查询兜底

```
任意调用 GetQuestDefinition(QuestID)
  → 命中 DefinitionTable? → 返回
  → 未命中 → LoadQuestDefinition 兜底拉取
     → 仍找不到(索引里没有)→ 返回 nullptr(任务未注册)
```

## 关键设计决策

### 1. 索引而非全量构造

`QuestRowMap` 只存 `FName → FName`(QuestID → 行名),`QuestDataSource` 存 `FName → UDataTable*`。索引建立代价极低(遍历行名),不触发行数据读取。定义对象只在真正需要时才 `FindRow` + `NewObject`。

### 2. 接取时连带同 Giver

玩家接取任务 A(铁匠给)时,连带加载铁匠的全部任务定义。这样 `GetAvailableQuests(铁匠Tag)` 直接查内存中的 `DefinitionTable`,无需在查询时懒加载。代价是可能加载了玩家不会接的同 Giver 任务,但同 Giver 任务通常数量有限(一个 NPC 几个到十几个),可接受。

### 3. 链上任务不连带 Giver

`LoadQuestChain` 沿 `NextQuestID` 递归,每个链上任务只 `LoadQuestDefinition` 加载自身定义,不触发 `LoadQuestsByGiver`。理由:链上任务可能跨越多个 Giver(铁匠→村长→守卫),若每个都连带,一次接取可能扩散加载多个 NPC 的全部任务,违背按需初衷。链上任务的 Giver 连带加载推迟到玩家实际接取该链上任务时。

### 4. 兜底自动加载

`GetQuestDefinition` 未命中时自动调 `LoadQuestDefinition`。这保证现有调用点(如 `ArePrerequisitesMet`、`GetAvailableQuests` 内部)无需逐个改造,降低接入风险。兜底只加载单个定义,不连带 Giver/链(避免在只读查询中触发扩散加载)。

### 5. 深度上限防失控

`LoadQuestChain` 设 `MaxDepth = 32`。正常任务链不会这么长;若配置错误导致循环引用(A→B→A),深度上限防止无限递归。超出上限时输出警告日志并停止。

### 6. 资产注册方式不索引化

`RegisterQuestDefinition(UQuestDefinition*)`(单资产方式)保持直接入 `DefinitionTable`。资产方式本就是开发者显式持有资产引用并注册,属于主动加载,无需索引化。索引化只针对 DataTable 批量方式。

## 与现有接入指南的兼容性

| 接入指南步骤 | 改动影响 |
|---|---|
| 第三步 注册任务定义 | `RegisterQuestDefinitionsFromDataTable` 签名不变,返回值语义从"已构造数量"变为"已索引数量"(对调用方无感) |
| 第五步 接取/放弃/完成 | `AcceptQuest` 签名不变,内部增加按需加载,对调用方透明 |
| 存档 Save/Load | `SaveToData/LoadFromData` 签名不变,`LoadFromData` 内部增加定义加载,对调用方透明 |

接入指南无需改动 API 示例,仅需在"第三步"补充说明:注册后定义不会立即构造,而是在接取/存档恢复时按需构造。

## 风险与缓解

| 风险 | 缓解 |
|---|---|
| `GetQuestDefinition` 兜底加载在热路径(如 UI 每帧查询)被频繁触发,导致重复 `FindRow` | `LoadQuestDefinition` 入口先查 `DefinitionTable`,已命中直接返回,不重复构造;`FindRow` 只在首次未命中时触发一次 |
| 任务链存在循环引用(A→B→A)导致 `LoadQuestChain` 无限递归 | `MaxDepth = 32` 硬上限 + 递归过程中检测已访问集合,遇环即停并告警 |
| `LoadFromData` 时定义已在 `DefinitionTable`(上次会话加载过且未卸载) | `LoadQuestDefinition` 入口查表短路,已存在不重复构造 |
| 同一 QuestID 在多张 DataTable 注册 | `RegisterQuestDefinitionsFromDataTable` 跳过已索引的 QuestID(沿用现有 `DefinitionTable.Contains` 逻辑,改为 `QuestRowMap.Contains`) |
| `QuestDataSource` 的 `UDataTable*` 被卸载(GC) | `UPROPERTY()` 强引用持有,只要 Manager 存活则 DataTable 不被 GC |

## 编译验证

按 `ue-cpp-build-debug` 技能用 UBT 编译 QuestSystem 模块,确认无错误无警告。
