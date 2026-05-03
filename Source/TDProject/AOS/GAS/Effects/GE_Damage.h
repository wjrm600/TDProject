// GAS Phase 2: 데미지 GameplayEffect
// SetByCaller "Data.Damage" magnitude → AttributeSet::Damage 메타 속성으로 더해짐
// PostGameplayEffectExecute 가 Damage → Health 차감 + 메타 리셋 처리

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Damage.generated.h"

/**
 * Instant GE — Damage 메타 속성에 SetByCaller(Data.Damage) 만큼 더함.
 * AttributeSet::PostGameplayEffectExecute 가 메타→Health 적용 처리.
 *
 * 사용 예:
 *   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_Damage::StaticClass(), 1.f, Ctx);
 *   Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Data.Damage"), 25.f);
 *   ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
 */
UCLASS()
class TDPROJECT_API UGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Damage();
};
