// GAS Phase 3 (montage-driven): 기본 공격 GameplayAbility 구현
//
// 변경 이력:
//   Phase 3  초기: 즉시 데미지 + PlayAnimMontage (시각용)
//   Phase 3+ 재작성: PlayMontageAndWait + WaitGameplayEvent("AnimNotify.AttackHit")
//              → 몽타주 notify 시점에 데미지 적용 (자연스러운 무기 충격 타이밍)

#include "GA_Attack.h"
#include "AOSAttributeSet.h"
#include "Effects/GE_Damage.h"
#include "Effects/GE_Cooldown_Attack.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AOS/AOSCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"

UGA_Attack::UGA_Attack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"));
	// UE 5.x 마이그레이션: AbilityTags 는 deprecated → GetAssetTags()/SetAssetTags() (생성자, GA_SkillBase 패턴).
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityTag);
	SetAssetTags(AssetTags);
	ActivationOwnedTags.AddTag(AbilityTag);

	// 트리거: GameplayEvent("Ability.Attack.Basic")
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = AbilityTag;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	// 쿨다운 GE
	CooldownGameplayEffectClass = UGE_Cooldown_Attack::StaticClass();

	// Rule B: State.HitReact 태그 활성 중엔 공격 차단 — HitReact ↔ Attack 충돌 방지.
	// GE_HitReact_State 가 HitReactMontage 재생 시간 동안 이 태그를 ASC 에 부여한다.
	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.HitReact")));

	// Phase 4+: 스킬 시전 중 일반 공격 차단 — 스킬 GA(UGA_SkillBase)가 State.Casting 부여.
	// 이걸로 막지 않으면 AttackEnemy → GA_Attack 활성화 직후 UseQ → GA_Skill 이 같은 슬롯에
	// 새 몽타주를 깔면서 AM_Attack 을 interrupt → GA_Attack 의 PlayMontageAndWait 가
	// bAllowInterruptAfterBlendOut=false 일 때 OnInterrupted 콜백이 누락되어 EndAbility 가
	// 호출 안 됨 → GA_Attack 인스턴스가 영구 active 로 stuck → 이후 일반 공격 영영 발동 X.
	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.Casting")));
}

void UGA_Attack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 활성화 상태 초기화 — InstancedPerActor 이므로 매 활성화 시 리셋 필수
	bDamageAppliedThisActivation = false;
	bIsCritThisActivation = false;
	CachedTarget = nullptr;

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

	// 0. AttackSpeed 추출 (몽타주 Rate + Cooldown duration 양쪽에 적용 — 일관성).
	//    안전 가드: 0 또는 음수면 1.0 fallback (cooldown 무한대 / 몽타주 freeze 회피).
	const UAOSAttributeSet* SourceAttrSet = SourceASC->GetSet<UAOSAttributeSet>();
	const float RawAttackSpeed = SourceAttrSet ? SourceAttrSet->GetAttackSpeed() : 1.0f;
	const float AttackSpeed = (RawAttackSpeed > KINDA_SMALL_NUMBER) ? RawAttackSpeed : 1.0f;

	// 1. 쿨다운 GE 명시 적용 (단일 진실 공급원 — CommitAbility 의 쿨다운 적용과 별개).
	//    Duration = 1.0/AttackSpeed via SetByCaller(Data.Duration).
	if (CooldownGameplayEffectClass)
	{
		FGameplayEffectContextHandle CDCtx = SourceASC->MakeEffectContext();
		CDCtx.AddSourceObject(ActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle CDSpec = SourceASC->MakeOutgoingSpec(
			CooldownGameplayEffectClass, 1.0f, CDCtx);
		if (CDSpec.IsValid())
		{
			const float CooldownDuration = 1.0f / AttackSpeed;
			CDSpec.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(FName("Data.Duration")),
				CooldownDuration);
			SourceASC->ApplyGameplayEffectSpecToSelf(*CDSpec.Data.Get());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GA_Attack] CooldownGameplayEffectClass=nullptr — 쿨다운 미적용"));
	}

	// 2. 코스트 커밋 (cooldown 은 위에서 직접 처리했으므로 cost 만)
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. 타겟 cache — TriggerEventData.Target 이 데미지 대상
	AActor* TargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Attack] TriggerEventData.Target 없음 — 능력 종료"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	CachedTarget = TargetActor;

	// 4. AttackMontage 확인 — 없으면 즉시 데미지 fallback
	AAOSCharacter* AttackerChar = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* AttackMontage = AttackerChar ? AttackerChar->GetAttackMontage() : nullptr;

	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_Attack] AttackMontage 없음 → 즉시 데미지 fallback (montage-driven 비활성)"));
		ApplyDamageToCachedTarget();
		bDamageAppliedThisActivation = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 4.5. 크리티컬 roll + 재생 섹션 선택.
	//    크리 시 CritSectionName(예: PrimaryAttack_D), 아니면 NormalAttackSectionNames 중 랜덤(A/B).
	//    ⚠️ 각 섹션은 몽타주에서 Next Section=None 이어야 한 번만 재생하고 정지한다.
	//    섹션명이 비었거나 몽타주에 없으면 StartSection=NAME_None (처음부터 재생) fallback.
	{
		const float CritChance = SourceAttrSet ? SourceAttrSet->GetCritChance() : 0.0f;
		bIsCritThisActivation = (CritChance > 0.0f) && (FMath::FRand() < CritChance);
	}

	FName StartSection = NAME_None;
	if (bIsCritThisActivation && !CritSectionName.IsNone())
	{
		StartSection = CritSectionName;
	}
	else if (NormalAttackSectionNames.Num() > 0)
	{
		StartSection = NormalAttackSectionNames[FMath::RandRange(0, NormalAttackSectionNames.Num() - 1)];
	}
	// 몽타주에 해당 섹션이 실제로 없으면 처음부터 재생 (안전 fallback)
	if (!StartSection.IsNone() && !AttackMontage->IsValidSectionName(StartSection))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_Attack] 섹션 '%s' 이 몽타주에 없음 → StartSection=None fallback"),
			*StartSection.ToString());
		StartSection = NAME_None;
	}

	// 5. PlayMontageAndWait task
	//    Rate=AttackSpeed: 몽타주 재생 속도 = AttackSpeed (1.0=평소, 2.0=2x 빠름).
	//    bStopWhenAbilityEnds=true: Ability 가 취소될 때 몽타주도 함께 중단
	//    bAllowInterruptAfterBlendOut=true: BlendOut 이후에도 Interrupted 콜백 보장 →
	//    스킬 몽타주가 같은 슬롯을 덮어쓸 때 OnInterrupted 가 확실히 fire 되어 EndAbility 호출 →
	//    GA_Attack 인스턴스가 영구 stuck 되는 race 차단.
	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			AttackMontage,
			/*Rate=*/AttackSpeed,
			/*StartSection=*/StartSection,
			/*bStopWhenAbilityEnds=*/true,
			/*AnimRootMotionTranslationScale=*/1.0f,
			/*StartTimeSeconds=*/0.0f,
			/*bAllowInterruptAfterBlendOut=*/true);

	if (MontageTask)
	{
		MontageTask->OnCompleted.AddDynamic(this, &UGA_Attack::OnMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_Attack::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_Attack::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_Attack::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GA_Attack] PlayMontageAndWait task 생성 실패 → 즉시 데미지 fallback"));
		ApplyDamageToCachedTarget();
		bDamageAppliedThisActivation = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 6. WaitGameplayEvent task — AnimNotify.AttackHit 수신 시 OnAttackHitEvent 호출
	//    OnlyTriggerOnce=true: 한 활성화 당 한 번만 처리 (다중 notify 중복 방지)
	//    OnlyMatchExact=true: AnimNotify.AttackHit 정확 매칭 (하위 태그 무시)
	static const FGameplayTag NotifyTag =
		FGameplayTag::RequestGameplayTag(FName("AnimNotify.AttackHit"));
	UAbilityTask_WaitGameplayEvent* EventTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			NotifyTag,
			/*OptionalExternalTarget=*/nullptr,
			/*OnlyTriggerOnce=*/true,
			/*OnlyMatchExact=*/true);

	if (EventTask)
	{
		EventTask->EventReceived.AddDynamic(this, &UGA_Attack::OnAttackHitEvent);
		EventTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_Attack] WaitGameplayEvent task 생성 실패 — OnMontageCompleted fallback 에서 데미지 적용"));
		// MontageTask 는 이미 실행 중 — OnMontageCompleted fallback 이 데미지 처리
	}

	// 이후: 몽타주 재생 중 — task 들이 비동기 대기
	// OnAttackHitEvent 또는 OnMontageCompleted(fallback) 에서 데미지 적용 후 EndAbility
}

// ---------------------------------------------------------------------------
// WaitGameplayEvent 콜백 — AnimNotify.AttackHit 수신 시 데미지 적용
// ---------------------------------------------------------------------------
void UGA_Attack::OnAttackHitEvent(FGameplayEventData Payload)
{
	if (bDamageAppliedThisActivation)
	{
		// OnlyTriggerOnce=true 지만 방어적 중복 방지
		return;
	}

	ApplyDamageToCachedTarget();
	bDamageAppliedThisActivation = true;
	// EndAbility 는 OnMontageCompleted 가 담당 — 몽타주가 완전히 끝난 후 능력 종료
}

// ---------------------------------------------------------------------------
// PlayMontageAndWait 콜백
// ---------------------------------------------------------------------------

void UGA_Attack::OnMontageCompleted()
{
	// Notify 미수신 케이스 fallback — 몽타주가 완료됐는데 데미지가 안 들어갔으면 적용
	if (!bDamageAppliedThisActivation && CachedTarget.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_Attack] 몽타주 완료 — AnimNotify.AttackHit 미수신 → 데미지 fallback 적용"));
		ApplyDamageToCachedTarget();
		bDamageAppliedThisActivation = true;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Attack::OnMontageBlendOut()
{
	// BlendOut 시작 알림 — EndAbility 는 OnMontageCompleted/OnMontageInterrupted 가 담당.
	// 별도 처리 없음.
}

void UGA_Attack::OnMontageInterrupted()
{
	// 외부 몽타주 덮어씀(스킬 시전 등) 또는 엔진 내부 중단 — 데미지 미적용으로 능력 종료
	// bAllowInterruptAfterBlendOut=true 이므로 BlendOut 이후에도 이 콜백이 안전하게 fire.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Attack::OnMontageCancelled()
{
	// Ability 취소(Ability cancel) 로 인한 중단 — 데미지 미적용으로 능력 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// ---------------------------------------------------------------------------
// 데미지 적용 헬퍼
// ---------------------------------------------------------------------------
void UGA_Attack::ApplyDamageToCachedTarget()
{
	if (!CurrentActorInfo || !CachedTarget.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_Attack] ApplyDamageToCachedTarget: ActorInfo 또는 CachedTarget 유효하지 않음 — 스킵"));
		return;
	}

	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!SourceASC)
	{
		return;
	}

	// AttackPower 속성값 = 데미지 양
	const UAOSAttributeSet* SourceAttrSet = SourceASC->GetSet<UAOSAttributeSet>();
	float DamageAmount = SourceAttrSet ? SourceAttrSet->GetAttackPower() : 0.0f;
	if (DamageAmount <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Attack] DamageAmount <= 0 — 데미지 미적용"));
		return;
	}

	// Phase 4: State.EnhancedAttack 태그 활성 시 데미지 1.5배 + 효과 1회 소비 (Alex Q 후속).
	static const FGameplayTag EnhancedAttackTag =
		FGameplayTag::RequestGameplayTag(FName("State.EnhancedAttack"));
	if (SourceASC->HasMatchingGameplayTag(EnhancedAttackTag))
	{
		DamageAmount *= 1.5f;
		// GE_EnhancedAttack 효과 제거 — 1회 소비. RemoveActiveEffectsWithGrantedTags 는
		// State.EnhancedAttack 을 GrantedTag 로 가진 모든 active GE 를 종료시킴.
		FGameplayTagContainer RemoveTags;
		RemoveTags.AddTag(EnhancedAttackTag);
		const int32 Removed = SourceASC->RemoveActiveEffectsWithGrantedTags(RemoveTags);
		UE_LOG(LogTemp, Verbose,
			TEXT("[GA_Attack] EnhancedAttack 활성 — 데미지 1.5배 적용 (소비된 GE: %d)"),
			Removed);
	}

	// 크리티컬: ActivateAbility 에서 roll 한 bIsCritThisActivation 이면 CritDamage 배수 적용.
	//    (섹션 선택과 데미지 배수가 같은 roll 을 공유하므로 시각/수치 일관성 보장)
	if (bIsCritThisActivation)
	{
		const float CritMult = SourceAttrSet ? SourceAttrSet->GetCritDamage() : 2.0f;
		DamageAmount *= CritMult;
		UE_LOG(LogTemp, Verbose, TEXT("[GA_Attack] 크리티컬! 데미지 %.1f배"), CritMult);
	}

	AActor* TargetActor = CachedTarget.Get();
	if (!TargetActor)
	{
		return;
	}

	// Phase 5: AOSStructure 도 IAbilitySystemInterface 구현 → 모던 경로로 일원화
	// Cast<AAOSStructure> fallback 분기 없음
	IAbilitySystemInterface* AsiTarget = Cast<IAbilitySystemInterface>(TargetActor);
	if (!AsiTarget || !AsiTarget->GetAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_Attack] 타겟 %s 가 IAbilitySystemInterface 없음 — 데미지 미적용"),
			*TargetActor->GetName());
		return;
	}

	UAbilitySystemComponent* TargetASC = AsiTarget->GetAbilitySystemComponent();

	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddSourceObject(CurrentActorInfo->AvatarActor.Get());

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		UGE_Damage::StaticClass(), 1.0f, Ctx);

	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(
			FGameplayTag::RequestGameplayTag(FName("Data.Damage")), DamageAmount);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}
