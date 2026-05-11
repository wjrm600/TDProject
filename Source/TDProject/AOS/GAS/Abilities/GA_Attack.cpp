// GAS Phase 3: 기본 공격 GameplayAbility 구현

#include "GA_Attack.h"
#include "AOSAttributeSet.h"
#include "Effects/GE_Damage.h"
#include "Effects/GE_Cooldown_Attack.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AOS/AOSCharacter.h"

UGA_Attack::UGA_Attack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"));
	AbilityTags.AddTag(AbilityTag);
	ActivationOwnedTags.AddTag(AbilityTag);

	// 트리거: GameplayEvent("Ability.Attack.Basic")
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = AbilityTag;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	// 쿨다운 GE
	CooldownGameplayEffectClass = UGE_Cooldown_Attack::StaticClass();
}

void UGA_Attack::ActivateAbility(
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

	// 쿨다운 GE 명시적 적용 (이게 단일 진실 공급원)
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
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GA_Attack] CooldownGameplayEffectClass=nullptr — 쿨다운 미적용"));
	}

	// 코스트만 별도 커밋 (cooldown 은 위에서 직접 처리)
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* TargetActor = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (!TargetActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// AttackPower 속성값 = 데미지 양
	const UAOSAttributeSet* SourceAttrSet = SourceASC->GetSet<UAOSAttributeSet>();
	const float DamageAmount = SourceAttrSet ? SourceAttrSet->GetAttackPower() : 0.0f;

	if (DamageAmount > 0.0f)
	{
		// Phase 5: AOSStructure 도 IAbilitySystemInterface 를 구현하므로 모던 경로로 일원화
		// (이전 Cast<AAOSStructure> fallback 분기 제거)
		IAbilitySystemInterface* AsiTarget = Cast<IAbilitySystemInterface>(TargetActor);
		if (AsiTarget && AsiTarget->GetAbilitySystemComponent())
		{
			UAbilitySystemComponent* TargetASC = AsiTarget->GetAbilitySystemComponent();

			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddSourceObject(ActorInfo->AvatarActor.Get());

			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
				UGE_Damage::StaticClass(), 1.0f, Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(
					FGameplayTag::RequestGameplayTag(FName("Data.Damage")), DamageAmount);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}

	// 시각용 공격 몽타주 재생 (자산 있으면) — 데미지 흐름과 무관, nullptr-safe
	// ServerInitiated 정책 → 서버에서 PlayAnimMontage 호출 → 엔진이 클라에 자동 replicate
	if (AAOSCharacter* AttackerChar = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get()))
	{
		if (UAnimMontage* M = AttackerChar->GetAttackMontage())
		{
			AttackerChar->PlayAnimMontage(M);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
