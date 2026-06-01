// Phase 4+: 데이터 주도 스킬 쿨다운 GE 의 공통 부모.
// BP child 가 UTargetTagsGameplayEffectComponent 로 자기 cooldown 태그(Cooldown.Skill.*)를 추가하는 패턴.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_SkillCooldown_Base.generated.h"

/**
 * 모든 스킬 쿨다운 GE 의 공통 부모.
 *
 * - DurationPolicy: HasDuration
 * - DurationMagnitude: SetByCaller(Data.Duration) — UGA_SkillBase 가 CooldownDuration 으로 set
 * - GrantedTag: BP child 에서 Components → TargetTagsGameplayEffectComponent 로 추가
 *
 * 사용 예 (BP):
 *   BP_GE_Cooldown_Alex_Q : UGE_SkillCooldown_Base
 *     → Components → Add → "Target Tags Gameplay Effect Component"
 *     → Inherited Tags → Added: "Cooldown.Skill.Alex.Q"
 *
 * 이전 패턴은 캐릭터별 4개 C++ 쿨다운 GE 클래스였으나, 데이터 주도 마이그레이션으로 BP 자산 4개로 대체됨.
 */
UCLASS()
class TDPROJECT_API UGE_SkillCooldown_Base : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_SkillCooldown_Base();
};
