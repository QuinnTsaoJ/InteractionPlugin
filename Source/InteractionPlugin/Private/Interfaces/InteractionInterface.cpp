#include "Interfaces/InteractionInterface.h"
#include "Components/MeshComponent.h"

// IInteractionInterface 的默认实现。
// 当游戏层未覆盖这些函数时，提供安全的回退行为。

bool IInteractionInterface::CanInteract_Implementation(const FInteractionContext& Context) const
{
	// 默认：始终允许交互
	return true;
}

FInteractionData IInteractionInterface::GetInteractionData_Implementation() const
{
	// 默认：返回空数据 —— 游戏层应覆盖此函数
	return FInteractionData();
}

void IInteractionInterface::OnInteractionRequest_Implementation(const FInteractionContext& Context)
{
	// 默认：空操作 —— 游戏层在此实现具体行为
}

TArray<UMeshComponent*> IInteractionInterface::GetOutlineMeshes_Implementation() const
{
	// 默认：返回所属 Actor 上的所有 UMeshComponent
	if (const UActorComponent* Comp = Cast<UActorComponent>(this))
	{
		if (AActor* Owner = Comp->GetOwner())
		{
			TArray<UMeshComponent*> Result;
			Owner->GetComponents<UMeshComponent>(Result);
			return Result;
		}
	}
	return {};
}
