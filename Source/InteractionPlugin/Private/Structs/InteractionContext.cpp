#include "Structs/InteractionContext.h"
#include "Components/InteractableComponent.h"

UInteractableComponent* FInteractionContext::GetTargetInteractable() const
{
	return Cast<UInteractableComponent>(TargetComponent);
}
