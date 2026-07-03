// 任务系统模块入口
// QuestSystem Plugin

#pragma once

#include "Modules/ModuleManager.h"

class FQuestSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
