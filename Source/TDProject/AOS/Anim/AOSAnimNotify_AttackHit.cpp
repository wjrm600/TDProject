#include "AOSAnimNotify_AttackHit.h"

void UAOSAnimNotify_AttackHit::Notify(USkeletalMeshComponent* MeshComp,
                                       UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    // 현재 Phase: 로깅만 수행 (빈 껍데기).
    // Phase 4 에서 GA_Attack → WaitGameplayEvent("AnimNotify.AttackHit") 패턴 연결 예정.
    UE_LOG(LogTemp, Verbose, TEXT("[AOSAnimNotify_AttackHit] Fired on %s"),
           MeshComp ? *GetNameSafe(MeshComp->GetOwner()) : TEXT("null"));
}

#if WITH_EDITOR
FString UAOSAnimNotify_AttackHit::GetNotifyName_Implementation() const
{
    return TEXT("AttackHit");
}
#endif
