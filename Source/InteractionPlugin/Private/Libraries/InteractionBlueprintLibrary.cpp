#include "Libraries/InteractionBlueprintLibrary.h"
#include "Components/InteractionComponent.h"
#include "Components/InteractableComponent.h"

bool UInteractionBlueprintLibrary::IsValidInteractionContext(const FInteractionContext& Context)
{
	return Context.IsValid();
}

FText UInteractionBlueprintLibrary::GetInteractionDisplayName(UInteractableComponent* Interactable)
{
	if (!Interactable)
	{
		return FText::GetEmpty();
	}
	return Interactable->GetInteractionData().Name;
}

FGameplayTag UInteractionBlueprintLibrary::GetInteractionTag(const FInteractionContext& Context)
{
	return Context.InteractionTag;
}

TArray<FInteractionOption> UInteractionBlueprintLibrary::GetInteractableOptions(UInteractableComponent* Interactable)
{
	if (!Interactable)
	{
		return TArray<FInteractionOption>();
	}
	return Interactable->GetOptions();
}

void UInteractionBlueprintLibrary::TriggerInteraction(
	UInteractionComponent* InteractionComp,
	const FGameplayTag& InteractionTag)
{
	if (!InteractionComp)
	{
		return;
	}
	InteractionComp->TryInteractWithTag(InteractionTag);
}

bool UInteractionBlueprintLibrary::ContextHasHitResult(const FInteractionContext& Context)
{
	return Context.HasHitResult();
}

FVector UInteractionBlueprintLibrary::GetContextHitLocation(const FInteractionContext& Context)
{
	return Context.HitLocation;
}

TArray<FInteractionCandidate> UInteractionBlueprintLibrary::GetNearbyInteractables(UInteractionComponent* InteractionComp)
{
	if (!InteractionComp)
	{
		return TArray<FInteractionCandidate>();
	}
	return InteractionComp->GetNearbyInteractables();
}

EInteractionTraceMode UInteractionBlueprintLibrary::GetTraceMode(UInteractionComponent* InteractionComp)
{
	if (!InteractionComp)
	{
		return EInteractionTraceMode::FirstPerson;
	}
	return InteractionComp->GetTraceMode();
}

bool UInteractionBlueprintLibrary::IsHoldingInteraction(UInteractionComponent* InteractionComp)
{
	return InteractionComp ? InteractionComp->IsHolding() : false;
}

float UInteractionBlueprintLibrary::GetHoldProgress(UInteractionComponent* InteractionComp)
{
	return InteractionComp ? InteractionComp->GetCurrentHoldProgress() : 0.0f;
}

bool UInteractionBlueprintLibrary::IsInteractionEnabled(UInteractionComponent* InteractionComp)
{
	return InteractionComp ? InteractionComp->IsInteractionEnabled() : false;
}

bool UInteractionBlueprintLibrary::IsOnCooldown(UInteractableComponent* Interactable)
{
	return Interactable ? Interactable->IsOnCooldown() : false;
}

float UInteractionBlueprintLibrary::GetCooldownRemaining(UInteractableComponent* Interactable)
{
	return Interactable ? Interactable->GetCooldownRemaining() : 0.0f;
}
