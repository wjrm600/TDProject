// GAS Phase 3: 기본 공격 쿨다운 GE 구현 (UE 5.4+ 컴포넌트 시스템)

#include "GE_Cooldown_Attack.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Cooldown_Attack::UGE_Cooldown_Attack()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 1초 고정 (Phase 4+ 에서 SetByCaller 또는 AttackSpeed 속성 기반으로 동적화 검토)
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

	// UE 5.4+: cooldown 검증은 UTargetTagsGameplayEffectComponent 만 인식.
	// 생성자에서 NewObject() 직접 호출은 금지 (CDO 생성 중 fatal) →
	// CreateDefaultSubobject + GEComponents 직접 추가 패턴 사용.
	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("Cooldown.Attack.Basic")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);

		GEComponents.Add(TagsComp);
	}
}
