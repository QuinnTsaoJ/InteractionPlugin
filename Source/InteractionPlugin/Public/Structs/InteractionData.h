#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"                // UInputAction（FInteractionActionConfig::Action）
#include "InteractionOption.h"
#include "InteractionData.generated.h"

/**
 * @brief 可交互对象的 UI 展示数据。
 *
 * 携带显示交互提示所需的所有信息。
 * 存储在 UInteractableComponent 上，通过 IInteractionInterface 返回。
 */
USTRUCT(BlueprintType)
struct FInteractionData
{
	GENERATED_BODY()

	/** UI 中显示的交互对象名称（如"木门"） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Name;

	/** 动作描述（如"打开"、"搜索"） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText ActionText;

	/** 按住时长：>0 表示需要长按触发交互 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0"))
	float HoldTime = 0.0f;

	/** 交互冷却时长（秒）：>0 表示两次交互间需间隔该时长，0 = 无冷却 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0"))
	float CooldownTime = 0.0f;

	/** 当前是否允许交互 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCanInteract = true;

	/** 可用的交互选项列表（支持多交互目标） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TArray<FInteractionOption> Options;
};

/**
 * @brief 交互动作配置：InputAction 与其提示图标的绑定。
 *
 * 由 UInteractionComponent 集中配置（每个交互动作一个配置项），
 * UInteractionWidget 在焦点进入时据此填充 KeyIcon。
 * 与具体物理按键无关——只描述"这个动作用什么图标表示"。
 */
USTRUCT(BlueprintType)
struct FInteractionActionConfig
{
	GENERATED_BODY()

	/** 交互动作（如点按 / 查询 / 长按对应的 InputAction） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UInputAction> Action = nullptr;

	/** 该动作的提示图标（显示在 UI 上） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UTexture2D> Icon = nullptr;
};
