// Phase 6: State Tree Conditions for AOS AI

#include "AOSStateTreeConditions.h"
#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "GAS/AOSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

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
