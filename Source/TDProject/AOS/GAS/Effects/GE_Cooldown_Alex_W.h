// Phase 4: Alex W (Courage) 쿨다운 GE — 15초 고정 Duration.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Alex_W.generated.h"

/**
 * Alex W (Courage) 쿨다운.
 * GrantedTag: Cooldown.Skill.Alex.W
 * Duration: 15.0s
 */
UCLASS()
class TDPROJECT_API UGE_Cooldown_Alex_W : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Cooldown_Alex_W();
};
