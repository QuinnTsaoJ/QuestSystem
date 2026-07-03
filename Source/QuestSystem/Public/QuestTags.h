// 任务系统 GameplayTag 定义 —— 默认内置标签,用户可在编辑器中扩展
// tag 名为中文,与项目现有风格(库存插件.*、交互.*)一致,用"任务插件"作前缀
// 共 25 个:6 事件 + 6 指引 + 9 目标(1 自定义容器 + 8 具体) + 3 货币 + 1 属性 + 0 职业
// 注:敌人(仅野兽)/区域(仅城镇)/NPC/职业及交互(仅门/宝箱)的具体档位因缺乏普遍适用性未内置,由项目按需在编辑器 Tag 面板自行注册
// QuestSystem Plugin

#pragma once

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

// === 事件标签(第一层过滤:外部系统发事件时填 EventTag) ===
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Event_击杀);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Event_拾取);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Event_到达);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Event_对话);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Event_交互);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Event_自定义);

// === 指引标签(HUD 据此选图标,插件内部用) ===
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Guide_击杀);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Guide_拾取);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Guide_到达);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Guide_对话);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Guide_交互);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Guide_自定义);  // 自定义事件目标的指引图标

// === 目标标签(第二层过滤:目标类 TargetXxxTag 配置时选,Payload.TargetTag 填) ===
// 自定义目标的容器节点,业务特殊目标建议挂在此分支下
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_自定义);

// 敌人目标(Kill 目标类的 TargetEnemyTags 选)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_敌人_野兽);   // 野狼/熊/野猪等自然野兽
// 注:人形/亡灵/首领等具体敌人档未内置(缺乏普遍适用性),由项目按需在"任务插件.目标.敌人.*"下自行注册

// 物品目标(Collect 目标类的 TargetItemTags 选)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_物品_草药);     // 草药/花卉等可采集植物
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_物品_矿石);     // 铁矿/金矿等矿物
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_物品_材料);     // 皮革/布料/木材等材料
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_物品_任务物品); // 剧情道具/钥匙/信件等

// 区域目标(Reach 目标类的 TargetRegionTag 选)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_区域_城镇);   // 村庄/城镇等安全区
// 注:副本/地标等具体区域档未内置,由项目按需在"任务插件.目标.区域.*"下自行注册

// NPC 目标(Talk / Deliver 目标类的 TargetNPCTag 选)
// 注:商人/铁匠/村长/守卫等具体 NPC 档未内置(缺乏普遍适用性),
// 由项目按需在"任务插件.目标.NPC.*"下自行注册

// 可交互物目标(Interact 目标类的 TargetInteractableTag 选)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_交互_门);     // 门/传送门
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Target_交互_宝箱);   // 宝箱/箱子
// 注:机关/符文等具体交互档未内置(缺乏普遍适用性),由项目按需在"任务插件.目标.交互.*"下自行注册


// === 货币标签(Condition_Reputation 声望查询用) ===
// 注意:任务奖励(FQuestReward)为纯数据,不再使用货币 Tag 发放,由游戏层在 OnQuestCompleted 回调中按 ItemRowID 自行处理
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Currency_经验);   // 保留供旧数据引用;奖励已改用 ItemRowID,不再经此 Tag 发放
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Currency_金币);   // 通用货币
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Currency_声望);   // 阵营声望(可细分阵营,见下方声望细分)

// === 属性标签(Condition_Level 等条件查询用,GetCurrency 实现) ===
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Quest_Attribute_等级);  // 等级条件查询用

// === 职业标签(Condition_Class 的 RequiredClassTag 选) ===
// 注:战士/法师等具体职业档未内置(缺乏普遍适用性),
// 由项目按需在"任务插件.职业.*"下自行注册;Condition_Class 接受任意职业 tag
