# 奖励系统重构：多态 UObject → 纯数据结构体

## 背景与动机

当前任务系统的奖励是多态 `UObject`：`UQuestRewardBase` 基类 + 三个内置子类（货币 / 经验 / 物品）。任务完成时 `UQuestInstance::GrantRewards(Context)` 遍历奖励实例，逐个调用 `Grant()`，该方法进入 `IQuestRewardRecipientInterface` 的 `GrantCurrency` / `GrantItem`，由任务系统**直接**向玩家发放物品与货币。

问题：任务系统作为事件驱动的通用框架，却承担了「物品生成」这一业务职责，与「零业务依赖」的设计目标相悖。奖励类型每增加一种（如新货币），都要派生新 `UObject` 子类，扩展成本高。

## 目标

- 删除奖励类层级，改为一个纯数据结构体 `FQuestReward { ItemID, Amount }`。
- 经验、金币、声望等统一用 `ItemID` 表示，由游戏层在任务完成回调中自行解释与发放。
- 任务系统**只记录与通知**，不生成任何物品：`CompleteQuest` 不再调用任何 `Grant*`。
- 清理奖励类删除后产生的死方法 `GrantCurrency`。
- 保留 `Deliver` 目标与接取条件仍依赖的接口方法（`GrantItem` / `RemoveItem` / `GetCurrency` / `GetItemCount`）。

## 非目标

- 不改动 `Deliver` 目标的提交 / 退还逻辑（那是目标机制，非奖励机制）。
- 不改动接取条件（等级 / 声望）对 `GetCurrency` 的查询。
- 不改动存档结构（奖励来自数据表，非运行时状态，不进存档）。
- 不新增委托（`OnQuestCompleted` 已广播完整 `UQuestInstance*`，足以携带奖励数据）。

## 数据结构

```cpp
// QuestSystem/Public/Rewards/QuestReward.h
USTRUCT(BlueprintType)
struct QUESTSYSTEM_API FQuestReward
{
    GENERATED_BODY()

    /** 物品定义 ID（对应游戏层 DataTable 行名，如 "Item.Herb"）。
     *  经验 / 金币 / 声望等也统一用 ItemID 表示，如 "Exp"。
     *  任务系统不解释此 ID，由游戏层在 OnQuestCompleted 回调中自行查表发放。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Quest|Reward",
        meta=(Tooltip="物品定义ID,对应游戏层DataTable行名;经验/金币等也统一用ItemID表示,如\"Exp\""))
    FName ItemID;

    /** 数量 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Quest|Reward")
    int32 Amount = 1;
};
```

经验 / 金币 / 声望统一视为「物品」，用一个 `ItemID` 表达。具体语义由游戏层决定，任务系统对此一无所知。

## 通知机制（不改）

`UQuestComponent::OnQuestCompleted` 已是 `OnQuestCompleted.Broadcast(UQuestInstance*)`，广播完整任务实例。新设计下 `Inst->Rewards` 是 `TArray<FQuestReward>`，游戏层订阅此委托即可读取奖励数据并自行发放。**无需新增委托。**

## 改动清单

### 删除

| 文件 | 说明 |
|---|---|
| `Public/Rewards/QuestRewardBase.h` | `UQuestRewardBase` 基类 + Currency/Experience/Item 三子类 |
| `Private/Rewards/QuestRewardBase.cpp` | 上述类的实现 |

### 删除的死方法

| 位置 | 说明 |
|---|---|
| `IQuestRewardRecipientInterface::GrantCurrency` | 奖励类删除后无人调用 |

### 修改

| 文件 | 改动 |
|---|---|
| `Public/QuestDefinitionRow.h` | `TArray<TSubclassOf<UQuestRewardBase>> RewardClasses` → `TArray<FQuestReward> Rewards`；前置声明改 `struct FQuestReward`；include `QuestReward.h` |
| `Public/QuestDefinition.h` | 同上 |
| `Public/QuestInstance.h` | `TArray<TObjectPtr<UQuestRewardBase>> Rewards` → `TArray<FQuestReward> Rewards`；删除 `GrantRewards(UObject*)` 声明；更新注释（删「遍历 Rewards 执行 Grant」） |
| `Private/QuestInstance.cpp` | `InitFromDefinition` 中 `NewObject` 实例化奖励循环 → 直接 `Rewards = InDefinition->Rewards`；删除 `GrantRewards` 实现；移除 `#include "Rewards/QuestRewardBase.h"` |
| `Private/QuestComponent.cpp` | `CompleteQuest` 删除 `Inst->GrantRewards(GetOwner())` 一行；`GetViewData` 中 `GetRewardText()` 循环 → 遍历 `Inst->Rewards` 生成 `"ItemID ×Amount"` 文本；移除 `#include "Rewards/QuestRewardBase.h"` |
| `Private/QuestManager.cpp` | `Def->RewardClasses = Row->RewardClasses` → `Def->Rewards = Row->Rewards` |
| `Public/Interfaces/QuestRewardRecipientInterface.h` | 删除 `GrantCurrency` 方法声明（保留 `GrantItem` / `RemoveItem` / `GetCurrency` / `GetItemCount`） |

### 保留不动

| 项 | 理由 |
|---|---|
| `IQuestRewardRecipientInterface::GrantItem` / `RemoveItem` | `Deliver` 目标提交 / 退还任务物品依赖 |
| `IQuestRewardRecipientInterface::GetCurrency` / `GetItemCount` | 接取条件查询等级 / 声望、`Deliver` 检查背包依赖 |
| `TAG_Quest_Currency_经验` | 条件查询用，与本变更正交 |
| `FQuestSaveData` 结构 | 奖励来自数据表，非运行时状态，不进存档 |
| `QuestTypes.h` 的 `TArray<FText> RewardTexts` | 文本来源改为由结构体生成，字段本身保留 |
| `OnQuestCompleted` 委托签名 | 已广播完整 `UQuestInstance*`，足以携带奖励 |

### 文档

| 文件 | 改动 |
|---|---|
| `docs/QuestSystem接入指南.md` | 奖励章节重写：结构体配置方式、`OnQuestCompleted` 回调读取 `Inst->Rewards` 自行发放、删除旧 `Grant` 路径说明、示例代码更新 |

## 数据流

```
策划在数据表配置 Rewards: [{ItemID="Exp", Amount=100}, {ItemID="Item.Herb", Amount=5}]
    ↓
QuestManager 从数据表加载到 QuestDefinition.Rewards
    ↓
QuestInstance.InitFromDefinition 复制到 Inst->Rewards (TArray<FQuestReward>)
    ↓
玩家完成所有目标 → QuestComponent.CompleteQuest
    ↓ (不再调 GrantRewards)
OnQuestCompleted.Broadcast(Inst)
    ↓
游戏层回调: for (auto& R : Inst->Rewards) { 解释 ItemID 并发放 }
```

任务系统在整条链路中只做「复制、携带、广播」，不触碰任何 `Grant*`。

## UI 文本格式

`GetViewData` 中 `RewardTexts` 由结构体生成，格式为 `"ItemID ×Amount"`（如 `"Exp ×100"`）。友好名（中文显示名）是游戏层 UI 职责，任务系统不查表转换。

## 编译验证

按 `ue-cpp-build-debug` 技能用 UBT 编译 QuestSystem 模块，确认无错误无警告。

## 风险与缓解

| 风险 | 缓解 |
|---|---|
| 外部代码引用了已删除的 `UQuestRewardBase` / `GrantCurrency` | 已全局搜索，仅插件内部引用；游戏层尚未实现奖励发放 |
| 现有数据表使用了 `RewardClasses`（`TSubclassOf`） | 数据表内容需重新填写为 `FQuestReward` 结构体；属预期内的迁移，非破坏性回归 |
