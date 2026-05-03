// GAS Phase 1: AttributeSet 구현

#include "AOSAttributeSet.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UAOSAttributeSet::UAOSAttributeSet()
{
	// Phase 1: 캐릭터 기본값으로 초기화 (구조물은 Phase 5 에서 GE_InitTower/GE_InitCC 로 덮어씀)
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitAttackPower(10.f);
	InitAttackRange(500.f);
	InitAttackSpeed(1.f);
	InitMoveSpeed(600.f);
	InitDamage(0.f);
}

void UAOSAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_None + REPNOTIFY_Always — 값이 같아도 OnRep 호출 (이펙트 갱신 보장)
	DOREPLIFETIME_CONDITION_NOTIFY(UAOSAttributeSet, Health,      COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAOSAttributeSet, MaxHealth,   COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAOSAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAOSAttributeSet, AttackRange, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAOSAttributeSet, AttackSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAOSAttributeSet, MoveSpeed,   COND_None, REPNOTIFY_Always);
	// Damage 는 메타 속성 — 리플리케이션 안 함
}

void UAOSAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Health 는 [0, MaxHealth] 로 클램프
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	// MaxHealth 는 0 미만 금지
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UAOSAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Phase 2: Damage 메타 속성 → Health 차감 + 메타 리셋
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f); // 메타 리셋 (다음 GE 적용 시 깨끗한 상태)

		if (LocalDamage > 0.f)
		{
			const float OldHealth = GetHealth();
			const float NewHealth = FMath::Clamp(OldHealth - LocalDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);

			// 사망 처리: Health 가 처음 0 으로 떨어지는 순간만 1회 발동
			if (NewHealth <= 0.f && OldHealth > 0.f)
			{
				AActor* Owner = GetOwningActor();
				if (AAOSCharacter* Char = Cast<AAOSCharacter>(Owner))
				{
					Char->OnCharacterDeath();
				}
				else if (AAOSStructure* Struct = Cast<AAOSStructure>(Owner))
				{
					// Phase 5: 구조물 파괴 처리 (캐릭터와 동일 진입점)
					Struct->OnStructureDestroyed();
				}
			}
		}
	}
}

void UAOSAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAOSAttributeSet, Health, OldHealth);
}

void UAOSAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAOSAttributeSet, MaxHealth, OldMaxHealth);
}

void UAOSAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAOSAttributeSet, AttackPower, OldAttackPower);
}

void UAOSAttributeSet::OnRep_AttackRange(const FGameplayAttributeData& OldAttackRange)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAOSAttributeSet, AttackRange, OldAttackRange);
}

void UAOSAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAOSAttributeSet, AttackSpeed, OldAttackSpeed);
}

void UAOSAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAOSAttributeSet, MoveSpeed, OldMoveSpeed);
}
