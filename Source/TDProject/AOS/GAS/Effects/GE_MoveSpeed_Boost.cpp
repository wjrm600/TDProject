// Phase 4: GE_MoveSpeed_Boost 구현.

#include "GE_MoveSpeed_Boost.h"
#include "GAS/AOSAttributeSet.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_MoveSpeed_Boost::UGE_MoveSpeed_Boost()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Duration 3.0s 고정 (Phase 4 hardcoded).
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.0f));

	// Modifier: MoveSpeed Additive +300 (base 600 의 +50%).
	FGameplayModifierInfo MoveSpeedMod;
	MoveSpeedMod.Attribute = UAOSAttributeSet::GetMoveSpeedAttribute();
	MoveSpeedMod.ModifierOp = EGameplayModOp::Additive;
	MoveSpeedMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(300.0f));
	Modifiers.Add(MoveSpeedMod);

	// GrantedTag: State.SpeedBoost
	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("State.SpeedBoost")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);
		GEComponents.Add(TagsComp);
	}
}
