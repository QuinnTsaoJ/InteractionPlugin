#include "InteractionPlugin.h"

#define LOCTEXT_NAMESPACE "FInteractionPluginModule"

void FInteractionPluginModule::StartupModule()
{
	// 模块启动——无需特殊初始化。
	// 子系统和组件通过自身的生命周期方法自动注册。
}

void FInteractionPluginModule::ShutdownModule()
{
	// 模块关闭——由子系统和组件自行清理。
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInteractionPluginModule, InteractionPlugin)
