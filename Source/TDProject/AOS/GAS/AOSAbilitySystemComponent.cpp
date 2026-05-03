// GAS Phase 1: AbilitySystemComponent wrapper

#include "AOSAbilitySystemComponent.h"

UAOSAbilitySystemComponent::UAOSAbilitySystemComponent()
{
	// 기본 ASC 설정 — Mixed Replication Mode 는 PlayerState 기반 ASC 에 적합.
	// AI 캐릭터(self-owned ASC) 는 Replicated 모드(기본값) 가 적합.
	SetIsReplicated(true);
}
