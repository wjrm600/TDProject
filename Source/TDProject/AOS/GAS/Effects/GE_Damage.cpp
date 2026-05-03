// GAS Phase 2: 데미지 GE 구현

#include "GE_Damage.h"
#include "AOSAttributeSet.h"
#include "GameplayTagContainer.h"

UGE_Damage::UGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Damage 메타 속성에 SetByCaller magnitude 를 Add
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UAOSAttributeSet::GetDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerMagnitude;
	SetByCallerMagnitude.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerMagnitude);

	Modifiers.Add(DamageModifier);

	// NOTE: Instant GE 는 TargetTagsGameplayEffectComponent 를 사용하면 안 됨
	// (UE 5.4+ IsDataValid 가 에러 — Instant 는 태그를 grant 할 수 없음).
	// 데미지 타입 분류가 필요하면 GameplayEffectContext 또는 GameplayCue 로 처리.
}
