# QuestSystem 接入指南

一个事件驱动、零业务依赖的 UE5 任务系统插件。任务系统只管生命周期(接取/追踪/完成),具体业务(击杀/拾取/对话)由你的系统完成后**发事件通知**任务系统。奖励为纯数据,任务系统只记录与通知,不生成物品。

## 核心特性

- **触发式加载**:注册时只建索引,定义对象在接取/查询时按需构造
- **可组合双轨判定**:目标通过 `bRespondToEvents`/`bNeedsTick` 两个开关组合出三种机制——纯事件型(配置驱动)、纯持续型(Tick 驱动)、混合型(两者兼有)
- **事件统计**:自动按事件/目标 Tag 累积统计,供玩家查看游戏数据或扩展成就系统
- **纯数据奖励**:任务完成广播 `OnQuestCompleted(实例)`,游戏层读 `Inst->Rewards` 自行发放

---

## 前置条件

1. 插件 **QuestSystem** 已启用。
2. 25 个 GameplayTag 已由 C++ Native 注册,编辑器 Tag 选择器可见全部内置 tag。敌人(仅野兽)/区域(仅城镇)/NPC/职业及交互(仅门/宝箱)的具体档位由项目按需注册。
3. 你已有可用的背包/经验系统(任务系统通过接口调用,不直接依赖)。

---

## 第一步:实现玩家任务接口

`IQuestInterface` 是任务系统与游戏层的唯一硬契约。让你的 **PlayerController** 实现它。

```cpp
// MyPlayerController.h
#include "GameFramework/PlayerController.h"
#include "Interfaces/QuestInterface.h"
#include "QuestTags.h"
#include "MyPlayerController.generated.h"

class UQuestComponent;

UCLASS()
class AMyPlayerController : public APlayerController, public IQuestInterface
{
    GENERATED_BODY()

public:
    // === 物品(提交/退还用,ItemRowID 对应背包 DataTable 行名)===
    virtual void GrantItem_Implementation(FName ItemRowID, int32 Amount) override
    { InventoryComponent->AddItemByID(ItemRowID, Amount); }

    virtual void RemoveItem_Implementation(FName ItemRowID, int32 Amount) override
    { InventoryComponent->RemoveItemByID(ItemRowID, Amount); }

    virtual int32 GetItemCount_Implementation(FName ItemRowID) const override
    { return InventoryComponent ? InventoryComponent->CountItemByID(ItemRowID) : 0; }

    // === 货币查询(等级/声望条件用)===
    virtual int32 GetCurrency_Implementation(FGameplayTag CurrencyTag) const override
    {
        if (CurrencyTag == TAG_Quest_Attribute_等级) return PlayerLevelComponent->GetLevel();
        return CurrencyComponent ? CurrencyComponent->Get(CurrencyTag) : 0;
    }

    // === 任务组件定位(供 TryAcceptQuest 等便捷入口用)===
    virtual UQuestComponent* GetQuestComponent_Implementation() const override
    { return QuestComponent; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
    UQuestComponent* QuestComponent;
};
```

5 个方法说明:

| 方法 | 用途 |
|------|------|
| `GrantItem` | 发放物品(回滚退还用) |
| `RemoveItem` | 扣除物品(提交任务时用) |
| `GetItemCount` | 查询持有数量(提交前检查背包) |
| `GetCurrency` | 查货币/等级/声望(接取条件用) |
| `GetQuestComponent` | 返回玩家身上的 QuestComponent(TryAcceptQuest 等用) |

> 任务奖励(`FQuestReward`)是纯数据,**不通过本接口发放**——见第六步。

---

## 第二步:挂 QuestComponent

在 PlayerController 上添加 `UQuestComponent`。

```cpp
// MyPlayerController.cpp 构造函数
QuestComponent = CreateDefaultSubobject<UQuestComponent>(TEXT("QuestComponent"));
```

蓝图方式:PlayerController 蓝图 → Components 面板 → Add → 搜 `QuestComponent`。

组件挂上后自动注册到 `UQuestManager`,开始接收事件。

---

## 第三步:注册任务定义

### 方式 A:DataTable(推荐,任务多时用)

1. 编辑器 → Content Browser → 右键 → Miscellaneous → DataTable
2. 行结构选 **FQuestDefinitionRow**
3. 填任务配置,每行一个任务:

| QuestID | DisplayName | GiverTag | NextQuestID | ObjectiveClasses | Rewards | TimeLimit |
|---------|-------------|----------|-------------|------------------|---------|-----------|
| Quest.KillWolves | 消灭狼群 | 任务插件.目标.NPC.铁匠 | Quest.Report | [UQuestObjective] | [{ItemRowID=Exp,Amount=100},{ItemRowID=Item.Gold,Amount=50}] | -1 |

4. 游戏启动时注册:

```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    UQuestManager* QuestMgr = GetGameInstance()->GetSubsystem<UQuestManager>();
    if (QuestTable)
    {
        int32 Count = QuestMgr->RegisterQuestDefinitionsFromDataTable(QuestTable);
        UE_LOG(LogTemp, Log, TEXT("索引了 %d 个任务"), Count);
    }
}
```

> 此调用**只建索引**(QuestID → 行名),不构造定义对象。定义对象在接取/查询时按需构造。

### 方式 B:UQuestDefinition 资产(任务少时用)

创建 UQuestDefinition 派生蓝图,调 `QuestMgr->RegisterQuestDefinition(DefAsset)` 逐个注册。直接构造并缓存(不走索引)。两种方式可混用。

---

## 第四步:配置任务目标

`ObjectiveClasses` 里选 `UQuestObjective`。每个目标的字段直接在该目标实例上配置。

### UQuestObjective 配置字段

| 字段 | 类型 | 说明 | 默认值 |
|------|------|------|--------|
| `bRespondToEvents` | bool | 是否响应事件(HandleEvent 处理) | true |
| `bNeedsTick` | bool | 是否需要每帧Tick(OnObjectiveTick 处理) | false |
| `ListenEvents` | TagContainer | 监听什么事件 | 空 |
| `TargetTags` | TagContainer | 匹配什么目标(空=通配) | 空 |
| `RequiredCount` | int32 | 需要几次。0=即完成;>0=计数达标 | 1 |
| `RequiredItems` | FQuestRequiredItem[] | 物品需求列表(多种不同数量) | 空 |
| `bConsumeItems` | bool | true=扣除;false=仅检测持有 | true |
| `DisplayName` | FText | 显示名(UI 进度文本) | — |

### 纯事件型配置示例(bRespondToEvents=true, bNeedsTick=false)

| 场景 | 配置 |
|------|------|
| 杀 5 只野兽 | ListenEvents=[事件.击杀], TargetTags=[目标.敌人.野兽], RequiredCount=5 |
| 到达城镇 | ListenEvents=[事件.到达], TargetTags=[目标.区域.城镇], RequiredCount=0 |
| 提交 3 草药+2 矿石 | ListenEvents=[事件.对话], TargetTags=[目标.NPC.铁匠], RequiredCount=0, RequiredItems=[{Item.Herb,3},{Item.Ore,2}], bConsumeItems=true |
| 检测持有钥匙不扣除 | RequiredItems=[{Item.Key,1}], bConsumeItems=false, RequiredCount=0 |

---

## 第五步:外部系统发事件

业务发生时,构造 `FQuestEventPayload` 调 `QuestManager->NotifyEvent`。

### FQuestEventPayload 字段

| 字段 | 说明 |
|------|------|
| `EventTag` | 事件类型(如 任务插件.事件.击杀) |
| `TargetTag` | 目标标识(如 任务插件.目标.敌人.野兽) |
| `Amount` | 数量(击杀1只/拾取3个) |
| `SourceActor` | 关联 Actor(可选,游戏层自行解释) |
| `CustomData` | 扩展数据(可选,游戏层自行解释) |

### 击杀示例

```cpp
void UMyCombatComponent::OnEnemyKilled(AActor* Killer, AActor* Victim)
{
    FQuestEventPayload Payload;
    Payload.EventTag = TAG_Quest_Event_击杀;
    Payload.TargetTag = TAG_Quest_Target_敌人_野兽;
    Payload.Amount = 1;

    GetGameInstance()->GetSubsystem<UQuestManager>()->NotifyEvent(Payload);
}
```

### 到达示例(碰撞箱触发)

```cpp
void AQuestRegionTrigger::OnTriggerEnter(...)
{
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !Pawn->IsLocallyControlled()) return;
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;

    FQuestEventPayload Payload;
    Payload.EventTag = TAG_Quest_Event_到达;
    Payload.TargetTag = TAG_Quest_Target_区域_城镇;
    Payload.Amount = 1;

    UQuestManager* QuestMgr = GetGameInstance()->GetSubsystem<UQuestManager>();

    // 推荐:定向通知(走 IQuestInterface 拿 Component,多人安全)
    if (PC->Implements<UQuestInterface>())
    {
        UQuestComponent* QuestComp = IQuestInterface::Execute_GetQuestComponent(PC);
        if (QuestComp) QuestMgr->NotifyEventToPlayer(QuestComp, Payload);
    }
    // 单机兜底:全量广播
    // QuestMgr->NotifyEvent(Payload);
}
```

> **NotifyEvent vs NotifyEventToPlayer**:两者都调 `Component->HandleEvent`,判定逻辑一致,区别只在分发范围。`NotifyEventToPlayer` 只通知指定玩家(多人安全),`NotifyEvent` 广播全部玩家(单机用)。

**两层匹配**:系统先用 `ListenEvents` 过滤事件类型,再用 `TargetTags` 二次匹配目标。发事件时 EventTag + TargetTag 必须与目标配置一致。

### 通配(任意目标)

`TargetTags` 留空即通配——只匹配事件类型,不限制具体目标:

| 场景 | 配置 |
|------|------|
| 任意击杀 5 个敌人 | ListenEvents=[事件.击杀], TargetTags 留空, RequiredCount=5 |
| 与任意 NPC 对话 | ListenEvents=[事件.对话], TargetTags 留空, RequiredCount=0 |

---

## 第六步:在完成回调里发放奖励

任务系统**只记录与通知,不生成物品**。任务完成时广播 `OnQuestCompleted(UQuestInstance*)`,你订阅后读 `Inst->Rewards` 自行发放。

```cpp
// BeginPlay
QuestComponent->OnQuestCompleted.AddDynamic(this, &AMyPlayerController::HandleQuestCompleted);

UFUNCTION()
void AMyPlayerController::HandleQuestCompleted(UQuestInstance* Quest)
{
    if (!Quest) return;
    for (const FQuestReward& Rew : Quest->Rewards)
    {
        DispatchReward(Rew.ItemRowID, Rew.Amount);  // 按 ItemRowID 解释并发放
    }
}
```

---

## UI 订阅

任务 UI 直接订阅 `QuestComponent` 的 9 个委托,无需轮询。

| 委托 | 触发时机 | UI 动作 |
|------|---------|---------|
| `OnQuestAdded` | 接取成功 | 列表追加项 |
| `OnQuestAcceptFailed` | 接取失败 | 弹提示(等级不足等) |
| `OnQuestUpdated` | 进度变化 | 刷新进度 |
| `OnQuestCompleted` | 任务完成 | 移到"已完成"区 |
| `OnQuestFailed` | 任务失败 | 标红/移除 |
| `OnQuestAbandoned` | 放弃任务 | 移除项 |
| `OnQuestUnlocked` | 解锁后续 | 弹"新任务可接" |
| `OnObjectiveCompleted` | 单目标完成 | 勾选该目标 |
| `OnQuestGuideUpdated` | 指引变化 | 刷新 HUD(指引点数据由游戏层维护) |

初始化用 `GetActiveQuests()` 拉全量列表;点开任务用 `GetQuestViewData(QuestID)` 取详情。

### 接取/放弃/完成

```cpp
FText Reason;
QuestComp->AcceptQuest(TEXT("Quest.KillWolves"), Reason);   // 接取
QuestComp->AbandonQuest(TEXT("Quest.KillWolves"));          // 放弃
QuestComp->CompleteQuest(TEXT("Quest.KillWolves"));         // 完成通常自动触发
```

### 碰撞箱触发接取

```cpp
void AQuestGiverTrigger::OnTriggerEnter(...)
{
    APlayerController* PC = ...;
    UQuestManager* QuestMgr = GetGameInstance()->GetSubsystem<UQuestManager>();
    FText Reason;
    QuestMgr->TryAcceptQuest(PC, QuestIDToAccept, Reason);  // 内部走 IQuestInterface 找组件
}
```

> `TryAcceptQuest` 检查可接性并接取。成功广播 `OnQuestAdded`,失败广播 `OnQuestAcceptFailed`。触发侧不需预先查可接列表。

---

## 目标判定机制(三种可组合)

目标通过 `bRespondToEvents`/`bNeedsTick` 两个开关组合出三种机制。

### 机制一:纯事件型(bRespondToEvents=true, bNeedsTick=false)

靠离散事件推进,纯数据配置,无需写代码。覆盖击杀/到达/拾取/提交/对话。

**判定流程**(HandleEvent → OnEventReceived 默认实现):
```
事件到达 → bRespondToEvents? → ListenEvents匹配? → TargetTags匹配?
  → RequiredItems检查(够数?bConsumeItems决定扣除?) → CurrentCount += Amount
  → RequiredCount=0 或 CurrentCount>=RequiredCount? → MarkCompleted
```

### 机制二:纯持续型(bRespondToEvents=false, bNeedsTick=true)

靠每帧 Tick 推进,继承 `UQuestObjective` 重写 `OnObjectiveTick`。覆盖守卫/护送/生存。

**示例:守卫某地 60 秒**

```cpp
UCLASS()
class UMyObjective_Defend : public UQuestObjective
{
    GENERATED_BODY()
public:
    UMyObjective_Defend()
    {
        bRespondToEvents = false;
        bNeedsTick = true;
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defend")
    FVector DefendLocation = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defend")
    float DefendRadius = 500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Defend")
    float RequiredTime = 60.f;

    float ElapsedTime = 0.f;

    virtual void OnObjectiveTick_Implementation(float DeltaTime, UObject* Owner) override
    {
        APawn* Pawn = Cast<APawn>(Owner);
        if (!Pawn) return;
        float DistSq = FVector::DistSquared2D(Pawn->GetActorLocation(), DefendLocation);
        if (DistSq > DefendRadius * DefendRadius) { ElapsedTime = 0.f; return; }  // 离开重置
        ElapsedTime += DeltaTime;
        CurrentCount = FMath::FloorToInt(ElapsedTime);
        if (ElapsedTime >= RequiredTime) MarkCompleted();
    }

    virtual FText GetProgressText_Implementation() const override
    {
        return FText::FromString(FString::Printf(
            TEXT("守卫中 %d/%d 秒"), CurrentCount, FMath::FloorToInt(RequiredTime)));
    }
};
```

### 机制三:混合型(bRespondToEvents=true, bNeedsTick=true)

事件计数 + Tick 持续逻辑同时运行。覆盖限时挑战、多阶段任务。

**示例:3 分钟内杀 10 只狼**

```cpp
UCLASS()
class UMyObjective_TimedKill : public UQuestObjective
{
    GENERATED_BODY()
public:
    UMyObjective_TimedKill()
    {
        bRespondToEvents = true;
        bNeedsTick = true;
        ListenEvents.AddTag(TAG_Quest_Event_击杀);
        TargetTags.AddTag(TAG_Quest_Target_敌人_野兽);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TimedKill")
    float TimeLimit = 180.f;

    float RemainingTime = 0.f;

    virtual void OnObjectiveTick_Implementation(float DeltaTime, UObject* Owner) override
    {
        RemainingTime -= DeltaTime;
        if (RemainingTime <= 0.f) MarkFailed();  // 超时失败
    }
    // OnEventReceived 用默认实现(计数+达标),无需重写

    virtual FText GetProgressText_Implementation() const override
    {
        int32 Min = FMath::Max(0, FMath::FloorToInt(RemainingTime / 60.f));
        int32 Sec = FMath::Max(0, FMath::FloorToInt(RemainingTime) % 60);
        return FText::FromString(FString::Printf(
            TEXT("击杀 %d/%d (剩余 %d:%02d)"), CurrentCount, RequiredCount, Min, Sec));
    }
};
```

### 可 override 的虚函数

| 虚函数 | 调用时机 | 默认实现 |
|--------|---------|---------|
| `OnEventReceived(Payload, Owner)` | HandleEvent 通过过滤后 | 物品检查+计数+达标 |
| `OnObjectiveTick(DeltaTime, Owner)` | 每帧(仅 bNeedsTick=true) | 空 |
| `GetProgressText()` | UI 拉取 | 计数型 "N/M",即完成型状态文本 |

蓝图也可 override(`BlueprintNativeEvent`)。

---

## 事件统计(玩家游戏数据)

任务系统每次收到事件时自动按事件 Tag 和目标 Tag 累积统计(Amount 累加),供玩家查看游戏数据或扩展成就系统。

```cpp
int32 TotalKills = QuestComp->GetEventTagCount(TAG_Quest_Event_击杀);      // 总击杀次数
int32 BeastKills = QuestComp->GetTargetTagCount(TAG_Quest_Target_敌人_野兽); // 杀了多少野兽

for (const FTargetTagStat& Stat : QuestComp->GetAllTargetTagStats())
{
    // Stat.TargetTag + Stat.Count → 成就判定
}
```

统计数据随存档持久化,按玩家独立。

---

## 存档

### 手动存读

```cpp
FQuestSaveData QuestData = QuestComp->SaveToData();  // 存档
QuestComp->LoadFromData(QuestData);                   // 读档
```

存档内容:进行中任务快照(目标 CurrentCount + 状态 + 时间戳)、已完成/放弃/解锁 ID 缓存、事件统计数据。

### 对接 SaveSystem 插件(自动存读)

QuestSystem 不依赖 SaveSystem 插件，但提供了 `SaveToData()`/`LoadFromData()` 供游戏层桥接。在 PlayerController 上实现 `ISaveSystemClient` 接口（3 个方法 + 注册/反注册），SaveSystem 存读档时会自动调 `GatherSaveData`/`ApplySaveData` 收集/恢复任务数据。

#### C++ 实现

```cpp
// MyPlayerController.h
#include "GameFramework/PlayerController.h"
#include "Interfaces/QuestInterface.h"          // QuestSystem 的接口
#include "ISaveSystemClient.h"                   // SaveSystem 的接口
#include "SaveSystemSaveGame.h"                  // SaveSystem 数据载体基类
#include "QuestSaveData.h"                        // QuestSystem 存档结构
#include "MyPlayerController.generated.h"

// 1. 定义任务数据载体(继承 USaveSystemSaveGameData,放强类型字段)
UCLASS()
class UQuestSaveCarrier : public USaveSystemSaveGameData
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame, BlueprintReadWrite, Category="Quest")
    FQuestSaveData QuestData;
};

// 2. PlayerController 同时实现 IQuestInterface 和 ISaveSystemClient
UCLASS()
class AMyPlayerController : public APlayerController, public IQuestInterface, public ISaveSystemClient
{
    GENERATED_BODY()

    // ... IQuestInterface 的 5 个方法(见第一步)...

    // === ISaveSystemClient:重写 _Implementation ===
    virtual FName GetClientName_Implementation() const override
    { return TEXT("Quest"); }

    virtual USaveSystemSaveGameData* GatherSaveData_Implementation() override
    {
        UQuestSaveCarrier* Carrier = NewObject<UQuestSaveCarrier>(this);
        Carrier->QuestData = QuestComponent->SaveToData();  // 复用 QuestSystem 已有接口
        return Carrier;
    }

    virtual void ApplySaveData_Implementation(USaveSystemSaveGameData* Data) override
    {
        if (UQuestSaveCarrier* Carrier = Cast<UQuestSaveCarrier>(Data))  // 判空 + Cast
        {
            QuestComponent->LoadFromData(Carrier->QuestData);
        }
    }

protected:
    virtual void BeginPlay() override
    {
        Super::BeginPlay();
        USaveSystemBPLibrary::RegisterSaveClient(this);  // 必须先注册
        // 若需启动加载:在注册后调 LoadGame
    }

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
    {
        USaveSystemBPLibrary::UnregisterSaveClient(this);  // 必须反注册
        Super::EndPlay(EndPlayReason);
    }
};
```

> **注意**：`RegisterSaveClient` 必须在 `LoadGame` 之前调用——SaveSystem 遍历已注册客户端注入数据，未注册的客户端收不到 `ApplySaveData`。

#### Blueprint 实现

1. PlayerController 蓝图 → Class Settings → Interfaces → Add `SaveSystemClient`
2. 创建 `UQuestSaveCarrier` 的蓝图子类（父类选 `SaveSystemSaveGameData`），加一个 `Quest Save Data` 变量
3. Override 三个事件：

```
事件:GetClientName (Override) → Return "Quest"

事件:GatherSaveData (Override)
  → Construct Object (QuestSaveCarrier) → Set QuestData = QuestComponent.SaveToData → Return

事件:ApplySaveData (Override)
  Data → Cast To QuestSaveCarrier → Get QuestData → QuestComponent.LoadFromData

Event BeginPlay → Register Save Client(self)
Event EndPlay   → Unregister Save Client(self)
```

#### 对接检查清单

- [ ] PlayerController 实现了 `ISaveSystemClient`（3 个方法）
- [ ] `BeginPlay` 调了 `RegisterSaveClient`，`EndPlay` 调了 `UnregisterSaveClient`
- [ ] `LoadGame` 在 `RegisterSaveClient` **之后**调用
- [ ] 数据载体继承 `USaveSystemSaveGameData`，字段标了 `SaveGame`
- [ ] `ApplySaveData` 里对 `Data` 做了 `Cast` + 判空

对接完成后，SaveSystem 的 `SaveGame`/`LoadGame` 会自动存读任务数据，游戏层无需手动调 `SaveToData`/`LoadFromData`。

---

## 触发式加载

| 时机 | 触发点 | 行为 |
|------|--------|------|
| 接取 | `AcceptQuest` | 沿 `NextQuestID` 预加载整条链 |
| 查询 | `GetQuestDefinition` | 未命中缓存时自动加载 |
| 读档 | `LoadFromData` | 经 `GetQuestDefinition` 兜底加载 |

手动加载接口(通常无需手动调):
```cpp
QuestMgr->LoadQuestChain(QuestID);        // 预加载任务链
QuestMgr->LoadQuestsByGiver(GiverTag);    // 按给予者加载
QuestMgr->LoadQuestDefinition(QuestID);   // 加载单个
```

---

## 完整时序

```
[玩家接取]
  → AcceptQuest → 检查条件 → 实例化目标 → 目标状态置 InProgress
    ├ LoadQuestChain(预加载后续链)
    ├ 广播 OnQuestAdded
    └ 广播 OnQuestGuideUpdated

[玩家击杀野狼]
  → NotifyEvent(击杀, 敌人.野兽)
    → HandleEvent:
      1. bRespondToEvents ✓
      2. ListenEvents 匹配 ✓
      3. TargetTags 匹配 ✓
      4. OnEventReceived: CurrentCount += 1 (3/5)
    → 广播 OnQuestUpdated

[击杀第5只]
  → OnEventReceived: CurrentCount=5/5 → MarkCompleted
    → 全目标完成 → CompleteQuest
      ├ 广播 OnQuestCompleted(Inst) ← 游戏层读 Rewards 发放
      └ TryUnlockNextQuest
```

---

## 常见问题

**Q: 持续型任务(守卫/护送/生存)怎么做?**
设 `bNeedsTick=true`,继承重写 `OnObjectiveTick`。见"机制二"。

**Q: 纯配置表达不了的复杂逻辑怎么办?**
继承重写 `OnEventReceived`(事件判定)或 `OnObjectiveTick`(持续逻辑)或两者。见"目标判定机制"。

**Q: 限时任务怎么配?**
任务级限时用 `QuestDefinition.TimeLimit`(Tick 检查超时失败)。目标级限时用混合型目标(事件计数+Tick倒计时),见"机制三"。

**Q: 放弃任务后物品没退回?**
`bConsumeItems=true` 的目标在判定时已扣除,放弃时重置计数但不自动退还——如需退还,游戏层在 `OnQuestAbandoned` 回调里自行处理。

**Q: 失败的任务能重接吗?**
能。失败即回滚+移除实例,直接再 `AcceptQuest`。

---

## 内置类型速查

| 类别 | 类型 | 用途 |
|------|------|------|
| Objective | `UQuestObjective`(可继承) | 纯配置覆盖事件型;继承重写覆盖持续型/混合型 |
| Reward | `FQuestReward`(纯结构体) | 物品行 ID + 数量 |
| Condition | Level / Reputation / PrerequisiteQuest / Class | 等级/声望/前置任务/职业 |

---

## Tag 速查(已 Native 注册)

代码中直接用全局变量(如 `TAG_Quest_Event_击杀`),需 `#include "QuestTags.h"`。

### 事件标签

| 全局变量 | Tag 名 |
|----------|--------|
| TAG_Quest_Event_击杀/拾取/到达/对话/交互/自定义 | 任务插件.事件.击杀 等 |

### 目标标签

| 全局变量 | Tag 名 | 用途 |
|----------|--------|------|
| TAG_Quest_Target_自定义 | 任务插件.目标.自定义 | 自定义目标 |
| TAG_Quest_Target_敌人_野兽 | 任务插件.目标.敌人.野兽 | 击杀目标 |
| TAG_Quest_Target_物品_草药/矿石/材料/任务物品 | 任务插件.目标.物品.草药 等 | 收集/提交 |
| TAG_Quest_Target_区域_城镇 | 任务插件.目标.区域.城镇 | 到达 |
| (NPC 未内置) | 任务插件.目标.NPC.* | 对话/提交(项目按需注册) |
| TAG_Quest_Target_交互_门/宝箱 | 任务插件.目标.交互.门 等 | 交互 |

### 货币/属性标签

| 全局变量 | Tag 名 | 用途 |
|----------|--------|------|
| TAG_Quest_Currency_金币/声望 | 任务插件.货币.金币 等 | 条件查询 |
| TAG_Quest_Attribute_等级 | 任务插件.属性.等级 | 等级条件查询 |
