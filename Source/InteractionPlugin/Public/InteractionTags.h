// 交互动作 GameplayTag 定义 —— 默认内置标签，用户可在编辑器中扩展
// InteractionPlugin

#pragma once

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

// C++ 预定义交互标签，在 InteractionTags.cpp 中注册
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_打开);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_关闭);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_使用);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_检查);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_对话);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_拾取);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_激活);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_关闭激活);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_搜索);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_装备);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_卸下);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_制造);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_锁定);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_解锁);
