// Phase 4: Alex E (Judgment AoE 회전) 구현.

#include "GA_Alex_E.h"
#include "AOS/AOSCharacter.h"
#include "AOS/AOSGameMode.h"   // EAOSTeam
#include "GAS/Effects/GE_Cooldown_Alex_E.h"
#include "GAS/Effects/GE_Damage.h"
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

UGA_Alex_E::UGA_Alex_E()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.E"));
	AbilityTags.AddTag(AbilityTag);
	ActivationOwnedTags.AddTag(AbilityTag);

	// E 활성 중 State.Spinning 태그 부여 — 시각/AI 차단에 활용 가능.
	ActivationOwnedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.Spinning")));

	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.HitReact")));

	CooldownGameplayEffectClass = UGE_Cooldown_Alex_E::StaticClass();
}

void UGA_Alex_E::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 상태 초기화
	SpinTickCount = 0;

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

	// 3. 회전 Timer 시작 — 0.5초 간격, 6회 (총 3s)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpinTimerHandle,
			this, &UGA_Alex_E::OnSpinTick,
			SpinTickInterval, /*bLoop=*/true, /*FirstDelay=*/SpinTickInterval);
	}

	// 4. 몽타주 재생 (회전 모션)
	AAOSCharacter* Char = Cast<AAOSCharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* SkillMontage = Char
		? Char->GetSkillMontage(FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Alex.E")))
		: nullptr;

	if (!SkillMontage)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GA_Alex_E] SkillMontage 없음 — Timer 만으로 진행"));
		// 몽타주 없어도 timer 6 tick 완료까지 진행 — StopSpinTimer 에서 EndAbility
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
		MontageTask->OnCompleted.AddDynamic(this, &UGA_Alex_E::OnMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_Alex_E::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_Alex_E::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_Alex_E::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}
}

void UGA_Alex_E::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// EndAbility (정상 / interrupt / cancel 모두) 시 Timer 정리 — 안전장치
	StopSpinTimer();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Alex_E::OnSpinTick()
{
	SpinTickCount++;

	if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
	{
		StopSpinTimer();
		return;
	}

	AAOSCharacter* SourceChar = Cast<AAOSCharacter>(CurrentActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!SourceChar || !SourceASC)
	{
		StopSpinTimer();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		StopSpinTimer();
		return;
	}

	// 반경 250 OverlapMultiByChannel (Pawn 채널)
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SpinRadius);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpinAoE), false);
	QueryParams.AddIgnoredActor(SourceChar);

	const FVector Origin = SourceChar->GetActorLocation();
	World->OverlapMultiByChannel(
		Overlaps, Origin, FQuat::Identity,
		ECollisionChannel::ECC_Pawn, Sphere, QueryParams);

	const EAOSTeam SourceTeam = SourceChar->GetTeam();
	int32 HitsApplied = 0;

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* OverlapActor = Result.GetActor();
		if (!OverlapActor || OverlapActor == SourceChar) continue;

		// AOSCharacter 만 대상 (구조물 제외)
		AAOSCharacter* TargetChar = Cast<AAOSCharacter>(OverlapActor);
		if (!TargetChar || !TargetChar->IsAlive()) continue;

		// 같은 팀 제외
		if (TargetChar->GetTeam() == SourceTeam) continue;

		IAbilitySystemInterface* AsiTarget = Cast<IAbilitySystemInterface>(TargetChar);
		if (!AsiTarget || !AsiTarget->GetAbilitySystemComponent()) continue;

		// GE_Damage 적용 (SetByCaller Data.Damage = 50)
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(SourceChar);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
			UGE_Damage::StaticClass(), 1.0f, Ctx);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(FName("Data.Damage")),
				SpinDamagePerTick);
			SourceASC->ApplyGameplayEffectSpecToTarget(
				*Spec.Data.Get(), AsiTarget->GetAbilitySystemComponent());
			HitsApplied++;
		}
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("[GA_Alex_E] SpinTick %d/%d — %d 적에 데미지 %.0f 적용"),
		SpinTickCount, MaxSpinTicks, HitsApplied, SpinDamagePerTick);

	if (SpinTickCount >= MaxSpinTicks)
	{
		StopSpinTimer();
	}
}

void UGA_Alex_E::StopSpinTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpinTimerHandle);
	}
}

void UGA_Alex_E::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Alex_E::OnMontageBlendOut() {}

void UGA_Alex_E::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Alex_E::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
