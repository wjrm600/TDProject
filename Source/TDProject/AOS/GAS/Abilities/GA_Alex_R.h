// Phase 4: Alex R — DemacianJustice (데마시아의 정의, 궁극기).
// LoL 가렌 R 와 유사: 단일 적에 처형 데미지 (잃은 체력 비례).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Alex_R.generated.h"

/**
 * Alex R (DemacianJustice) GameplayAbility — 단일 대상 처형.
 *
 * 흐름:
 *   1. 쿨다운 GE (UGE_Cooldown_Alex_R, 90s — 궁극기) 적용
 *   2. CommitCost
 *   3. TriggerEventData.Target = 대상. 없으면 EndAbility.
 *   4. 데미지 = 250 (base) + (Target.MaxHealth - Target.Health) * 0.3
 *   5. GE_Damage 로 단일 대상에 적용
 *   6. SkillMontages["Ability.Skill.Alex.R"] 재생 (있으면)
 *   7. 몽타주 종료 시 EndAbility
 *
 * 단순화: Phase 4 에선 데미지를 즉시 적용 (montage-driven 보다 즉발성이 처형 느낌 어울림).
 * 추후 enhance 시 PlayMontageAndWait + WaitGameplayEvent("AnimNotify.UltimateHit") 패턴 검토.
 */
UCLASS()
class TDPROJECT_API UGA_Alex_R : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Alex_R();

	// 시전 중 이동 가능 여부. false(R): 처형 모션 중 고정 — GA 가 State.Rooted 부여 + StopMovement
	// → AI 가 스킬 끝까지 홀드.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Skill")
	bool bAllowMovementDuringCast = false;

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

	// 단순화: ActivateAbility 시점에 즉시 데미지 적용
	void ApplyExecuteDamage(AActor* TargetActor, UAbilitySystemComponent* SourceASC);

	static constexpr float BaseDamage = 250.0f;
	static constexpr float MissingHealthMultiplier = 0.3f;
};
