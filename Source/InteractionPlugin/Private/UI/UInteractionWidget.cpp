#include "UI/UInteractionWidget.h"
#include "Components/InteractionComponent.h"
#include "Components/InteractableComponent.h"
#include "Structs/InteractionData.h"

#include "Engine/Texture2D.h"                // UTexture2D（KeyIcon 资源类型）
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteractionWidget, Log, All);

UInteractionWidget::UInteractionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =========================================================================
// 生命周期
// =========================================================================

void UInteractionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(false);
	SetPromptContentVisible(false);
}

void UInteractionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UInteractionComponent* Component = InteractionComponent.Get())
	{
		// Slate 重建可以重新订阅；提示注销时由组件显式释放引用。
		BindComponentEvents(Component);
		SyncCurrentFocus();
	}
}

TSharedRef<SWidget> UInteractionWidget::RebuildWidget()
{
	if (WidgetTree && (!PromptContent || WidgetTree->RootWidget != PromptContent))
	{
		UWidget* OriginalRoot = WidgetTree->RootWidget;
		// 在实例中包裹原树，保留旧蓝图的四个绑定控件、布局和动画目标。
		PromptContent = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InteractionPromptContent"));
		WidgetTree->RootWidget = PromptContent;
		if (OriginalRoot)
		{
			UOverlaySlot* ContentSlot = PromptContent->AddChildToOverlay(OriginalRoot);
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	SetPromptContentVisible(bHasPromptContent);
	return Super::RebuildWidget();
}

void UInteractionWidget::SetPromptContentVisible(bool bVisible)
{
	bHasPromptContent = bVisible;
	if (PromptContent) { PromptContent->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void UInteractionWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	// 仅在可见 + 有焦点目标 + 正在长按时推进进度条
	if (!bHasPromptContent) { return; }
	if (!bIsHolding) { return; }

	UInteractionComponent* Comp = InteractionComponent.Get();
	if (!Comp) { return; }

	if (HoldProgressBar)
	{
		const float Progress = Comp->GetCurrentHoldProgress();
		HoldProgressBar->SetPercent(Progress);
	}
}

void UInteractionWidget::NativeDestruct()
{
	// 销毁前解绑所有委托，避免悬空
	UnbindComponentEvents();
	Super::NativeDestruct();
}

void UInteractionWidget::ReleaseInteractionComponent()
{
	UnbindComponentEvents();
	InteractionComponent.Reset();
	FocusedInteractable.Reset();
	bIsHolding = false;
	if (HoldProgressBar) { HoldProgressBar->SetPercent(0.0f); }
	SetPromptContentVisible(false);
}

// =========================================================================
// 组件引用注入
// =========================================================================

void UInteractionWidget::NativeInit(UInteractionComponent* InComponent)
{
	if (!IsValid(InComponent))
	{
		UE_LOG(LogInteractionWidget, Warning, TEXT("NativeInit: InComponent 无效，跳过初始化"));
		return;
	}

	// 幂等：先解绑旧的
	UnbindComponentEvents();

	// 保存弱引用并订阅事件
	InteractionComponent = InComponent;
	BindComponentEvents(InComponent);

	// 同步当前焦点（组件 BeginPlay 时可能已有焦点，防御性同步）
	SyncCurrentFocus();

	// 触发蓝图视觉初始化钩子
	OnWidgetCreated();
}

// =========================================================================
// 事件订阅
// =========================================================================

void UInteractionWidget::BindComponentEvents(UInteractionComponent* InComponent)
{
	if (!InComponent) { return; }

	InComponent->OnFocusBegin.AddUniqueDynamic(this, &ThisClass::HandleFocusBegin);
	InComponent->OnFocusEnd.AddUniqueDynamic(this, &ThisClass::HandleFocusEnd);
	InComponent->OnHoldStarted.AddUniqueDynamic(this, &ThisClass::HandleHoldStarted);
	InComponent->OnHoldCanceled.AddUniqueDynamic(this, &ThisClass::HandleHoldCanceled);
	InComponent->OnHoldCompleted.AddUniqueDynamic(this, &ThisClass::HandleHoldCompleted);
}

void UInteractionWidget::UnbindComponentEvents()
{
	UInteractionComponent* Comp = InteractionComponent.Get();
	if (!Comp) { return; }

	Comp->OnFocusBegin.RemoveDynamic(this, &ThisClass::HandleFocusBegin);
	Comp->OnFocusEnd.RemoveDynamic(this, &ThisClass::HandleFocusEnd);
	Comp->OnHoldStarted.RemoveDynamic(this, &ThisClass::HandleHoldStarted);
	Comp->OnHoldCanceled.RemoveDynamic(this, &ThisClass::HandleHoldCanceled);
	Comp->OnHoldCompleted.RemoveDynamic(this, &ThisClass::HandleHoldCompleted);
}

// =========================================================================
// 焦点同步
// =========================================================================

void UInteractionWidget::SyncCurrentFocus()
{
	UInteractionComponent* Comp = InteractionComponent.Get();
	if (!Comp) { return; }

	UInteractableComponent* Current = Comp->CurrentInteractable;
	if (Current)
	{
		// 复用 HandleFocusBegin 的填充逻辑
		HandleFocusBegin(Comp->CurrentFocusedActor, Current);
	}
}

// =========================================================================
// 事件处理器
// =========================================================================

void UInteractionWidget::HandleFocusBegin(AActor* TargetActor, UInteractableComponent* Interactable)
{
	if (!IsValid(Interactable)) { return; }

	// 缓存焦点目标
	FocusedInteractable = Interactable;

	// 填充动作文本
	const FInteractionData& Data = Interactable->GetInteractionData();
	if (ActionText)
	{
		ActionText->SetText(Data.ActionText);
	}

	// 填充交互类型文案：HoldTime>0 为长按，否则为点击
	if (InteractionTypeText)
	{
		InteractionTypeText->SetText(Data.HoldTime > 0.0f ? HoldText : ClickText);
	}

	// 进度条可见性：HoldTime>0 显示 0%，否则隐藏
	if (HoldProgressBar)
	{
		if (Data.HoldTime > 0.0f)
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::Visible);
			HoldProgressBar->SetPercent(0.0f);
		}
		else
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 填充按键提示图标：从 InteractionComponent 读取配置的 InteractActionIcon
	if (KeyIcon)
	{
		if (UInteractionComponent* Comp = InteractionComponent.Get())
		{
			if (UTexture2D* Icon = Comp->GetInteractActionIcon())
			{
				KeyIcon->SetVisibility(ESlateVisibility::Visible);
				KeyIcon->SetBrushResourceObject(Icon);
			}
			else
			{
				// 未配置图标则隐藏，避免残留上次焦点的图标
				KeyIcon->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}

	SetPromptContentVisible(true);

	// 触发蓝图淡入钩子
	OnPromptShown();
}

void UInteractionWidget::HandleFocusEnd(AActor* TargetActor, UInteractableComponent* Interactable)
{
	SetPromptContentVisible(false);

	// 清空焦点缓存
	FocusedInteractable = nullptr;
	bIsHolding = false;

	// 触发蓝图淡出钩子
	OnPromptHidden();
}

void UInteractionWidget::HandleHoldStarted(AActor* TargetActor, UInteractableComponent* Interactable, float HoldTime)
{
	bIsHolding = true;

	if (HoldProgressBar && HoldTime > 0.0f)
	{
		HoldProgressBar->SetVisibility(ESlateVisibility::Visible);
		HoldProgressBar->SetPercent(0.0f);
	}
}

void UInteractionWidget::HandleHoldCanceled(AActor* TargetActor, UInteractableComponent* Interactable)
{
	bIsHolding = false;

	// 进度条归零；若目标 HoldTime>0 保留可见（0%），否则隐藏
	if (HoldProgressBar)
	{
		UInteractableComponent* Focused = FocusedInteractable.Get();
		const bool bKeepVisible = Focused && Focused->GetInteractionData().HoldTime > 0.0f;
		if (bKeepVisible)
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		HoldProgressBar->SetPercent(0.0f);
	}
}

void UInteractionWidget::HandleHoldCompleted(AActor* TargetActor, UInteractableComponent* Interactable)
{
	bIsHolding = false;

	// 长按完成：进度条归零并按目标 HoldTime 决定可见性（与取消一致），
	// 长按型目标保留 0% 可见以等待下次长按，避免从满格突跳回 0 的视觉异常。
	if (HoldProgressBar)
	{
		UInteractableComponent* Focused = FocusedInteractable.Get();
		const bool bKeepVisible = Focused && Focused->GetInteractionData().HoldTime > 0.0f;
		if (bKeepVisible)
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		HoldProgressBar->SetPercent(0.0f);
	}
}
