// Phase 4: 받는 데미지 감소 GE — Duration 2s, State.DamageShield 태그 부여.
// AOSAttributeSet::PostGameplayEffectExecute 가 이 태그 활성 시 LocalDamage *= 0.5 적용.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_DamageShield.generated.h"

/**
 * 받는 데미지 감소 Duration GE — Alex W (Courage) 후속 효과.
 *
 * - Modifier 없음 (태그만 부여하는 마커 효과; 실제 데미지 감소는 PostGameplayEffectExecute 가 처리)
 * - GrantedTag: State.DamageShield
 * - Duration: 2.0s
 * - 효과: AOSAttributeSet 의 데미지 적용 분기에서 LocalDamage *= (1 - 0.5) = 50% 감소
 * - Phase 4 단순화: 감소율은 hardcoded 0.5. 추후 SetByCaller("Data.DamageReductionPct") 또는
 *   AttributeSet 의 DamageReduction 속성 도입 검토.
 */
UCLASS()
class TDPROJECT_API UGE_DamageShield : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_DamageShield();
};
