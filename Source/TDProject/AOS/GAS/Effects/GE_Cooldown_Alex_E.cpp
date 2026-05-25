// Phase 4: Alex E 쿨다운 GE 구현.

#include "GE_Cooldown_Alex_E.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Cooldown_Alex_E::UGE_Cooldown_Alex_E()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.0f));

	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("Cooldown.Skill.Alex.E")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);
		GEComponents.Add(TagsComp);
	}
}
