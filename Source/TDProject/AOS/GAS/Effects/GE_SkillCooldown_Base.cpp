// Phase 4+: 데이터 주도 스킬 쿨다운 GE base 구현.

#include "GE_SkillCooldown_Base.h"
#include "GameplayTagContainer.h"

UGE_SkillCooldown_Base::UGE_SkillCooldown_Base()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// SetByCaller(Data.Duration) — UGA_SkillBase 가 활성화 시점에 CooldownDuration 으로 set.
	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	// 태그 grant 는 BP child 가 UTargetTagsGameplayEffectComponent 로 추가.
	// (base 에서 일괄 부여하면 모든 스킬이 같은 cooldown 태그 공유 → 의도 안 맞음)
}
