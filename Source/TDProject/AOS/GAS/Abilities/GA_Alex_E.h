// Phase 4: Alex E — Judgment (심판).
// LoL 가렌 E 와 유사: 3초간 회전하면서 0.5초마다 주변 반경 250 적에게 데미지 (6틱 × 50 damage).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/TimerHandle.h"
#include "GA_Alex_E.generated.h"

/**
 * Alex E (Judgment) GameplayAbility — AoE periodic.
 *
 * 흐름:
 *   1. 쿨다운 GE (UGE_Cooldown_Alex_E, 10s) 적용
 *   2. CommitCost
 *   3. SkillMontages["Ability.Skill.Alex.E"] 재생 (회전 모션, 약 3s)
 *   4. 0.5초 간격 Timer 시작 (총 6회 tick)
 *   5. 매 tick: 반경 250 OverlapMultiByChannel(Pawn) → 적팀 캐릭터에 GE_Damage 적용
 *   6. 6회 완료 또는 몽타주 종료 시 EndAbility (Timer 정리)
 *
 * ActivationOwnedTags: Ability.Skill.Alex.E + State.Spinning
 *   → AnimInstance 가 bIsSpinning 미러 가능 (Phase 4+ 검토)
 */
UCLASS()
class TDPROJECT_API UGA_Alex_E : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Alex_E();

	// 시전 중 이동 가능 여부. false(E): 회전 중 고정 — GA 가 State.Rooted 부여 + StopMovement
	// → AI 가 스킬(회전) 끝까지 홀드. true 로 바꾸면 회전하며 이동 가능 (LoL 가렌식).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Skill")
	bool bAllowMovementDuringCast = false;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	// Timer 콜백 — 매 tick (0.5s) AoE 데미지 적용
	void OnSpinTick();

	// 6회 완료 후 정리 (또는 abort 시 EndAbility 가 호출)
	void StopSpinTimer();

	// === 상태 ===
	FTimerHandle SpinTimerHandle;
	int32 SpinTickCount = 0;
	static constexpr int32 MaxSpinTicks = 6;
	static constexpr float SpinTickInterval = 0.5f;
	static constexpr float SpinRadius = 250.0f;
	static constexpr float SpinDamagePerTick = 50.0f;
};
