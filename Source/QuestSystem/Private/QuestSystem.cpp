// 任务系统模块入口实现
// QuestSystem Plugin

#include "QuestSystem.h"

#define LOCTEXT_NAMESPACE "FQuestSystemModule"

void FQuestSystemModule::StartupModule()
{
	// 模块加载后执行
	// FQuestTags 的 Native GameplayTag 由 QuestTags.cpp 的 FNativeGameplayTag 静态对象自动注册
}

void FQuestSystemModule::ShutdownModule()
{
	// 模块卸载前执行
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FQuestSystemModule, QuestSystem)
