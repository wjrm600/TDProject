// Phase 4+: 캐릭터 스킬 데이터 주도식 부모 GA.
// 캐릭터 개별 스킬(Q/W/E/R 등)은 이 클래스를 상속한 BP 자산으로 작성한다
// (BP_GA_<Char>_<Slot>). C++ 클래스를 매 스킬마다 추가하지 않음.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"
#include "GA_SkillBase.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UAbilitySystemComponent;
class AAOSThrownWeapon;
class AAOSCharacter;

/** 스킬 타겟팅 방식. */
UENUM(BlueprintType)
enum class ESkillTargetType : uint8
{
	Self            UMETA(DisplayName = "Self (self-buff only)"),
	SingleEnemy     UMETA(DisplayName = "Single Enemy (TriggerData or AIController target)"),
	AoE_Sphere      UMETA(DisplayName = "AoE Sphere (caster 위치 반경, 적팀만)")
};

/**
 * UGA_SkillBase — 캐릭터 스킬의 데이터 주도식 부모.
 *
 * BP child 에서 설정해 다양한 스킬을 작성:
 *   - SkillIdentityTag           (Ability.Skill.Alex.Q 등 — AbilityTags + ActivationOwnedTags 자동 추가)
 *   - CooldownGameplayEffectClass (BP_GE_Cooldown_*) + CooldownDuration
 *   - SelfAppliedEffects          (자기 버프 GE 들, 예: GE_MoveSpeed_Boost, GE_DamageShield)
 *   - TargetType + AoERadius      (Single/Sphere)
 *   - DamageGameplayEffectClass + BaseDamage + MissingHpDamageScale
 *   - bAllowMovementDuringCast    (false 면 GE_Rooted 로 root, 끝까지 홀드)
 *   - PeriodicTickCount/Interval  (AoE 주기적 데미지 — E 패턴)
 *   - ThrownWeaponClass           (투척 무기 — 데미지를 착지 시점으로 미룸, Kwang Q 패턴)
 *
 * 특수 데미지식(R 의 처형)은 BlueprintNativeEvent CalculateTargetDamage 를 BP 에서 override.
 *
 * 흐름:
 *   1. 쿨다운 GE 적용 (SetByCaller Data.Duration = CooldownDuration)
 *   2. CommitAbilityCost
 *   3. SelfAppliedEffects 모두 self 에 적용
 *   4. TargetType 별:
 *      Self        : skip
 *      SingleEnemy : TriggerEventData->Target → AIController fallback → 데미지 + TargetAppliedEffects
 *      AoE_Sphere  : PeriodicTickCount>0 → timer × Interval, 아니면 즉발 1회
 *      (ThrownWeaponClass 가 있으면 위 대신: 타겟을 향해 돌아서고 ThrowReleaseDelay 뒤 무기 스폰 →
 *       무기가 착지할 때 ApplyThrownWeaponImpact 로 반경 피해)
 *   5. !bAllowMovementDuringCast → ApplyCastRoot(ExplicitRootDuration > 0 ? :
 *                                                Periodic > 0 ? Count*Interval :
 *                                                Montage 길이)
 *   6. SkillMontage (AOSCharacter::GetSkillMontage(SkillIdentityTag)) 재생
 *   7. 몽타주 종료 또는 Periodic 마지막 tick 에서 EndAbility (둘 중 늦은 쪽)
 *
 * 공통:
 *   - InstancingPolicy = InstancedPerActor
 *   - NetExecutionPolicy = ServerInitiated
 *   - ActivationOwnedTags += State.Casting (ABP 상하체 분리 트리거)
 *   - ActivationBlockedTags += State.HitReact (HitReact 중 스킬 차단)
 *   - ActivationBlockedTags += State.WeaponThrown (무기를 던져 손에 없는 동안 스킬 차단)
 */
UCLASS(Abstract)
class TDPROJECT_API UGA_SkillBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SkillBase();

	// ============================================================
	// Identity
	// ============================================================

	/** 이 스킬의 식별 태그. AbilityTags + ActivationOwnedTags 에 자동 추가됨. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Identity",
		meta = (Categories = "Ability.Skill"))
	FGameplayTag SkillIdentityTag;

	// ============================================================
	// Cast (이동 가능 여부 + root duration override)
	// ============================================================

	/** false 면 시전 중 이동 불가 (GE_Rooted + StopMovement). AI 가 스킬 끝까지 holds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Cast")
	bool bAllowMovementDuringCast = false;

	/** -1 = 자동 (Periodic 우선 → 몽타주 길이). 양수면 그 값으로 root duration 고정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Cast",
		meta = (ClampMin = "-1.0"))
	float ExplicitRootDuration = -1.0f;

	// ============================================================
	// Launch (Q/R 점프 — LaunchCharacter 속도 구동. Paragon 원본이 in-place 라 루트모션 대신 속도로 띄움.)
	// ============================================================

	/** true 면 활성화 시 캐릭터를 LaunchCharacter 로 띄운다 (Q 낮게 / R 높게).
	 *  launch 스킬은 cast root(StopMovement)를 자동으로 건너뛴다 — root 가 launch 속도를 죽이므로. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Launch")
	bool bLaunchOnActivate = false;

	/** 수직 발사 속도 (cm/s). Q ~450 낮게, R ~900 높게. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Launch",
		meta = (EditCondition = "bLaunchOnActivate", ClampMin = "0.0"))
	float LaunchZSpeed = 600.0f;

	/** 수평 발사 속도 (cm/s, 타겟/전방 방향). 0 이면 제자리 수직 점프. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Launch",
		meta = (EditCondition = "bLaunchOnActivate", ClampMin = "0.0"))
	float LaunchForwardSpeed = 0.0f;

	// ============================================================
	// Cooldown
	// (CooldownGameplayEffectClass 는 UGameplayAbility 표준 슬롯 — BP 에서 BP_GE_Cooldown_* 지정)
	// ============================================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Cooldown",
		meta = (ClampMin = "0.0"))
	float CooldownDuration = 5.0f;

	// ============================================================
	// Effects
	// ============================================================

	/** 활성화 직후 self 에 적용할 GE 들 (Q: MoveSpeed_Boost + EnhancedAttack, W: DamageShield 등). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Effects")
	TArray<TSubclassOf<UGameplayEffect>> SelfAppliedEffects;

	/** 타겟(또는 AoE 내 각 적)에 적용할 부수 효과 GE 들. 데미지 외 슬로우/스턴 등. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Effects")
	TArray<TSubclassOf<UGameplayEffect>> TargetAppliedEffects;

	/** 데미지 GE (BP 에서 GE_Damage 지정). nullptr 이면 데미지 미적용 (자기 버프 전용 스킬). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Effects")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	// ============================================================
	// Targeting
	// ============================================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Targeting")
	ESkillTargetType TargetType = ESkillTargetType::Self;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Targeting",
		meta = (EditCondition = "TargetType == ESkillTargetType::AoE_Sphere",
				ClampMin = "0.0"))
	float AoERadius = 250.0f;

	// ============================================================
	// Damage (단일 식: Base + MissingHp * Scale. 특이 식은 BP override)
	// ============================================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Damage",
		meta = (ClampMin = "0.0"))
	float BaseDamage = 0.0f;

	/** Damage = BaseDamage + (Target.MaxHP - Target.HP) * MissingHpDamageScale. R 의 처형식 = 0.3 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Damage",
		meta = (ClampMin = "0.0"))
	float MissingHpDamageScale = 0.0f;

	// ============================================================
	// Periodic (E 같은 다단 AoE)
	// ============================================================

	/** > 0 이면 주기적 AoE — Interval 마다 Count 회 ApplyAoEPulse. 0 이면 즉발 1회. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Periodic",
		meta = (ClampMin = "0"))
	int32 PeriodicTickCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Periodic",
		meta = (EditCondition = "PeriodicTickCount > 0", ClampMin = "0.01"))
	float PeriodicTickInterval = 0.5f;

	// ============================================================
	// Thrown Weapon (투척 무기 — Kwang Q 검 던지기)
	// ============================================================

	/** 지정하면 TargetType 데미지 대신 ThrowReleaseDelay 뒤 이 액터를 타겟 발밑으로 던진다.
	 *  데미지(BaseDamage · TargetAppliedEffects)는 무기가 착지할 때 반경 내 적팀에게 들어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Thrown Weapon")
	TSubclassOf<AAOSThrownWeapon> ThrownWeaponClass;

	/** 활성화 → 손에서 놓는 시점(초). 몽타주에서 무기 본이 사라지는 프레임에 맞춘다 (Ability_Q_Throw = 0.50). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Thrown Weapon",
		meta = (EditCondition = "ThrownWeaponClass != nullptr", ClampMin = "0.0"))
	float ThrowReleaseDelay = 0.5f;

	/** 최대 투척 거리 (수평 cm). 타겟이 더 멀면 그 방향으로 이 거리에 떨어진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS Skill|Thrown Weapon",
		meta = (EditCondition = "ThrownWeaponClass != nullptr", ClampMin = "100.0"))
	float ThrowRange = 900.0f;

	/** 투척 무기 착지 — Origin 반경 내 적팀에게 데미지 + 부수효과. AAOSThrownWeapon 이 서버에서 호출. */
	void ApplyThrownWeaponImpact(const FVector& Origin, float Radius);

	// ============================================================
	// UObject / GameplayAbility override
	// ============================================================

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

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

	// ============================================================
	// BP-overridable hook
	// ============================================================

	/** 타겟에게 줄 데미지 계산. 기본 = BaseDamage + Target.MissingHP * MissingHpDamageScale. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AOS Skill")
	float CalculateTargetDamage(AActor* TargetActor) const;
	virtual float CalculateTargetDamage_Implementation(AActor* TargetActor) const;

protected:
	// 몽타주 콜백
	UFUNCTION() void OnMontageCompleted();
	UFUNCTION() void OnMontageBlendOut();
	UFUNCTION() void OnMontageInterrupted();
	UFUNCTION() void OnMontageCancelled();

	// Periodic
	void OnPeriodicTick();
	void StopPeriodicTimer();

	// 헬퍼
	void ApplyCooldownAndCost(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		UAbilitySystemComponent* SourceASC, bool& bOutCommitOk);
	void ApplySelfEffects(UAbilitySystemComponent* SourceASC);
	void ApplyAoEPulse(UAbilitySystemComponent* SourceASC);
	void ApplyAoEPulseAt(UAbilitySystemComponent* SourceASC, const FVector& Origin, float Radius);
	void ApplyDamageAndEffectsToTarget(AActor* TargetActor, UAbilitySystemComponent* SourceASC);
	AActor* ResolveSingleTarget(const FGameplayEventData* TriggerEventData) const;
	float ResolveRootDuration(UAnimMontage* SkillMontage) const;

	void EnsureIdentityTags();

	// Thrown Weapon
	void BeginThrow(AAOSCharacter* Char, const FGameplayEventData* TriggerEventData, bool bHasMontage);
	void SpawnThrownWeapon();

	// 상태
	FTimerHandle PeriodicTimerHandle;
	int32 PeriodicTickIndex = 0;
	bool bMontagePlaying = false;

	FTimerHandle ThrowReleaseTimerHandle;
	TWeakObjectPtr<AActor> PendingThrowTarget;
	FVector PendingThrowLocation = FVector::ZeroVector;
};
