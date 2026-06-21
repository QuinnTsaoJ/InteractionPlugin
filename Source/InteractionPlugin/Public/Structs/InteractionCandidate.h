#pragma once

#include "CoreMinimal.h"
#include "InteractionCandidate.generated.h"

class UInteractableComponent;

/**
 * @brief 周围可交互物品的候选条目。
 *
 * 由 UInteractionComponent::GetNearbyInteractables() 产出，
 * 描述检测范围内的一个可交互对象及其相对玩家的几何关系。
 * 用于 UI（如轮盘、列表）展示"我周围有哪些可交互物品"。
 */
USTRUCT(BlueprintType)
struct FInteractionCandidate
{
	GENERATED_BODY()

	/** 候选的可交互组件 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UInteractableComponent> Interactable = nullptr;

	/** 候选到检测原点的距离（厘米） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Distance = 0.0f;

	/** 候选方向与玩家朝向的点积（1=正前方，0=90度，-1=正后方） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Dot = 0.0f;

	/** 该候选是否为当前主焦点（即 UInteractionComponent::CurrentInteractable） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bInFocus = false;

	/** 是否拥有有效目标 */
	bool IsValid() const { return Interactable != nullptr; }
};
