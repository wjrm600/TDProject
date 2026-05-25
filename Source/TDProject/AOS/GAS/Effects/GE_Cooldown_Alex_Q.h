// Phase 4: Alex Q (DecisiveStrike) 쿨다운 GE — 8초 고정 Duration.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Alex_Q.generated.h"

/**
 * Alex Q (DecisiveStrike) 쿨다운.
 * GrantedTag: Cooldown.Skill.Alex.Q — 다음 활성화 차단.
 * Duration: 8.0s (Phase 4 hardcoded — Phase 4+ 에서 캐릭터별 데이터 테이블 검토).
 */
UCLASS()
class TDPROJECT_API UGE_Cooldown_Alex_Q : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Cooldown_Alex_Q();
};
