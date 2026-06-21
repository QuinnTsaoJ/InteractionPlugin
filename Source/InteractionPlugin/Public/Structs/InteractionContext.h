#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "InteractionContext.generated.h"

class UInteractableComponent;

/**
 * @brief 整个交互流程的核心数据载体。
 *
 * 贯穿交互的每个阶段：从请求到校验再到事件广播。
 * 包含谁（Who）发起了交互、目标是什么（What）、交互发生在哪里（Where）、
 * 何时发生（When）、以及触发的是哪种交互类型（Which）。
 *
 * TargetComponent 使用 UActorComponent* 存储，以避免循环包含。
 * 使用 GetTargetInteractable() 获取类型化的 UInteractableComponent 指针。
 */
USTRUCT(BlueprintType)
struct FInteractionContext
{
	GENERATED_BODY()

	/** 交互发起者（通常是玩家 Pawn） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Instigator = nullptr;

	/** 交互目标 Actor */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** 目标身上的 InteractableComponent（使用基类指针存储以打破循环依赖） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UActorComponent> TargetComponent = nullptr;

	/** 射线命中结果（如果交互来自 Overlap Query 则可能无效） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FHitResult HitResult;

	/** 交互点的世界坐标 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector HitLocation = FVector::ZeroVector;

	/** 标识交互类型的 GameplayTag（如 Interaction.Open） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTag InteractionTag;

	/** 交互触发时的世界时间（GetWorld()->GetTimeSeconds()） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	double Timestamp = 0.0;

	/** 便捷方法：HitResult 是否包含有效的阻挡命中 */
	bool HasHitResult() const { return HitResult.bBlockingHit; }

	/** 返回核心字段是否已填充 */
	bool IsValid() const
	{
		return Instigator != nullptr && TargetActor != nullptr;
	}

	/** 类型化访问器：将 TargetComponent 转换为 UInteractableComponent */
	UInteractableComponent* GetTargetInteractable() const;
};
