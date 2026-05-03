// GAS Phase 3: 기본 공격 GameplayAbility
// GameplayEvent("Ability.Attack.Basic") 로 트리거되어 단일 타겟에 GE_Damage 적용

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Attack.generated.h"

/**
 * 기본 공격 능력.
 *
 * 트리거: AIController 가 ASC->HandleGameplayEvent("Ability.Attack.Basic", EventData)
 *         호출 시 활성화. EventData.Target 이 데미지 대상.
 *
 * 효과: AttributeSet::AttackPower 만큼 GE_Damage 를 타겟에 적용.
 *       AOSStructure (ASC 미보유) 는 Phase 5 까지 fallback 으로 ReceiveDamage(float) 직접 호출.
 *
 * 쿨다운: GE_Cooldown_Attack (1초) 의 GrantedTag "Cooldown.Attack.Basic" 가 다음 활성화를 차단.
 */
UCLASS()
class TDPROJECT_API UGA_Attack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Attack();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
