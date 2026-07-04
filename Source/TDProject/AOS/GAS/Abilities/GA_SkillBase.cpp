// Phase 4+: 데이터 주도 스킬 base GA 구현.

#include "GA_SkillBase.h"
#include "AOS/AOSCharacter.h"
#include "AOS/AOSGameMode.h"   // EAOSTeam
#include "AOS/AOSAIController.h"
#include "GAS/AOSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"

// ============================================================
// 생성자 + Tag 초기화
// ============================================================

UGA_SkillBase::UGA_SkillBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// ABP 상하체 분리 트리거 (bIsCasting 미러 — 모든 스킬 공통)
	ActivationOwnedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.Casting")));

	// HitReact 중 스킬 차단 (Rule B 일관)
	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.HitReact")));
}

void UGA_SkillBase::PostInitProperties()
{
	Super::PostInitProperties();
	EnsureIdentityTags();
}

void UGA_SkillBase::PostLoad()
{
	Super::PostLoad();
	EnsureIdentityTags();
}

void UGA_SkillBase::EnsureIdentityTags()
{
	// BP CDO 에 SkillIdentityTag 가 설정돼 있으면 AssetTags(구 AbilityTags) + ActivationOwnedTags 에 자동 반영.
	if (!SkillIdentityTag.IsValid()) return;

	// UE 5.x 마이그레이션: AbilityTags 는 deprecated → GetAssetTags()/SetAssetTags() 사용.
	// (다음 UE 릴리스에서 AbilityTags 가 private 화됨)
	const FGameplayTagContainer& CurrentAssetTags = GetAssetTags();
	if (!CurrentAssetTags.HasTagExact(SkillIdentityTag))
	{
		FGameplayTagContainer NewAssetTags = CurrentAssetTags;
		NewAssetTags.AddTag(SkillIdentityTag);
		SetAssetTags(NewAssetTags);
	}
	if (!ActivationOwnedTags.HasTagExact(SkillIdentityTag))
	{
		ActivationOwnedTags.AddTag(SkillIdentityTag);
	}
}

// ============================================================
// ActivateAbility — 통합 흐름
// ============================================================

void UGA_SkillBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 상태 초기화
	PeriodicTickIndex = 0;
	bMontagePlaying = false;

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

	// 0. 스킬 시작 전에 살아있는 일반 공격(GA_Attack) 인스턴스를 강제로 cancel.
	//    이유: GA_Attack 의 PlayMontageAndWait 가 스킬 몽타주에 의해 interrupt 될 때
	//    bAllowInterruptAfterBlendOut=false 와 슬롯 충돌이 겹치면 OnInterrupted 콜백이
	//    누락되어 EndAbility 가 호출되지 않고 인스턴스가 영구 active 로 stuck 되는 race 가
	//    있음. 스킬 시작 시점에 명시적으로 cancel 해서 차단.
	{
		static const FGameplayTag AttackTag =
			FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"));
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(AttackTag);
		SourceASC->CancelAbilities(&CancelTags, nullptr, this);
	}

	// 1. 쿨다운 GE + 2. Cost commit
	bool bCommitOk = false;
	ApplyCooldownAndCost(Handle, ActorInfo, ActivationInfo, SourceASC, bCommitOk);
	if (!bCommitOk)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Self GE 들 일괄 적용
	ApplySelfEffects(SourceASC);

	// 4. TargetType 별 타겟 처리
	switch (TargetType)
	{
	case ESkillTargetType::Self:
		// no-op
		break;

	case ESkillTargetType::SingleEnemy:
		if (AActor* T = ResolveSingleTarget(TriggerEventData))
		{
			ApplyDamageAndEffectsToTarget(T, SourceASC);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[%s] SingleEnemy 타겟 없음 (TriggerEvent + AIController fallback 모두 실패)"),
				*GetName());
		}
		break;

	case ESkillTargetType::AoE_Sphere:
		if (PeriodicTickCount > 0)
		{
			// Periodic — Timer 로 Interval 마다 Pulse, MaxTicks 회
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					PeriodicTimerHandle,
					this, &UGA_SkillBase::OnPeriodicTick,
					PeriodicTickInterval, /*bLoop=*/true,
					/*FirstDelay=*/PeriodicTickInterval);
			}
		}
		else
		{
			// 즉발 1회 AoE
			ApplyAoEPulse(SourceASC);
		}
		break;
	}

	// 5. Cast root (이동 불가 스킬)
	AAOSCharacter* Char = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* SkillMontage = (Char && SkillIdentityTag.IsValid())
		? Char->GetSkillMontage(SkillIdentityTag) : nullptr;

	// 4.7. Launch (점프 스킬) — LaunchCharacter 로 띄운다. 적/전방으로 수평 + 수직 발사.
	//    Paragon 점프 애니는 in-place(pelvis만 점프, root 고정)라 루트모션 대신 속도로 캡슐을 띄운다.
	//    ⚠️ launch 스킬은 아래 cast root(StopMovementImmediately)를 건너뛴다 — 안 그러면 launch 속도가 즉시 0 이 됨.
	if (bLaunchOnActivate && Char)
	{
		FVector LaunchDir = Char->GetActorForwardVector();
		if (AActor* LaunchTarget = ResolveSingleTarget(TriggerEventData))
		{
			FVector ToTarget = LaunchTarget->GetActorLocation() - Char->GetActorLocation();
			ToTarget.Z = 0.0f;
			if (!ToTarget.IsNearlyZero())
			{
				LaunchDir = ToTarget.GetSafeNormal();
			}
		}
		const FVector LaunchVel = LaunchDir * LaunchForwardSpeed + FVector(0.0f, 0.0f, LaunchZSpeed);
		Char->LaunchCharacter(LaunchVel, /*bXYOverride=*/true, /*bZOverride=*/true);
	}

	// launch 스킬은 root 를 건너뛴다 (공중 이동을 살리기 위해).
	if (!bAllowMovementDuringCast && Char && !bLaunchOnActivate)
	{
		const float RootDur = ResolveRootDuration(SkillMontage);
		if (RootDur > 0.0f)
		{
			Char->ApplyCastRoot(RootDur);
		}
	}

	// 6. 몽타주 재생 (있으면)
	if (SkillMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, SkillMontage,
				/*Rate=*/1.0f, /*StartSection=*/NAME_None,
				/*bStopWhenAbilityEnds=*/true,
				/*AnimRootMotionTranslationScale=*/1.0f,
				/*StartTimeSeconds=*/0.0f,
				/*bAllowInterruptAfterBlendOut=*/false);

		if (MontageTask)
		{
			bMontagePlaying = true;
			MontageTask->OnCompleted.AddDynamic(this, &UGA_SkillBase::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UGA_SkillBase::OnMontageBlendOut);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_SkillBase::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UGA_SkillBase::OnMontageCancelled);
			MontageTask->ReadyForActivation();
		}
		else
		{
			// task 생성 실패 — Periodic 이 없으면 즉시 종료
			if (PeriodicTickCount == 0)
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			}
		}
	}
	else if (PeriodicTickCount == 0)
	{
		// 몽타주 없음 + Periodic 없음 → 효과만 적용했으니 즉시 종료
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
	// else: 몽타주 없고 Periodic 만 있음 → OnPeriodicTick 마지막에서 EndAbility
}

void UGA_SkillBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	StopPeriodicTimer();
	bMontagePlaying = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ============================================================
// 헬퍼 — Cooldown + Cost
// ============================================================

void UGA_SkillBase::ApplyCooldownAndCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	UAbilitySystemComponent* SourceASC, bool& bOutCommitOk)
{
	bOutCommitOk = false;

	if (CooldownGameplayEffectClass)
	{
		FGameplayEffectContextHandle CDCtx = SourceASC->MakeEffectContext();
		CDCtx.AddSourceObject(ActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle CDSpec = SourceASC->MakeOutgoingSpec(
			CooldownGameplayEffectClass, 1.0f, CDCtx);
		if (CDSpec.IsValid())
		{
			CDSpec.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(FName("Data.Duration")),
				CooldownDuration);
			SourceASC->ApplyGameplayEffectSpecToSelf(*CDSpec.Data.Get());
		}
	}

	bOutCommitOk = CommitAbilityCost(Handle, ActorInfo, ActivationInfo);
}

// ============================================================
// 헬퍼 — Self GE 적용
// ============================================================

void UGA_SkillBase::ApplySelfEffects(UAbilitySystemComponent* SourceASC)
{
	if (!SourceASC) return;
	for (TSubclassOf<UGameplayEffect> GEClass : SelfAppliedEffects)
	{
		if (!GEClass) continue;
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(GEClass, 1.0f, Ctx);
		if (Spec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
}

// ============================================================
// 헬퍼 — 타겟 찾기 (SingleEnemy)
// ============================================================

AActor* UGA_SkillBase::ResolveSingleTarget(const FGameplayEventData* TriggerEventData) const
{
	// 1. TriggerEventData->Target (GameplayEvent 로 활성화된 경우)
	// 주의: FGameplayEventData::Target 은 TObjectPtr<const AActor> 라 IsValid() 없음 → Get() 후 null 체크.
	AActor* Target = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	if (Target) return Target;

	// 2. AIController fallback (TryActivateAbilitiesByTag 경로)
	if (AAOSCharacter* SelfChar = Cast<AAOSCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (AAOSAIController* AIC = Cast<AAOSAIController>(SelfChar->GetController()))
		{
			AActor* T = AIC->GetCurrentTargetCharacter();
			if (!T) T = AIC->FindNearestEnemy();
			return T;
		}
	}
	return nullptr;
}

// ============================================================
// 헬퍼 — 단일 타겟 데미지 + 부수효과 적용
// ============================================================

void UGA_SkillBase::ApplyDamageAndEffectsToTarget(AActor* TargetActor, UAbilitySystemComponent* SourceASC)
{
	if (!TargetActor || !SourceASC) return;

	IAbilitySystemInterface* AsiTarget = Cast<IAbilitySystemInterface>(TargetActor);
	if (!AsiTarget || !AsiTarget->GetAbilitySystemComponent()) return;
	UAbilitySystemComponent* TargetASC = AsiTarget->GetAbilitySystemComponent();

	// 데미지 (DamageGameplayEffectClass 가 설정돼 있고 Damage > 0 일 때만)
	const float Damage = CalculateTargetDamage(TargetActor);
	if (DamageGameplayEffectClass && Damage > 0.0f)
	{
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
			DamageGameplayEffectClass, 1.0f, Ctx);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Damage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}

	// 부수효과 GE 들 (슬로우/스턴/마크 등 — 디자이너가 BP 에 채움)
	for (TSubclassOf<UGameplayEffect> GEClass : TargetAppliedEffects)
	{
		if (!GEClass) continue;
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(GEClass, 1.0f, Ctx);
		if (Spec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}

// ============================================================
// 헬퍼 — AoE 1회 (반경 내 적팀만)
// ============================================================

void UGA_SkillBase::ApplyAoEPulse(UAbilitySystemComponent* SourceASC)
{
	UWorld* World = GetWorld();
	if (!World || !SourceASC) return;

	AAOSCharacter* SourceChar = Cast<AAOSCharacter>(GetAvatarActorFromActorInfo());
	if (!SourceChar) return;

	TArray<FOverlapResult> Overlaps;
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(AoERadius);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkillAoE), false);
	Params.AddIgnoredActor(SourceChar);

	const FVector Origin = SourceChar->GetActorLocation();
	World->OverlapMultiByChannel(
		Overlaps, Origin, FQuat::Identity,
		ECollisionChannel::ECC_Pawn, Sphere, Params);

	const EAOSTeam SourceTeam = SourceChar->GetTeam();
	int32 HitsApplied = 0;

	for (const FOverlapResult& R : Overlaps)
	{
		AActor* A = R.GetActor();
		if (!A || A == SourceChar) continue;

		AAOSCharacter* TC = Cast<AAOSCharacter>(A);
		if (!TC || !TC->IsAlive()) continue;
		if (TC->GetTeam() == SourceTeam) continue;

		ApplyDamageAndEffectsToTarget(TC, SourceASC);
		HitsApplied++;
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("[%s] AoE Pulse — radius=%.0f, hits=%d"),
		*GetName(), AoERadius, HitsApplied);
}

// ============================================================
// Periodic
// ============================================================

void UGA_SkillBase::OnPeriodicTick()
{
	PeriodicTickIndex++;

	if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		StopPeriodicTimer();
		return;
	}
	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();

	ApplyAoEPulse(SourceASC);

	if (PeriodicTickIndex >= PeriodicTickCount)
	{
		StopPeriodicTimer();
		// 몽타주가 안 돌고 있으면 여기서 종료. 돌고 있으면 OnMontageCompleted 가 종료.
		if (!bMontagePlaying)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
}

void UGA_SkillBase::StopPeriodicTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicTimerHandle);
	}
}

// ============================================================
// Root duration 결정
// ============================================================

float UGA_SkillBase::ResolveRootDuration(UAnimMontage* SkillMontage) const
{
	if (ExplicitRootDuration > 0.0f) return ExplicitRootDuration;
	if (PeriodicTickCount > 0)        return PeriodicTickCount * PeriodicTickInterval;
	if (SkillMontage)                 return SkillMontage->GetPlayLength();
	return 0.0f;
}

// ============================================================
// CalculateTargetDamage (BP override 가능)
// ============================================================

float UGA_SkillBase::CalculateTargetDamage_Implementation(AActor* TargetActor) const
{
	// 기본 식: BaseDamage + (Target.MaxHP - Target.HP) * MissingHpDamageScale
	// MissingHpDamageScale=0 이면 BaseDamage 만 (Q 의 일반 데미지 스킬 / E 의 tick damage 패턴)
	if (!TargetActor || MissingHpDamageScale <= 0.0f) return BaseDamage;

	IAbilitySystemInterface* AsiTarget = Cast<IAbilitySystemInterface>(TargetActor);
	if (!AsiTarget || !AsiTarget->GetAbilitySystemComponent()) return BaseDamage;

	const UAOSAttributeSet* AttrSet =
		AsiTarget->GetAbilitySystemComponent()->GetSet<UAOSAttributeSet>();
	if (!AttrSet) return BaseDamage;

	const float MissingHp = FMath::Max(0.0f, AttrSet->GetMaxHealth() - AttrSet->GetHealth());
	return BaseDamage + MissingHp * MissingHpDamageScale;
}

// ============================================================
// 몽타주 콜백
// ============================================================

void UGA_SkillBase::OnMontageCompleted()
{
	bMontagePlaying = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SkillBase::OnMontageBlendOut()
{
	// OnCompleted 가 EndAbility 담당. 별도 처리 없음.
}

void UGA_SkillBase::OnMontageInterrupted()
{
	bMontagePlaying = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_SkillBase::OnMontageCancelled()
{
	bMontagePlaying = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
