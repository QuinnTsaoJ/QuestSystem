# QuestSystem — UE5 任务系统插件

一个事件驱动、零业务依赖的 Unreal Engine 5 任务系统插件。任务系统只管生命周期（接取/追踪/完成），具体业务（击杀/拾取/对话）由你的系统完成后**发事件通知**任务系统。奖励为纯数据，任务系统只记录与通知，不生成物品。

## 核心特性

- **事件驱动**：外部系统通过 `NotifyEvent` 上传事件，任务系统自动匹配目标并推进进度
- **可组合双轨判定**：目标通过 `bRespondToEvents`/`bNeedsTick` 两个开关组合出三种机制——纯事件型（配置驱动）、纯持续型（Tick 驱动）、混合型（两者兼有）
- **触发式加载**：注册时只建索引，定义对象在接取/查询时按需构造，启动内存开销极低
- **纯数据奖励**：任务完成广播 `OnQuestCompleted(实例)`，游戏层读 `Inst->Rewards` 自行发放
- **事件统计**：自动按事件/目标 Tag 累积统计，供玩家查看游戏数据或扩展成就系统
- **零插件依赖**：不依赖任何第三方插件，仅依赖引擎原生模块

## 快速开始

### 1. 实现玩家任务接口

让 PlayerController 实现 `IQuestInterface`（5 个方法：`GrantItem`/`RemoveItem`/`GetItemCount`/`GetCurrency`/`GetQuestComponent`）。

### 2. 挂 QuestComponent

在 PlayerController 上添加 `UQuestComponent`，运行时自动注册到 `UQuestManager`。

### 3. 注册任务定义

创建 DataTable（行结构 `FQuestDefinitionRow`），配置任务，启动时调 `RegisterQuestDefinitionsFromDataTable`。

### 4. 配置目标

`ObjectiveClasses` 里选 `UQuestObjective`，配置 `ListenEvents`/`TargetTags`/`RequiredCount` 等字段。

### 5. 外部系统发事件

构造 `FQuestEventPayload` 调 `QuestManager->NotifyEvent`。

### 6. 发放奖励

订阅 `OnQuestCompleted`，读 `Inst->Rewards` 自行发放。

> 完整接入流程见 [接入指南](docs/QuestSystem接入指南.md)。

## 架构概览

```
游戏层（插件外）
  ├─ PlayerController ──实现──▶ IQuestInterface（唯一硬契约）
  └─ 外部系统 ──发事件──▶ NotifyEvent

QuestSystem 插件
  ├─ UQuestManager（GameInstanceSubsystem）
  │   ├─ 定义表索引 + 按需加载
  │   ├─ 事件总线（NotifyEvent → 遍历 Component）
  │   └─ TryAcceptQuest（便捷接取入口）
  ├─ UQuestComponent（挂 PlayerController）
  │   ├─ HandleEvent（事件型判定）
  │   ├─ TickContinuousObjectives（持续型判定）
  │   ├─ 9 个广播委托
  │   └─ SaveToData / LoadFromData（存档）
  ├─ UQuestInstance（运行时任务实例）
  │   ├─ Objectives: UQuestObjective[]
  │   └─ Rewards: FQuestReward[]
  └─ UQuestObjective（纯数据 + 可 override 虚函数）
      ├─ bRespondToEvents / bNeedsTick（双轨开关）
      ├─ OnEventReceived（事件判定入口）
      ├─ OnObjectiveTick（持续逻辑入口）
      └─ GetProgressText（进度文本）
```

## 目标判定机制

| 机制 | 开关 | 适用场景 | 实现方式 |
|------|------|---------|---------|
| 纯事件型 | `bRespondToEvents=true, bNeedsTick=false` | 击杀/到达/拾取/提交/对话 | 纯数据配置，默认 `OnEventReceived` |
| 纯持续型 | `bRespondToEvents=false, bNeedsTick=true` | 守卫/护送/生存 | 继承重写 `OnObjectiveTick` |
| 混合型 | `bRespondToEvents=true, bNeedsTick=true` | 限时挑战/多阶段 | 两者兼用 |

## 模块结构

```
QuestSystem/
├── Source/QuestSystem/
│   ├── Public/
│   │   ├── Objectives/QuestObjective.h       # 任务目标（纯数据 + 虚函数）
│   │   ├── Conditions/QuestConditionBase.h   # 接取条件 + 4 内置子类
│   │   ├── Interfaces/QuestInterface.h        # 玩家任务接口（唯一硬契约）
│   │   ├── Rewards/QuestReward.h             # 奖励结构体（纯数据）
│   │   ├── QuestManager.h                    # 全局服务（定义表 + 事件总线）
│   │   ├── QuestComponent.h                  # 玩家代理（生命周期/查询/存档/统计）
│   │   ├── QuestInstance.h                   # 运行时任务实例
│   │   ├── QuestDefinition.h                 # 任务定义（资产方式）
│   │   ├── QuestDefinitionRow.h              # 任务定义（DataTable 行）
│   │   ├── QuestTypes.h                     # 枚举/事件载荷/视图数据/指引点
│   │   ├── QuestSaveData.h                  # 存档结构 + 事件统计结构
│   │   └── QuestTags.h                      # 25 个 Native GameplayTag
│   └── Private/                              # 实现文件
├── Content/                                  # DataTable 资产
├── docs/
│   └── QuestSystem接入指南.md                # 完整接入文档
└── QuestSystem.uplugin
```

## 内置 GameplayTag

25 个中文 Tag 由 C++ Native 注册，编辑器 Tag 选择器直接可见：

- **6 事件标签**：击杀/拾取/到达/对话/交互/自定义
- **6 指引标签**：击杀/拾取/到达/对话/交互/自定义
- **9 目标标签**：自定义/敌人(野兽)/物品(草药·矿石·材料·任务物品)/区域(城镇)/交互(门·宝箱)
- **3 货币标签**：经验/金币/声望
- **1 属性标签**：等级

## 存档

提供 `SaveToData()`/`LoadFromData()` 接口，游戏层可自行持久化。也可在 PlayerController 上实现 `ISaveSystemClient` 对接 SaveSystem 插件实现自动存读（见接入指南）。

## 环境要求

- Unreal Engine 5.5+
- C++ 项目

## 作者

一氧化二氢
