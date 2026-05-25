// Phase 4: Alex Q (DecisiveStrike) 구현.

#include "GA_Alex_Q.h"
#include "AOS/AOSCharacter.h"
#include "GAS/Effects/GE_Cooldown_Alex_Q.h"
#include "GAS/Effects/GE_MoveSpeed_Boost.h"
#include "GAS/Effects/GE_EnhancedAttack.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimMontage.h"

UGA_Alex_Q::UGA_Alex_Q()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.Q"));
	AbilityTags.AddTag(AbilityTag);
	ActivationOwnedTags.AddTag(AbilityTag);

	// HitReact 중엔 스킬 차단 (Rule B 와 일관)
	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.HitReact")));

	// 쿨다운 GE
	CooldownGameplayEffectClass = UGE_Cooldown_Alex_Q::StaticClass();
}

void UGA_Alex_Q::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

	// 1. 쿨다운 GE 명시 적용 (Cooldown.Skill.Alex.Q 태그 부여 8s)
	if (CooldownGameplayEffectClass)
	{
		FGameplayEffectContextHandle CDCtx = SourceASC->MakeEffectContext();
		CDCtx.AddSourceObject(ActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle CDSpec = SourceASC->MakeOutgoingSpec(
			CooldownGameplayEffectClass, 1.0f, CDCtx);
		if (CDSpec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*CDSpec.Data.Get());
		}
	}

	// 2. 코스트 커밋
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Buff GE 2개 적용 (MoveSpeed_Boost + EnhancedAttack) — 둘 다 self 적용
	auto ApplyGEToSelf = [&](TSubclassOf<UGameplayEffect> GEClass)
	{
		if (!GEClass) return;
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(ActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(GEClass, 1.0f, Ctx);
		if (Spec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	};
	ApplyGEToSelf(UGE_MoveSpeed_Boost::StaticClass());
	ApplyGEToSelf(UGE_EnhancedAttack::StaticClass());

	// 4. 스킬 몽타주 재생 (자산 있으면)
	AAOSCharacter* Char = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* SkillMontage = Char
		? Char->GetSkillMontage(FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.Q")))
		: nullptr;

	if (!SkillMontage)
	{
		// 몽타주 없으면 buff 만 적용 후 종료 (fallback)
		UE_LOG(LogTemp, Verbose, TEXT("[GA_Alex_Q] SkillMontage 없음 — buff 만 적용, 즉시 종료"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 5. PlayMontageAndWait — 몽타주 종료 시 EndAbility
	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			SkillMontage,
			/*Rate=*/1.0f,
			/*StartSection=*/NAME_None,
			/*bStopWhenAbilityEnds=*/true,
			/*AnimRootMotionTranslationScale=*/1.0f,
			/*StartTimeSeconds=*/0.0f,
			/*bAllowInterruptAfterBlendOut=*/false);

	if (MontageTask)
	{
		MontageTask->OnCompleted.AddDynamic(this, &UGA_Alex_Q::OnMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_Alex_Q::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_Alex_Q::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_Alex_Q::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}
	else
	{
		// task 생성 실패 — 즉시 종료
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_Alex_Q::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Alex_Q::OnMontageBlendOut()
{
	// 별도 처리 없음 — OnCompleted 가 EndAbility 담당
}

void UGA_Alex_Q::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Alex_Q::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
