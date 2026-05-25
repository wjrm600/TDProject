// Phase 6: State Tree Conditions for AOS AI
// AIController/캐릭터 상태를 검사해 transition 결정에 사용

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "AOSStateTreeConditions.generated.h"

class AAIController;
class AAOSAIController;
class AAOSCharacter;

// =============================================================================
// FStateTreeCond_HasNearbyEnemy
// AIController 가 EnemyDetectionRange 내에서 적 캐릭터를 찾을 수 있으면 SUCCESS
// =============================================================================

USTRUCT()
struct FAOSCond_HasNearbyEnemyInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Has Nearby Enemy", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeCond_HasNearbyEnemy : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_HasNearbyEnemyInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

// =============================================================================
// FStateTreeCond_HealthBelowPct
// 캐릭터의 Health/MaxHealth 비율이 Threshold 미만이면 SUCCESS
// =============================================================================

USTRUCT()
struct FAOSCond_HealthBelowPctInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.3f;
};

USTRUCT(DisplayName = "Health Below %", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeCond_HealthBelowPct : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_HealthBelowPctInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

// =============================================================================
// FStateTreeCond_HasCooldownTag
// 캐릭터의 ASC 가 지정 GameplayTag 를 보유하면 SUCCESS
// (예: "Cooldown.Skill.Heal" — 쿨다운 active 인지 체크)
// =============================================================================

USTRUCT()
struct FAOSCond_HasCooldownTagInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Tag;
};

USTRUCT(DisplayName = "Has Cooldown Tag", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeCond_HasCooldownTag : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_HasCooldownTagInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

// =============================================================================
// FStateTreeCond_TargetInAttackRange
// AIController 의 CurrentTarget (또는 다음 웨이포인트) 이
// effective AttackRange 내에 있으면 SUCCESS
// =============================================================================

USTRUCT()
struct FAOSCond_TargetInAttackRangeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Target In Attack Range", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeCond_TargetInAttackRange : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_TargetInAttackRangeInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** true = 현재 타겟 캐릭터(CurrentTarget)에 대한 거리, false = 다음 웨이포인트 구조물에 대한 거리 */
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bUseCurrentTargetCharacter = true;
};

// =============================================================================
// FStateTreeCond_HasCurrentWaypointStructure
// AIController 의 현재 웨이포인트가 아직 파괴 안 된 적 구조물이면 SUCCESS
// =============================================================================

USTRUCT()
struct FAOSCond_HasCurrentWaypointStructureInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

USTRUCT(DisplayName = "Has Current Waypoint Structure", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeCond_HasCurrentWaypointStructure : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_HasCurrentWaypointStructureInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

// =============================================================================
// FStateTreeCond_HasNearbyEnemies (Phase 4)
// 캐릭터 주변 Radius 안에 같은 채널(Pawn)의 적팀 캐릭터가 MinCount 명 이상이면 SUCCESS.
// Alex E (AoE) 사용 판단에 활용.
// =============================================================================

USTRUCT()
struct FAOSCond_HasNearbyEnemiesInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1"))
	int32 MinCount = 2;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float Radius = 300.0f;
};

USTRUCT(DisplayName = "Has Nearby Enemies (Count, Radius)", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeCond_HasNearbyEnemies : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_HasNearbyEnemiesInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

// =============================================================================
// FStateTreeCond_TargetHealthBelowPct (Phase 4)
// AIController 의 CurrentTarget 캐릭터의 Health/MaxHealth 가 Threshold 미만이면 SUCCESS.
// Alex R (처형) 사용 판단에 활용.
// =============================================================================

USTRUCT()
struct FAOSCond_TargetHealthBelowPctInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.3f;
};

USTRUCT(DisplayName = "Target Health Below %", Category = "AOS|AI")
struct TDPROJECT_API FStateTreeCond_TargetHealthBelowPct : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAOSCond_TargetHealthBelowPctInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};
