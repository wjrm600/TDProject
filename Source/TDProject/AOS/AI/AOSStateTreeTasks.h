// Phase 6: State Tree Tasks for AOS AI
// AIController 의 행동 함수들을 ST task 로 노출

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "AOSStateTreeTasks.generated.h"

class AAIController;
class AAOSAIController;
class AAOSCharacter;
class AAOSStructure;

// =============================================================================
// FStateTreeTask_FindNearestEnemy
// 적 캐릭터 검색 → 발견 시 RUNNING (state 진입 유지), 못 찾으면 FAILED
// EnterState 에서 한 번만 검색, AIController 의 CurrentTarget 에 저장
// =============================================================================

USTRUCT()
struct FAOSTask_FindNearestEnemyInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Find Nearest Enemy", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeTask_FindNearestEnemy : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_FindNearestEnemyInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// =============================================================================
// FStateTreeTask_MoveToCurrentTarget
// AIController 의 CurrentTarget 으로 MoveToActor — 사거리 도달 시 SUCCESS
// =============================================================================

USTRUCT()
struct FAOSTask_MoveToCurrentTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Move To Current Target", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeTask_MoveToCurrentTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_MoveToCurrentTargetInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// =============================================================================
// FStateTreeTask_MoveToCurrentWaypoint
// AIController 의 CurrentMoveTarget (= 다음 웨이포인트 위치) 으로 이동
// 도착 시 SUCCESS, 이동 중 RUNNING
// =============================================================================

USTRUCT()
struct FAOSTask_MoveToCurrentWaypointInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Move To Current Waypoint", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeTask_MoveToCurrentWaypoint : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_MoveToCurrentWaypointInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// =============================================================================
// FStateTreeTask_AdvanceWaypoint
// AIController 의 CurrentWaypointIndex 를 +1, 다음 목표 위치 갱신 → SUCCESS 즉시
// =============================================================================

USTRUCT()
struct FAOSTask_AdvanceWaypointInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Advance Waypoint", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeTask_AdvanceWaypoint : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_AdvanceWaypointInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

// =============================================================================
// FStateTreeTask_SendAttackEvent
// 캐릭터의 ASC 에 GameplayEvent("Ability.Attack.Basic") 트리거
// Target 을 EventData 에 담아 GA_Attack 활성화
// 캐릭터/구조물 둘 다 가능 (bUseCurrentTargetCharacter 로 선택)
// =============================================================================

USTRUCT()
struct FAOSTask_SendAttackEventInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Send Attack Event", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeTask_SendAttackEvent : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_SendAttackEventInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/** true = AIController->FindNearestEnemy() 를 타겟으로, false = 현재 웨이포인트 구조물을 타겟으로 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bTargetCurrentEnemy = true;
};

// =============================================================================
// FStateTreeTask_ActivateAbilityByTag
// 캐릭터의 ASC 에 지정 GameplayTag 매칭 ability 활성화 시도 (Phase 4 스킬 용)
// =============================================================================

USTRUCT()
struct FAOSTask_ActivateAbilityByTagInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag AbilityTag;
};

USTRUCT(DisplayName = "Activate Ability By Tag", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeTask_ActivateAbilityByTag : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_ActivateAbilityByTagInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
