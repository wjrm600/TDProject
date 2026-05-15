#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AOSAnimNotify_AttackHit.generated.h"

/**
 * 공격 몽타주의 데미지 적용 타이밍 마커 (단발 Notify).
 *
 * GA_Attack 의 WaitGameplayEvent("AnimNotify.AttackHit") 가 이 이벤트를 받아 데미지 적용.
 * Owner 의 ASC 에 HandleGameplayEvent("AnimNotify.AttackHit") 를 송출한다.
 *
 * DS 환경:
 *  - Notify 는 서버/클라 양쪽에서 fire 됨.
 *  - HasAuthority() 가드로 서버에서만 GameplayEvent 를 송출 (GA_Attack 인스턴스가 서버에만 존재).
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
