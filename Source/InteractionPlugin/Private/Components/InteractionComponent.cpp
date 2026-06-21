#include "Components/InteractionComponent.h"

#include "Components/InteractableComponent.h"
#include "Interfaces/InteractionInterface.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EnhancedInputComponent.h"           // UEnhancedInputComponent
#include "Blueprint/UserWidget.h"              // CreateWidget
#include "UI/UInteractionWidget.h"             // UInteractionWidget（CreateWidget 模板实例化需要完整类型）

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	//SetIsReplicatedByDefault(true);
}

void UInteractionComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

// =========================================================================
// 生命周期（V2.1 新增：管理 Widget 创建/销毁）
// =========================================================================

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	CreateInteractionWidget();

	// 尝试自动绑定输入；若 Owner 的 EnhancedInputComponent 尚未就绪，
	// 由 Tick 兜底补绑（见 TickComponent）。
	BindInputIfNeeded();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInput();
	DestroyInteractionWidget();
	Super::EndPlay(EndPlayReason);
}

// =========================================================================
// Widget 生命周期（V2.1 新增）
// =========================================================================

void UInteractionComponent::CreateInteractionWidget()
{
	// 未配置 Widget 类 → 跳过（组件逻辑仍正常工作）
	if (!InteractionWidgetClass) { return; }

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InteractionComponent] CreateInteractionWidget: GetWorld() 返回 null，跳过 Widget 创建"));
		return;
	}

	// 创建并初始化
	InteractionWidget = CreateWidget<UInteractionWidget>(World, InteractionWidgetClass);
	if (InteractionWidget)
	{
		// 注入组件引用 + 订阅事件 + 同步当前焦点 + 触发 OnWidgetCreated
		InteractionWidget->NativeInit(this);
		InteractionWidget->AddToViewport(InteractionWidgetZOrder);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[InteractionComponent] CreateInteractionWidget: CreateWidget 返回 null"));
	}
}

void UInteractionComponent::DestroyInteractionWidget()
{
	if (InteractionWidget)
	{
		InteractionWidget->RemoveFromParent();
		InteractionWidget = nullptr;
	}
}

// =========================================================================
// Enhanced Input 绑定（V2.0 新增）
// =========================================================================

bool UInteractionComponent::BindInputIfNeeded()
{
	// 已绑定：无需重复
	if (bInputBound) { return true; }

	// 仅本地控制时绑定玩家输入，避免服务端/非控制端误绑。
	// Owner 可能是 PlayerController 或 Pawn（两者均可挂本组件）。
	AActor* Owner = GetOwner();
	if (!Owner) { return false; }

	// 判定本地控制：PlayerController 用 IsLocalController，Pawn 用 IsLocallyControlled
	const bool bIsLocalController = Owner->IsA<APlayerController>()
		&& Cast<APlayerController>(Owner)->IsLocalController();
	const bool bIsLocalPawn = Owner->IsA<APawn>()
		&& Cast<APawn>(Owner)->IsLocallyControlled();
	if (!bIsLocalController && !bIsLocalPawn)
	{
		// 非本地控制（含服务端）：视为无需绑定，标记完成
		bInputBound = true;
		return true;
	}

	// 取 Owner 的 InputComponent，须已是 EnhancedInputComponent
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Owner->InputComponent);
	if (!EIC)
	{
		// 尚未就绪（SetupInputComponent 流程未完成），等待 Tick 兜底重试
		return false;
	}

	// 清理可能的旧句柄（防御性，正常路径此时无旧绑定）
	UnbindInput();
	BoundInputComponent = EIC;

	// 点按 1：交互（Triggered = 触发器求值通过时触发，由 IA 资产的 Trigger 决定时机）
	if (InteractAction)
	{
		BoundActionHandles.Add(FInputBindingHandle(EIC->BindAction(
			InteractAction, ETriggerEvent::Triggered, this, &ThisClass::TryInteract)));
	}

	// 点按 2：周围检测
	if (QueryAction)
	{
		BoundActionHandles.Add(FInputBindingHandle(EIC->BindAction(
			QueryAction, ETriggerEvent::Triggered, this, &ThisClass::HandleNearbyQuery)));
	}

	// 长按：Started 进累积（按下即开始），Completed/Canceled 出累积。
	// 长按用 Started 而非 Triggered——否则要等 Hold Trigger 时长才进累积，
	// 与插件自身的 HoldTime 累积重复。
	if (HoldAction)
	{
		BoundActionHandles.Add(EIC->BindAction(
			HoldAction, ETriggerEvent::Started,   this, &ThisClass::BeginHold));
		BoundActionHandles.Add(EIC->BindAction(
			HoldAction, ETriggerEvent::Completed, this, &ThisClass::EndHold));
		BoundActionHandles.Add(EIC->BindAction(
			HoldAction, ETriggerEvent::Canceled,  this, &ThisClass::EndHold));
	}

	bInputBound = true;
	return true;
}

void UInteractionComponent::UnbindInput()
{
	if (BoundInputComponent.IsValid())
	{
		UEnhancedInputComponent* OldComp = BoundInputComponent.Get();
		for (const FInputBindingHandle& Handle : BoundActionHandles)
		{
			if (Handle.GetHandle() != 0)
			{
				OldComp->RemoveBinding(Handle);
			}
		}
	}
	BoundActionHandles.Reset();
	BoundInputComponent = nullptr;
	bInputBound = false;
}

void UInteractionComponent::HandleNearbyQuery()
{
	if (!bInteractionEnabled) { return; }

	TArray<FInteractionCandidate> Candidates = GetNearbyInteractables();
	OnNearbyQueried.Broadcast(this, Candidates);
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 输入绑定兜底：BeginPlay 时若 EnhancedInputComponent 未就绪，每帧重试直至成功。
	// 成功后 bInputBound=true，此分支恒为 false，无持续开销。
	if (!bInputBound)
	{
		BindInputIfNeeded();
	}

	// 长按累积推进：每帧执行，不受 TraceInterval 间隔限制（长按需平滑推进）。
	// 总开关关闭时 bIsHolding 已被 SetInteractionEnabled 置 false，不会进入此分支。
	if (bIsHolding)
	{
		HoldTimeAccum += DeltaTime;
		if (HoldTimeAccum >= CurrentHoldTime)
		{
			// 累积满：以当前焦点发起一次普通交互，并结束长按状态。
			const FGameplayTag Tag = CurrentHoldTag;
			FHitResult Hit;
			// 捕获广播参数（后续状态重置不影响副本）
			AActor* CompletedActor = CurrentFocusedActor;
			UInteractableComponent* CompletedInteractable = CurrentInteractable;
			// 先结束状态，避免 RequestInteraction 内部再次进入长按分支
			bIsHolding = false;
			HoldTimeAccum = 0.0f;
			CurrentHoldTime = 0.0f;
			// 广播长按完成（纯本地事件，不依赖服务端校验结果）
			OnHoldCompleted.Broadcast(CompletedActor, CompletedInteractable);
			RequestInteraction(Tag, Hit);
		}
	}

	// 遵循检测间隔（仅影响射线检测频率，不影响长按推进）
	TraceTimeAccum += DeltaTime;
	if (TraceInterval > 0.0f && TraceTimeAccum < TraceInterval)
	{
		return;
	}
	TraceTimeAccum = 0.0f;
	
	// 仅在本地控制的客户端或独立模式下执行检测。
	// Owner 可能是 PlayerController 或 Pawn。
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const bool bIsLocalController = Owner->IsA<APlayerController>()
		&& Cast<APlayerController>(Owner)->IsLocalController();
	const bool bIsLocalPawn = Owner->IsA<APawn>()
		&& Cast<APawn>(Owner)->IsLocallyControlled();
	if (!bIsLocalController && !bIsLocalPawn)
	{
		return;
	}

	// 玩家侧总开关关闭：跳过检测与长按推进（焦点已在关闭时清除）
	if (!bInteractionEnabled)
	{
		return;
	}
	
	// 执行射线检测
	FHitResult HitResult;
	UInteractableComponent* FoundInteractable = PerformTrace(HitResult);

	// 确定目标 Actor
	AActor* FoundActor = FoundInteractable ? FoundInteractable->GetOwner() : nullptr;

	// 更新焦点
	UpdateFocus(FoundActor, FoundInteractable, HitResult);
}

// --------------------------------------------------------------------
// 公开 API
// --------------------------------------------------------------------

void UInteractionComponent::TryInteract()
{
	if (!bInteractionEnabled)
	{
		return;
	}

	UInteractableComponent* Interactable = CurrentInteractable;
	if (!Interactable || !Interactable->IsEnabled())
	{
		return;
	}

	// 长按型目标（HoldTime>0）不响应点按入口，必须通过长按累积满触发。
	// 与 BeginHold 对称：点按入口只负责瞬时目标，长按入口只负责长按目标。
	if (Interactable->GetInteractionData().HoldTime > 0.0f)
	{
		return;
	}

	FGameplayTag DefaultTag = Interactable->GetDefaultInteractionTag();
	FHitResult Hit;
	RequestInteraction(DefaultTag, Hit);
}

void UInteractionComponent::TryInteractWithTag(const FGameplayTag& InteractionTag)
{
	if (!bInteractionEnabled)
	{
		return;
	}

	UInteractableComponent* Interactable = CurrentInteractable;
	if (!Interactable || !Interactable->IsEnabled())
	{
		return;
	}

	if (!InteractionTag.IsValid())
	{
		return;
	}

	// 长按型目标（HoldTime>0）不响应点按入口，必须通过长按累积满触发
	if (Interactable->GetInteractionData().HoldTime > 0.0f)
	{
		return;
	}

	FHitResult Hit;
	RequestInteraction(InteractionTag, Hit);
}

void UInteractionComponent::ForceRefreshFocus()
{
	TraceTimeAccum = TraceInterval;  // 强制下一帧立即执行检测
}

// --------------------------------------------------------------------
// 长按交互
// --------------------------------------------------------------------

void UInteractionComponent::BeginHold()
{
	// 总开关关闭则拒绝长按入口（瞬时交互也不由此触发）
	if (!bInteractionEnabled || !bEnableHoldInteraction)
	{
		return;
	}

	UInteractableComponent* Interactable = CurrentInteractable;
	if (!Interactable || !Interactable->IsEnabled())
	{
		return;
	}

	// 读取目标配置的长按时长
	const float HoldTime = Interactable->GetInteractionData().HoldTime;

	// HoldTime <= 0：瞬时目标，不属于长按范畴，直接忽略。
	// 点按交互由 TryInteract（IA_Interact 的 Started 事件）独占负责，
	// 此处不再退化为点按，避免同一物理按键同时绑 IA_Interact + IA_Hold 时重复触发。
	if (HoldTime <= 0.0f)
	{
		return;
	}

	// 进入长按累积
	bIsHolding = true;
	CurrentHoldTime = HoldTime;
	HoldTimeAccum = 0.0f;
	CurrentHoldTag = Interactable->GetDefaultInteractionTag();
	OnHoldStarted.Broadcast(CurrentFocusedActor, Interactable, HoldTime);
}

void UInteractionComponent::EndHold()
{
	// 未在长按累积中（瞬时触发或未发起）：无操作
	if (!bIsHolding)
	{
		return;
	}

	CancelHoldInternal();
}

void UInteractionComponent::CancelHoldInternal()
{
	if (!bIsHolding)
	{
		return;
	}

	AActor* Target = CurrentFocusedActor;
	UInteractableComponent* Interactable = CurrentInteractable;

	bIsHolding = false;
	HoldTimeAccum = 0.0f;
	CurrentHoldTime = 0.0f;
	CurrentHoldTag = FGameplayTag();

	OnHoldCanceled.Broadcast(Target, Interactable);
}

float UInteractionComponent::GetCurrentHoldProgress() const
{
	if (!bIsHolding || CurrentHoldTime <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(HoldTimeAccum / CurrentHoldTime, 0.0f, 1.0f);
}

// --------------------------------------------------------------------
// 玩家侧总开关
// --------------------------------------------------------------------

void UInteractionComponent::SetInteractionEnabled(bool bNewEnabled)
{
	if (bInteractionEnabled == bNewEnabled)
	{
		return;
	}

	bInteractionEnabled = bNewEnabled;
	OnInteractionEnabledChanged.Broadcast(this, bNewEnabled);

	if (!bNewEnabled)
	{
		// 关闭：取消长按并清除焦点，实现完全静默
		CancelHoldInternal();
		ClearFocus();
	}
	else
	{
		// 开启：强制下一帧立即恢复检测
		ForceRefreshFocus();
	}
}

// --------------------------------------------------------------------
// 检测模式
// --------------------------------------------------------------------

void UInteractionComponent::SetTraceMode(EInteractionTraceMode NewMode)
{
	if (TraceMode == NewMode)
	{
		return;
	}

	TraceMode = NewMode;
	OnTraceModeChanged.Broadcast(this, NewMode);

	// 强制下一帧立即用新模式重新检测，避免最长 TraceInterval 的滞后
	ForceRefreshFocus();
}

TArray<FInteractionCandidate> UInteractionComponent::GetNearbyInteractables() const
{
	TArray<FInteractionCandidate> Candidates;

	FVector Origin;
	FVector Forward;
	ComputeViewOriginAndForward(Origin, Forward);

	// 分模式：第三人称按扇形（带角度过滤），第一人称按整球（无角度限制）
	if (TraceMode == EInteractionTraceMode::ThirdPerson)
	{
		GatherOverlapCandidates(Origin, Forward, TraceDistance, TraceHalfAngle, true, Candidates);
	}
	else
	{
		GatherOverlapCandidates(Origin, Forward, TraceDistance, 0.0f, false, Candidates);
	}

	// 标记当前主焦点
	for (FInteractionCandidate& C : Candidates)
	{
		C.bInFocus = (C.Interactable == CurrentInteractable);
	}

	// UI 轮盘通常按距离排——升序
	Candidates.Sort([](const FInteractionCandidate& A, const FInteractionCandidate& B)
	{
		return A.Distance < B.Distance;
	});

	return Candidates;
}

// --------------------------------------------------------------------
// 射线检测
// --------------------------------------------------------------------

UInteractableComponent* UInteractionComponent::PerformTrace(FHitResult& OutHit) const
{
	// 分派入口：按 TraceMode 调用对应的子检测函数
	if (TraceMode == EInteractionTraceMode::ThirdPerson)
	{
		return PerformThirdPersonTrace(OutHit);
	}
	return PerformFirstPersonTrace(OutHit);
}

void UInteractionComponent::ComputeViewOriginAndForward(FVector& OutOrigin, FVector& OutForward) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutOrigin = FVector::ZeroVector;
		OutForward = FVector::ForwardVector;
		return;
	}

	if (TraceMode == EInteractionTraceMode::ThirdPerson)
	{
		auto PlayerPawn = Cast<APlayerController>(Owner)->GetCharacter();
		// 第三人称：以角色本体为中心，避免相机绕轨时扇形乱晃
		OutOrigin = PlayerPawn->GetActorLocation();
		OutForward = PlayerPawn->GetActorForwardVector();
		return;
	}

	// 第一人称：沿用 PlayerController 视点
	const APlayerController* PC = Cast<APlayerController>(Owner);
	FVector ViewLocation;
	FRotator ViewRotation;
	if (PC)
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		ViewLocation = Owner->GetActorLocation();
		ViewRotation = Owner->GetActorForwardVector().Rotation();
	}

	OutOrigin = ViewLocation;
	OutForward = ViewRotation.Vector();
}

UInteractableComponent* UInteractionComponent::PerformFirstPersonTrace(FHitResult& OutHit) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FVector TraceDirection;
	ComputeViewOriginAndForward(ViewLocation, TraceDirection);

	const FVector TraceEnd = ViewLocation + TraceDirection * TraceDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bTraceComplex = false;

	bool bHit = false;

	if (TraceRadius > 0.0f)
	{
		// 球体检测
		bHit = GetWorld()->SweepSingleByChannel(
			OutHit,
			ViewLocation,
			TraceEnd,
			FQuat::Identity,
			TraceChannel.GetValue(),
			FCollisionShape::MakeSphere(TraceRadius),
			QueryParams
		);

#if ENABLE_DRAW_DEBUG
		// 绘制球体扫掠范围：中心线 + 起止两端球壳，颜色按命中状态区分
		{
			const FColor TraceColor = bHit ? FColor::Red : FColor::Green;
			const FVector ImpactPoint = bHit ? OutHit.ImpactPoint : TraceEnd;

			// 中心线
			DrawDebugLine(GetWorld(), ViewLocation, TraceEnd, TraceColor, false, TraceInterval, 0, 1.0f);
			// 起止两端球壳，直观显示扫掠半径
			DrawDebugSphere(GetWorld(), ViewLocation, TraceRadius, 12, TraceColor, false, TraceInterval, 1.0f);
			DrawDebugSphere(GetWorld(), TraceEnd, TraceRadius, 12, TraceColor, false, TraceInterval, 1.0f);

			if (bHit)
			{
				DrawDebugPoint(GetWorld(), ImpactPoint, 12.0f, FColor::Yellow, false, TraceInterval);
			}
		}
#endif
	}
	else
	{
		// 线形检测
		bHit = GetWorld()->LineTraceSingleByChannel(
			OutHit,
			ViewLocation,
			TraceEnd,
			TraceChannel.GetValue(),
			QueryParams
		);

#if ENABLE_DRAW_DEBUG
		// 绘制射线：起点到终点的线段，命中时延伸至命中点并标记，颜色按命中状态区分
		{
			const FColor TraceColor = bHit ? FColor::Red : FColor::Green;
			const FVector ImpactPoint = bHit ? OutHit.ImpactPoint : TraceEnd;

			DrawDebugLine(GetWorld(), ViewLocation, TraceEnd, TraceColor, false, TraceInterval, 0, 1.0f);

			if (bHit)
			{
				DrawDebugPoint(GetWorld(), ImpactPoint, 12.0f, FColor::Yellow, false, TraceInterval);
			}
		}
#endif
	}

	if (!bHit || !OutHit.GetActor())
	{
		return nullptr;
	}

	// 在命中 Actor 上查找 InteractableComponent
	AActor* HitActor = OutHit.GetActor();
	return HitActor->FindComponentByClass<UInteractableComponent>();
}

UInteractableComponent* UInteractionComponent::PerformThirdPersonTrace(FHitResult& OutHit) const
{
	FVector Origin;
	FVector Forward;
	ComputeViewOriginAndForward(Origin, Forward);

	TArray<FInteractionCandidate> Candidates;
	GatherOverlapCandidates(Origin, Forward, TraceDistance, TraceHalfAngle, true, Candidates);

	if (Candidates.Num() == 0)
	{
		OutHit = FHitResult();
		return nullptr;
	}

	// Gather 内已按 Dot 降序排列，首元素即主焦点（最接近视线中心）
	const FInteractionCandidate& Focus = Candidates[0];
	AActor* FocusActor = Focus.Interactable->GetOwner();

	// Overlap 不产生自然的 FHitResult，下游 BuildContext/HitLocation 依赖它。
	// 约定：用主焦点 Actor 的位置作为 ImpactPoint，构造一个 bBlockingHit=true 的合成命中。
	const FVector FocusLocation = FocusActor ? FocusActor->GetActorLocation() : Origin;
	OutHit = FHitResult(Origin, Origin + Forward * TraceDistance);
	OutHit.bBlockingHit = true;
	OutHit.ImpactPoint = FocusLocation;
	OutHit.Location = FocusLocation;
	OutHit.Normal = -Forward;

#if ENABLE_DRAW_DEBUG
	// 第三人称扇形调试：中心线 + 半角边线 + 圆弧（多点折线模拟）+ 命中点
	{
		const FRotator ForwardRot = Forward.Rotation();
		// 中心朝向线（白）
		DrawDebugLine(GetWorld(), Origin, Origin + Forward * TraceDistance, FColor::White, false, TraceInterval, 0, 1.0f);
		// 两条半角边线（白）
		const FRotator LeftRot = ForwardRot + FRotator(0.0f, -TraceHalfAngle, 0.0f);
		const FRotator RightRot = ForwardRot + FRotator(0.0f, TraceHalfAngle, 0.0f);
		DrawDebugLine(GetWorld(), Origin, Origin + LeftRot.Vector() * TraceDistance, FColor::White, false, TraceInterval, 0, 1.0f);
		DrawDebugLine(GetWorld(), Origin, Origin + RightRot.Vector() * TraceDistance, FColor::White, false, TraceInterval, 0, 1.0f);
		// 圆弧（白）：用分段折线模拟，跨度 -HalfAngle ~ +HalfAngle
		constexpr int32 ArcSegments = 16;
		FVector PrevArcPoint = Origin + LeftRot.Vector() * TraceDistance;
		for (int32 i = 1; i <= ArcSegments; ++i)
		{
			const float Alpha = static_cast<float>(i) / ArcSegments;
			const float AngleDeg = FMath::Lerp(-TraceHalfAngle, TraceHalfAngle, Alpha);
			const FVector ArcDir = (ForwardRot + FRotator(0.0f, AngleDeg, 0.0f)).Vector();
			const FVector CurrArcPoint = Origin + ArcDir * TraceDistance;
			DrawDebugLine(GetWorld(), PrevArcPoint, CurrArcPoint, FColor::White, false, TraceInterval, 0, 1.0f);
			PrevArcPoint = CurrArcPoint;
		}
		// 主焦点命中点（黄）
		DrawDebugPoint(GetWorld(), FocusLocation, 12.0f, FColor::Yellow, false, TraceInterval);
		// 其余命中点（青）
		for (int32 i = 1; i < Candidates.Num(); ++i)
		{
			const AActor* OtherActor = Candidates[i].Interactable->GetOwner();
			if (OtherActor)
			{
				DrawDebugPoint(GetWorld(), OtherActor->GetActorLocation(), 8.0f, FColor::Cyan, false, TraceInterval);
			}
		}
		// 起点（白点）
		DrawDebugPoint(GetWorld(), Origin, 8.0f, FColor::White, false, TraceInterval);
	}
#endif

	return Focus.Interactable;
}

void UInteractionComponent::GatherOverlapCandidates(
	FVector Origin, FVector Forward, float Radius, float HalfAngleDeg,
	bool bApplyAngleFilter,
	TArray<FInteractionCandidate>& OutCandidates) const
{
	OutCandidates.Reset();

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 球形 Overlap 收集所有命中
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bTraceComplex = false;

	World->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		TraceChannel.GetValue(),
		FCollisionShape::MakeSphere(Radius),
		QueryParams);

	// 角度过滤阈值：Dot 小于 cos(HalfAngle) 的候选落在扇形外，丢弃
	const float AngleThreshold = bApplyAngleFilter
		? FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg))
		: -1.0f;

	// 同一 Actor 多 mesh 命中时去重
	TSet<AActor*> ProcessedActors;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || ProcessedActors.Contains(HitActor))
		{
			continue;
		}
		ProcessedActors.Add(HitActor);

		UInteractableComponent* Interactable = HitActor->FindComponentByClass<UInteractableComponent>();
		if (!Interactable || !Interactable->IsEnabled())
		{
			continue;
		}

		const FVector TargetLoc = HitActor->GetActorLocation();
		const FVector ToTarget = (TargetLoc - Origin).GetSafeNormal();
		const float Dot = FVector::DotProduct(Forward, ToTarget);

		// 扇形外丢弃
		if (bApplyAngleFilter && Dot < AngleThreshold)
		{
			continue;
		}

		FInteractionCandidate Candidate;
		Candidate.Interactable = Interactable;
		Candidate.Distance = (TargetLoc - Origin).Size();
		Candidate.Dot = Dot;
		OutCandidates.Add(Candidate);
	}

	// 按 Dot 降序，便于主焦点取首元素
	OutCandidates.Sort([](const FInteractionCandidate& A, const FInteractionCandidate& B)
	{
		return A.Dot > B.Dot;
	});
}

// --------------------------------------------------------------------
// 焦点管理
// --------------------------------------------------------------------

void UInteractionComponent::UpdateFocus(AActor* NewTarget, UInteractableComponent* NewInteractable, const FHitResult& Hit)
{
	AActor* OldTarget = CurrentFocusedActor;
	UInteractableComponent* OldInteractable = CurrentInteractable;

	// 焦点未变化——无需操作
	if (OldTarget == NewTarget && OldInteractable == NewInteractable)
	{
		return;
	}

		// 焦点切换：取消进行中的长按，避免对着旧目标继续累积
		CancelHoldInternal();

		// 清除旧焦点
		if (OldTarget && OldInteractable)
		{
			OldInteractable->NotifyFocusLost();
			OnFocusEnd.Broadcast(OldTarget, OldInteractable);
		}

	// 更新状态
	CurrentFocusedActor = NewTarget;
	CurrentInteractable = NewInteractable;

		// 设置新焦点
		if (NewTarget && NewInteractable)
		{
			NewInteractable->NotifyFocusGained();
			OnFocusBegin.Broadcast(NewTarget, NewInteractable);
		}

	// 通知焦点变更
	OnFocusChanged.Broadcast(OldTarget, NewTarget, NewInteractable);
}

void UInteractionComponent::ClearFocus()
{
	CancelHoldInternal();  // 清除焦点前先取消长按，防止残留累积

	AActor* OldTarget = CurrentFocusedActor;
	UInteractableComponent* OldInteractable = CurrentInteractable;

	if (OldTarget && OldInteractable)
	{
		OldInteractable->NotifyFocusLost();
		OnFocusEnd.Broadcast(OldTarget, OldInteractable);
	}

	CurrentFocusedActor = nullptr;
	CurrentInteractable = nullptr;
}

// --------------------------------------------------------------------
// 交互请求
// --------------------------------------------------------------------

FInteractionContext UInteractionComponent::BuildContext(const FGameplayTag& Tag, const FHitResult& Hit) const
{
	FInteractionContext Context;
	Context.Instigator = GetOwner();
	Context.TargetActor = CurrentFocusedActor;
	Context.TargetComponent = CurrentInteractable;
	Context.HitResult = Hit;
	Context.HitLocation = Hit.bBlockingHit ? Hit.ImpactPoint : FVector::ZeroVector;
	Context.InteractionTag = Tag;
	Context.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return Context;
}

void UInteractionComponent::RequestInteraction(const FGameplayTag& InteractionTag, const FHitResult& Hit)
{
	AActor* Target = CurrentFocusedActor;
	UInteractableComponent* Interactable = CurrentInteractable;

	if (!Target || !Interactable || !Interactable->IsEnabled())
	{
		return;
	}

	FInteractionContext Context = BuildContext(InteractionTag, Hit);
	if (!Context.IsValid())
	{
		return;
	}

	// 广播本地事件（按值传递以兼容 DYNAMIC_MULTICAST）
	OnInteractionSent.Broadcast(GetOwner(), Context);

	// 执行交互：组件当前未启用复制，仅在 Authority 端（单机/Listen Server Host）直接执行完整流程。
	// 内部的 Accepted/Rejected 通知直接调用本地 _Implementation，确保本地监听者（如发起者监听
	// OnInteractionAccepted）能收到广播——NetMulticast 在未复制组件上为空操作。
	// 待组件启用复制后：需在 _Implementation 内改回 Multicast_XXX(Context) 以同步到其他客户端，
	// 并在此处对非 Authority 端启用 Server_RequestInteraction(Context) RPC。
	if (GetOwnerRole() == ROLE_Authority)
	{
		Server_RequestInteraction_Implementation(Context);
	}
	// else { Server_RequestInteraction(Context); }
}

// --------------------------------------------------------------------
// 服务端 RPC
// --------------------------------------------------------------------

bool UInteractionComponent::Server_RequestInteraction_Validate(const FInteractionContext& Context)
{
	return Context.Instigator != nullptr && Context.TargetActor != nullptr;
}

void UInteractionComponent::Server_RequestInteraction_Implementation(const FInteractionContext& Context)
{
	AActor* Target = Context.TargetActor;
	if (!Target)
	{
		// 组件当前未启用复制：直接在本地执行通知逻辑（NetMulticast 在未复制组件上为空操作）
		Multicast_InteractionRejected_Implementation(Context);
		return;
	}

	// 冷却校验：目标仍在冷却中则拒绝
	UInteractableComponent* TargetInteractable = Context.GetTargetInteractable();
	if (TargetInteractable && TargetInteractable->IsOnCooldown())
	{
		Multicast_InteractionRejected_Implementation(Context);
		return;
	}

	// 通过交互接口进行校验
	bool bCanInteract = true;
	if (Target->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
	{
		bCanInteract = IInteractionInterface::Execute_CanInteract(Target, Context);
	}

	if (!bCanInteract)
	{
		Multicast_InteractionRejected_Implementation(Context);
		return;
	}

	// 执行交互——框架调用，游戏层实现
	if (Target->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
	{
		IInteractionInterface::Execute_OnInteractionRequest(Target, Context);
	}

	// 交互被接受：刷新目标冷却起点
	if (TargetInteractable)
	{
		TargetInteractable->NotifyInteracted();
	}

	// 组件当前未启用复制：直接在本地执行通知逻辑（NetMulticast 在未复制组件上为空操作）
	// 启用复制后，应改回 Multicast_InteractionAccepted(Context) 以同步到其他客户端
	Multicast_InteractionAccepted_Implementation(Context);
}

// --------------------------------------------------------------------
// NetMulticast
// --------------------------------------------------------------------

void UInteractionComponent::Multicast_InteractionAccepted_Implementation(const FInteractionContext& Context)
{
	OnInteractionAccepted.Broadcast(GetOwner(), Context);
	OnInteractionCompleted.Broadcast(GetOwner(), Context);
}

void UInteractionComponent::Multicast_InteractionRejected_Implementation(const FInteractionContext& Context)
{
	OnInteractionRejected.Broadcast(GetOwner(), Context);
}
