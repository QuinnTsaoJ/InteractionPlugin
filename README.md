# InteractionPlugin

UE5 事件驱动、组件化的交互框架。负责发现可交互对象、管理焦点、分发交互请求并广播事件。**只定义协议,不实现任何玩法行为**——开门、拾取、打开 UI 等具体逻辑由游戏层通过 `IInteractionInterface` 与委托扩展。

## 设计理念

框架与玩法严格分层:
- **框架侧**只做三件事:检测目标 → 管理焦点 → 分发请求并广播事件
- **玩法侧**通过实现接口或绑定委托决定"能不能交互、交互时发生什么"
- 所有交互以 `FInteractionContext` 为数据载体贯穿请求→校验→广播全流程
- GameplayTag 标识交互类型,支持单对象多选项(如"打开 / 搜索 / 检查")

## 架构

```
玩家侧 UInteractionComponent (挂在 Character / PlayerController / Pawn)
    │  射线检测（第一/第三人称）→ 维护焦点 → 处理输入 → 服务端 RPC
    │
    ├── FInteractionContext      交互数据载体（Who/What/Where/When/Which）
    │
    ├── UInteractionWidget       交互提示 UI 基类（自动创建/事件驱动显隐）
    │
    └── 目标侧 UInteractableComponent (挂在任意 Actor)
            ├── FInteractionData      显示数据 + 选项 + 冷却
            ├── IInteractionInterface 玩法扩展点（CanInteract / OnInteractionRequest ...）
            └── 描边支持（Custom Depth Stencil）
```

## 模块结构

| 文件 | 职责 |
|------|------|
| `UInteractionComponent` | 玩家侧交互组件:射线检测、焦点管理、输入绑定、网络分发 |
| `UInteractableComponent` | 目标侧组件:存储交互数据、选项、冷却、描边状态 |
| `IInteractionInterface` | 玩法扩展接口:CanInteract / GetInteractionData / OnInteractionRequest / GetOutlineMeshes |
| `FInteractionContext` | 交互核心数据载体,贯穿请求→校验→广播 |
| `FInteractionData` | UI 显示数据(名称、动作、长按时长、冷却、选项) |
| `FInteractionOption` | 单个交互选项(GameplayTag + 显示文本 + 图标) |
| `FInteractionCandidate` | 周围检测的候选条目(距离、点积、是否主焦点) |
| `UInteractionWidget` | 交互提示 Widget 抽象基类,BindWidget 契约 + 视觉钩子 |
| `InteractionTags` | C++ 预定义交互标签(打开/关闭/使用/拾取/搜索 ...) |

## 功能

- **双检测模式**:第一人称(视点射线/球扫)、第三人称(扇形 Overlap + 点积取主焦点),运行时可切换
- **焦点管理**:自动焦点进入/离开/切换事件,目标自动描边(Custom Depth Stencil)
- **点按 / 长按交互**:`HoldTime` 区分类型,长按带进度累积、松手/失焦自动取消
- **多交互选项**:多个有效唯一 Tag 经现有交互键打开 UISystem 原生菜单；单项保留默认 Tag 流程
- **网络校验**:客户端请求 → 服务端 `CanInteract` 校验 → Multicast 广播接受/拒绝
- **交互冷却**:`CooldownTime > 0` 时交互被接受后进入冷却,UI 可读取剩余时间
- **周围检测**:按需查询范围内可交互物数组,按距离排序,主焦点标记
- **总开关**:`SetInteractionEnabled` 一键静默交互(过场/UI/对话场景)
- **交互提示 UI**:框架自动创建/管理 Widget 生命周期,事件驱动显隐,长按进度条自动推进
- **Enhanced Input 自动绑定**:BeginPlay 自动从本地控制 Owner 取 InputComponent 绑定三个 Action,Tick 兜底补绑,服务端自动跳过
- **UISystem 接入**:本地玩家提示登记为 HUD，输入策略阻止时取消长按、清除焦点与描边；UI 退出保留原业务开关

## 快速开始

1. 启用 `InteractionPlugin` 与 `EnhancedInput` 插件
2. 在 `DefaultEngine.ini` 定义 `Interactable` 碰撞通道(`ECC_GameTraceChannel2`),可交互对象碰撞体对该通道 Block
3. 创建三个 InputAction:`IA_Interact`(点按)、`IA_Query`(周围检测)、`IA_Hold`(长按),并在 IMC 绑定按键
4. 玩家 Character 挂载 `UInteractionComponent`,在 Details 面板配置三个 Action、图标、`Interaction Widget Class`
5. 可交互对象挂载 `UInteractableComponent`,编辑 `Interaction Data`(名称/动作/HoldTime/冷却)
6. 对象实现 `IInteractionInterface`,在 `OnInteractionRequest` 中写玩法行为(开门动画/拾取销毁等)
7. 派生 `UInteractionWidget` 蓝图,提供四个 BindWidget 控件(`ActionText` / `KeyIcon` / `HoldProgressBar` / `InteractionTypeText`)

## 扩展点

- **自定义交互类型**:在 `InteractionTags` 预定义标签外,于编辑器添加自定义 GameplayTag,在 `Options` 中引用
- **玩法行为**:实现 `IInteractionInterface::OnInteractionRequest` 处理目标被作用的行为(开门、点亮、破坏)
- **发起者侧能力**:绑定 `OnInteractionAccepted`,在 Controller 侧响应"动用发起者自身能力"的交互(打开背包 UI、接取任务)
- **焦点反馈**:绑定 `OnFocusBegin` / `OnFocusEnd` 播放高亮、音效,或覆写 `GetOutlineMeshes` 精确指定描边网格
- **长按反馈**:绑定 `OnHoldStarted` / `OnHoldCompleted` 播放音效;UI 进度条由基类自动驱动
- **拒绝处理**:绑定 `OnInteractionRejected` 弹出"门已锁"等提示
- **过场静默**:过场/对话期间 `SetInteractionEnabled(false)`,结束恢复

## 执行权选择

交互的执行权归属取决于"动作改变谁的内部状态":

| 交互本质 | 执行权 | 例子 |
|---|---|---|
| 物品是被作用对象,动作改变物品自身 | **物品侧**(`OnInteractionRequest`) | 开门、点亮火把、破坏箱子、销毁拾取物 |
| 物品只是触发点,动作是发起者能力 | **发起者侧**(监听 `OnInteractionAccepted`) | 打开容器 UI、学习技能、接取任务 |

> 详细示例与反面案例见 `Source/InteractionPlugin/UsageGuide.md` 第 9 节。

## 相关文档

- [UI-011-X01 实施回执](docs/UI-011-X01_实施回执.md) — 原生交互选项、关闭后派发规则与待集中验证范围
- [背包发现交互模块回执](docs/背包发现_交互模块实施回执.md) — UI 前台只读空间查询、验证证据及 InventoryUI 直接接入状态
- [背包发现项目验收工单](docs/背包发现_项目蓝图接入工单.md) — 旧发现接口桥接已撤销，实际碰撞配置与现有关卡人工 PIE 待验收
- [UI-001 隔离 PIE 验收指南](docs/UI-001_PIE验收指南.md) — 独立工程、Controller/Pawn 关卡、键位与验收步骤
- `Source/InteractionPlugin/UsageGuide.md` — 从零接入的完整使用教程
- `Source/InteractionPlugin/TechnicalDesignDocument.md` — 技术设计文档
