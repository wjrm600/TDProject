// Phase 4: 다음 기본공격 강화 마커 GE — Modifier 없음, GrantedTag(State.EnhancedAttack) 만.
// 사용처: Alex Q (DecisiveStrike) 후속 효과. GA_Attack 이 데미지 적용 시 태그 확인 + 1.5배 + 효과 제거.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_EnhancedAttack.generated.h"

/**
 * 다음 기본공격 강화 Duration GE.
 *
 * - Modifier 없음 (태그만 부여하는 마커 효과)
 * - GrantedTag: State.EnhancedAttack
 * - Duration: 3.0s (Q 효과 지속 시간과 일치)
 * - GA_Attack 이 데미지 적용 시점에 이 태그를 보고 데미지 1.5배 + 효과 제거
 *   (RemoveActiveEffectsWithGrantedTags 로 1회 소비)
 */
UCLASS()
class TDPROJECT_API UGE_EnhancedAttack : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_EnhancedAttack();
};
