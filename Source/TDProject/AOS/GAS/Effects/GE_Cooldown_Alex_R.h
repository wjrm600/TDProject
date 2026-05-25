// Phase 4: Alex R (DemacianJustice — 궁극기) 쿨다운 GE — 90초 고정 Duration.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Alex_R.generated.h"

/**
 * Alex R (DemacianJustice — 궁극기) 쿨다운.
 * GrantedTag: Cooldown.Skill.Alex.R
 * Duration: 90.0s (궁극기 — 매우 긴 쿨다운)
 */
UCLASS()
class TDPROJECT_API UGE_Cooldown_Alex_R : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Cooldown_Alex_R();
};
