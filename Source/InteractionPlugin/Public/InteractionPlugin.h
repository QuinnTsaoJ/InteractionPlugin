#pragma once

#include "Modules/ModuleManager.h"

/**
 * @brief InteractionFramework 模块入口点。
 *
 * Runtime 模块，提供事件驱动的交互基础设施：
 * - UInteractionComponent（玩家侧：射线检测/输入/请求）
 * - UInteractableComponent（目标侧：数据/配置）
 * - IInteractionInterface（行为扩展契约）
 * - UInteractionSubsystem（全局注册表/查询）
 * - UInteractionBlueprintLibrary（蓝图工具函数）
 */
class FInteractionPluginModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
