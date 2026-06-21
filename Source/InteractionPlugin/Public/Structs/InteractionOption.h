#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InteractionOption.generated.h"

/**
 * @brief 表示 InteractableComponent 中的单个交互选项。
 *
 * 用于多交互目标（例如一个宝箱可以"打开"、"搜索"或"检查"）。
 * 每个选项通过 GameplayTag 标识，并携带 UI 显示数据。
 */
USTRUCT(BlueprintType)
struct FInteractionOption
{
	GENERATED_BODY()

	/** 标识该交互类型的 GameplayTag（如 Interaction.Open） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTag Tag;

	/** 该选项的显示文本 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText DisplayText;

	/** 可选的 UI 图标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 返回该选项是否拥有有效的 GameplayTag */
	bool IsValid() const
	{
		return Tag.IsValid();
	}
};
