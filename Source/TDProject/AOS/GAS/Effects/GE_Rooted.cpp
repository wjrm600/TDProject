// Phase 4+: GE_Rooted 구현.

#include "GE_Rooted.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Rooted::UGE_Rooted()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Duration = SetByCaller(Data.Duration) — 호출자가 몽타주 길이 전달 (GE_HitReact_State 패턴).
	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	// GrantedTag: State.Rooted
	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("State.Rooted")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);

		GEComponents.Add(TagsComp);
	}
}
