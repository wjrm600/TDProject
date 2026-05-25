// Phase 4: GE_EnhancedAttack 구현.

#include "GE_EnhancedAttack.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_EnhancedAttack::UGE_EnhancedAttack()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Duration 3.0s — Alex Q 의 SpeedBoost 와 일치.
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.0f));

	// GrantedTag: State.EnhancedAttack
	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("State.EnhancedAttack")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);
		GEComponents.Add(TagsComp);
	}
}
