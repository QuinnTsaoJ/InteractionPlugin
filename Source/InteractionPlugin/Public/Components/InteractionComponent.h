#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "Structs/InteractionContext.h"
#include "Structs/InteractionCandidate.h"
#include "Components/InteractableComponent.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"          // FInputBindingHandle / UEnhancedInputComponent（值类型，需完整定义）
#include "Engine/Texture2D.h"                // UTexture2D（Icon 字段完整定义）
#include "UITypes.h"
#include "InteractionComponent.generated.h"


class UInteractableComponent;
class UInputAction;
class UEnhancedInputComponent;
class UInteractionWidget;
class UInteractionOptionsWidget;
class UUISubsystem;
class APawn;

// -------------------------------------------------------------------------
// 检测模式
// -------------------------------------------------------------------------

/** 交互检测模式 */
UENUM(BlueprintType)
enum class EInteractionTraceMode : uint8
{
	FirstPerson UMETA(DisplayName = "第一人称"),
	ThirdPerson UMETA(DisplayName = "第三人称")
};

// -------------------------------------------------------------------------
// 焦点事件 —— 由 UInteractionComponent 广播
// -------------------------------------------------------------------------

/** Actor 获得交互焦点时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnFocusBegin, AActor*, TargetActor, UInteractableComponent*, Interactable);

/** Actor 失去交互焦点时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnFocusEnd, AActor*, TargetActor, UInteractableComponent*, Interactable);

/** 焦点从一个目标切换到另一个目标时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnFocusChanged, AActor*, OldTarget, AActor*, NewTarget, UInteractableComponent*, NewInteractable);

/** 检测模式发生切换时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTraceModeChanged, UInteractionComponent*, Component,
	EInteractionTraceMode, NewMode);

// -------------------------------------------------------------------------
// 交互事件 —— 由 UInteractionComponent 广播
// -------------------------------------------------------------------------

/** 客户端发出交互请求时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnInteractionSent, AActor*, Instigator, FInteractionContext, Context);

/** 目标接受交互时广播（经服务端验证） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnInteractionAccepted, AActor*, Instigator, FInteractionContext, Context);

/** 目标拒绝交互时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnInteractionRejected, AActor*, Instigator, FInteractionContext, Context);

/** 交互流程完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnInteractionCompleted, AActor*, Instigator, FInteractionContext, Context);

// -------------------------------------------------------------------------
// 长按事件 —— 由 UInteractionComponent 广播
// -------------------------------------------------------------------------

/** 长按交互开始累积时广播（携带本次需累积的总时长，供 UI 显示） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnHoldStarted, AActor*, TargetActor, UInteractableComponent*, Interactable, float, HoldTime);

/** 长按被取消时广播（松手或失去焦点） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnHoldCanceled, AActor*, TargetActor, UInteractableComponent*, Interactable);

/** 长按累积满时广播（本地纯客户端事件，不依赖服务端校验结果） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnHoldCompleted, AActor*, TargetActor, UInteractableComponent*, Interactable);

/** 玩家侧交互总开关状态改变时广播（过场/UI/对话期间禁用交互） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnInteractionEnabledChanged, UInteractionComponent*, Component, bool, bEnabled);

// -------------------------------------------------------------------------
// 周围查询事件 —— 由 UInteractionComponent 广播
// -------------------------------------------------------------------------

/** 点按"周围检测"输入后，向游戏层广播检测到的候选列表 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnNearbyQueried, UInteractionComponent*, Component,
	const TArray<FInteractionCandidate>&, Candidates);


/**
 * @brief 玩家侧的交互组件。
 *
 * 挂载到 PlayerCharacter、PlayerController 或 Pawn 上。
 * 执行基于射线的目标检测、管理焦点、处理输入，
 * 并向服务端分发交互请求。
 *
 * 不实现任何玩法行为——仅触发事件并传递 InteractionContext。
 * 游戏层通过绑定事件来响应交互。
 */

UCLASS(ClassGroup = "Interaction", meta = (BlueprintSpawnableComponent))
class INTERACTIONPLUGIN_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	// --------------------------------------------------------------------
	// 生命周期
	// --------------------------------------------------------------------

	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --------------------------------------------------------------------
	// 生命周期（V2.1 新增：管理 Widget 创建/销毁）
	// --------------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --------------------------------------------------------------------
	// 公开 API
	// --------------------------------------------------------------------

	/**
	 * @brief 对当前焦点目标发起交互；多有效选项打开菜单，否则沿用默认 Tag。
	 *
	 * 蓝图可调用的入口。通常绑定到输入操作。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction", meta = (ToolTip = "对当前焦点目标发起交互；多有效选项打开菜单，否则沿用默认 Tag。"))
	void TryInteract();

	/**
	 * @brief 对当前焦点目标使用指定的 GameplayTag 触发交互。
	 *
	 * 用于多选项交互（如"搜索"vs"打开"）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction", meta = (AutoCreateRefTerm = "InteractionTag"))
	void TryInteractWithTag(const FGameplayTag& InteractionTag);

	/** 选择当前前台菜单中的显式动作；仅在关闭已同步提交并通过上下文复检后发送，不表示玩法成功。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|UI", meta = (ToolTip = "选择当前前台菜单中的显式动作；仅在关闭已同步提交并通过上下文复检后发送，不表示玩法成功。"))
	bool SelectInteractionOption(const FGameplayTag& InteractionTag);

	/** 取消当前选项菜单并清除捕获上下文，不执行交互。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|UI", meta = (ToolTip = "取消当前选项菜单并清除捕获上下文，不执行交互。"))
	void CancelInteractionOptions();

	/** 查询选项菜单是否已打开；被更高界面挂起时仍返回真。 */
	UFUNCTION(BlueprintPure, Category = "Interaction|UI", meta = (ToolTip = "查询选项菜单是否已打开；被更高界面挂起时仍返回真。"))
	bool IsInteractionOptionsOpen() const;

	// --------------------------------------------------------------------
	// 长按交互
	// --------------------------------------------------------------------

	/**
	 * @brief 开始长按。通常绑定到输入的 Pressed 时刻。
	 *
	 * - 若当前焦点目标的 HoldTime > 0：进入长按累积，广播 OnHoldStarted，
	 *   由 Tick 推进进度，累积满后自动触发一次普通交互。
	 * - 若 HoldTime <= 0（瞬时目标）：立即触发一次普通交互，等价于 TryInteract()。
	 *
	 * 调用前要求存在有效焦点（CurrentInteractable）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Hold")
	void BeginHold();

	/**
	 * @brief 结束长按。通常绑定到输入的 Released 时刻。
	 *
	 * 若正在长按累积中，取消累积并广播 OnHoldCanceled。
	 * 若已累积满并触发交互，或本来是瞬时触发，则无操作。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Hold")
	void EndHold();

	/** 是否正在长按累积中 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Hold")
	bool IsHolding() const{ return bIsHolding; }

	/** 当前长按的目标累积总时长（秒），未在长按时为 0 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Hold")
	float GetCurrentHoldTime() const{ return CurrentHoldTime; }

	/** 当前长按进度（0-1，未在长按时为 0），供 UI Tick 查询绘制读条 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Hold")
	float GetCurrentHoldProgress() const;

	// --------------------------------------------------------------------
	// 玩家侧总开关
	// --------------------------------------------------------------------

	/**
	 * @brief 启用或禁用玩家侧的整个交互能力。
	 *
	 * 关闭时：跳过检测、清除当前焦点、拒绝所有交互入口（含长按）。
	 * 适用于玩法主动禁用交互；UI 阻止由独立策略管理，不覆盖此业务开关。
	 * 重新开启后下一帧自动恢复检测。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bNewEnabled);

	/** 玩家侧交互是否启用 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const{ return bInteractionEnabled; }

	/** 设置 UI 输入阻止状态，不改变业务开关，也不广播业务开关事件。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|UI", meta = (ToolTip = "设置 UI 输入阻止状态，不改变业务开关，也不广播业务开关事件。"))
	void SetUIInteractionBlocked(bool bBlocked);

	/** 查询业务开关与 UI 输入策略共同决定的有效交互允许状态。 */
	UFUNCTION(BlueprintPure, Category = "Interaction", meta = (ToolTip = "查询业务开关与 UI 输入策略共同决定的有效交互允许状态。"))
	bool IsInteractionEffectivelyEnabled() const{ return bInteractionEnabled && !bUIInteractionBlocked; }

	/** 强制立即刷新焦点检测 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ForceRefreshFocus();

	// --------------------------------------------------------------------
	// 检测模式
	// --------------------------------------------------------------------

	/**
	 * @brief 切换检测模式。若发生变化则广播 OnTraceModeChanged，
	 *        并强制下一帧立即刷新焦点。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Trace")
	void SetTraceMode(EInteractionTraceMode NewMode);

	/** 返回当前检测模式 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Trace")
	EInteractionTraceMode GetTraceMode() const{ return TraceMode; }

	/**
	 * @brief 按需查询周围可交互物品数组（从不缓存，每次调用实时查询）。
	 *
	 * - 第一人称：以视点为中心做球形 Overlap 收集（半径=TraceDistance）。
	 * - 第三人称：以玩家为中心做半径=TraceDistance 的 Overlap，再用 TraceHalfAngle 剔除。
	 *
	 * 返回值按距离升序排序，当前主焦点会标记 bInFocus=true。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Trace")
	TArray<FInteractionCandidate> GetNearbyInteractables() const;

	/**
	 * @brief 从当前本地 Pawn 查询附近可见候选，仅供 UI 读取，不执行交互或改变焦点。
	 * @param Searcher 此组件所属本地控制器当前控制的 Pawn，失效或更换后旧 Pawn 返回空数组。
	 * @return 以 Pawn 位置为原点、TraceDistance 为半径的实时整球候选，去重并按距离升序返回。
	 * 只绕过 UI 输入阻止；手动禁用、组件结束及目标禁用仍生效，遮挡使用 UIQueryVisibilityChannel。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Interaction|UI",
		meta = (ToolTip = "从当前本地 Pawn 查询附近可见候选，仅供 UI 读取，不执行交互或改变焦点。只绕过 UI 输入阻止；手动禁用、组件结束及目标禁用仍生效。"))
	TArray<FInteractionCandidate> QueryNearbyInteractablesForUI(APawn* Searcher) const;

	/** 检测模式发生切换时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Trace")
	FOnTraceModeChanged OnTraceModeChanged;

	// --------------------------------------------------------------------
	// Enhanced Input 绑定
	// --------------------------------------------------------------------

	// 输入绑定改为自动：BeginPlay 尝试从 Owner 取 EnhancedInputComponent，
	// 依据 Details 面板配置的三个 Action 自动 BindAction；若此时 InputComponent
	// 尚未就绪，由 Tick 兜底补绑一次。Action 是字段的唯一职责，不再由外部传入。

	/** 当前绑定的点按交互 Action（供外部读取） */
	UFUNCTION(BlueprintPure, Category = "Interaction|Input")
	UInputAction* GetInteractAction() const{ return InteractAction; }

	/** 点按交互 Action 的提示图标（供交互提示 UI 读取） */
	UFUNCTION(BlueprintPure, Category = "Interaction|Input")
	UTexture2D* GetInteractActionIcon() const{ return InteractActionIcon; }

	/** 周围检测 Action 的提示图标 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Input")
	UTexture2D* GetQueryActionIcon() const{ return QueryActionIcon; }

	/** 长按 Action 的提示图标 */
	UFUNCTION(BlueprintPure, Category = "Interaction|Input")
	UTexture2D* GetHoldActionIcon() const{ return HoldActionIcon; }

	// --------------------------------------------------------------------
	// 交互提示 Widget 生命周期（V2.1 新增）
	// --------------------------------------------------------------------

	/** 交互提示 Widget 类（蓝图在 Details 面板选定 BP_InteractionWidget） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	TSubclassOf<UInteractionWidget> InteractionWidgetClass = nullptr;

	/** 多选项菜单表现类，留空使用原生列表；会话与输入策略仍由 UISystem 管理。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI", meta = (ToolTip = "多选项菜单表现类，留空使用原生列表；会话与输入策略仍由 UISystem 管理。"))
	TSubclassOf<UInteractionOptionsWidget> OptionsWidgetClass = nullptr;

	/** 旧资产兼容字段；提示层级现在由 UISystem 的 HUD 策略决定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI",
		meta = (ClampMin = "0", DeprecatedProperty, DeprecationMessage = "提示层级由 UISystem HUD 策略决定。", ToolTip = "旧资产兼容字段；提示层级现在由 UISystem 的 HUD 策略决定。"))
	int32 InteractionWidgetZOrder = 0;

	/** 已创建的 Widget 实例（运行时只读，蓝图可用于查询状态） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|UI")
	TObjectPtr<UInteractionWidget> InteractionWidget = nullptr;

	// --------------------------------------------------------------------
	// 焦点状态（外部只读）
	// --------------------------------------------------------------------

	/** 当前获得焦点的 Actor（可能为 nullptr） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Focus")
	TObjectPtr<AActor> CurrentFocusedActor = nullptr;

	/** 当前获得焦点的 InteractableComponent（可能为 nullptr） */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Focus")
	TObjectPtr<UInteractableComponent> CurrentInteractable = nullptr;

	// --------------------------------------------------------------------
	// 焦点事件
	// --------------------------------------------------------------------

	/** Actor 获得交互焦点时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Focus")
	FOnFocusBegin OnFocusBegin;

	/** Actor 失去交互焦点时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Focus")
	FOnFocusEnd OnFocusEnd;

	/** 焦点从一个目标切换到另一个目标时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Focus")
	FOnFocusChanged OnFocusChanged;

	// --------------------------------------------------------------------
	// 交互事件
	// --------------------------------------------------------------------

	/** 交互请求发出时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnInteractionSent OnInteractionSent;

	/** 目标接受交互时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnInteractionAccepted OnInteractionAccepted;

	/** 目标拒绝交互时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnInteractionRejected OnInteractionRejected;

	/** 交互流程完成时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnInteractionCompleted OnInteractionCompleted;

	// --------------------------------------------------------------------
	// 长按事件
	// --------------------------------------------------------------------

	/** 长按开始累积时广播（携带本次需累积的总时长） */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Hold")
	FOnHoldStarted OnHoldStarted;

	/** 长按被取消时广播（松手或失去焦点） */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Hold")
	FOnHoldCanceled OnHoldCanceled;

	/** 长按累积满时广播（纯本地事件，与网络校验结果无关；触发于本地累积满的当帧） */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Hold")
	FOnHoldCompleted OnHoldCompleted;

	// --------------------------------------------------------------------
	// 玩家侧总开关事件
	// --------------------------------------------------------------------

	/** 玩家侧交互启用状态改变时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionEnabledChanged OnInteractionEnabledChanged;

	/** 周围可交互物品查询完成时广播（由点按 Query 触发） */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Trace")
	FOnNearbyQueried OnNearbyQueried;

protected:
	// --------------------------------------------------------------------
	// 输入处理（V2.0 新增）
	// --------------------------------------------------------------------

	/**
	 * @brief 点按"周围检测"输入的处理函数。
	 *
	 * 调用 GetNearbyInteractables() 收集候选并广播 OnNearbyQueried。
	 * 有效交互允许状态为 false 时直接返回。
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Input")
	void HandleNearbyQuery();

	// --------------------------------------------------------------------
	// 配置
	// --------------------------------------------------------------------

	/** 点按交互 Action（对当前焦点交互） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Input")
	TObjectPtr<UInputAction> InteractAction = nullptr;

	/** 点按交互 Action 的提示图标（显示在交互提示 UI 上） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Input")
	TObjectPtr<UTexture2D> InteractActionIcon = nullptr;

	/** 点按周围检测 Action（触发 GetNearbyInteractables 并广播 OnNearbyQueried） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Input")
	TObjectPtr<UInputAction> QueryAction = nullptr;

	/** 周围检测 Action 的提示图标 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Input")
	TObjectPtr<UTexture2D> QueryActionIcon = nullptr;

	/** 长按 Action（推门等玩法） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Input")
	TObjectPtr<UInputAction> HoldAction = nullptr;

	/** 长按 Action 的提示图标 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Input")
	TObjectPtr<UTexture2D> HoldActionIcon = nullptr;
	// --------------------------------------------------------------------
	// 配置
	// --------------------------------------------------------------------

	/** 最大射线检测距离 */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Trace", meta = (ClampMin = "0"))
	float TraceDistance = 300.0f;

	/** 射线半径（>0 = SphereTrace，0 = LineTrace） */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Trace", meta = (ClampMin = "0"))
	float TraceRadius = 0.0f;

	/** 射线使用的碰撞通道 */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel2;

	/** UI 只读查询的遮挡射线通道；墙体须阻挡此通道，默认使用可见性通道。 */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Trace", meta = (ToolTip = "UI 只读查询的遮挡射线通道；墙体须阻挡此通道，默认使用可见性通道。"))
	TEnumAsByte<ECollisionChannel> UIQueryVisibilityChannel = ECC_Visibility;

	/** 自动射线扫描间隔（秒，0 = 每帧检测） */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Trace", meta = (ClampMin = "0"))
	float TraceInterval = 0.1f;

	/** 当前检测模式，运行时可通过 SetTraceMode() 切换 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace")
	EInteractionTraceMode TraceMode = EInteractionTraceMode::FirstPerson;

	/**
	 * @brief 第三人称扇形的半角（度），仅在 ThirdPerson 模式下生效。
	 *
	 * 整个扇形的张角 = 2 × TraceHalfAngle。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace",
		meta = (ClampMin = "0", ClampMax = "180"))
	float TraceHalfAngle = 60.0f;

	/** 是否处理长按类型的交互 */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Input")
	bool bEnableHoldInteraction = true;

	/** 玩家侧交互总开关（运行时可由 SetInteractionEnabled 修改） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	bool bInteractionEnabled = true;

private:
	/** 更新本地玩家接入；玩家、输入组件或世界变化后重建绑定。 */
	void RefreshLocalUIContext();
	/** 解析本地 Controller；Pawn 延迟被控制时由后续 Tick 重试。 */
	APlayerController* ResolveLocalController() const;
	/** 释放旧玩家的提示登记与框架委托。 */
	void ReleaseUIContext();
	/** 根据有效允许状态的变化取消操作或立即恢复检测。 */
	void ApplyEffectiveInteractionChange(bool bWasAllowed);
	UFUNCTION()
	void HandleUIInputPolicyChanged(const FUIInputPolicy& Policy);
	UFUNCTION()
	void HandleUIViewStateChanged(FUIViewHandle Handle, EUIViewState PreviousState, EUIViewState State, EUICloseReason Reason);
	UPROPERTY(Transient)
	TWeakObjectPtr<UUISubsystem> UISubsystem;
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> UIController;
	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> UIWorld;
	FUIViewHandle PromptView;
	bool bUIInteractionBlocked = false;
	bool bEndingPlay = false;

	/** 按有效 Tag 去重，多项时打开菜单，否则保留默认请求流程。 */
	void RequestDefaultInteraction(const FGameplayTag& DefaultTag, const FHitResult& Hit, float CompletedHoldTime = 0.f);
	/** 登记并打开本次目标的原生选择器，不执行目标玩法。 */
	void OpenInteractionOptions(const TArray<FInteractionOption>& Options);
	/** 释放选择器登记与捕获上下文。 */
	void DestroyInteractionOptions();
	/** 清除本次候选快照；不会触发任何动作。 */
	void ResetInteractionOptionsContext();
	/** 检查当前 Pawn、目标与长按配置是否仍对应打开菜单时的快照。 */
	bool IsOptionsContextCurrent() const;
	/** 在真正派发及玩法校验回调后复检选中动作的当前上下文。 */
	bool IsSelectedContextCurrent(const FInteractionContext& Context, const APawn* ExpectedPawn, float ExpectedHoldTime) const;
	UPROPERTY(Transient)
	TObjectPtr<UInteractionOptionsWidget> OptionsWidget;
	FUIViewHandle OptionsView;
	TWeakObjectPtr<APawn> OptionsPawn;
	TWeakObjectPtr<AActor> OptionsTarget;
	TWeakObjectPtr<UInteractableComponent> OptionsInteractable;
	TArray<FGameplayTag> OptionsTags;
	float OptionsHoldTime = 0.f;
	bool bSelectingOption = false;
	bool bOptionsCloseConfirmed = false;
	// --------------------------------------------------------------------
	// 内部状态
	// --------------------------------------------------------------------

	/** 射线间隔的时间累加器 */
	float TraceTimeAccum = 0.0f;

	// --------------------------------------------------------------------
	// Enhanced Input 内部状态（V2.0 新增）
	// --------------------------------------------------------------------

	/** 已绑定的 EnhancedInputComponent（用于重绑时 Remove 旧句柄） */
	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	/** 已绑定的 Action → 句柄映射（重绑前 Remove） */
	TArray<FInputBindingHandle> BoundActionHandles;

	/** 是否已成功完成自动输入绑定；为 false 时由 Tick 兜底重试 */
	bool bInputBound = false;

	// --------------------------------------------------------------------
	// Enhanced Input 自动绑定内部函数
	// --------------------------------------------------------------------

	/**
	 * @brief 尝试从本地控制 Owner 取 EnhancedInputComponent 并绑定三个 Action。
	 *
	 * 仅在本地控制时执行，避免服务端误绑。绑定成功后置 bInputBound=true。
	 * 未就绪（InputComponent 为空或非 Enhanced）时返回 false，等待下次重试。
	 * 幂等：已绑定则直接返回 true。
	 */
	bool BindInputIfNeeded();

	/** 解绑所有已绑定的 Action 句柄（EndPlay 调用，避免悬空） */
	void UnbindInput();

	// --------------------------------------------------------------------
	// Widget 生命周期内部函数（V2.1 新增）
	// --------------------------------------------------------------------

	/** 创建并初始化交互提示 Widget（BeginPlay 调用） */
	void CreateInteractionWidget();

	/** 销毁交互提示 Widget（EndPlay 调用） */
	void DestroyInteractionWidget();

	// --------------------------------------------------------------------
	// 长按内部状态
	// --------------------------------------------------------------------

	/** 是否正在长按累积 */
	bool bIsHolding = false;

	/** 本次长按需累积的总时长（秒，从目标 HoldTime 读取） */
	float CurrentHoldTime = 0.0f;

	/** 本次长按已累积的时长（秒） */
	float HoldTimeAccum = 0.0f;

	/** 本次长按使用的交互 Tag（保持与发起时一致） */
	FGameplayTag CurrentHoldTag;

	// --------------------------------------------------------------------
	// 内部函数
	// --------------------------------------------------------------------

	/** 分派入口：按 TraceMode 调用对应子检测函数 */
	UInteractableComponent* PerformTrace(FHitResult& OutHit) const;

	/** 第一人称检测：视点出发，TraceRadius>0 用球扫，否则用射线 */
	UInteractableComponent* PerformFirstPersonTrace(FHitResult& OutHit) const;

	/** 第三人称检测：以玩家为中心的扇形 Overlap，主焦点取 Dot 最大者 */
	UInteractableComponent* PerformThirdPersonTrace(FHitResult& OutHit) const;

	/**
	 * @brief 球形 Overlap 收集器（第三人称焦点检测与 GetNearbyInteractables 共用）。
	 *
	 * - Origin/Forward：检测原点与朝向
	 * - Radius：Overlap 球半径
	 * - HalfAngleDeg：半角（度），bApplyAngleFilter=false 时忽略
	 * - bApplyAngleFilter：是否按半角剔除方向外的候选
	 * - OutCandidates：输出候选（按 Dot 降序排列，便于主焦点取首元素）
	 * - Searcher：只读 UI 查询时额外忽略的当前 Pawn
	 * - bRequireVisibility：只读 UI 查询时启用逐目标遮挡射线，游戏查询保留原规则
	 */
	void GatherOverlapCandidates(
		FVector Origin, FVector Forward, float Radius, float HalfAngleDeg,
		bool bApplyAngleFilter,
		TArray<FInteractionCandidate>& OutCandidates,
		AActor* Searcher = nullptr, bool bRequireVisibility = false) const;

	/** 计算检测原点与朝向（第一人称取 PlayerViewPoint，第三人称取 Actor 位置/Forward） */
	void ComputeViewOriginAndForward(FVector& OutOrigin, FVector& OutForward) const;

	/** 更新焦点状态并触发相应事件 */
	void UpdateFocus(AActor* NewTarget, UInteractableComponent* NewInteractable, const FHitResult& Hit);

	/** 清除当前焦点 */
	void ClearFocus();

	/** 取消长按累积（焦点丢失时调用），若未在长按则无操作 */
	void CancelHoldInternal();

	/** 根据当前状态构建 InteractionContext */
	FInteractionContext BuildContext(const FGameplayTag& Tag, const FHitResult& Hit, UInteractableComponent* Interactable) const;

	/** 客户端侧：校验并发送交互请求 */
	void RequestInteraction(const FGameplayTag& InteractionTag, const FHitResult& Hit);

	/** 共享请求链路；显式选项额外校验捕获 Pawn 与长按配置，不依赖已清除的世界焦点。 */
	void DispatchInteractionContext(const FInteractionContext& Context, APawn* ExpectedPawn = nullptr, float ExpectedHoldTime = 0.f);
	/** 保留原玩法校验与接受通知，选项请求在校验回调后再次检查上下文。 */
	void ExecuteInteractionContext(const FInteractionContext& Context, APawn* ExpectedPawn = nullptr, float ExpectedHoldTime = 0.f);

	// --------------------------------------------------------------------
	// 网络 RPC
	// --------------------------------------------------------------------

	/** 服务端 RPC：请求交互校验和执行 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestInteraction(const FInteractionContext& Context);

	/** NetMulticast：广播交互已被接受 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_InteractionAccepted(const FInteractionContext& Context);

	/** NetMulticast：广播交互已被拒绝 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_InteractionRejected(const FInteractionContext& Context);
};
