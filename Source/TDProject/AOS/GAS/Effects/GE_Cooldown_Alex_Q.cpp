// Phase 4: Alex Q 쿨다운 GE 구현.

#include "GE_Cooldown_Alex_Q.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Cooldown_Alex_Q::UGE_Cooldown_Alex_Q()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 8초 고정 — Phase 4 단순화 (추후 SetByCaller / Attribute 기반 동적화 검토)
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(8.0f));

	// UE 5.4+: cooldown 인식은 UTargetTagsGameplayEffectComponent 만.
	// CreateDefaultSubobject + GEComponents 직접 추가 (생성자 안전 패턴).
	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("Cooldown.Skill.Alex.Q")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);

		GEComponents.Add(TagsComp);
	}
}
