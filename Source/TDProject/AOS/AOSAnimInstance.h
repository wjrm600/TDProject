#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AOSAnimInstance.generated.h"

class AAOSCharacter;
class UAbilitySystemComponent;
class UAnimSequenceBase;

/**
 * AOS 캐릭터용 커스텀 AnimInstance.
 * ABP 스테이트 머신이 참조할 Locomotion/GAS 태그 미러 프로퍼티를 제공한다.
 *
 * DS 환경 주의:
 *  - NativeUpdateAnimation 은 DS 에서도 호출되지만 렌더 결과 없음 — 가볍게 유지.
 *  - CachedASC 는 서버/클라 양쪽에서 유효. HasMatchingGameplayTag 는 replicated tag container 참조.
 */
UCLASS()
class TDPROJECT_API UAOSAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UAOSAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // -------------------------------------------------------------------------
    // Locomotion — Velocity/CMC 기반. DS Velocity 가 클라이언트로 replicate 됨.
    // -------------------------------------------------------------------------

    /** 수평 이동 속도 (cm/s). 블렌드 스페이스 X축. */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    float Speed = 0.f;

    /** 이동 방향 각도 (도, -180~180). 블렌드 스페이스 Y축. */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    float Direction = 0.f;

    /** 이동 중 여부 (Speed > KINDA_SMALL_NUMBER). */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    bool bIsMoving = false;

    /** 낙하/점프 중 여부. */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    bool bIsFalling = false;

    // -------------------------------------------------------------------------
    // 로코모션 시퀀스 (캐릭터별) — OwningCharacter 에서 미러.
    // 공유 ABP_AOSCharacter 의 시퀀스 플레이어가 이 변수에 바인딩 → ABP 1개로 캐릭터별 모션.
    // (마네킹 스켈레톤 시퀀스. nullptr 면 ABP 의 기본/폴백 시퀀스가 재생되도록 둔다.)
    // -------------------------------------------------------------------------
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    TObjectPtr<UAnimSequenceBase> IdleAnim = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    TObjectPtr<UAnimSequenceBase> RunAnim = nullptr;

    // -------------------------------------------------------------------------
    // GAS 태그 미러 — AnimGraph 트랜지션 단순화용.
    // HasMatchingGameplayTag 는 replicated tag container 를 참조하므로 클라에서도 유효.
    // -------------------------------------------------------------------------

    /** 기본 공격 능력 활성 중 (Ability.Attack.Basic). */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    bool bIsAttacking = false;

    /**
     * 스킬 능력 활성 중 (Ability.Skill.*).
     * TODO: Phase 4 — Ability.Skill.Heal / Ability.Skill.Charge 태그 정의 후 갱신.
     * 현재는 Ability.Skill 부모 태그 체크 (HasMatchingGameplayTag 는 exact match,
     * HasAnyMatchingGameplayTags 로 자식 포함 검사 예정).
     */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    bool bIsCasting = false;

    /**
     * 히트 리액션 상태 (State.HitReact).
     * Phase 4 에서 태그 정의 예정 — 현재는 태그 미정의이므로 항상 false.
     * TODO: Phase 4 — DefaultGameplayTags.ini 에 State.HitReact 추가 후 활성화.
     */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    bool bIsHitReacting = false;

    /** 사망 상태 (State.Dead). */
    UPROPERTY(BlueprintReadOnly, Category = "AOS|Anim")
    bool bIsDead = false;

protected:
    /** 소유 캐릭터. TryGetPawnOwner() 가 null 이 될 수 있으므로 TWeakObjectPtr 사용. */
    UPROPERTY(Transient)
    TWeakObjectPtr<AAOSCharacter> OwningCharacter;

    /** ASC 캐시. OwningCharacter 가 유효한 경우에만 세팅됨. */
    UPROPERTY(Transient)
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
};
