// GAS Phase 1: AbilitySystemComponent wrapper
// 후속 Phase 의 확장 지점 (현재는 빈 wrapper)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AOSAbilitySystemComponent.generated.h"

/**
 * AOS 게임용 AbilitySystemComponent.
 * 캐릭터/구조물이 자체 소유 (PlayerState 미사용 — AI 캐릭터 패턴).
 * Phase 2~5 에서 데미지/Ability/Cue 처리 확장 지점.
 */
UCLASS(ClassGroup=(GAS), meta=(BlueprintSpawnableComponent))
class TDPROJECT_API UAOSAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UAOSAbilitySystemComponent();
};
