# InteractionFramework — 技术设计文档

> **使用教程见同目录 [`UsageGuide.md`](UsageGuide.md)**（面向用户的接入指南：从配置到点按/长按的完整流程）。
> 本文档为架构与接口设计参考，面向维护者。

**版本：** V1.0
**插件名称：** InteractionFramework
**插件类型：** Runtime Plugin
**支持引擎：** UE 5.4+ / 5.5+ / 5.6+ / 5.7+
**作者：** 一氧化二氢
**日期：** 2026-06-14

---

## 1. 项目目标

### 1.1 插件定位

InteractionFramework 是一个：

- 事件驱动
- 组件化
- 接口化
- 低耦合
- 即插即用
- 可网络同步
- 蓝图扩展友好

的通用交互框架。

其职责**仅限于**：

1. 发现可交互对象
2. 管理交互焦点
3. 发送交互请求
4. 传递交互上下文
5. 广播交互事件

插件**不负责**：

- 开门
- 拾取
- 对话
- 驾驶
- 开箱
- 使用工作台
- 任务推进

### 1.2 设计哲学

本框架遵循与 GAS（Gameplay Ability System）相同的设计哲学，但作用于交互领域：**只定义协议、上下文和事件流，绝不定义具体的玩法行为。** 这使得背包插件、任务插件、对话插件、制造插件和锁定插件都可以统一挂接到这一交互协议之上，而彼此之间无直接依赖。

---

## 2. 系统边界

### 2.1 插件负责内容

| 领域 | 说明 |
|------|------|
| 目标发现 | Line Trace、Sphere Trace、Overlap Query |
| 焦点管理 | FocusBegin、FocusEnd、FocusChanged |
| 焦点视觉反馈 | 聚焦描边（可选），基于 Custom Depth + Stencil 驱动后处理材质 |
| 数据获取 | DisplayName、ActionText、InteractionOptions |
| 交互图标 | 由 UInteractionComponent 集中配置（InteractActionIcon 等），与物理按键无关 |
| 请求发送 | InteractionRequest（Client → Server） |
| 网络同步 | Client→Server RPC、Server 校验、事件广播 |
| 事件广播 | OnFocusBegin、OnFocusEnd、OnInteractionSent、OnInteractionAccepted、OnInteractionRejected、OnInteractionCompleted |

### 2.2 明确不负责的内容

以下内容属于游戏逻辑层：

```cpp
// 插件源码中禁止出现
Door->Open();
Chest->Loot();
NPC->Talk();
Inventory->AddItem();
Quest->Accept();
```

---

## 3. 架构总览

```
Player
  │
  ▼
UInteractionComponent          ← 玩家端：检测、输入、焦点、请求
  │
  ▼  （通过 Trace/Overlap 检测）
  │
UInteractableComponent         ← 目标端：数据、选项、启用状态
  │
  ▼  （通过接口查询）
  │
IInteractionInterface          ← 契约：CanInteract、GetInteractionData、OnInteractionRequest
  │
  ▼  （通过结构体传递）
  │
FInteractionContext            ← 数据载体：谁、什么、哪里、何时、哪个Tag
  │
  ▼  （通过委托广播）
  │
事件分发                        ← 订阅方：UI、Quest、Inventory、Dialogue、Game Logic
```

---

## 4. 模块划分

单一 Runtime 模块：`InteractionPlugin`

```
InteractionPlugin/
├── Public/
│   ├── InteractionPlugin.h              ← 模块入口
│   ├── Components/
│   │   ├── InteractionComponent.h
│   │   └── InteractableComponent.h
│   ├── Interfaces/
│   │   └── InteractionInterface.h
│   ├── Structs/
│   │   ├── InteractionContext.h
│   │   ├── InteractionData.h
│   │   ├── InteractionOption.h
│   │   └── OutlineConfig.h
│   ├── Delegates/
│   │   └── InteractionDelegates.h
│   └── Libraries/
│       └── InteractionBlueprintLibrary.h
├── Private/
│   ├── InteractionPlugin.cpp
│   ├── Structs/
│   │   └── InteractionContext.cpp
│   ├── Components/
│   │   ├── InteractionComponent.cpp
│   │   └── InteractableComponent.cpp
│   ├── Interfaces/
│   │   └── InteractionInterface.cpp
│   └── Libraries/
│       └── InteractionBlueprintLibrary.cpp
```

---

## 5. 数据结构设计

### 5.1 FInteractionContext

交互上下文，整个框架的核心数据载体。贯穿每一条交互流程。

```cpp
USTRUCT(BlueprintType)
struct FInteractionContext
{
    GENERATED_BODY()

    /** 交互发起者（通常是玩家 Pawn） */
    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AActor> Instigator = nullptr;

    /** 交互目标 Actor */
    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AActor> TargetActor = nullptr;

    /** 目标身上的 InteractableComponent（使用 UActorComponent* 存储，
     *  以打破循环头文件依赖；用 GetTargetInteractable() 获取类型化指针） */
    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UActorComponent> TargetComponent = nullptr;

    /** 射线命中结果（Overlap Query 触发时可能无效） */
    UPROPERTY(BlueprintReadOnly)
    FHitResult HitResult;

    /** 交互点世界坐标 */
    UPROPERTY(BlueprintReadOnly)
    FVector HitLocation = FVector::ZeroVector;

    /** 标识交互类型的 GameplayTag（如 Interaction.Open） */
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag InteractionTag;

    /** 交互触发时的世界时间 */
    UPROPERTY(BlueprintReadOnly)
    double Timestamp = 0.0;

    /** 便捷方法：HitResult 是否包含有效命中 */
    bool HasHitResult() const { return HitResult.bBlockingHit; }

    /** 类型化访问器——将 TargetComponent 转换为 UInteractableComponent* */
    UInteractableComponent* GetTargetInteractable() const;
};
```

> **设计说明：** `TargetComponent` 使用 `UActorComponent*` 而非 `UInteractableComponent*`，是为了打破 `InteractionContext.h` 与 `InteractableComponent.h` 之间的循环头文件依赖。类型化访问器 `GetTargetInteractable()` 在 .cpp 中实现，此时两个头文件均可见。这是 UE 中避免 USTRUCT 头文件循环依赖的标准做法。

### 5.2 FInteractionData

UI 展示数据。

```cpp
USTRUCT(BlueprintType)
struct FInteractionData
{
    GENERATED_BODY()

    /** UI 显示名称（如"木门"） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Name;

    /** 动作描述（如"打开"、"搜索"） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText ActionText;

    /** 按住时长：>0 表示需要长按触发 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float HoldTime = 0.0f;

    /** 当前是否允许交互 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanInteract = true;

    /** 可用的交互选项（多交互支持） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FInteractionOption> Options;
};
```

### 5.3 FInteractionOption

多交互支持（如同一个宝箱上可"打开"/"搜索"/"检查"）。

```cpp
USTRUCT(BlueprintType)
struct FInteractionOption
{
    GENERATED_BODY()

    /** 该选项的 GameplayTag（如 Interaction.Open、Interaction.Search） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag Tag;

    /** 选项显示文本 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayText;

    /** 可选图标 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UTexture2D> Icon = nullptr;

    /** 该选项是否拥有有效 Tag */
    bool IsValid() const { return Tag.IsValid(); }
};
```

### 5.4 FOutlineConfig

对象获得交互焦点时的描边配置，由 UInteractableComponent 持有。

```cpp
USTRUCT(BlueprintType)
struct FOutlineConfig
{
    GENERATED_BODY()

    /** 是否在该对象获得交互焦点时启用描边 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outline")
    bool bEnableOutline = true;

    /** 显式指定要描边的网格组件；为空时自动使用 Owner 上所有 UMeshComponent */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outline",
              meta = (AllowedClasses = "/Script/Engine.StaticMeshComponent,/Script/Engine.SkeletalMeshComponent"))
    TArray<TObjectPtr<UMeshComponent>> OutlineMeshes;

    /** 自定义深度模板值（0-255），后处理材质据此区分描边样式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outline",
              meta = (ClampMin = "0", ClampMax = "255"))
    int32 StencilValue = 1;
};
```

> **设计说明：** 框架仅负责按配置开关目标 Mesh 的 `SetRenderCustomDepth(true/false)` + `SetCustomDepthStencilValue(N)`，不操作任何具体材质。描边颜色、粗细、样式由用户的后处理材质（挂到 PostProcessVolume 的 Blendables）依据 Stencil 值自主决定，保持框架纯协议层定位。

**描边触发机制：**

```
UInteractionComponent::UpdateFocus()
  ├── 旧焦点目标 → Interactable->NotifyFocusLost()
  │     └── ApplyOutline(false)  // 关闭 Custom Depth
  └── 新焦点目标 → Interactable->NotifyFocusGained()
        └── ApplyOutline(true)   // 根据 OutlineMeshes（或自动取 Owner 的 Mesh）开启
```

**注意事项：**
- 描边仅在焦点切换瞬间触发，零每帧开销。
- `UninitializeComponent()` 会主动清理描边，防止组件销毁后残留。
- `Interactable` 碰撞通道需在 DefaultEngine.ini 配置，详见 §8.1 说明。

---

## 6. 委托声明

所有框架事件使用 `DECLARE_DYNAMIC_MULTICAST_DELEGATE` 以支持蓝图绑定。

> **重要：** `FInteractionContext` 参数按值传递（非 const 引用），因为 `DYNAMIC_MULTICAST` 委托不支持引用传递。

```cpp
// --- 焦点事件 ---

/** 目标获得交互焦点时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnFocusBegin, AActor*, TargetActor, UInteractableComponent*, Interactable);

/** 目标失去交互焦点时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnFocusEnd, AActor*, TargetActor, UInteractableComponent*, Interactable);

/** 焦点从一目标切换到另一目标时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnFocusChanged, AActor*, OldTarget, AActor*, NewTarget, UInteractableComponent*, NewInteractable);

// --- 描边事件 ---

/** InteractableComponent 描边状态改变时广播（获得/失去焦点） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnOutlineStateChanged, UInteractableComponent*, Interactable, bool, bIsOutlined);

// --- 交互事件 ---

/** 交互请求从客户端发出时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnInteractionSent, AActor*, Instigator, FInteractionContext, Context);

/** 目标接受交互时广播（经服务器验证） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnInteractionAccepted, AActor*, Instigator, FInteractionContext, Context);

/** 目标拒绝交互时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnInteractionRejected, AActor*, Instigator, FInteractionContext, Context);

/** 交互流程完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnInteractionCompleted, AActor*, Instigator, FInteractionContext, Context);

// --- 长按事件 ---

/** 长按开始累积时广播（携带本次需累积的总时长，供 UI 显示） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnHoldStarted, AActor*, TargetActor, UInteractableComponent*, Interactable, float, HoldTime);

/** 长按被取消时广播（松手或失去焦点） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnHoldCanceled, AActor*, TargetActor, UInteractableComponent*, Interactable);

/** 长按累积满时广播（纯本地事件，不依赖服务端校验结果） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnHoldCompleted, AActor*, TargetActor, UInteractableComponent*, Interactable);
```

---

## 7. 接口设计

### 7.1 IInteractionInterface

基于 BlueprintNativeEvent 的交互行为扩展接口。游戏层通过实现此接口定义具体行为。

```cpp
UINTERFACE(Blueprintable)
class UInteractionInterface : public UInterface
{
    GENERATED_BODY()
};

class INTERACTIONPLUGIN_API IInteractionInterface
{
    GENERATED_BODY()

public:
    /** 在给定上下文中，该 Actor 是否允许被交互？
     *  服务端校验时调用。示例：门是否锁定、玩家是否持有钥匙、任务条件是否满足。
     *  默认实现：返回 true */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool CanInteract(const FInteractionContext& Context) const;

    /** 返回用于 UI 显示的交互数据。
     *  客户端获取焦点时调用，用于填充 UI。
     *  默认实现：返回空 FInteractionData */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FInteractionData GetInteractionData() const;

    /** 交互请求到达该 Actor 时调用。
     *  这是唯一的玩法行为扩展点。框架只负责调用，不做任何解释。
     *  游戏层在此实现：播放动画、修改状态、触发任务等。
     *  默认实现：空操作 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void OnInteractionRequest(const FInteractionContext& Context);
};
```

**关键设计决策：**

- 接口标记为 `Blueprintable`（未使用 `CannotImplementInterfaceInBlueprint`），允许蓝图完全实现所有函数。C++ 提供安全的默认 `_Implementation` 实现。
- `OnInteractionRequest` 是唯一的玩法行为扩展点。框架绝不对其做任何解读。
- 所有三个函数都有 C++ 默认实现，蓝图可选择性地覆盖任意函数。

---

## 8. 组件设计

### 8.1 UInteractionComponent

挂载对象：PlayerCharacter / PlayerController / Pawn

**职责：**

1. 周期性 Trace 检测可交互对象
2. 焦点管理（Begin/End/Changed）
3. 输入处理（Press/Hold/Release）
4. 发送交互请求（Client→Server RPC）
5. 广播焦点和交互事件

**核心属性：**

| 属性 | 类型 | 访问级别 | 默认值 | 说明 |
|------|------|----------|--------|------|
| TraceDistance | float | EditDefaultsOnly | 300.0 | Trace 距离 |
| TraceRadius | float | EditDefaultsOnly | 0.0 | 0=LineTrace，>0=SphereTrace |
| TraceChannel | ECollisionChannel | EditDefaultsOnly | GameTraceChannel2 (Interactable) | 碰撞通道；需在 DefaultEngine.ini 定义 |
| TraceInterval | float | EditDefaultsOnly | 0.1 | Trace 扫描间隔（秒） |
| bEnableHoldInteraction | bool | EditDefaultsOnly | true | 是否处理长按交互 |
| TraceMode | EInteractionTraceMode | EditAnywhere | FirstPerson | 检测模式（第一人称/第三人称），运行时可用 SetTraceMode() 切换 |
| TraceHalfAngle | float | EditAnywhere | 60.0 | 第三人称扇形半角（度），仅在 ThirdPerson 模式下生效；扇形张角 = 2 × TraceHalfAngle |
| bInteractionEnabled | bool | EditAnywhere | true | 玩家侧交互总开关（运行时可由 SetInteractionEnabled 修改） |
| CurrentFocusedActor | AActor* | BlueprintReadOnly | nullptr | 当前焦点目标 |
| CurrentInteractable | UInteractableComponent* | BlueprintReadOnly | nullptr | 当前焦点交互组件 |
| InteractActionIcon / QueryActionIcon / HoldActionIcon | UTexture2D* | EditDefaultsOnly | nullptr | 三个交互动作（点按/查询/长按）各自的提示图标，集中在此配置；KeyIcon 在焦点进入时读取 InteractActionIcon 填充 |

**核心函数：**

| 函数 | 网络角色 | 说明 |
|------|----------|------|
| `PerformTrace()` | Client | 执行 Trace，更新焦点 |
| `UpdateFocus(AActor*, UInteractableComponent*)` | Client | 管理焦点切换和事件触发 |
| `RequestInteraction(FGameplayTag)` | Client | 校验并发送 Server RPC |
| `Server_RequestInteraction(FInteractionContext)` | Server RPC | 服务端校验并执行 |
| `Multicast_InteractionAccepted(FInteractionContext)` | NetMulticast | 广播交互接受事件 |
| `Multicast_InteractionRejected(FInteractionContext)` | NetMulticast | 广播交互拒绝事件 |
| `TryInteract()` | Client | 蓝图可调用的默认交互入口 |
| `TryInteractWithTag(FGameplayTag)` | Client | 蓝图可调用，指定 Tag 交互 |
| `ForceRefreshFocus()` | Client | 强制立即刷新焦点检测 |
| `SetTraceMode(EInteractionTraceMode)` | Client | 切换检测模式，广播 OnTraceModeChanged 并强制刷新焦点 |
| `GetTraceMode()` | Client | 返回当前检测模式 |
| `GetNearbyInteractables()` | Client | 按需查询周围可交互物品数组（FInteractionCandidate），按距离升序，标记当前主焦点 |
| `SetInteractionEnabled(bool)` | Client | 启用/禁用玩家侧整个交互能力，广播 OnInteractionEnabledChanged |
| `BeginHold()` / `EndHold()` | Client | 长按交互入口（累积 HoldTime 后自动触发） |
| `BindInputIfNeeded()` / `UnbindInput()` | Client | 自动输入绑定：依据 Details 配置的三个 Action 绑定 EnhancedInputComponent；BeginPlay 尝试，未就绪时 Tick 兜底，EndPlay 解绑 |

> 输入绑定改为自动：三个 Action（InteractAction/QueryAction/HoldAction）在 Details 面板配置为唯一来源，BeginPlay 起从本地控制 Owner 的 EnhancedInputComponent 自动 BindAction，无需蓝图手动调用。仅本地控制端绑定，服务端/非控制端跳过。

**生命周期流程：**

```
Tick/Timer
  → PerformTrace()
    → 发现可交互对象？
      → 与当前焦点相同？→ 无操作
      → 不同？→ UpdateFocus()
        → 旧目标：NotifyFocusLost() → OnFocusEnd
        → 新目标：NotifyFocusGained() → OnFocusBegin
        → 广播：OnFocusChanged
    → 未发现？
      → 之前有焦点？→ 清除焦点，OnFocusEnd

输入：交互键按下
  → TryInteract()
    → 有焦点？→ RequestInteraction(DefaultTag)
      → Server_RequestInteraction()
        → 通过 IInteractionInterface::CanInteract 校验
        → 有效？→ 执行 IInteractionInterface::OnInteractionRequest
                  → Multicast_InteractionAccepted（NetMulticast）
        → 无效？→ Multicast_InteractionRejected（NetMulticast）
```

### 8.2 UInteractableComponent

挂载对象：Door、Chest、NPC、Button、Workbench 等

**职责：**

1. 存储交互配置和选项
2. 向 InteractionSubsystem 注册/注销
3. 为 InteractionComponent 查询提供数据

**核心属性：**

| 属性 | 类型 | 访问级别 | 默认值 | 说明 |
|------|------|----------|--------|------|
| InteractionData | FInteractionData | EditAnywhere | - | 显示数据（含 Options） |
| OutlineConfig | FOutlineConfig | EditAnywhere | - | 聚焦描边配置（可选） |
| bEnabled | bool | EditAnywhere | true | 是否允许交互 |
| DefaultInteractionTag | FGameplayTag | EditAnywhere | - | 未指定选项时的默认 Tag |

**核心函数：**

| 函数 | 说明 |
|------|------|
| `SetEnabled(bool)` | 切换启用状态，广播 OnEnabledChanged 事件 |
| `IsEnabled()` | 返回当前启用状态 |
| `GetInteractionData()` | 返回 InteractionData |
| `GetOptions()` | 返回 Options 数组 |
| `FindOptionByTag(FGameplayTag)` | 按 Tag 查找选项 |
| `GetDefaultInteractionTag()` | 返回默认交互 Tag（优先 DefaultInteractionTag，其次首个 Option 的 Tag） |
| `NotifyFocusGained()` | 由 InteractionComponent 在获得焦点时调用，触发描边 |
| `NotifyFocusLost()` | 由 InteractionComponent 在失去焦点时调用，关闭描边 |
| `IsOutlined()` | 返回当前是否处于描边状态 |

**生命周期：**

```
UninitializeComponent()
  → 若处于描边状态则 ApplyOutline(false)  // 防止组件销毁后残留
```

### 8.3 UInteractionWidget

挂载对象：由 UInteractionComponent 在 BeginPlay 创建并管理的交互提示 UI（蓝图派生类提供视觉）。

**必需控件契约（meta=BindWidget）：**

| 控件 | 类型 | 说明 |
|------|------|------|
| ActionText | UTextBlock | 动作描述（如"打开"），来自 FInteractionData::ActionText |
| KeyIcon | UImage | 交互动作提示图标，OnFocusBegin 时从 InteractionComponent.InteractActionIcon 读取填充（与物理按键无关） |
| HoldProgressBar | UProgressBar | 长按进度条，仅在 HoldTime>0 时可见，NativeTick 推进 |
| InteractionTypeText | UTextBlock | 交互类型说明，OnFocusBegin 时按 HoldTime 填充"点击"或"长按" |

**可配置属性：**

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| ClickText | FText | "点击" | 瞬时目标（HoldTime<=0）显示的类型文案 |
| HoldText | FText | "长按" | 长按目标（HoldTime>0）显示的类型文案 |

> 交互类型文案在焦点进入时一次性确定，依据目标静态配置的 HoldTime 而非运行时按下手势；与 HoldProgressBar 显隐判定保持一致。

---

## 9. 蓝图函数库设计

便捷工具函数，提供蓝图访问入口。

```cpp
UCLASS()
class UInteractionBlueprintLibrary : public UBlueprintFunctionLibrary
{
    /** 交互上下文是否有效 */
    UFUNCTION(BlueprintPure, Category="Interaction")
    static bool IsValidInteractionContext(const FInteractionContext& Context);

    /** 获取可交互组件的显示名称 */
    UFUNCTION(BlueprintPure, Category="Interaction")
    static FText GetInteractionDisplayName(UInteractableComponent* Interactable);

    /** 获取交互上下文的 GameplayTag */
    UFUNCTION(BlueprintPure, Category="Interaction")
    static FGameplayTag GetInteractionTag(const FInteractionContext& Context);

    /** 获取可交互组件的全部选项 */
    UFUNCTION(BlueprintPure, Category="Interaction")
    static TArray<FInteractionOption> GetInteractableOptions(UInteractableComponent* Interactable);

    /** 触发指定 Tag 的交互 */
    UFUNCTION(BlueprintCallable, Category="Interaction")
    static void TriggerInteraction(
        UInteractionComponent* InteractionComp, FGameplayTag InteractionTag);

    /** 上下文是否包含有效命中结果 */
    UFUNCTION(BlueprintPure, Category="Interaction")
    static bool ContextHasHitResult(const FInteractionContext& Context);

    /** 获取交互上下文的命中位置 */
    UFUNCTION(BlueprintPure, Category="Interaction")
    static FVector GetContextHitLocation(const FInteractionContext& Context);
};
```

---

## 10. GameplayTag 设计

框架建议（但不强制）以下 Tag 层级：

```
交互.打开
交互.关闭
交互.使用
交互.检查
交互.对话
交互.拾取
交互.激活
交互.关闭激活
交互.搜索
交互.装备
交互.卸下
交互.制造
交互.锁定
交互.解锁
```

框架**不解释** Tag 的语义，仅通过 `FInteractionContext::InteractionTag` 传递。

**实现方式：** 框架通过 C++ Native Tag 静态注册上述标签（见 `InteractionTags.h` / `InteractionTags.cpp`），编辑器内可直接选用；用户可在编辑器中扩展更多子 Tag。

---

## 11. 网络架构

```
┌──────────┐                                  ┌──────────┐
│  Client   │                                  │  Server   │
└─────┬─────┘                                  └─────┬─────┘
      │                                              │
      │  PerformTrace()（仅本地）                    │
      │  UpdateFocus()（仅本地）                     │
      │                                              │
      │  TryInteract()                               │
      │    │                                         │
      │    ▼                                         │
      │  Server_RequestInteraction(Context) ────────►│
      │                                              │  校验：
      │                                              │  IInteractionInterface::CanInteract()
      │                                              │
      │                                              │  ┌─ 是否有效？
      │                                              │  │  是 → OnInteractionRequest()
      │                                              │  │       Multicast_InteractionAccepted
      │                                              │  │  否 → Multicast_InteractionRejected
      │                                              │  └─
      │  ◄── Multicast_InteractionAccepted/Rejected  │
      │                                              │
      ▼                                              ▼
```

**复制规则：**

- `UInteractionComponent`：Replicated，Owner = 拥有 Pawn 的客户端
- `UInteractableComponent`：Replicated，无 Owner（服务器权威）
- `CurrentFocusedActor` / `CurrentInteractable`：仅客户端有效（不复制）
- `FInteractionContext`：作为 RPC 参数传递（结构体序列化）
- 所有关键事件通过 NetMulticast 在服务器和所有客户端上广播

---

## 12. 蓝图扩展方案

**核心原则：** C++ 提供框架，蓝图实现行为。

### 示例：门 Actor

1. 创建 `ADoor`（或 BP_Door）Actor
2. 添加 `UInteractableComponent` 组件
3. 配置：Name="木门"、ActionText="打开"、Tag=Interaction.Open
4. 在蓝图（或 C++）中实现 `IInteractionInterface`
5. 在 `OnInteractionRequest` 事件中：
   - 播放开门动画
   - 播放音效
   - 修改门的状态变量
6. 在 `CanInteract` 中：
   - 如果 `bIsLocked` 为 true 则返回 false

框架完全不知道门是如何打开的，它只确保交互请求抵达门的 `OnInteractionRequest`。

---

## 13. 开发原则

| 优先级 | 原则 | 允许 | 禁止 |
|--------|------|------|------|
| P1 | 事件优先 | `Broadcast()` | `Door->Open()` |
| P2 | 接口优先 | `Execute_OnInteractionRequest()` | `Cast<ADoor>()` |
| P3 | 组件优先 | `InteractableComponent` | 继承固定基类 |
| P4 | 数据驱动 | `GameplayTag`、`DataAsset`、`DataTable` | 大量硬编码 Enum |

---

## 14. V1.0 验收标准

### 即插即用

任意 Actor → 添加 `InteractableComponent` → 立即被任意 `InteractionComponent` 检测到。

### 无业务逻辑

插件源码中不存在：

```cpp
OpenDoor()
PickupItem()
StartDialogue()
```

### 蓝图可扩展

无需修改插件源码即可实现：开门、拾取、对话、任务推进、驾驶、工作台——全部由蓝图或游戏层 C++ 实现。

### 插件解耦

同时支持：Inventory Plugin、Quest Plugin、Dialogue Plugin、Craft Plugin、Lock Plugin——彼此之间零直接依赖。

---

## 15. 头文件依赖图

```
InteractionOption.h             （依赖：GameplayTag）
         │
         ▼
InteractionData.h               （依赖：InteractionOption、Texture2D）
         │
         ▼
InteractableComponent.h         （依赖：InteractionData、OutlineConfig、ActorComponent）
                                  （声明本地委托：FOnInteractableEnabledChanged、FOnOutlineStateChanged）

InteractionContext.h             （依赖：GameplayTag、HitResult；前向声明 UInteractableComponent）
         │
         ▼
InteractionDelegates.h          （依赖：InteractionContext；前向声明 UInteractableComponent）
         │
         ▼
InteractionInterface.h          （依赖：InteractionContext、InteractionData）
         │
         ▼
InteractionComponent.h          （依赖：InteractableComponent、InteractionDelegates、InteractionContext）
         │
         ▼
InteractionBlueprintLibrary.h   （依赖：InteractionContext、InteractableComponent、InteractionComponent）
```

**循环依赖打破方式：** `InteractionContext.h` 中的 `TargetComponent` 使用 `UActorComponent*`，`GetTargetInteractable()` 的类型化转换放在 .cpp 中实现。

---

## 16. Build.cs 依赖

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "GameplayTags"
});

PrivateDependencyModuleNames.AddRange(new string[] {
    "Slate",
    "SlateCore"
});
```

无需额外插件依赖。
