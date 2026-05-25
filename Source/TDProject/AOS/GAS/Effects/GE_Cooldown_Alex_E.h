// Phase 4: Alex E (Judgment) 쿨다운 GE — 10초 고정 Duration.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Alex_E.generated.h"

/**
 * Alex E (Judgment AoE 회전) 쿨다운.
 * GrantedTag: Cooldown.Skill.Alex.E
 * Duration: 10.0s
 */
UCLASS()
class TDPROJECT_API UGE_Cooldown_Alex_E : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Cooldown_Alex_E();
};
