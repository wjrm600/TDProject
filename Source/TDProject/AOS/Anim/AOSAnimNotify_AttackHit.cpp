#include "AOSAnimNotify_AttackHit.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"

void UAOSAnimNotify_AttackHit::Notify(USkeletalMeshComponent* MeshComp,
                                       UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;

    // 디버그 로그 — 양쪽 (서버/클라) 에서 fire 됨을 확인용
    UE_LOG(LogTemp, Verbose, TEXT("[AOSAnimNotify_AttackHit] Fired on %s (Authority=%s)"),
           Owner ? *GetNameSafe(Owner) : TEXT("null"),
           (Owner && Owner->HasAuthority()) ? TEXT("YES") : TEXT("NO"));

    // 서버 전용: GameplayEvent 송출 (GA_Attack 은 ServerInitiated → ability 인스턴스가 서버에만 존재.
    // 클라이언트에서 송출해도 받을 ability 가 없음. 또한 데미지는 서버-권위적이어야 함.)
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner);
    if (!ASI)
    {
        return;
    }

    UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    // GameplayEvent 송출 — GA_Attack 의 WaitGameplayEvent 가 받아서 데미지 적용.
    // static 으로 캐시하여 매 frame RequestGameplayTag 호출 비용 절감.
    static const FGameplayTag NotifyTag =
        FGameplayTag::RequestGameplayTag(FName("AnimNotify.AttackHit"));

    FGameplayEventData EventData;
    EventData.EventTag = NotifyTag;
    EventData.Instigator = Owner;
    // Target 은 GA_Attack 이 ActivateAbility 시점에 cache 한 값을 사용 (notify 시점엔 target 모름)

    ASC->HandleGameplayEvent(NotifyTag, &EventData);
}

#if WITH_EDITOR
FString UAOSAnimNotify_AttackHit::GetNotifyName_Implementation() const
{
    return TEXT("AttackHit");
}
#endif
