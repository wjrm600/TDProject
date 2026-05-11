#include "AOSAnimInstance.h"
#include "AOSCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UAOSAnimInstance::UAOSAnimInstance()
{
    // 루트모션은 활성 몽타주에서만 추출 (AM_Death 같은 root motion 몽타주가 캐릭터 이동).
    // 기본값(NoRootMotionExtraction)이면 AnimSequence 의 bEnableRootMotion=true 도 무시됨.
    // CharacterMovement 가 자동으로 root motion delta 를 처리하여 ActorLocation 갱신.
    RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
}

// ─────────────────────────────────────────────────────────────────────────────
// NativeInitializeAnimation
// ─────────────────────────────────────────────────────────────────────────────

void UAOSAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    OwningCharacter = Cast<AAOSCharacter>(TryGetPawnOwner());
    if (OwningCharacter.IsValid())
    {
        // IAbilitySystemInterface::GetAbilitySystemComponent() 호출
        CachedASC = OwningCharacter->GetAbilitySystemComponent();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// NativeUpdateAnimation
// ─────────────────────────────────────────────────────────────────────────────

void UAOSAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // OwningCharacter 가 유효하지 않으면 DS/클라 모두 조기 반환
    if (!OwningCharacter.IsValid())
    {
        return;
    }

    // ------------------------------------------------------------------
    // Locomotion 갱신
    // ------------------------------------------------------------------
    const FVector Velocity = OwningCharacter->GetVelocity();

    // 수평 속도 (XY 평면)
    Speed = Velocity.Size2D();
    bIsMoving = Speed > KINDA_SMALL_NUMBER;

    // 이동 방향 계산 — UKismetAnimationLibrary 의존성 없이 직접 계산
    // ActorRotation 기준 Local Space 로 변환 후 Atan2
    if (bIsMoving)
    {
        const FRotator ActorRotation = OwningCharacter->GetActorRotation();
        const FVector LocalVelocity = ActorRotation.UnrotateVector(Velocity);
        Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
    }
    else
    {
        Direction = 0.f;
    }

    // 낙하/점프 상태
    if (UCharacterMovementComponent* CMC = OwningCharacter->GetCharacterMovement())
    {
        bIsFalling = CMC->IsFalling();
    }

    // ------------------------------------------------------------------
    // GAS 태그 미러 갱신
    // ------------------------------------------------------------------
    if (!CachedASC.IsValid())
    {
        // ASC 가 아직 초기화되지 않은 경우 — 재캐시 시도 (BeginPlay 이전 race 방어)
        CachedASC = OwningCharacter->GetAbilitySystemComponent();
        if (!CachedASC.IsValid())
        {
            return;
        }
    }

    UAbilitySystemComponent* ASC = CachedASC.Get();

    // Ability.Attack.Basic — DefaultGameplayTags.ini 에 정의됨 → bErrorIfNotFound=true OK
    static const FGameplayTag AttackBasicTag =
        FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"), /*bErrorIfNotFound=*/true);
    bIsAttacking = ASC->HasMatchingGameplayTag(AttackBasicTag);

    // State.Dead — DefaultGameplayTags.ini 에 정의됨 → bErrorIfNotFound=true OK
    static const FGameplayTag DeadTag =
        FGameplayTag::RequestGameplayTag(FName("State.Dead"), /*bErrorIfNotFound=*/true);
    bIsDead = ASC->HasMatchingGameplayTag(DeadTag);

    // State.HitReact — 현재 미정의. false 고정.
    // TODO: Phase 4 — DefaultGameplayTags.ini 에 State.HitReact 추가 후 아래 활성화:
    //   static const FGameplayTag HitReactTag =
    //       FGameplayTag::RequestGameplayTag(FName("State.HitReact"), false);
    //   bIsHitReacting = ASC->HasMatchingGameplayTag(HitReactTag);
    bIsHitReacting = false;

    // Ability.Skill — 부모 태그 (Ability.Skill.Heal / Charge 는 Phase 4 에서 정의 예정).
    // HasMatchingGameplayTag 는 exact match → 부모 태그 "Ability.Skill" 이 ASC 에
    // 직접 부여될 일이 없으므로 현재는 항상 false.
    // TODO: Phase 4 — HasAnyMatchingGameplayTags + FGameplayTagContainer{Skill tags} 로 갱신.
    bIsCasting = false;
}
