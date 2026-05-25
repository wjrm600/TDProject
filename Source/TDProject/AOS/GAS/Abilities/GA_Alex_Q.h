// Phase 4: Alex Q — DecisiveStrike (결단).
// LoL 가렌 Q 와 유사: 3초간 이동속도 증가 + 다음 기본공격 강화 (1.5배 데미지).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GA_Alex_Q.generated.h"

/**
 * Alex Q (DecisiveStrike) GameplayAbility.
 *
 * 흐름:
 *   1. 쿨다운 GE (UGE_Cooldown_Alex_Q, 8s) 적용
 *   2. CommitCost
 *   3. UGE_MoveSpeed_Boost (3s, +300 MoveSpeed, State.SpeedBoost) 적용
 *   4. UGE_EnhancedAttack (3s, State.EnhancedAttack) 적용 — GA_Attack 이 다음 1회 강화
 *   5. SkillMontages["Ability.Skill.Alex.Q"] 재생 (있으면)
 *   6. 몽타주 종료 시 EndAbility (몽타주 없으면 즉시 종료)
 *
 * 트리거: ASC->TryActivateAbilitiesByTag(Ability.Skill.Alex.Q) — StateTree 가 호출
 * Net: ServerInitiated (서버에서만 활성화, 효과는 replicate)
 */
UCLASS()
class TDPROJECT_API UGA_Alex_Q : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Alex_Q();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// PlayMontageAndWait 콜백
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();
};
