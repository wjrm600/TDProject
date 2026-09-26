// 선호 행동(캐릭터별 AI 개성) — StateTree task/condition 구현

#include "AOSStateTreeBehaviorNodes.h"
#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "AbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"
#include "Navigation/PathFollowingComponent.h"	// EPathFollowingStatus 값 정의 (AIController.h 는 전방 선언만)

namespace AOSBehaviorNodes
{
	static AAOSAIController* ResolveAI(const TObjectPtr<AAIController>& AIController)
	{
		return Cast<AAOSAIController>(AIController);
	}

	static UAbilitySystemComponent* ResolveASC(const TObjectPtr<AAIController>& AIController)
	{
		const AAOSAIController* AOSAI = ResolveAI(AIController);
		const AAOSCharacter* Char = AOSAI ? Cast<AAOSCharacter>(AOSAI->GetPawn()) : nullptr;
		return Char ? Char->GetAbilitySystemComponent() : nullptr;
	}

	static bool IsRooted(const UAbilitySystemComponent* ASC)
	{
		static const FGameplayTag RootedTag = FGameplayTag::RequestGameplayTag(FName("State.Rooted"));
		return ASC && ASC->HasMatchingGameplayTag(RootedTag);
	}
}

// =============================================================================
// Conditions
// =============================================================================

bool FStateTreeCond_OwnerHasTag::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UAbilitySystemComponent* ASC = AOSBehaviorNodes::ResolveASC(InstanceData.AIController);
	if (!ASC || !InstanceData.Tag.IsValid()) return false;

	return ASC->HasMatchingGameplayTag(InstanceData.Tag) != InstanceData.bInvert;
}

bool FStateTreeCond_EnemyCountInRadius::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	if (!AOSAI) return false;

	const int32 Count = AOSAI->CountEnemiesInRadius(InstanceData.Radius);
	if (Count < InstanceData.MinCount) return false;
	return InstanceData.MaxCount < 0 || Count <= InstanceData.MaxCount;
}

bool FStateTreeCond_BasicAttackCountAtLeast::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	return AOSAI && AOSAI->GetBasicAttackCount() >= InstanceData.Count;
}

bool FStateTreeCond_InPostAttackWindow::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	return AOSAI && AOSAI->IsInPostAttackWindow(InstanceData.WindowSeconds);
}

bool FStateTreeCond_FriendlyTowerDistance::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	const APawn* Pawn = AOSAI ? AOSAI->GetPawn() : nullptr;
	if (!Pawn) return false;

	const AAOSStructure* Structure = AOSAI->FindNearestFriendlyStructure();
	if (!Structure) return false;

	const bool bInside = FVector::Dist2D(Pawn->GetActorLocation(), Structure->GetActorLocation()) <= InstanceData.Radius;
	return bInside == InstanceData.bWithin;
}

bool FStateTreeCond_BehaviorCooldownReady::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	return AOSAI && AOSAI->IsBehaviorReady(InstanceData.BehaviorKey, InstanceData.CooldownSeconds);
}

// =============================================================================
// FStateTreeTask_SelectLowestMaxHealthEnemy
// =============================================================================

EStateTreeRunStatus FStateTreeTask_SelectLowestMaxHealthEnemy::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	AAOSCharacter* Target = AOSAI->FindLowestMaxHealthEnemyInRadius(InstanceData.Radius);
	if (!Target) return EStateTreeRunStatus::Failed;

	if (AOSAI->GetCurrentTargetCharacter() != Target)
	{
		UE_LOG(LogTemp, Log, TEXT("[ST/Behavior] %s 집중 타겟 → %s (MaxHP %.0f, 반경 %.0f)"),
			*GetNameSafe(AOSAI->GetPawn()), *Target->GetName(), Target->GetMaxHealth(), InstanceData.Radius);
	}
	AOSAI->SetCurrentTarget(Target);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_SelectLowestMaxHealthEnemy::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	if (!AOSAI) return EStateTreeRunStatus::Failed;

	// 타겟이 죽으면 반경 안에서 다시 고른다 — 없으면 state 종료
	const AAOSCharacter* Current = AOSAI->GetCurrentTargetCharacter();
	if (!Current || !Current->IsAlive())
	{
		AAOSCharacter* NewTarget = AOSAI->FindLowestMaxHealthEnemyInRadius(InstanceData.Radius);
		if (!NewTarget) return EStateTreeRunStatus::Failed;
		AOSAI->SetCurrentTarget(NewTarget);
	}
	return EStateTreeRunStatus::Running;
}

// =============================================================================
// FStateTreeTask_StrafeAroundTarget
// =============================================================================

namespace AOSBehaviorNodes
{
	static AActor* ResolveStrafeTarget(AAOSAIController* AOSAI, bool bTargetCurrentEnemy)
	{
		if (!AOSAI) return nullptr;
		if (bTargetCurrentEnemy)
		{
			AAOSCharacter* Enemy = AOSAI->GetCurrentTargetCharacter();
			return (Enemy && Enemy->IsAlive()) ? Enemy : nullptr;
		}
		return AOSAI->GetCurrentWaypointStructure();
	}
}

EStateTreeRunStatus FStateTreeTask_StrafeAroundTarget::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	AActor* Target = AOSBehaviorNodes::ResolveStrafeTarget(AOSAI, InstanceData.bTargetCurrentEnemy);
	if (!Target) return EStateTreeRunStatus::Failed;

	AOSAI->UpdateStrafe(Target, InstanceData.StepDistance);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_StrafeAroundTarget::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	AActor* Target = AOSBehaviorNodes::ResolveStrafeTarget(AOSAI, InstanceData.bTargetCurrentEnemy);
	if (!Target) return EStateTreeRunStatus::Failed;

	AOSAI->UpdateStrafe(Target, InstanceData.StepDistance);
	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_StrafeAroundTarget::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController))
	{
		AOSAI->EndStrafe();
	}
}

// =============================================================================
// FStateTreeTask_RetreatToFriendlyTower
// =============================================================================

namespace AOSBehaviorNodes
{
	static EStateTreeRunStatus FinishRetreat(AAOSAIController* AOSAI, FAOSTask_RetreatToFriendlyTowerInstanceData& Data,
		const AAOSStructure* Structure, float Dist, const TCHAR* Reason)
	{
		AOSAI->MarkBehaviorUsed(Data.CooldownKey);
		UE_LOG(LogTemp, Log, TEXT("[ST/Behavior] %s 후퇴 완료(%s) → %s 거리 %.0f. '%s' 쿨다운 시작"),
			*GetNameSafe(AOSAI->GetPawn()), Reason, *GetNameSafe(Structure), Dist, *Data.CooldownKey.ToString());
		// RetreatTarget 은 비우지 않는다 — 완료 직후 Tick 이 한 번 더 와도 "목표 변경"으로 오인해
		// 이동을 재요청하지 않게. (새 후퇴는 EnterState 가 초기화)
		return EStateTreeRunStatus::Succeeded;
	}

	// 후퇴 한 틱 — 도착이면 완료(+쿨다운 기록), 아니면 목표 타워로 가는 이동을 유지
	static EStateTreeRunStatus UpdateRetreat(FAOSTask_RetreatToFriendlyTowerInstanceData& Data)
	{
		AAOSAIController* AOSAI = ResolveAI(Data.AIController);
		const APawn* Pawn = AOSAI ? AOSAI->GetPawn() : nullptr;
		if (!Pawn) return EStateTreeRunStatus::Failed;

		AAOSStructure* Structure = AOSAI->FindNearestFriendlyStructure();
		if (!Structure) return EStateTreeRunStatus::Failed;

		const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), Structure->GetActorLocation());
		if (Dist <= Data.StopRadius)
		{
			return FinishRetreat(AOSAI, Data, Structure, Dist, TEXT("반경 도달"));
		}

		const bool bTargetChanged = (Data.RetreatTarget != Structure);
		const bool bIdle = (AOSAI->GetMoveStatus() == EPathFollowingStatus::Idle);

		// 경로 끝까지 왔는데도 StopRadius 밖 = 타워 중심이 내비 밖이라 더 못 가는 자리 → 여기서 완료
		if (bIdle && !bTargetChanged)
		{
			return FinishRetreat(AOSAI, Data, Structure, Dist, TEXT("경로 끝"));
		}

		// 첫 틱 또는 가던 타워가 부서져 목표가 바뀌면 이동 (재)요청
		if (bTargetChanged)
		{
			if (!AOSAI->RequestMoveStep(Structure->GetActorLocation(), Data.StopRadius * 0.5f))
			{
				UE_LOG(LogTemp, Warning, TEXT("[ST/Behavior] %s 후퇴 이동 요청 실패 → %s"),
					*GetNameSafe(Pawn), *Structure->GetName());
				return EStateTreeRunStatus::Failed;
			}
			UE_LOG(LogTemp, Log, TEXT("[ST/Behavior] %s 후퇴 %s → %s (거리 %.0f)"),
				*GetNameSafe(Pawn), Data.RetreatTarget ? TEXT("목표 변경") : TEXT("시작"),
				*Structure->GetName(), Dist);
			Data.RetreatTarget = Structure;
		}
		return EStateTreeRunStatus::Running;
	}
}

EStateTreeRunStatus FStateTreeTask_RetreatToFriendlyTower::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RetreatTarget = nullptr;
	return AOSBehaviorNodes::UpdateRetreat(InstanceData);
}

EStateTreeRunStatus FStateTreeTask_RetreatToFriendlyTower::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return AOSBehaviorNodes::UpdateRetreat(Context.GetInstanceData(*this));
}

// =============================================================================
// FStateTreeTask_ActivateAbilityAndResetCount
// =============================================================================

EStateTreeRunStatus FStateTreeTask_ActivateAbilityAndResetCount::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAOSAIController* AOSAI = AOSBehaviorNodes::ResolveAI(InstanceData.AIController);
	UAbilitySystemComponent* ASC = AOSBehaviorNodes::ResolveASC(InstanceData.AIController);
	if (!AOSAI || !ASC || !InstanceData.AbilityTag.IsValid()) return EStateTreeRunStatus::Failed;

	if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(InstanceData.AbilityTag)))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.bResetBasicAttackCount)
	{
		UE_LOG(LogTemp, Log, TEXT("[ST/Behavior] %s 스킬 %s 발동 — 기본공격 카운트 %d → 0"),
			*GetNameSafe(AOSAI->GetPawn()), *InstanceData.AbilityTag.ToString(), AOSAI->GetBasicAttackCount());
		AOSAI->ResetBasicAttackCount();
	}

	// 공용 ActivateAbilityByTag 와 동일: 이동 불가 스킬(State.Rooted)은 root 가 풀릴 때까지 홀드
	return AOSBehaviorNodes::IsRooted(ASC) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FStateTreeTask_ActivateAbilityAndResetCount::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UAbilitySystemComponent* ASC = AOSBehaviorNodes::ResolveASC(InstanceData.AIController);
	return AOSBehaviorNodes::IsRooted(ASC) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}
