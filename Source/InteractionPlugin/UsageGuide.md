# InteractionPlugin 使用教程

本教程演示如何从零接入交互系统：玩家检测到可交互对象 → 显示提示 UI → 点按/长按触发交互 → 游戏层响应。

---

## 1. 前置准备

### 1.1 启用插件

项目启用 `InteractionPlugin`；插件会通过单向依赖启用 `UISystem` 与 `EnhancedInput`。必须保留这些依赖插件：

```json
"Plugins": [
  { "Name": "EnhancedInput", "Enabled": true },
  { "Name": "InteractionPlugin", "Enabled": true }
]
```

### 1.2 配置碰撞通道

`InteractionComponent` 默认用 `Interactable` 碰撞通道（`ECC_GameTraceChannel2`）做射线检测。需在 `Config/DefaultEngine.ini` 中定义该通道：

```ini
[/Script/Engine.CollisionProfile]
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,Name="Interactable",bCollisionResponse=true)
```

随后在可交互对象的碰撞体上，将 **Collision Presets** 的 `Visibility`（或相机通道）对 `Interactable` 通道设为 **Block**，确保射线能命中。

> 若已使用其他自定义通道，可在 `InteractionComponent` 的 Details 面板修改 `Trace Channel`。

### 1.3 准备 Enhanced Input 资产

创建三个 `InputAction` 资产（右键 → Input → Input Action）：

| 资产名        | 用途                         |
|---------------|------------------------------|
| `IA_Interact` | 点按交互（瞬时触发）         |
| `IA_Query`    | 周围检测（查询附近可交互物） |
| `IA_Hold`     | 长按交互（按住累积）         |

创建一个 `InputMappingContext`（`IMC_Default`），把上述 Action 绑定到物理按键（如 `IA_Interact`→`E`、`IA_Hold`→`F`）。在 PlayerController 的 BeginPlay 里
`Add Mapping Context`。

> InputAction 与 IMC 的创建、按键绑定是 UE Enhanced Input 的标准流程，不属于本插件范畴。

---

## 2. 玩家侧配置

### 2.1 挂载 InteractionComponent

在玩家 Character（或 PlayerController / Pawn）上添加 `UInteractionComponent` 组件。

### 2.2 配置三个 Action 与图标

在 `InteractionComponent` 的 Details 面板， **Interaction | Input** 分类下配置：

| 字段                   | 值                                 |
|------------------------|------------------------------------|
| `Interact Action`      | `IA_Interact`                      |
| `Interact Action Icon` | 对应的按键图标（UTexture2D，可选） |
| `Query Action`         | `IA_Query`                         |
| `Query Action Icon`    | 对应图标（可选）                   |
| `Hold Action`          | `IA_Hold`                          |
| `Hold Action Icon`     | 对应图标（可选）                   |

> **输入自动绑定：** 无需在蓝图里手动调用绑定函数。组件在 BeginPlay 起自动从本地控制 Owner 的 `EnhancedInputComponent` 绑定三个 Action；若此时 InputComponent
> 尚未就绪，由 Tick 兜底补绑。仅本地控制端绑定，服务端自动跳过。

### 2.3 配置交互提示 UI 类

在 **Interaction | UI** 分类下设置：

| 字段                        | 值                                                      |
|-----------------------------|---------------------------------------------------------|
| `Interaction Widget Class`  | 你派生的 `BP_InteractionWidget`（见第 4 节）            |
| `Interaction Widget ZOrder` | 旧资产兼容字段，已停用；层级由 UISystem 的 HUD 策略决定 |

组件解析所属的本地 PlayerController，使用该 Controller 创建 Widget，调用 `NativeInit` 注入组件，再以 `InteractionPlugin.Prompt` 登记并打开 UISystem
HUD。远端实例不创建提示；Pawn 的 Controller 或 LocalPlayer 延迟就绪时由后续 Tick 完成接入。每个本地玩家应只挂载一个玩家侧交互组件，以对应唯一的提示会话。

UI 是否阻止世界交互只依据 `FUIInputPolicy.bBlockGameActions`。首次接入读取快照，并订阅策略变更；即使未配置 Widget 类，也会阻止交互。`SetInteractionEnabled` 和
`IsInteractionEnabled` 保留业务开关语义；`SetUIInteractionBlocked` 独立管理 UI 阻止状态，不广播 `OnInteractionEnabledChanged`。`IsInteractionEffectivelyEnabled`
返回业务开关开启且 UI 未阻止。

Menu、Conversation 或 Pause 打开时会取消长按，清除焦点与描边，并拒绝新交互和周围查询。关闭 UI 后只恢复检测，必须重新按下长按键；业务原本禁用时仍保持禁用。EndPlay
注销提示并解绑框架通知与输入。

### 2.4 检测参数（按需调整）

**Interaction | Trace** 分类下可调整：

| 字段               | 默认        | 说明                                     |
|--------------------|-------------|------------------------------------------|
| `Trace Distance`   | 300         | 射线最大距离                             |
| `Trace Radius`     | 0           | 0=LineTrace，>0=SphereTrace              |
| `Trace Interval`   | 0.1         | 检测间隔（秒），0=每帧                   |
| `Trace Mode`       | FirstPerson | 检测模式，运行时可 `SetTraceMode()` 切换 |
| `Trace Half Angle` | 60          | 第三人称扇形半角（度）                   |

---

## 3. 可交互对象配置

以"一扇门"为例。

### 3.1 挂载 InteractableComponent

在 `BP_Door`（或 C++ Actor）上添加 `UInteractableComponent`。

### 3.2 配置交互数据

在 `InteractableComponent` 的 Details 面板，编辑 `Interaction Data`：

| 字段             | 示例值   | 说明                                                        |
|------------------|----------|-------------------------------------------------------------|
| `Name`           | "木门"   | UI 显示的对象名                                             |
| `Action Text`    | "打开"   | UI 显示的动作描述                                           |
| `Hold Time`      | 0 或 1.5 | **>0 = 长按型**（需按住该秒数）；**0 = 点按型**（瞬时触发） |
| `Cooldown Time`  | 0        | 交互冷却（秒），0=无冷却                                    |
| `b Can Interact` | true     | 是否允许交互                                                |

> `Hold Time` 是区分点按/长按的唯一依据。UI 的进度条与交互类型文案都据此判定。

### 3.3 多交互选项（可选）

若一个对象支持多种交互（如宝箱可"打开"/"搜索"），在 `Options` 数组里添加多个 `FInteractionOption`，每项含：

- `Tag`：标识该交互类型的 GameplayTag（如 `Interaction.Open`、`Interaction.Search`）
- `Display Text`：该选项的显示文本

`Options` 按有效 Tag 去重后，多于一项时按现有交互键打开原生列表，使用 `InteractionPlugin.Options` 的 UISystem Menu／MainMenu 会话，不暂停世界。长按目标仍先完成 `Hold Time`，再显示列表。零项或单项沿用 `Default Interaction Tag` 的原回退规则；已有显式 `TryInteractWithTag` 入口保留。

使用鼠标或既有控件焦点导航选择动作，取消按钮或 UI Root 的 ESC 返回不执行。选中后先完成选择器关闭，再复检打开时捕获的 Pawn、目标、动作 Tag、长按配置及当前检测结果。自己的菜单不会阻止合法选择，Conversation／Pause／System 等更高前台界面仍会阻止。若关闭返回 Queued，本次选择返回 false，不缓存等待重放的动作。

默认表现无需新增输入资产或 Widget 蓝图；`OptionsWidgetClass` 留空使用原生 `UInteractionOptionsWidget`，原有 `WBP_InteractionWidget` 提示继续使用。自定义原生表现可覆写 `SetOptions`、`GetInitialFocusWidget` 并调用组件的 `SelectInteractionOption`／`CancelInteractionOptions`。业务关闭、读档清理、Pawn 替换和 Owner 结束均清除旧选择。Accepted／Completed 保留请求语义，不能用作任务交付成功结果。

### 3.4 实现 IInteractionInterface（定义行为）

让 `BP_Door` 的父类（或 C++ 类）实现 `IInteractionInterface`。这是 **唯一**的玩法扩展点，框架不实现任何行为。

四个可覆写函数（蓝图中均为可覆盖事件）：

| 函数                            | 作用                                     | 何时调用              |
|---------------------------------|------------------------------------------|-----------------------|
| `CanInteract(Context)`          | 是否允许交互（门是否上锁等）             | 服务端校验时          |
| `GetInteractionData()`          | 返回显示数据                             | 获得焦点时（UI 填充） |
| `OnInteractionRequest(Context)` | **核心**：响应交互（开门动画/音效/状态） | 交互被接受时          |
| `GetOutlineMeshes()`            | 返回需描边的网格列表                     | 获得焦点时            |

**蓝图实现示例（门）：**

- `OnInteractionRequest`：播放开门动画 Timeline + 音效 + 翻转 `bIsOpen` 变量
- `CanInteract`：若 `bIsLocked` 为 true 则返回 false

### 3.5 描边配置（可选）

`InteractableComponent` 的 **Outline** 分类下：

- `Enable Outline`：是否在获得焦点时描边（默认 true）
- `Stencil Value`：自定义深度模板值（0-255），后处理材质据此区分描边样式

---

## 4. UI 配置

### 4.1 派生 Widget 蓝图

创建一个 Widget Blueprint，父类选择 `UInteractionWidget`（插件提供的抽象基类）。

### 4.2 提供四个必需控件（BindWidget 契约）

Widget 蓝图内 **必须**包含以下四个控件，且 **名称完全一致**（大小写敏感），否则蓝图编译报 BindWidget 缺失：

| 控件名                | 类型         | 作用                                                                    |
|-----------------------|--------------|-------------------------------------------------------------------------|
| `ActionText`          | Text Block   | 动作描述（如"打开"）                                                    |
| `KeyIcon`             | Image        | 交互动作提示图标（来自 InteractionComponent 配置的 InteractActionIcon） |
| `HoldProgressBar`     | Progress Bar | 长按进度条（仅长按型目标可见，Tick 自动推进）                           |
| `InteractionTypeText` | Text Block   | 交互类型说明（"点击"或"长按"）                                          |

### 4.3 可配置文案

Widget 的 **Interaction | Type** 分类下可自定义交互类型文案：

| 字段         | 默认   | 显示时机                   |
|--------------|--------|----------------------------|
| `Click Text` | "点击" | 目标 HoldTime<=0（点按型） |
| `Hold Text`  | "长按" | 目标 HoldTime>0（长按型）  |

### 4.4 可选视觉钩子（蓝图重写事件）

以下事件可在 Widget 蓝图中重写，用于播动画/特效：

| 事件                | 触发时机                        |
|---------------------|---------------------------------|
| `On Widget Created` | Widget 创建完成（一次性初始化） |
| `On Prompt Shown`   | 提示出现（焦点进入）            |
| `On Prompt Hidden`  | 提示消失（焦点离开）            |

> 控件内容的填充（ActionText 文本、KeyIcon 图标、进度条进度、类型文案）由 C++ 基类自动完成，蓝图只需设计视觉布局与动画。

### 4.5 设计布局并指定为 Widget Class

在 Widget 蓝图里排版四个控件，保存。回到第 2.3 节，将此 Widget 蓝图指定给 `InteractionComponent` 的 `Interaction Widget Class`。

C++ 在实例 `RebuildWidget` 中自动以 `InteractionPromptContent` 容器包裹原控件树，无需重命名四个既有 BindWidget 控件或更改原动画目标。目标焦点只改变内部内容容器的显隐；外层挂载、遮挡及输入由
UISystem 管理。蓝图视觉钩子可修改内部控件和动画，不应再调用顶层 `SetVisibility`、`AddToViewport` 或 `RemoveFromParent`。鼠标显示且输入策略未阻止游戏操作时，提示仍按目标正常显示。

---

## 5. 两条交互路径

### 5.1 点按路径（HoldTime = 0）

```
玩家按 E（IA_Interact）
  → InteractionComponent.TryInteract()
    → RequestInteraction(DefaultTag) → Server RPC
      → 服务端 CanInteract 校验
        → 通过 → OnInteractionRequest() 执行行为 → Multicast_InteractionAccepted
        → 拒绝 → Multicast_InteractionRejected
```

### 5.2 长按路径（HoldTime > 0）

```
玩家按住 F（IA_Hold）
  → InteractionComponent.BeginHold()
    → 进入累积，广播 OnHoldStarted（UI 进度条出现）
  → 每帧 Tick 推进累积，UI 进度条 0→1
  → 累积满
    → 广播 OnHoldCompleted（本地即时，不等服务端）
    → RequestInteraction → 同点按路径走服务端校验
  → 若中途松手
    → EndHold → 取消累积，广播 OnHoldCanceled（进度条归零）
  → 若中途失去焦点
    → 自动取消累积，广播 OnHoldCanceled
```

---

## 6. 关键事件速查

### 6.1 InteractionComponent 事件（玩家侧，蓝图可绑定）

| 事件                             | 参数                                | 触发时机                         |
|----------------------------------|-------------------------------------|----------------------------------|
| `On Focus Begin`                 | Actor, Interactable                 | 焦点进入新目标                   |
| `On Focus End`                   | Actor, Interactable                 | 焦点离开                         |
| `On Focus Changed`               | OldActor, NewActor, NewInteractable | 焦点切换                         |
| `On Interaction Sent`            | Instigator, Context                 | 客户端发出请求                   |
| `On Interaction Accepted`        | Instigator, Context                 | 服务端接受                       |
| `On Interaction Rejected`        | Instigator, Context                 | 服务端拒绝                       |
| `On Interaction Completed`       | Instigator, Context                 | 交互流程完成                     |
| `On Hold Started`                | Actor, Interactable, HoldTime       | 长按开始累积                     |
| `On Hold Canceled`               | Actor, Interactable                 | 长按中途取消                     |
| `On Hold Completed`              | Actor, Interactable                 | 长按累积满（纯本地，与网络无关） |
| `On Interaction Enabled Changed` | Component, bEnabled                 | 交互总开关切换                   |
| `On Trace Mode Changed`          | Component, NewMode                  | 检测模式切换                     |
| `On Nearby Queried`              | Component, Candidates               | 周围检测完成                     |

### 6.2 InteractableComponent 事件（目标侧）

| 事件                       | 参数                      | 触发时机        |
|----------------------------|---------------------------|-----------------|
| `On Enabled Changed`       | Interactable, bEnabled    | 该对象启用/禁用 |
| `On Outline State Changed` | Interactable, bIsOutlined | 描边状态改变    |

### 6.3 典型用法

- **开门动画**：绑定 `On Interaction Accepted`，播放 Timeline
- **长按进度反馈**：UI 基类已自动处理进度条；额外音效可绑 `On Hold Started`/`On Hold Completed`
- **拾取消失**：`On Interaction Accepted` 后销毁 Actor
- **锁定提示**：`On Interaction Rejected` 时弹"门已锁"提示
- **过场禁用交互**：调用 `SetInteractionEnabled(false)`，过场结束后 `SetInteractionEnabled(true)`

---

## 7. 运行时 API

### 7.1 玩家侧（InteractionComponent）

| 函数                          | 说明                                       |
|-------------------------------|--------------------------------------------|
| `TryInteract()`               | 多有效选项打开菜单；否则沿用默认 Tag       |
| `TryInteractWithTag(Tag)`     | 指定 Tag 触发                              |
| `SelectInteractionOption(Tag)` | 选择当前前台菜单中的动作，关闭提交并复检后发送；不表示玩法成功 |
| `CancelInteractionOptions()` | 取消菜单与捕获上下文，不执行交互           |
| `IsInteractionOptionsOpen()` | 查询选择器是否打开，挂起时仍返回真         |
| `BeginHold()` / `EndHold()`   | 长按入口（通常由输入自动绑定，也可手动调） |
| `IsHolding()`                 | 是否长按中                                 |
| `GetCurrentHoldProgress()`    | 长按进度 0-1                               |
| `SetInteractionEnabled(bool)` | 开关整个交互能力                           |
| `SetTraceMode(Mode)`          | 切换第一/第三人称检测                      |
| `GetNearbyInteractables()`    | 查询周围可交互物数组                       |
| `ForceRefreshFocus()`         | 强制立即刷新焦点                           |

### 7.2 目标侧（InteractableComponent）

| 函数                         | 说明                  |
|------------------------------|-----------------------|
| `SetEnabled(bool)`           | 启用/禁用此对象的交互 |
| `IsOnCooldown()`             | 是否冷却中            |
| `GetCooldownRemaining()`     | 冷却剩余秒数          |
| `GetDefaultInteractionTag()` | 默认交互 Tag          |

### 7.3 冷却机制

`FInteractionData::CooldownTime > 0` 时，交互被接受后会进入冷却。冷却期间 `IsOnCooldown()` 返回 true，`CanInteract` 校验会失败。UI 可用 `GetCooldownRemaining()`
显示冷却倒计时。

---

## 8. 完整示例：可拾取物品

**目标：** 走近物品 → 显示"拾取 [E]" → 按 E 拾取 → 物品消失。

1. 创建 `BP_Pickup`，加 `InteractableComponent`
2. 配置 `Interaction Data`：Name="药水"、Action Text="拾取"、Hold Time=0
3. `BP_Pickup` 实现 `IInteractionInterface`：
    - `OnInteractionRequest`：将物品加入背包（调用你的 Inventory 系统）→ `Destroy Actor`
4. 玩家 Character 加 `InteractionComponent`，配置 `IA_Interact` + `BP_InteractionWidget`
5. 运行：走近药水 → 自动显示提示 → 按 E → 药水入包并消失

---

## 9. 进阶：执行权的选择——物品侧 vs 发起者侧

第 8 节的拾取是 **物品侧执行**：物品在 `OnInteractionRequest` 里销毁自己。但有一类交互， **真正的动作不是作用于物品，而是动用发起者自身的能力**（打开自己的
UI、操作自己的背包、触发自己的技能）。这类交互的执行权必须放在 **发起者侧**，否则会出现"物品反向通知 Controller"的绕路调用链。

### 9.1 判断执行权归属

| 交互的本质                                 | 执行权在哪                                   | 例子                                 |
|--------------------------------------------|----------------------------------------------|--------------------------------------|
| 物品是**被作用对象**，动作改变物品自身状态 | **物品侧**（`OnInteractionRequest`）         | 开门、点亮火把、破坏箱子、销毁拾取物 |
| 物品只是**触发点**，真正动作是发起者的能力 | **发起者侧**（监听 `OnInteractionAccepted`） | 打开背包/容器 UI、学习技能、接取任务 |

**简单规则：动作改变谁的内部状态，执行权就在谁那侧。** 物品侧 `OnInteractionRequest` 只做"物品自己被作用的事"；凡是要动用发起者能力的，一律在发起者侧处理。

### 9.2 反面案例：物品反向通知（不要这样做）

```
❌ 储物箱.OnInteractionRequest()
     → 调用 Controller.OpenInventoryUI()   // 物品越俎代庖，反向调用发起者
```

问题：

- 储物箱 **强耦合** Controller 的具体类型与背包插件，无法复用
- 链路方向反转（物品→Controller），违背交互系统单向数据流
- 换 Controller 类型、换 UI 实现就全部失效

### 9.3 正面案例：发起者侧监听（推荐）

以"走近储物箱，按 E 打开箱子界面"为例。储物箱只声明"我是个容器"， **执行权完全在 Controller**。

**步骤 1：容器物品只实现轻量接口，不执行任何动作**

```cpp
// 容器接口：只声明"我是容器"并提供数据，不声明任何行为
UINTERFACE(BlueprintType)
class UContainerInterface : public UInterface {
    GENERATED_BODY()
};

class IContainerInterface {
    GENERATED_BODY()
public:
    // 返回容器组件，只提供数据访问，不执行动作
    virtual UContainerComponent* GetContainerComponent() const = 0;
};
```

储物箱实现 `IContainerInterface`。它的 `OnInteractionRequest` **留空或只标记状态**（如 `bIsBeingAccessed = true`），不调用任何 Controller 方法。

**步骤 2：Controller 监听 `OnInteractionAccepted`，自己决定要不要开 UI**

```cpp
// 在 Controller 的 BeginPlay / 初始化时绑定
InteractionComponent->OnInteractionAccepted.AddDynamic(
    this, &AMyPlayerController::HandleInteractionAccepted);

// 回调：Controller 主动响应，识别目标类型并执行自己的能力
void AMyPlayerController::HandleInteractionAccepted(
    AActor* Instigator, const FInteractionContext& Context)
{
    // 通过接口识别目标是不是容器（Controller 不知道具体类，只认接口）
    if (IContainerInterface* Container = Cast<IContainerInterface>(Context.TargetActor))
    {
        // 打开自己的 UI（这些是 Controller 自己的能力）
        OpenInventoryUI();                                         // 你背包插件已有
        OpenContainerUI(Container->GetContainerComponent());       // 容器界面
    }
}
```

**步骤 3：拒绝时也在发起者侧处理**

```cpp
InteractionComponent->OnInteractionRejected.AddDynamic(
    this, &AMyPlayerController::HandleInteractionRejected);

void AMyPlayerController::HandleInteractionRejected(
    AActor* Instigator, const FInteractionContext& Context)
{
    ShowMessage("打不开");   // 提示也由 Controller 自己控制
}
```

### 9.4 这样设计的收益

| 维度            | 物品反向通知（❌）                | 发起者侧监听（✅）              |
|-----------------|-----------------------------------|---------------------------------|
| 物品依赖        | 强耦合 Controller + 背包插件 + UI | 零依赖（只实现接口提供数据）    |
| Controller 依赖 | 无                                | 只认 `IContainerInterface` 接口 |
| 链路方向        | 反向（物品→Controller）           | 正向（Controller 自己响应）     |
| 可复用性        | 换 Controller 类型即失效          | Controller 自治，物品通用       |
| 扩展性          | 加新交互类型要改物品              | Controller 加新的判断分支即可   |

### 9.5 边界情况：拾取该放哪侧

第 8 节的拾取把"加入背包"写在物品 `OnInteractionRequest` 里——严格说"加入玩家的背包"是发起者能力，按规则该放发起者侧。但实践中 **销毁物品 + 加背包**
经常写在一起也合理，因为两者强相关。取舍标准：

- 若背包逻辑简单（直接调一个 `AddItem` 函数），写物品侧可接受，耦合度低
- 若涉及复杂 UI/网络同步（背包是复制状态、要开 UI、要通知客户端），应放发起者侧监听 `OnInteractionAccepted`

核心仍是那句话： **动作改变谁的内部状态，执行权就在谁那侧。** 模糊时，优先放发起者侧，因为发起者（Controller/Character）是能力的拥有者，物品只是触发条件。

---

## 10. 常见问题

### 背包前台的只读附近发现

背包前台由 UISystem 阻止游戏动作，`GetNearbyInteractables()` 会返回空数组。InventoryUI 已直接接入交互组件的 `QueryNearbyInteractablesForUI(Searcher)`，传入该组件所属本地控制器当前控制的
Pawn。此入口是带执行引脚的按需查询，不是逐帧或重复求值的蓝图纯节点。

查询以 Pawn 本体位置为中心、`TraceDistance` 为半径做整球 Overlap，不受第三人称扇形限制。沿用 `TraceChannel` 收集候选；同一 Actor 多个碰撞体去重，目标可交互组件须已注册且启用。再从
Pawn 本体位置向目标 Actor 位置发射遮挡射线，使用 `UIQueryVisibilityChannel`（默认 `Visibility`
）；无阻挡或首先命中目标自身可见，先命中其他对象时排除。项目须将墙体配置为阻挡遮挡通道，箱体／地面物品配置为响应发现通道并挂载可交互组件。返回值按距离升序，每次实时查询，空结果有效，不回退到缓存。

该入口仅绕过 UI 输入阻止，仍保留 `SetInteractionEnabled(false)`、组件结束／未注册、目标禁用和当前本地 Pawn 所属关系的约束。旧 Pawn、异世界 Pawn、已销毁 Pawn 或其他玩家
Pawn 返回空数组。查询不执行 `CanInteract`／`OnInteractionRequest` 玩法钩子，不广播交互或周围查询事件，不更改焦点、描边、长按、输入模式或库存。候选是空间发现结果，不代表拾取、使用或外部库存访问已经获准；操作仍由原业务入口校验。

依据 [背包侧接入回执](../../../InventorySystem/docs/背包发现-交互模块接入工单.md)，InventoryUI 的 `UInventoryInteractionComponent` 每次查询先从当前 PlayerController 查找交互组件，未找到时再从当前 Pawn 查找，不缓存组件。调用上述入口后，对本次候选的 `Interactable → GetOwner` 去重，再提取实际库存；普通道具没有库存时忽略，实际 `AInventoryWorldItem` 汇总为地面物品投影。库存的可发现性、访问权限与访问距离仍由背包校验。首次打开背包查询一次，Active 时每秒刷新，拖拽期间暂停刷新；有效空结果撤销旧目录及窗口。

项目无需实现 `InventoryDiscoveryProvider`、`QueryNearbyInventoryActors` 或 `FoundActors` 转换循环。已按旧说明添加的发现接口、函数和仅用于转换的变量应由项目负责人清理，保留原有 `InventoryProviderInterface`。Tab 的 `OpenPlayerInventory`／`ToggleInventory` 自动发现附近对象；明确交互后打开指定容器的原链路 `外部 Actor → GetInventoryComponent → GetStorage → OpenExternalInventory` 继续使用。

InventoryUI 已增加 InteractionPlugin 私有编译依赖，InventorySystem.uplugin 已声明插件依赖；InteractionPlugin 不反向依赖背包，库存数据核心 InventorySystem 模块也不增加交互或 UI 依赖。背包侧回执记录 DebugGame 构建及 42 项自动化通过，现有关卡人工 PIE 尚待验收。项目碰撞配置、蓝图编译和实际操作验收见 [项目验收工单](../../docs/背包发现_项目蓝图接入工单.md)，自动化结果不能替代这些验收。

复用日常工程的聚焦验证入口（工作目录为 Plugins，不创建独立测试工程或关卡）：

```powershell
& '.\InteractionPlugin\Scripts\Test-InventoryDiscovery.ps1'
```

**Q: 焦点检测不到对象？**

- 确认对象碰撞体对 `Interactable` 通道 Block
- 确认 `Trace Distance` 足够
- 确认 `InteractableComponent` 的 `b Enabled` 为 true

**Q: 按键无响应？**

- 确认 IMC 已 Add Mapping Context
- 确认三个 InputAction 已在 `InteractionComponent` 配置
- 输入绑定是自动的，无需蓝图手动调用

**Q: 进度条不显示？**

- 确认目标 `Hold Time > 0`（点按型不显示进度条）
- 确认 Widget 蓝图有名为 `HoldProgressBar` 的 Progress Bar 控件

**Q: 图标不显示？**

- 确认 `InteractionComponent` 的 `Interact Action Icon` 已赋值
- 确认 Widget 蓝图有名为 `KeyIcon` 的 Image 控件
- 图标来自 Component 配置，与物理按键无关

**Q: 长按完成后进度条跳回 0？**

- 这是已修复的行为：长按完成时进度条归零并按 HoldTime 决定可见性，长按型目标保留 0% 等待下次长按

**Q: 按一次键，OnInteractionRequest 被触发两次？**

- 检查 IMC：同一个物理按键 **不要同时绑定** `IA_Interact` 和 `IA_Hold`。一次按下会让两个 Action 的 `Started` 都触发，各自调用一次请求
- `BeginHold`（IA_Hold 入口）对 `HoldTime<=0` 的瞬时目标已改为直接忽略，点按交互由 `TryInteract`（IA_Interact）独占负责。只要按键不冲突，就不会重复
- 若两个 Action 必须用同一按键（罕见），应在 IMC 里用 Trigger 条件区分，而非让两者同时响应 `Started`

---

*本教程对应 InteractionPlugin 当前版本。接口细节以源码头文件为准。*
