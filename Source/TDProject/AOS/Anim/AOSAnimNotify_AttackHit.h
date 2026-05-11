#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AOSAnimNotify_AttackHit.generated.h"

/**
 * 공격 몽타주의 데미지 적용 타이밍 마커 (단발 Notify).
 *
 * 현재 Phase: 로깅만 수행. 빈 껍데기.
 * Phase 4: GA_Attack 의 PlayMontageAndWait + WaitGameplayEvent 패턴과 결합하여
 *          실제 데미지 판정을 이 Notify 시점에 트리거한다.
 *
 * DS 환경:
 *  - Notify 는 클라이언트(애니메이션 재생 쪽)에서 주로 실행됨.
 *  - 게임플레이 로직(데미지 적용 등)은 HasAuthority() 가드 필수.
 */
UCLASS(meta = (DisplayName = "AOS Attack Hit"))
class TDPROJECT_API UAOSAnimNotify_AttackHit : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp,
                        UAnimSequenceBase* Animation,
                        const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
    virtual FString GetNotifyName_Implementation() const override;
#endif
};
