#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"            // UTextBlock
#include "Components/Image.h"                // UImage
#include "Components/ProgressBar.h"          // UProgressBar
#include "UInteractionWidget.generated.h"

class UInteractionComponent;
class UInteractableComponent;
class UOverlay;

/**
 * @brief 交互提示 Widget 基类。
 *
 * 由 UInteractionComponent 在本地玩家就绪时创建，交由 UISystem 管理挂载。
 * 通过 meta=(BindWidget) 强制蓝图派生类提供四个必需控件：
 *   - ActionText (UTextBlock)：动作描述
 *   - KeyIcon (UImage)：交互动作提示图标（来自 InteractionComponent 配置的 InteractActionIcon）
 *   - HoldProgressBar (UProgressBar)：长按进度条
 *   - InteractionTypeText (UTextBlock)：交互类型说明（点击 / 长按）
 *
 * 焦点只驱动内部内容容器的显隐；外层遮挡由 UISystem 管理。
 * 可见时由 NativeTick 推进长按进度条。
 * InteractionTypeText 在 OnFocusBegin 时依据目标 HoldTime 填充文案：
 * HoldTime>0 显示 HoldText（长按），否则显示 ClickText（点击）。
 *
 * 蓝图只需设计视觉、提供四个必需控件、可选重写视觉钩子事件。
 */
UCLASS(ClassGroup = "Interaction", Abstract)
class INTERACTIONPLUGIN_API UInteractionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInteractionWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// --------------------------------------------------------------------
	// 生命周期
	// --------------------------------------------------------------------

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual void NativeDestruct() override;

	// --------------------------------------------------------------------
	// 组件引用注入（由 UInteractionComponent 在创建后自动调用）
	// --------------------------------------------------------------------

	/**
	 * @brief 组件创建 Widget 后自动调用，注入组件引用并订阅事件。
	 *
	 * 内部行为：保存组件弱引用 → 绑定 5 个事件 → SyncCurrentFocus → OnWidgetCreated。
	 * 幂等：重复调用先解绑旧的再绑新的。
	 *
	 * 蓝图不应直接调用——由 UInteractionComponent::CreateInteractionWidget 调用。
	 * 蓝图的视觉初始化钩子用 OnWidgetCreated（在本函数末尾触发）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction",
		meta = (BlueprintInternalUseOnly = "true"))
	void NativeInit(UInteractionComponent* InComponent);

	/** 显式释放组件订阅；注销提示时不依赖 Slate 延迟析构。 */
	void ReleaseInteractionComponent();

	// --------------------------------------------------------------------
	// 必需控件（BindWidget 契约）
	// --------------------------------------------------------------------

	/** 动作文本（如"打开 / 搜索"），蓝图必须提供名为 ActionText 的 UTextBlock */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Widgets",
		meta = (BindWidget))
	TObjectPtr<UTextBlock> ActionText = nullptr;

	/** 交互动作提示图标，蓝图必须提供名为 KeyIcon 的 UImage；内容由 InteractionComponent 配置的 InteractActionIcon 填充 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Widgets",
		meta = (BindWidget))
	TObjectPtr<UImage> KeyIcon = nullptr;

	/** 长按进度条，蓝图必须提供名为 HoldProgressBar 的 UProgressBar */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Widgets",
		meta = (BindWidget))
	TObjectPtr<UProgressBar> HoldProgressBar = nullptr;

	/** 交互类型说明文本（点击 / 长按），蓝图必须提供名为 InteractionTypeText 的 UTextBlock */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Widgets",
		meta = (BindWidget))
	TObjectPtr<UTextBlock> InteractionTypeText = nullptr;

	// --------------------------------------------------------------------
	// 可配置属性
	// --------------------------------------------------------------------

	/** 瞬时目标（HoldTime<=0）的交互类型文案，默认"点击"。蓝图 Details 面板可自定义 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Type")
	FText ClickText = FText::FromString(TEXT("点击"));

	/** 长按目标（HoldTime>0）的交互类型文案，默认"长按"。蓝图 Details 面板可自定义 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Type")
	FText HoldText = FText::FromString(TEXT("长按"));

	// KeyIcon 的内容来自 UInteractionComponent 配置的 InteractActionIcon，
	// Widget 自身不再持有按键相关配置。

	// --------------------------------------------------------------------
	// 蓝图可选实现事件（视觉钩子）
	// --------------------------------------------------------------------

	/** Widget 被组件创建并完成初始化后触发，蓝图可做一次性视觉初始化 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction|Events",
		meta = (DisplayName = "On Widget Created"))
	void OnWidgetCreated();

	/** 提示出现时触发，蓝图可播淡入动画 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction|Events",
		meta = (DisplayName = "On Prompt Shown"))
	void OnPromptShown();

	/** 提示隐藏时触发，蓝图可播淡出动画 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction|Events",
		meta = (DisplayName = "On Prompt Hidden"))
	void OnPromptHidden();

protected:
	// --------------------------------------------------------------------
	// 事件处理器
	// --------------------------------------------------------------------

	UFUNCTION()
	void HandleFocusBegin(AActor* TargetActor, UInteractableComponent* Interactable);

	UFUNCTION()
	void HandleFocusEnd(AActor* TargetActor, UInteractableComponent* Interactable);

	UFUNCTION()
	void HandleHoldStarted(AActor* TargetActor, UInteractableComponent* Interactable, float HoldTime);

	UFUNCTION()
	void HandleHoldCanceled(AActor* TargetActor, UInteractableComponent* Interactable);

	UFUNCTION()
	void HandleHoldCompleted(AActor* TargetActor, UInteractableComponent* Interactable);

private:
	/** 订阅/解绑组件事件的核心实现 */
	void BindComponentEvents(UInteractionComponent* InComponent);
	void UnbindComponentEvents();

	/** 同步当前焦点状态到 UI（NativeInit 后调用） */
	void SyncCurrentFocus();

	/** 当前持有的玩家侧交互组件弱引用（由组件在 NativeInit 时传入） */
	UPROPERTY(Transient)
	TWeakObjectPtr<UInteractionComponent> InteractionComponent;

	/** 当前焦点目标（缓存，供 Tick 读取） */
	UPROPERTY(Transient)
	TWeakObjectPtr<UInteractableComponent> FocusedInteractable;

	/** 是否正在长按累积中 */
	bool bIsHolding = false;

	/** 运行时包裹原控件树；外层可见性由 UISystem 管理，目标只控制内容。 */
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> PromptContent = nullptr;
	/** 内容存在时允许显示；控件树重建后保持当前焦点状态。 */
	bool bHasPromptContent = false;
	/** 更新内容容器，不写顶层 Widget 的可见性。 */
	void SetPromptContentVisible(bool bVisible);
};
