// Phase 4: GE_DamageShield 구현.

#include "GE_DamageShield.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_DamageShield::UGE_DamageShield()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Duration 2.0s 고정 (Phase 4 hardcoded).
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.0f));

	// GrantedTag: State.DamageShield
	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("State.DamageShield")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);
		GEComponents.Add(TagsComp);
	}
}
