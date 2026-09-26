// 선호 행동(캐릭터별 AI 개성) — StateTree task/condition
//
// Phase 6 의 공용 노드(AOSStateTreeTasks/Conditions)와 분리한 이유:
//   1) 캐릭터 전용 트리(ST_KwangAI 등)만 쓰는 노드라 공용 트리에 영향 0.
//   2) 모든 파라미터(반전 플래그 포함)를 **InstanceData** 에 둔다.
//      unreal-statetree MCP 는 InstanceData 속성만 설정할 수 있어서, 공용 노드처럼
//      bInvert 가 노드 본체에 있으면 MCP 로 저작한 트리에서 값을 바꿀 방법이 없다.
//
// 전투 메모리(기본공격 횟수·공격 종료 시각·옆걸음 방향)는 AAOSAIController 가 보관한다.
// 설계/테스트 기록: Guides/03_Implementation/KWANG_AI_STATETREE.md

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "AOSStateTreeBehaviorNodes.generated.h"

class AAIController;

// =============================================================================
// Conditions
// =============================================================================

// -----------------------------------------------------------------------------
// FStateTreeCond_OwnerHasTag
// 캐릭터 ASC 가 Tag 를 보유하면 SUCCESS (bInvert=true 면 보유하지 않을 때).
// 스킬 준비 = Cooldown 태그 없음(bInvert=true), 스킬 쿨 중 = Cooldown 태그 있음(bInvert=false).
// 공용 HasCooldownTag 와 같은 판정이지만 bInvert 를 MCP 로 설정할 수 있다.
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSCond_OwnerHasTagInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Tag;

	/** true = 태그가 "없을 때" 통과 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

USTRUCT(DisplayName = "Owner Has Tag", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeCond_OwnerHasTag : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_OwnerHasTagInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeCond_EnemyCountInRadius
// Radius(중심 간 거리) 안 살아있는 적 수가 [MinCount, MaxCount] 이면 SUCCESS.
// MaxCount < 0 = 상한 없음. 예) "딱 1명" = Min 1/Max 1, "2명 이상" = Min 2/Max -1, "아무도 없음" = Min 0/Max 0.
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSCond_EnemyCountInRadiusInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float Radius = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0"))
	int32 MinCount = 1;

	/** -1 = 상한 없음 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 MaxCount = -1;
};

USTRUCT(DisplayName = "Enemy Count In Radius", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeCond_EnemyCountInRadius : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_EnemyCountInRadiusInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeCond_BasicAttackCountAtLeast
// 마지막 리셋 이후 기본공격 횟수가 Count 이상이면 SUCCESS.
// 리셋은 FStateTreeTask_ActivateAbilityAndResetCount 가 스킬 발동 성공 시 수행.
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSCond_BasicAttackCountAtLeastInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0"))
	int32 Count = 3;
};

USTRUCT(DisplayName = "Basic Attack Count At Least", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeCond_BasicAttackCountAtLeast : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_BasicAttackCountAtLeastInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeCond_InPostAttackWindow
// 직전 기본공격이 끝난 지 WindowSeconds 가 안 지났으면 SUCCESS (공격 중이면 FAIL).
// 공격 몽타주가 쿨다운보다 길어 "쿨다운 중 틈"이 0 인 캐릭터에게 AI 가 일부러 틈을 만들 때 쓴다 (길수록 DPS 손해).
// ※ ST_KwangAI 는 현재 미사용 — Kwang 은 BasicAttackCooldownOverride(5초)로 틈을 만들고,
//   옆걸음 조건은 "OwnerHasTag(Cooldown.Attack.Basic) + OwnerHasTag(Ability.Attack.Basic, 반전)".
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSCond_InPostAttackWindowInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float WindowSeconds = 0.5f;
};

USTRUCT(DisplayName = "In Post-Attack Window", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeCond_InPostAttackWindow : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_InPostAttackWindowInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeCond_FriendlyTowerDistance
// 가장 가까운 아군 타워(없으면 아군 CC)까지의 거리 판정.
// bWithin=true → Radius 안이면 SUCCESS / false → Radius 밖이면 SUCCESS.
// 아군 구조물이 하나도 없으면 항상 FAIL (후퇴할 곳이 없음).
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSCond_FriendlyTowerDistanceInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float Radius = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bWithin = false;
};

USTRUCT(DisplayName = "Friendly Tower Distance", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeCond_FriendlyTowerDistance : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_FriendlyTowerDistanceInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeCond_BehaviorCooldownReady
// BehaviorKey 행동이 한 번도 안 쓰였거나, 마지막 사용 후 CooldownSeconds 가 지났으면 SUCCESS.
// 사용 기록은 해당 행동의 태스크가 남긴다 (예: RetreatToFriendlyTower 가 도착 시 "Retreat").
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSCond_BehaviorCooldownReadyInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName BehaviorKey = FName("Retreat");

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 60.0f;
};

USTRUCT(DisplayName = "Behavior Cooldown Ready", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeCond_BehaviorCooldownReady : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_BehaviorCooldownReadyInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// =============================================================================
// Tasks
// =============================================================================

// -----------------------------------------------------------------------------
// FStateTreeTask_SelectLowestMaxHealthEnemy
// Radius 안 적 중 **최대 체력**이 가장 낮은 적을 CurrentTarget 으로 → RUNNING. 없으면 FAILED.
// (현재 체력이 아니라 최대 체력 — "원래 약한 챔피언"을 먼저 노린다.)
// 부모 state 에 두고 자식(Strafe/Engage)이 CurrentTarget 을 쓴다.
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSTask_SelectLowestMaxHealthEnemyInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float Radius = 500.0f;
};

USTRUCT(DisplayName = "Select Lowest Max-Health Enemy", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeTask_SelectLowestMaxHealthEnemy : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_SelectLowestMaxHealthEnemyInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeTask_StrafeAroundTarget
// 타겟을 바라본 채 타겟 둘레로 StepDistance 만큼 옆걸음 — 공격 간격마다 1걸음, 좌우 교대.
// 사거리 밖이면 접근. 항상 RUNNING (끝내는 건 InPostAttackWindow 조건 + 부모 OnTick 재선택).
// ExitState 에서 회전 모드/포커스 원복.
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSTask_StrafeAroundTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 한 걸음의 옆 이동 거리 (원호 길이, cm) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float StepDistance = 150.0f;

	/** true = CurrentTarget(적 캐릭터) 기준, false = 현재 웨이포인트 구조물 기준 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bTargetCurrentEnemy = true;
};

USTRUCT(DisplayName = "Strafe Around Target", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeTask_StrafeAroundTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FStateTreeTask_StrafeAroundTarget()
	{
		// 부모의 OnTick→Root 재선택마다 Exit/Enter 가 반복되면 회전 모드가 매 tick 토글된다.
		// 같은 state 가 다시 선택되면 그대로 Tick 만 받도록 한다.
		bShouldStateChangeOnReselect = false;
	}

	using FInstanceDataType = FAOSTask_StrafeAroundTargetInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeTask_RetreatToFriendlyTower
// 가장 가까운 아군 타워(없으면 CC)의 StopRadius 안까지 **한 번에** 후퇴 → 도착하면 SUCCEEDED.
// 도착 시 CooldownKey 행동 사용을 기록 → BehaviorCooldownReady 조건이 그 시점부터 쿨다운을 센다.
// 후퇴는 끝까지 커밋(도중 재평가 없음) — 긴급 스킬이 후퇴 중에 다시 끼어들지 않게.
// 가던 타워가 부서지면 다음으로 가까운 타워로 목표를 갈아탄다.
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSTask_RetreatToFriendlyTowerInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 타워 중심에서 이 거리 안에 들어오면 후퇴 완료 (그룹의 FriendlyTowerDistance.Radius 와 맞출 것) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float StopRadius = 200.0f;

	/** 도착 시 기록할 행동 쿨다운 키 (None = 기록 안 함) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName CooldownKey = FName("Retreat");

	// ── 런타임 ──
	UPROPERTY()
	TObjectPtr<AActor> RetreatTarget = nullptr;
};

USTRUCT(DisplayName = "Retreat To Friendly Tower", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeTask_RetreatToFriendlyTower : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_RetreatToFriendlyTowerInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// -----------------------------------------------------------------------------
// FStateTreeTask_ActivateAbilityAndResetCount
// 공용 ActivateAbilityByTag 와 같은 동작(발동 + State.Rooted 동안 RUNNING 홀드)에,
// 발동 성공 시 기본공격 카운트를 0 으로 리셋하는 것만 더했다.
// Kwang Q/W 의 "일반 공격 3회 후 사용" 을 매 스킬마다 다시 쌓게 만든다.
// -----------------------------------------------------------------------------
USTRUCT()
struct FAOSTask_ActivateAbilityAndResetCountInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bResetBasicAttackCount = true;
};

USTRUCT(DisplayName = "Activate Ability (Reset Attack Count)", Category = "AOS|AI|Behavior")
struct TDPROJECT_API FStateTreeTask_ActivateAbilityAndResetCount : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSTask_ActivateAbilityAndResetCountInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
