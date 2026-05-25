// Phase 4: Alex R 쿨다운 GE 구현.

#include "GE_Cooldown_Alex_R.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Cooldown_Alex_R::UGE_Cooldown_Alex_R()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(90.0f));

	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("Cooldown.Skill.Alex.R")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);
		GEComponents.Add(TagsComp);
	}
}
