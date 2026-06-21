#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Structs/InteractionData.h"
#include "InteractableComponent.generated.h"

// -------------------------------------------------------------------------
// 本地委托（在使用它的类之前声明）
// -------------------------------------------------------------------------

/** InteractableComponent 启用状态发生改变时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnInteractableEnabledChanged,
	UInteractableComponent*, Interactable,
	bool, bIsEnabled);

/** InteractableComponent 描边状态改变时广播（获得/失去焦点时触发） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOutlineStateChanged,
	UInteractableComponent*, Interactable,
	bool, bIsOutlined);

/**
 * @brief 将 Actor 标记为可交互的组件。
 *
 * 将此组件附加到任意 Actor，即可被 UInteractionComponent 检测到。
 * 存储交互配置、显示数据和可用选项。
 *
 * 不实现任何玩法行为——那是游戏层通过 IInteractionInterface
 * 或蓝图事件处理器的责任。
 */
UCLASS(ClassGroup = "Interaction", meta = (BlueprintSpawnableComponent))
class INTERACTIONPLUGIN_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent();

	// --------------------------------------------------------------------
	// 生命周期
	// --------------------------------------------------------------------

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;

	// --------------------------------------------------------------------
	// 启用状态
	// --------------------------------------------------------------------

	/** 启用或禁用此组件的交互能力 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetEnabled(bool bNewEnabled);

	/** 返回此组件当前是否可交互 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsEnabled() const { return bEnabled; }
	
	// --------------------------------------------------------------------
	// 数据访问
	// --------------------------------------------------------------------

	/** 返回交互显示数据 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	const FInteractionData& GetInteractionData() const { return InteractionData; }

	/** 返回所有交互选项 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	const TArray<FInteractionOption>& GetOptions() const { return InteractionData.Options; }

	/** 根据 GameplayTag 查找选项 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool FindOptionByTag(FGameplayTag Tag, FInteractionOption& OutOption) const;

	/** 返回默认交互 Tag（优先返回 DefaultInteractionTag，其次返回第一个选项的 Tag） */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FGameplayTag GetDefaultInteractionTag() const;

	// --------------------------------------------------------------------
	// 冷却
	// --------------------------------------------------------------------

	/** 当前是否处于交互冷却中 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Cooldown")
	bool IsOnCooldown() const;

	/** 当前冷却剩余时间（秒，未在冷却时为 0），供 UI 显示 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Cooldown")
	float GetCooldownRemaining() const;

	// --------------------------------------------------------------------
	// 事件（本地可用，蓝图可绑定）
	// --------------------------------------------------------------------

	/** 该组件启用或禁用时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractableEnabledChanged OnEnabledChanged;

	/** 该组件描边状态改变时广播（获得/失去焦点） */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Outline")
	FOnOutlineStateChanged OnOutlineStateChanged;

	// --------------------------------------------------------------------
	// 焦点回调（由 UInteractionComponent 驱动）
	// --------------------------------------------------------------------

	/** 由 UInteractionComponent 在获得焦点时调用，触发描边 */
	void NotifyFocusGained();

	/** 由 UInteractionComponent 在失去焦点时调用，关闭描边 */
	void NotifyFocusLost();

	/** 返回当前是否处于描边状态 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Outline")
	bool IsOutlined() const { return bIsOutlined; }

protected:
	// --------------------------------------------------------------------
	// 描边配置
	// --------------------------------------------------------------------

	/** 自定义深度模板值（0-255），后处理材质据此区分描边样式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outline", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "255"))
	int32 StencilValue = 1;

	/** 是否在该对象获得交互焦点时启用描边 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outline", meta = (AllowPrivateAccess = "true"))
	bool bEnableOutline = true;

	// --------------------------------------------------------------------
	// 属性
	// --------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	FInteractionData InteractionData;

	/** 此组件当前是否接受交互 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	bool bEnabled = true;

	/** 未指定具体选项时的回退 Tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	FGameplayTag DefaultInteractionTag;

public:
	/** 由 UInteractionComponent 在交互被接受后调用，刷新冷却起点 */
	void NotifyInteracted();
private:
	// --------------------------------------------------------------------
	// 描边内部状态与辅助
	// --------------------------------------------------------------------

	/** 当前是否处于描边状态 */
	bool bIsOutlined = false;

	/** 上一次描边的网格弱引用缓存（用于失焦时准确关闭） */
	TArray<TWeakObjectPtr<UMeshComponent>> OutlinedMeshes;

	/** 对目标网格组件根据配置开关 Custom Depth */
	void ApplyOutline(bool bShouldOutline);

	// --------------------------------------------------------------------
	// 冷却内部状态
	// --------------------------------------------------------------------

	/** 上次成功交互的世界时间（GetWorld()->GetTimeSeconds()），<0 表示尚未交互过 */
	float LastInteractionTime = -1.0f;

	
};
