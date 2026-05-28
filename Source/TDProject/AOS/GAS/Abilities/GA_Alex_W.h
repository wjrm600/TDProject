// Phase 4: Alex W — Courage (용기).
// LoL 가렌 W 와 유사: 2초간 받는 데미지 50% 감소.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Alex_W.generated.h"

/**
 * Alex W (Courage) GameplayAbility.
 *
 * 흐름:
 *   1. 쿨다운 GE (UGE_Cooldown_Alex_W, 15s) 적용
 *   2. CommitCost
 *   3. UGE_DamageShield (2s, State.DamageShield) 적용
 *      → AOSAttributeSet 가 데미지 적용 시점에 50% 감소 처리
 *   4. SkillMontages["Ability.Skill.Alex.W"] 재생 (있으면)
 *   5. 몽타주 종료 시 EndAbility (몽타주 없으면 즉시 종료)
 */
UCLASS()
class TDPROJECT_API UGA_Alex_W : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Alex_W();

	// 시전 중 이동 가능 여부. true(W): 이동하며 시전.
	// false: GA 가 State.Rooted 부여 + StopMovement → AI 가 스킬 끝까지 홀드.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Skill")
	bool bAllowMovementDuringCast = true;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();
};
