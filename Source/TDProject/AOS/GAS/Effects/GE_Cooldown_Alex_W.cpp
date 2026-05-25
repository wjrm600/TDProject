// Phase 4: Alex W 쿨다운 GE 구현.

#include "GE_Cooldown_Alex_W.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Cooldown_Alex_W::UGE_Cooldown_Alex_W()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(15.0f));

	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("Cooldown.Skill.Alex.W")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);
		GEComponents.Add(TagsComp);
	}
}
