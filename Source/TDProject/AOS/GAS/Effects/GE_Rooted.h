// Phase 4+: 시전 중 이동 불가 GE — Duration 동안 State.Rooted 태그 부여.
// 사용처: bAllowMovementDuringCast=false 인 스킬. StateTree task 가 State.Rooted 동안
//         RUNNING 유지 → AI 가 스킬 state 에 홀드 (이동 안 함).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Rooted.generated.h"

/**
 * 시전 중 root Duration GE.
 *
 * - Modifier 없음 (태그만 부여하는 마커)
 * - GrantedTag: State.Rooted
 * - Duration: SetByCaller(Data.Duration) — 호출자(AOSCharacter::ApplyCastRoot)가 몽타주 길이 전달
 * - 효과: StateTree 의 FStateTreeTask_ActivateAbilityByTag 가 State.Rooted 동안 RUNNING 유지하여
 *         AI 가 스킬 state 를 떠나지 않음 (이동 차단). GE 만료 시 태그 해제 → task Succeeded.
 */
UCLASS()
class TDPROJECT_API UGE_Rooted : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Rooted();
};
