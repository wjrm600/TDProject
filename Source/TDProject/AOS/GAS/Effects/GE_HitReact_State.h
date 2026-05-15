// HitReact ↔ Attack 충돌 방지 규칙 — Rule B 구현
// Duration 동안 State.HitReact 태그를 ASC 에 부여하여 GA_Attack 활성화 차단.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_HitReact_State.generated.h"

/**
 * 피격 리액션 상태 GE — Duration 동안 State.HitReact 태그 부여.
 *
 * Duration 은 SetByCaller(Data.Duration) 로 전달:
 *   Spec.Data->SetSetByCallerMagnitude(
 *       FGameplayTag::RequestGameplayTag("Data.Duration"),
 *       HitReactMontage->GetPlayLength());
 *
 * State.HitReact 태그는 GA_Attack::ActivationBlockedTags 에 등록되어
 * HitReact 재생 중 공격 능력 활성화를 차단한다 (Rule B).
 *
 * UE 5.4+ 컴포넌트 시스템:
 * UTargetTagsGameplayEffectComponent 로 태그 부여 (구식 InheritableOwnedTagsContainer 미사용).
 */
UCLASS()
class TDPROJECT_API UGE_HitReact_State : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_HitReact_State();
};
