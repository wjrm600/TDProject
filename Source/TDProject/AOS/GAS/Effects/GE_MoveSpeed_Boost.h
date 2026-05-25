// Phase 4: 이동속도 증가 GE — Duration 동안 MoveSpeed Additive +300 (캐릭터 base 600 기준 +50%).
// GrantedTag: State.SpeedBoost. 재사용 가능 (Alex Q 외 다른 ability 도 활용 가능).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_MoveSpeed_Boost.generated.h"

/**
 * 이동속도 증가 Duration GE.
 *
 * - Modifier: MoveSpeed Additive +300 (Phase 4 hardcoded — 추후 SetByCaller/AttributeBased 검토)
 * - GrantedTag: State.SpeedBoost (AI/UI 가 효과 active 여부 확인용)
 * - Duration: 3.0s (Phase 4 hardcoded)
 */
UCLASS()
class TDPROJECT_API UGE_MoveSpeed_Boost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_MoveSpeed_Boost();
};
