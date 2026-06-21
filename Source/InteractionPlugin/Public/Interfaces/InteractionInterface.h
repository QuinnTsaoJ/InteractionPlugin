#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Structs/InteractionContext.h"
#include "Structs/InteractionData.h"
#include "InteractionInterface.generated.h"

class UMeshComponent;

/**
 * @brief 交互行为的蓝图可扩展接口。
 *
 * 游戏层的 Actor 通过实现此接口来定义：
 * - 是否允许交互（CanInteract）
 * - 显示什么数据（GetInteractionData）
 * - 如何响应交互请求（OnInteractionRequest）
 * - 焦点描边时描哪些网格（GetOutlineMeshes）
 *
 * 框架仅负责调用这些函数——绝不解释结果或实现任何玩法行为。
 *
 * 蓝图可以覆盖所有函数。C++ 提供安全的默认实现。
 */
UINTERFACE(Blueprintable)
class UInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

class INTERACTIONPLUGIN_API IInteractionInterface
{
	GENERATED_BODY()

public:
	/**
	 * @brief 在给定上下文中，该 Actor 是否允许交互？
	 *
	 * 服务端在执行交互前调用此函数进行校验。
	 * 示例：门是否锁定、玩家是否拥有钥匙、任务条件是否满足。
	 * 默认实现：返回 true。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(const FInteractionContext& Context) const;
	virtual bool CanInteract_Implementation(const FInteractionContext& Context) const;

	/**
	 * @brief 返回交互提示所需的显示数据。
	 *
	 * 客户端在该 Actor 获得焦点时调用，用于填充 UI。
	 * 默认实现：返回空的 FInteractionData。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FInteractionData GetInteractionData() const;
	virtual FInteractionData GetInteractionData_Implementation() const;

	/**
	 * @brief 交互请求到达该 Actor 时调用。
	 *
	 * 这是玩法行为的核心扩展点。
	 * 框架仅负责调用，不对其做任何解释。
	 * 游戏层在此实现：播放动画、修改状态、触发任务等。
	 * 默认实现：空操作。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnInteractionRequest(const FInteractionContext& Context);
	virtual void OnInteractionRequest_Implementation(const FInteractionContext& Context);

	/**
	 * @brief 返回需要描边的网格组件列表。
	 *
	 * 该 Actor 获得交互焦点时，框架调用此函数获取要描边的目标。
	 * 默认实现：返回 Actor 上所有 UMeshComponent。
	 * 在蓝图覆写此事件即可精确指定描边目标。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Outline")
	TArray<UMeshComponent*> GetOutlineMeshes() const;
	virtual TArray<UMeshComponent*> GetOutlineMeshes_Implementation() const;
};
