// GAS Phase 3: 기본 공격 쿨다운 GameplayEffect
// Duration 1초 동안 ASC 에 "Cooldown.Attack.Basic" 태그 부여 → GA_Attack 재활성 차단

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Attack.generated.h"

/**
 * Duration GE — 1초간 "Cooldown.Attack.Basic" 태그 부여.
 * GA_Attack 의 CooldownGameplayEffectClass 로 지정되어 있어
 * CommitAbility 시점에 자동 적용된다.
 */
UCLASS()
class TDPROJECT_API UGE_Cooldown_Attack : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Cooldown_Attack();
};
