// GAS Phase 3: 기본 공격 쿨다운 GE 구현 (UE 5.4+ 컴포넌트 시스템)

#include "GE_Cooldown_Attack.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Cooldown_Attack::UGE_Cooldown_Attack()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Duration 은 SetByCaller(Data.Duration) — 호출자(GA_Attack) 가 1.0/AttackSpeed 전달.
	// AttackSpeed=1.0 → 1.0s, AttackSpeed=2.0 → 0.5s, AttackSpeed=0.5 → 2.0s.
	// 호출자에서 SetSetByCallerMagnitude 안 하면 0 으로 평가되어 즉시 만료 — GA_Attack 이 항상 명시 set.
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

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
