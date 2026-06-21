#include "Components/InteractableComponent.h"
#include "Components/MeshComponent.h"
#include "Interfaces/InteractionInterface.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	//SetIsReplicatedByDefault(true);
}

void UInteractableComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UInteractableComponent::UninitializeComponent()
{
	// 若当前处于描边状态则先关闭，防止组件被销毁后残留
	if (IsOutlined())
	{
		ApplyOutline(false);
	}

	Super::UninitializeComponent();
}

void UInteractableComponent::SetEnabled(bool bNewEnabled)
{
	if (bEnabled == bNewEnabled)
	{
		return;
	}

	bEnabled = bNewEnabled;
	OnEnabledChanged.Broadcast(this, bEnabled);
}

bool UInteractableComponent::FindOptionByTag(FGameplayTag Tag, FInteractionOption& OutOption) const
{
	for (const FInteractionOption& Option : InteractionData.Options)
	{
		if (Option.Tag == Tag)
		{
			OutOption = Option;
			return true;
		}
	}
	return false;
}

FGameplayTag UInteractableComponent::GetDefaultInteractionTag() const
{
	// 优先使用显式设置的默认 Tag
	if (DefaultInteractionTag.IsValid())
	{
		return DefaultInteractionTag;
	}
	// 回退到第一个选项的 Tag
	if (!InteractionData.Options.IsEmpty() && InteractionData.Options[0].Tag.IsValid())
	{
		return InteractionData.Options[0].Tag;
	}
	return FGameplayTag();
}

// --------------------------------------------------------------------
// 描边（焦点视觉反馈）
// --------------------------------------------------------------------

void UInteractableComponent::NotifyFocusGained()
{
	if (!bEnableOutline)
	{
		return;
	}
	ApplyOutline(true);
}

void UInteractableComponent::NotifyFocusLost()
{
	// 无条件关闭描边——即使配置中途禁用，也保证清理
	ApplyOutline(false);
}

void UInteractableComponent::ApplyOutline(bool bShouldOutline)
{
	if (bShouldOutline)
	{
		// 通过接口调用 Actor 上的 GetOutlineMeshes 实现
		TArray<UMeshComponent*> Meshes;
		if (AActor* Owner = GetOwner())
		{
			if (Owner->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
			{
				Meshes = IInteractionInterface::Execute_GetOutlineMeshes(Owner);
			}
		}
		OutlinedMeshes.Reset(Meshes.Num());

		for (UMeshComponent* Mesh : Meshes)
		{
			if (Mesh)
			{
				Mesh->SetRenderCustomDepth(true);
				Mesh->SetCustomDepthStencilValue(StencilValue);
				OutlinedMeshes.Add(Mesh);
			}
		}

		bIsOutlined = true;
	}
	else
	{
		// 关闭描边：仅遍历上次描过的弱引用，避免再次遍历 Actor 全量组件
		for (const TWeakObjectPtr<UMeshComponent>& WeakMesh : OutlinedMeshes)
		{
			if (UMeshComponent* Mesh = WeakMesh.Get())
			{
				Mesh->SetRenderCustomDepth(false);
				Mesh->SetCustomDepthStencilValue(0);
			}
		}

		OutlinedMeshes.Empty();
		bIsOutlined = false;
	}

	OnOutlineStateChanged.Broadcast(this, bIsOutlined);
}

// --------------------------------------------------------------------
// 冷却
// --------------------------------------------------------------------

bool UInteractableComponent::IsOnCooldown() const
{
	return GetCooldownRemaining() > 0.0f;
}

float UInteractableComponent::GetCooldownRemaining() const
{
	const float Cooldown = InteractionData.CooldownTime;
	if (Cooldown <= 0.0f || LastInteractionTime < 0.0f)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const float Now = World->GetTimeSeconds();
	const float Elapsed = Now - LastInteractionTime;
	if (Elapsed >= Cooldown)
	{
		return 0.0f;
	}
	return Cooldown - Elapsed;
}

void UInteractableComponent::NotifyInteracted()
{
	const UWorld* World = GetWorld();
	LastInteractionTime = World ? World->GetTimeSeconds() : 0.0f;
}
