// GAS Phase 3 (montage-driven): 기본 공격 GameplayAbility
// GameplayEvent("Ability.Attack.Basic") 로 트리거되어
// PlayMontageAndWait + WaitGameplayEvent("AnimNotify.AttackHit") 패턴으로
// 몽타주 notify 시점에 GE_Damage 를 타겟에 적용

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"  // FGameplayEventData
#include "GA_Attack.generated.h"

/**
 * 기본 공격 능력 (montage-driven damage).
 *
 * 트리거: AIController 가 ASC->HandleGameplayEvent("Ability.Attack.Basic", EventData)
 *         호출 시 활성화. EventData.Target 이 데미지 대상.
 *
 * 흐름:
 *   ActivateAbility → 쿨다운 GE → 코스트 커밋 → 타겟 cache
 *     → PlayMontageAndWait task + WaitGameplayEvent("AnimNotify.AttackHit") task
 *     → (몽타주 재생, notify 시점에 event fire)
 *     → OnAttackHitEvent → 데미지 적용
 *     → OnMontageCompleted → EndAbility
 *
 * 중단 케이스:
 *   - 몽타주 interrupt (사망/스턴) → OnMontageInterrupted → EndAbility (데미지 미적용)
 *   - Ability cancel → OnMontageCancelled → EndAbility (데미지 미적용)
 *   - Notify 미수신 → OnMontageCompleted 의 fallback 이 데미지 적용 후 EndAbility
 *
 * 쿨다운: GE_Cooldown_Attack (1초) 의 GrantedTag "Cooldown.Attack.Basic" 가 다음 활성화를 차단.
 * 멤버 보존: InstancedPerActor 정책 필수 — CachedTarget / bDamageAppliedThisActivation 작동 전제.
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

	// ============================================================
	// 몽타주 섹션 (랜덤 기본공격 + 크리티컬)
	// ============================================================

	/** 일반 공격에서 랜덤으로 하나 선택해 재생할 섹션 이름들 (AM_Attack 의 섹션명과 일치해야 함).
	 *  ⚠️ 각 섹션은 몽타주에서 Next Section = None 이어야 (한 번 재생 후 정지). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Attack|Sections")
	TArray<FName> NormalAttackSectionNames = { FName("AttackA"), FName("AttackB") };

	/** 크리티컬 히트 시 재생할 섹션 이름 (예: PrimaryAttack_D). 비우면 크리에도 일반 섹션 재생. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Attack|Sections")
	FName CritSectionName = FName("Crit");

protected:
	// 이번 활성화가 크리티컬인지 — ActivateAbility 에서 roll, ApplyDamage 에서 배수 적용
	bool bIsCritThisActivation = false;

	// 활성화 시 cache 한 타겟 — 몽타주 진행 중 보관, AnimNotify 시점에 데미지 적용 대상
	// TWeakObjectPtr: 몽타주 재생 중 타겟 사망/destroy 시 안전하게 null 처리
	UPROPERTY()
	TWeakObjectPtr<AActor> CachedTarget;

	// 한 번 활성화 동안 데미지 적용 여부 — 다중 notify 또는 fallback 시 중복 방지
	bool bDamageAppliedThisActivation = false;

	// 데미지 적용 헬퍼 — CachedTarget 에 GE_Damage 적용 (AttackPower → SetByCaller)
	void ApplyDamageToCachedTarget();

	// PlayMontageAndWait delegate 콜백 (4종)
	// OnCompleted: 정상 재생 완료 — fallback 데미지 처리 후 EndAbility
	UFUNCTION()
	void OnMontageCompleted();

	// OnBlendOut: 블렌드 아웃 시작 — EndAbility 는 OnCompleted 가 담당하므로 별도 처리 없음
	UFUNCTION()
	void OnMontageBlendOut();

	// OnMontageInterrupted: 사망/스턴 등 외부 몽타주 덮어씀 — 데미지 미적용, EndAbility
	// UE 5.3+ 에서 OnInterrupted delegate 는 deprecated; 엔진 내부 MontageEndedDelegate 로 처리
	// 여기서는 OnInterrupted (delegate) 에 바인드 — 실제 delegate 이름은 task.OnInterrupted
	UFUNCTION()
	void OnMontageInterrupted();

	// OnMontageCancelled: Ability 취소로 인한 중단 — 데미지 미적용, EndAbility
	UFUNCTION()
	void OnMontageCancelled();

	// WaitGameplayEvent delegate 콜백 — AnimNotify.AttackHit 수신 시 데미지 적용
	UFUNCTION()
	void OnAttackHitEvent(FGameplayEventData Payload);
};
