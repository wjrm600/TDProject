// Phase 6: State Tree Tasks for AOS AI

#include "AOSStateTreeTasks.h"
#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "GAS/AOSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "StateTreeExecutionContext.h"

// =============================================================================
// FStateTreeTask_FindNearestEnemy
// =============================================================================
EStateTreeRunStatus FStateTreeTask_FindNearestEnemy::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("[ST/FindNearestEnemy] AIController context is NULL — schema/binding 실패"));
		return EStateTreeRunStatus::Failed;
	}

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI)
	{
		UE_LOG(LogTemp, Error, TEXT("[ST/FindNearestEnemy] AIController not AAOSAIController — class mismatch"));
		return EStateTreeRunStatus::Failed;
	}

	AAOSCharacter* Enemy = AOSAI->FindNearestEnemy();
	UE_LOG(LogTemp, Verbose, TEXT("[ST/FindNearestEnemy] EnterState — Enemy=%s"),
		Enemy ? *Enemy->GetName() : TEXT("null"));
	if (!Enemy) return EStateTreeRunStatus::Failed;

	AOSAI->SetCurrentTarget(Enemy);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_FindNearestEnemy::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return EStateTreeRunStatus::Failed;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	// 적이 죽거나 사라진 경우 새로 탐색 — 발견 못 하면 state 종료
	AAOSCharacter* CurrentTarget = AOSAI->GetCurrentTargetCharacter();
	if (!CurrentTarget || !CurrentTarget->IsAlive())
	{
		AAOSCharacter* NewEnemy = AOSAI->FindNearestEnemy();
		if (!NewEnemy) return EStateTreeRunStatus::Failed;
		AOSAI->SetCurrentTarget(NewEnemy);
	}
	return EStateTreeRunStatus::Running;
}

// =============================================================================
// FStateTreeTask_MoveToCurrentTarget
// =============================================================================
EStateTreeRunStatus FStateTreeTask_MoveToCurrentTarget::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("[ST/MoveToCurrentTarget] AIController context is NULL"));
		return EStateTreeRunStatus::Failed;
	}

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	UE_LOG(LogTemp, Verbose, TEXT("[ST/MoveToCurrentTarget] EnterState"));
	AOSAI->RequestMoveToCurrentTarget();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_MoveToCurrentTarget::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return EStateTreeRunStatus::Failed;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	// 타겟 사라지면 FAILED → state 종료 → root 재선택
	AAOSCharacter* CurrentTarget = AOSAI->GetCurrentTargetCharacter();
	if (!CurrentTarget || !CurrentTarget->IsAlive())
	{
		return EStateTreeRunStatus::Failed;
	}

	// 사거리 내면 정지하고 RUNNING 유지 (Succeeded 반환 시 state 종료 → oscillation 위험)
	// SendAttackEvent task 가 같은 state 안에서 공격 처리
	if (AOSAI->IsCurrentTargetInAttackRange())
	{
		AOSAI->StopMovement();
		return EStateTreeRunStatus::Running;
	}

	// 매 tick 이동 갱신 (목표가 움직일 수 있음)
	AOSAI->RequestMoveToCurrentTarget();
	return EStateTreeRunStatus::Running;
}

// =============================================================================
// FStateTreeTask_MoveToCurrentWaypoint
// =============================================================================
EStateTreeRunStatus FStateTreeTask_MoveToCurrentWaypoint::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("[ST/MoveToCurrentWaypoint] AIController context is NULL — schema 또는 binding 문제"));
		return EStateTreeRunStatus::Failed;
	}

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	UE_LOG(LogTemp, Verbose, TEXT("[ST/MoveToCurrentWaypoint] EnterState — Pawn=%s"),
		AOSAI->GetPawn() ? *AOSAI->GetPawn()->GetName() : TEXT("null"));
	AOSAI->RequestMoveToCurrentWaypoint();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_MoveToCurrentWaypoint::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return EStateTreeRunStatus::Failed;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	// 목표 지점 도달 시 — 다음 웨이포인트로 자동 advance 후 RUNNING 유지
	// (state transition 에 의존하지 않고 task 안에서 큐 진행 처리)
	if (AOSAI->HasArrivedAtCurrentWaypoint())
	{
		AOSAI->StopMovement();
		AOSAI->AdvanceToNextWaypoint();
		// advance 후 다음 웨이포인트로 이동 시작 — RUNNING 유지로 같은 task 가 계속 흐름 담당
		AOSAI->RequestMoveToCurrentWaypoint();
		return EStateTreeRunStatus::Running;
	}

	// 매 tick 갱신 (NavMesh 가 자동 추적하지만 큐 변경 시 대응)
	AOSAI->RequestMoveToCurrentWaypoint();
	return EStateTreeRunStatus::Running;
}

// =============================================================================
// FStateTreeTask_AdvanceWaypoint
// =============================================================================
EStateTreeRunStatus FStateTreeTask_AdvanceWaypoint::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return EStateTreeRunStatus::Failed;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	AOSAI->AdvanceToNextWaypoint();
	return EStateTreeRunStatus::Succeeded;
}

// =============================================================================
// FStateTreeTask_SendAttackEvent
// =============================================================================
EStateTreeRunStatus FStateTreeTask_SendAttackEvent::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("[ST/SendAttackEvent] AIController context is NULL"));
		return EStateTreeRunStatus::Failed;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[ST/SendAttackEvent] EnterState — bTargetCurrentEnemy=%s"),
		bTargetCurrentEnemy ? TEXT("true") : TEXT("false"));

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	AAOSCharacter* SourceChar = Cast<AAOSCharacter>(AOSAI->GetPawn());
	if (!SourceChar) return EStateTreeRunStatus::Failed;

	UAbilitySystemComponent* ASC = SourceChar->GetAbilitySystemComponent();
	if (!ASC) return EStateTreeRunStatus::Failed;

	// 쿨다운 active 면 즉시 SUCCESS — state 가 다시 진입할 때까지 대기 (재선택 시 재시도)
	const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Attack.Basic"));
	if (ASC->HasMatchingGameplayTag(CooldownTag))
	{
		return EStateTreeRunStatus::Running; // tick 에서 cooldown 풀리면 trigger
	}

	// 타겟 결정
	AActor* TargetActor = nullptr;
	if (bTargetCurrentEnemy)
	{
		TargetActor = AOSAI->GetCurrentTargetCharacter();
	}
	else
	{
		TargetActor = AOSAI->GetCurrentWaypointStructure();
	}
	if (!TargetActor) return EStateTreeRunStatus::Failed;

	FGameplayEventData EventData;
	EventData.Target = TargetActor;
	EventData.Instigator = SourceChar;
	ASC->HandleGameplayEvent(
		FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic")),
		&EventData);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_SendAttackEvent::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	// state 가 active 인 동안 tick 마다 cooldown 체크 + 재시도
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return EStateTreeRunStatus::Failed;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	AAOSCharacter* SourceChar = Cast<AAOSCharacter>(AOSAI->GetPawn());
	if (!SourceChar) return EStateTreeRunStatus::Failed;

	UAbilitySystemComponent* ASC = SourceChar->GetAbilitySystemComponent();
	if (!ASC) return EStateTreeRunStatus::Failed;

	// 타겟 검증 — 사라지면 state 종료
	AActor* TargetActor = nullptr;
	if (bTargetCurrentEnemy)
	{
		AAOSCharacter* EnemyChar = AOSAI->GetCurrentTargetCharacter();
		if (!EnemyChar || !EnemyChar->IsAlive()) return EStateTreeRunStatus::Failed;
		TargetActor = EnemyChar;
	}
	else
	{
		AAOSStructure* Struct = AOSAI->GetCurrentWaypointStructure();
		if (!Struct || Struct->IsDestroyed()) return EStateTreeRunStatus::Failed;
		TargetActor = Struct;
	}

	// 쿨다운 active 면 RUNNING 유지 (다음 tick 에 재시도)
	const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Attack.Basic"));
	if (ASC->HasMatchingGameplayTag(CooldownTag))
	{
		return EStateTreeRunStatus::Running;
	}

	// 사거리 안 인지 확인 (밖이면 RUNNING — 외부 transition 이 다른 state 로 옮길 것)
	if (bTargetCurrentEnemy)
	{
		if (!AOSAI->IsCurrentTargetInAttackRange())
		{
			return EStateTreeRunStatus::Running;
		}
	}

	// 공격 발사
	FGameplayEventData EventData;
	EventData.Target = TargetActor;
	EventData.Instigator = SourceChar;
	ASC->HandleGameplayEvent(
		FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic")),
		&EventData);

	return EStateTreeRunStatus::Running;
}

// =============================================================================
// FStateTreeTask_ActivateAbilityByTag
// =============================================================================
EStateTreeRunStatus FStateTreeTask_ActivateAbilityByTag::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController || !InstanceData.AbilityTag.IsValid())
		return EStateTreeRunStatus::Failed;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	AAOSCharacter* SourceChar = Cast<AAOSCharacter>(AOSAI->GetPawn());
	if (!SourceChar) return EStateTreeRunStatus::Failed;

	UAbilitySystemComponent* ASC = SourceChar->GetAbilitySystemComponent();
	if (!ASC) return EStateTreeRunStatus::Failed;

	// 태그 매칭 ability 활성화 시도
	const FGameplayTagContainer TagContainer(InstanceData.AbilityTag);
	const bool bActivated = ASC->TryActivateAbilitiesByTag(TagContainer);

	if (!bActivated)
		return EStateTreeRunStatus::Failed;

	// 이동 불가 스킬: GA 가 ActivateAbility 내에서 State.Rooted 를 동기 부여(서버) →
	// 이미 태그가 붙어 있으면 RUNNING 으로 홀드 (AI 가 스킬 state 에 머무름).
	// 이동 가능 스킬(root 미부여): 즉시 Succeeded → Design B 대로 root 재선택.
	static const FGameplayTag RootedTag = FGameplayTag::RequestGameplayTag(FName("State.Rooted"));
	return ASC->HasMatchingGameplayTag(RootedTag)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FStateTreeTask_ActivateAbilityByTag::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
		return EStateTreeRunStatus::Succeeded;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	AAOSCharacter* SourceChar = AOSAI ? Cast<AAOSCharacter>(AOSAI->GetPawn()) : nullptr;
	UAbilitySystemComponent* ASC = SourceChar ? SourceChar->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
		return EStateTreeRunStatus::Succeeded;

	// State.Rooted 가 풀리면(GE_Rooted 만료 = 몽타주/회전 종료) state 완료 → root 재선택.
	// GE_Rooted 는 고정 Duration 이라 반드시 만료 → 무한 홀드 없음.
	static const FGameplayTag RootedTag = FGameplayTag::RequestGameplayTag(FName("State.Rooted"));
	return ASC->HasMatchingGameplayTag(RootedTag)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}
