// 任务系统 GameplayTag 注册
// tag 名为中文,与项目现有风格(库存插件.*、交互.*)一致
// 共 25 个:6 事件 + 6 指引 + 9 目标(1 自定义容器 + 8 具体) + 3 货币 + 1 属性 + 0 职业
// 注:敌人(仅野兽)/区域(仅城镇)/NPC/职业及交互(仅门/宝箱)的具体档位因缺乏普遍适用性未内置,由项目按需自行注册
// QuestSystem Plugin

#include "QuestTags.h"

// === 事件标签 ===
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Event_击杀, L"任务插件.事件.击杀");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Event_拾取, L"任务插件.事件.拾取");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Event_到达, L"任务插件.事件.到达");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Event_对话, L"任务插件.事件.对话");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Event_交互, L"任务插件.事件.交互");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Event_自定义, L"任务插件.事件.自定义");

// === 指引标签 ===
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Guide_击杀, L"任务插件.指引.击杀");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Guide_拾取, L"任务插件.指引.拾取");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Guide_到达, L"任务插件.指引.到达");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Guide_对话, L"任务插件.指引.对话");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Guide_交互, L"任务插件.指引.交互");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Guide_自定义, L"任务插件.指引.自定义");

// === 目标标签 ===
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_自定义, L"任务插件.目标.自定义");

// 敌人目标
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_敌人_野兽, L"任务插件.目标.敌人.野兽");

// 物品目标
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_物品_草药, L"任务插件.目标.物品.草药");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_物品_矿石, L"任务插件.目标.物品.矿石");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_物品_材料, L"任务插件.目标.物品.材料");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_物品_任务物品, L"任务插件.目标.物品.任务物品");

// 区域目标
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_区域_城镇, L"任务插件.目标.区域.城镇");

// 可交互物目标
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_交互_门, L"任务插件.目标.交互.门");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Target_交互_宝箱, L"任务插件.目标.交互.宝箱");

// === 货币标签 ===
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Currency_经验, L"任务插件.货币.经验");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Currency_金币, L"任务插件.货币.金币");
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Currency_声望, L"任务插件.货币.声望");

// === 属性标签 ===
UE_DEFINE_GAMEPLAY_TAG(TAG_Quest_Attribute_等级, L"任务插件.属性.等级");
