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

    // State.HitReact — DefaultGameplayTags.ini 에 정의됨 → bErrorIfNotFound=true OK
    // 호출 측: AAOSCharacter::Multicast_PlayHitReact 에서 UGE_HitReact_State 로 부여, duration 은
    // HitReactMontage 길이만큼 유지. ABP 가 이 변수로 transition 가능.
    static const FGameplayTag HitReactTag =
        FGameplayTag::RequestGameplayTag(FName("State.HitReact"), /*bErrorIfNotFound=*/true);
    bIsHitReacting = ASC->HasMatchingGameplayTag(HitReactTag);

    // State.Casting — 스킬 GA(UGA_SkillBase 자식 BP) 가 ActivationOwnedTags 로 부여.
    // ABP 가 이 변수로 상하체 분리(Layered Blend Per Bone) transition 트리거:
    //   이동 중 + bIsCasting → 상체 스킬 + 하체 locomotion
    //   정지 + bIsCasting   → 전신 스킬 애니
    static const FGameplayTag CastingTag =
        FGameplayTag::RequestGameplayTag(FName("State.Casting"), /*bErrorIfNotFound=*/true);
    bIsCasting = ASC->HasMatchingGameplayTag(CastingTag);

    // State.Rooted — 루트 스킬(E/R: bAllowMovementDuringCast=false)이 ApplyCastRoot 로 부여.
    // 루트모션 몽타주(예: AM_Alex_R 도약 슬램)는 CharacterMovement 의 Velocity 를 만들어
    // Speed>0 → bIsMoving=true 가 되고, ABP 의 Blend Poses by bool(bIsMoving)이 상하체 분리로
    // 전환해 하체에 달리기 locomotion 이 섞인다. 루트 중에는 이동 입력이 없으므로(=정지 의도)
    // bIsMoving 을 false 로 강제 → ABP 가 전신 스킬 포즈(UpperFull)를 선택한다.
    static const FGameplayTag RootedTag =
        FGameplayTag::RequestGameplayTag(FName("State.Rooted"), /*bErrorIfNotFound=*/true);
    if (ASC->HasMatchingGameplayTag(RootedTag))
    {
        bIsMoving = false;
        Direction = 0.f;
    }
}
