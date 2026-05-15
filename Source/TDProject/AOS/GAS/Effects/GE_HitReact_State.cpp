// HitReact ↔ Attack 충돌 방지 — GE_HitReact_State 구현 (UE 5.4+ 컴포넌트 시스템)

#include "GE_HitReact_State.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_HitReact_State::UGE_HitReact_State()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Duration 은 SetByCaller(Data.Duration) 로 동적 결정 — 호출자(ApplyHitReactStateGE)가
	// HitReactMontage->GetPlayLength() 값을 SetSetByCallerMagnitude 로 전달.
	// GE_Damage.cpp 와 동일한 안전 패턴: 멤버 개별 할당 후 Magnitude 래핑.
	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	// UE 5.4+: cooldown/blocked-tag 검증은 UTargetTagsGameplayEffectComponent 만 인식.
	// 생성자에서 NewObject() 직접 호출은 금지 (CDO 생성 중 fatal) →
	// CreateDefaultSubobject + GEComponents 직접 추가 패턴 사용.
	UTargetTagsGameplayEffectComponent* TagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
	if (TagsComp)
	{
		FInheritedTagContainer TagsContainer;
		TagsContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("State.HitReact")));
		TagsComp->SetAndApplyTargetTagChanges(TagsContainer);

		GEComponents.Add(TagsComp);
	}
}
