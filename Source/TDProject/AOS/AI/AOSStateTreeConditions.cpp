// Phase 6: State Tree Conditions for AOS AI

#include "AOSStateTreeConditions.h"
#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "GAS/AOSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"

// =============================================================================
// FStateTreeCond_HasNearbyEnemy
// =============================================================================
bool FStateTreeCond_HasNearbyEnemy::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return bInvert; // false ^ bInvert

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return bInvert;

	const bool bHasEnemy = (AOSAI->FindNearestEnemy() != nullptr);
	return bHasEnemy ^ bInvert;
}

// =============================================================================
// FStateTreeCond_HealthBelowPct
// =============================================================================
bool FStateTreeCond_HealthBelowPct::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return bInvert;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return bInvert;

	AAOSCharacter* Char = Cast<AAOSCharacter>(AOSAI->GetPawn());
	if (!Char) return bInvert;

	const float Max = Char->GetMaxHealth();
	if (Max <= 0.f) return bInvert;

	const float Pct = Char->GetCurrentHealth() / Max;
	const bool bBelow = (Pct < InstanceData.Threshold);
	return bBelow ^ bInvert;
}

// =============================================================================
// FStateTreeCond_HasCooldownTag
// =============================================================================
bool FStateTreeCond_HasCooldownTag::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController || !InstanceData.Tag.IsValid()) return bInvert;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return bInvert;

	AAOSCharacter* Char = Cast<AAOSCharacter>(AOSAI->GetPawn());
	if (!Char) return bInvert;

	UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent();
	if (!ASC) return bInvert;

	const bool bHasTag = ASC->HasMatchingGameplayTag(InstanceData.Tag);
	return bHasTag ^ bInvert;
}

// =============================================================================
// FStateTreeCond_TargetInAttackRange
// =============================================================================
bool FStateTreeCond_TargetInAttackRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return false;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return false;

	AAOSCharacter* Char = Cast<AAOSCharacter>(AOSAI->GetPawn());
	if (!Char) return false;

	const FVector MyLoc = Char->GetActorLocation();
	const float EffectiveRange = AOSAI->GetEffectiveAttackRange();

	if (bUseCurrentTargetCharacter)
	{
		AAOSCharacter* Target = AOSAI->FindNearestEnemy();
		if (!Target) return false;
		return FVector::Dist(MyLoc, Target->GetActorLocation()) <= EffectiveRange;
	}
	else
	{
		AAOSStructure* Structure = AOSAI->GetCurrentWaypointStructure();
		if (!Structure || Structure->IsDestroyed()) return false;
		return FVector::Dist(MyLoc, Structure->GetActorLocation()) <= EffectiveRange;
	}
}

// =============================================================================
// FStateTreeCond_HasCurrentWaypointStructure
// =============================================================================
bool FStateTreeCond_HasCurrentWaypointStructure::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return bInvert;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return bInvert;

	AAOSStructure* Structure = AOSAI->GetCurrentWaypointStructure();
	const bool bHas = (Structure != nullptr && !Structure->IsDestroyed());
	return bHas ^ bInvert;
}

// =============================================================================
// FStateTreeCond_HasNearbyEnemies (Phase 4)
// =============================================================================
bool FStateTreeCond_HasNearbyEnemies::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return bInvert;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return bInvert;

	AAOSCharacter* Self = Cast<AAOSCharacter>(AOSAI->GetPawn());
	if (!Self || !Self->IsAlive()) return bInvert;

	UWorld* World = Self->GetWorld();
	if (!World) return bInvert;

	// 반경 Radius 내 Pawn 채널 overlap 검색
	TArray<FOverlapResult> Overlaps;
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(InstanceData.Radius);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HasNearbyEnemies), false);
	QueryParams.AddIgnoredActor(Self);

	World->OverlapMultiByChannel(
		Overlaps,
		Self->GetActorLocation(),
		FQuat::Identity,
		ECollisionChannel::ECC_Pawn,
		Sphere,
		QueryParams);

	const EAOSTeam SelfTeam = Self->GetTeam();
	int32 EnemyCount = 0;
	for (const FOverlapResult& Result : Overlaps)
	{
		AAOSCharacter* OtherChar = Cast<AAOSCharacter>(Result.GetActor());
		if (!OtherChar || !OtherChar->IsAlive()) continue;
		if (OtherChar->GetTeam() == SelfTeam) continue;
		EnemyCount++;
		if (EnemyCount >= InstanceData.MinCount)
		{
			break; // early exit — 충분히 모임
		}
	}

	const bool bHasEnough = (EnemyCount >= InstanceData.MinCount);
	return bHasEnough ^ bInvert;
}

// =============================================================================
// FStateTreeCond_TargetHealthBelowPct (Phase 4)
// =============================================================================
bool FStateTreeCond_TargetHealthBelowPct::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController) return bInvert;

	AAOSAIController* AOSAI = Cast<AAOSAIController>(InstanceData.AIController);
	if (!AOSAI) return bInvert;

	AAOSCharacter* Target = AOSAI->GetCurrentTargetCharacter();
	if (!Target || !Target->IsAlive()) return bInvert;

	const float Max = Target->GetMaxHealth();
	if (Max <= 0.0f) return bInvert;

	const float Pct = Target->GetCurrentHealth() / Max;
	const bool bBelow = (Pct < InstanceData.Threshold);
	return bBelow ^ bInvert;
}
