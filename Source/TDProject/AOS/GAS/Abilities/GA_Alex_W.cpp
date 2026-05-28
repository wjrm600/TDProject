// Phase 4: Alex W (Courage) 구현.

#include "GA_Alex_W.h"
#include "AOS/AOSCharacter.h"
#include "GAS/Effects/GE_Cooldown_Alex_W.h"
#include "GAS/Effects/GE_DamageShield.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimMontage.h"

UGA_Alex_W::UGA_Alex_W()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.W"));
	AbilityTags.AddTag(AbilityTag);
	ActivationOwnedTags.AddTag(AbilityTag);
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Casting")));

	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.HitReact")));

	CooldownGameplayEffectClass = UGE_Cooldown_Alex_W::StaticClass();
}

void UGA_Alex_W::ActivateAbility(
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

	// 1. 쿨다운 GE
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

	// 3. GE_DamageShield 적용 (Self, Duration 2s)
	{
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(ActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
			UGE_DamageShield::StaticClass(), 1.0f, Ctx);
		if (Spec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// 4. 몽타주 재생
	AAOSCharacter* Char = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* SkillMontage = Char
		? Char->GetSkillMontage(FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.W")))
		: nullptr;

	if (!SkillMontage)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GA_Alex_W] SkillMontage 없음 — 효과만 적용, 즉시 종료"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Phase 4+: 이동 불가 스킬이면 root. W 는 기본 true 라 보통 skip.
	if (!bAllowMovementDuringCast && Char)
	{
		Char->ApplyCastRoot(SkillMontage->GetPlayLength());
	}

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
		MontageTask->OnCompleted.AddDynamic(this, &UGA_Alex_W::OnMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_Alex_W::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_Alex_W::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_Alex_W::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_Alex_W::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Alex_W::OnMontageBlendOut() {}

void UGA_Alex_W::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Alex_W::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
