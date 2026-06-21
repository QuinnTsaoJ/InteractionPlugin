#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Structs/InteractionContext.h"
#include "Structs/InteractionCandidate.h"
#include "Components/InteractionComponent.h"
#include "InteractionBlueprintLibrary.generated.h"

/**
 * @brief Interaction Framework 的蓝图可访问工具函数。
 *
 * 提供查询交互状态、从上下文中提取数据以及触发交互的便捷函数。
 */
UCLASS()
class INTERACTIONPLUGIN_API UInteractionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 返回上下文是否拥有有效的发起者和目标 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static bool IsValidInteractionContext(const FInteractionContext& Context);

	/** 从 InteractableComponent 的 InteractionData 中提取显示名称 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static FText GetInteractionDisplayName(UInteractableComponent* Interactable);

	/** 从 InteractionContext 中提取 GameplayTag */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static FGameplayTag GetInteractionTag(const FInteractionContext& Context);

	/** 获取 InteractableComponent 的所有交互选项 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static TArray<FInteractionOption> GetInteractableOptions(UInteractableComponent* Interactable);

	/** 在 InteractionComponent 上使用指定 Tag 触发交互 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	static void TriggerInteraction(
		UInteractionComponent* InteractionComp,
		const FGameplayTag& InteractionTag);

	/** 检查上下文中是否包含有效的命中结果 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static bool ContextHasHitResult(const FInteractionContext& Context);

	/** 从 InteractionContext 获取命中位置 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static FVector GetContextHitLocation(const FInteractionContext& Context);

	/**
	 * @brief 获取交互组件周围的可交互物品数组（转发至组件方法，做 nullptr 保护）。
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction",
		meta = (AutoCreateRefTerm = "InteractionComp"))
	static TArray<FInteractionCandidate> GetNearbyInteractables(UInteractionComponent* InteractionComp);

	/** 获取交互组件当前检测模式（转发至组件方法，做 nullptr 保护） */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static EInteractionTraceMode GetTraceMode(UInteractionComponent* InteractionComp);

	/** 交互组件是否正在长按累积（nullptr 安全） */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static bool IsHoldingInteraction(UInteractionComponent* InteractionComp);

	/** 交互组件当前长按进度 0-1（nullptr 安全，未在长按时为 0），供 UI 读条 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static float GetHoldProgress(UInteractionComponent* InteractionComp);

	/** 交互组件的玩家侧总开关是否启用（nullptr 安全） */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static bool IsInteractionEnabled(UInteractionComponent* InteractionComp);

	/** 指定可交互目标当前是否处于冷却中（nullptr 安全） */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static bool IsOnCooldown(UInteractableComponent* Interactable);

	/** 指定可交互目标的冷却剩余时间（nullptr 安全，未在冷却时为 0） */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	static float GetCooldownRemaining(UInteractableComponent* Interactable);
};
