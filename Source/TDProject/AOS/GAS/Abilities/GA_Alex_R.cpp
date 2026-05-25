// Phase 4: Alex R (DemacianJustice 궁극기) 구현.

#include "GA_Alex_R.h"
#include "AOS/AOSCharacter.h"
#include "AOS/AOSAIController.h"
#include "GAS/AOSAttributeSet.h"
#include "GAS/Effects/GE_Cooldown_Alex_R.h"
#include "GAS/Effects/GE_Damage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimMontage.h"

UGA_Alex_R::UGA_Alex_R()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.R"));
	AbilityTags.AddTag(AbilityTag);
	ActivationOwnedTags.AddTag(AbilityTag);

	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.HitReact")));

	CooldownGameplayEffectClass = UGE_Cooldown_Alex_R::StaticClass();
}

void UGA_Alex_R::ActivateAbility(
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

	// 1. 쿨다운 GE (90s)
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

	// 2. CommitCost
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Target 추출 — GameplayEvent 로 활성화 시 TriggerEventData.Target 사용.
	//    단, StateTree 의 ActivateAbilityByTag(TryActivateAbilitiesByTag) 로 활성화되면
	//    TriggerEventData 가 null → AIController 의 현재 타겟으로 fallback.
	AActor* TargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	if (!TargetActor)
	{
		if (AAOSCharacter* SelfChar = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (AAOSAIController* AIC = Cast<AAOSAIController>(SelfChar->GetController()))
			{
				TargetActor = AIC->GetCurrentTargetCharacter();
				if (!TargetActor)
				{
					TargetActor = AIC->FindNearestEnemy();
				}
			}
		}
	}
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Alex_R] Target 없음 (TriggerEventData + AIController fallback 모두 실패) — 능력 종료"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 4. 데미지 적용 (즉시) — 250 + missing HP * 0.3
	ApplyExecuteDamage(TargetActor, SourceASC);

	// 5. 몽타주 재생 (있으면) — 종료 시 EndAbility
	AAOSCharacter* Char = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* SkillMontage = Char
		? Char->GetSkillMontage(FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.R")))
		: nullptr;

	if (!SkillMontage)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GA_Alex_R] SkillMontage 없음 — 데미지만 적용, 즉시 종료"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
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
		MontageTask->OnCompleted.AddDynamic(this, &UGA_Alex_R::OnMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_Alex_R::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_Alex_R::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_Alex_R::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_Alex_R::ApplyExecuteDamage(AActor* TargetActor, UAbilitySystemComponent* SourceASC)
{
	if (!TargetActor || !SourceASC) return;

	IAbilitySystemInterface* AsiTarget = Cast<IAbilitySystemInterface>(TargetActor);
	if (!AsiTarget || !AsiTarget->GetAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_Alex_R] 타겟 %s 가 IAbilitySystemInterface 없음 — 데미지 미적용"),
			*TargetActor->GetName());
		return;
	}

	UAbilitySystemComponent* TargetASC = AsiTarget->GetAbilitySystemComponent();
	const UAOSAttributeSet* TargetAttrSet = TargetASC->GetSet<UAOSAttributeSet>();
	if (!TargetAttrSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Alex_R] 타겟 AttributeSet 없음 — 데미지 미적용"));
		return;
	}

	// 데미지 계산: BaseDamage + MissingHealth * MissingHealthMultiplier
	const float MaxHealth = TargetAttrSet->GetMaxHealth();
	const float CurHealth = TargetAttrSet->GetHealth();
	const float MissingHealth = FMath::Max(0.0f, MaxHealth - CurHealth);
	const float DamageAmount = BaseDamage + MissingHealth * MissingHealthMultiplier;

	UE_LOG(LogTemp, Warning,
		TEXT("[GA_Alex_R] DemacianJustice → %s | Base=%.0f + Missing(%.0f) * %.1f = %.0f"),
		*TargetActor->GetName(), BaseDamage, MissingHealth, MissingHealthMultiplier, DamageAmount);

	// GE_Damage 적용 (SetByCaller Data.Damage)
	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		UGE_Damage::StaticClass(), 1.0f, Ctx);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(
			FGameplayTag::RequestGameplayTag(FName("Data.Damage")), DamageAmount);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

void UGA_Alex_R::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Alex_R::OnMontageBlendOut() {}

void UGA_Alex_R::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Alex_R::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
