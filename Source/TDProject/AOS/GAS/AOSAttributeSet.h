// GAS Phase 1: AttributeSet 정의 (캐릭터/구조물 공통)
// 속성: Health, MaxHealth, AttackPower, AttackRange, AttackSpeed, MoveSpeed, Damage(메타)

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AOSAttributeSet.generated.h"

// ATTRIBUTE_ACCESSORS — Getter/Setter/Initter/HasGetter 자동 생성
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * AOS 캐릭터/구조물의 핵심 속성 집합.
 *
 * 기본값 (캐릭터):
 *   Health=100, MaxHealth=100, AttackPower=10, AttackRange=500, AttackSpeed=1.0, MoveSpeed=600
 *
 * Damage 는 메타 속성 — GE_Damage 의 SetByCaller 로 흘러들어와 PostGameplayEffectExecute 에서
 * Health 차감 후 0 으로 리셋됨 (Phase 2 에서 본문 구현).
 */
UCLASS()
class TDPROJECT_API UAOSAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAOSAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// --- 체력 ---
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Attributes",
		ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAOSAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Attributes",
		ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAOSAttributeSet, MaxHealth)

	// --- 공격 ---
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Attributes",
		ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UAOSAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Attributes",
		ReplicatedUsing = OnRep_AttackRange)
	FGameplayAttributeData AttackRange;
	ATTRIBUTE_ACCESSORS(UAOSAttributeSet, AttackRange)

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Attributes",
		ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(UAOSAttributeSet, AttackSpeed)

	// --- 이동 ---
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Attributes",
		ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UAOSAttributeSet, MoveSpeed)

	// --- 메타 속성 (리플리케이션 안 함, GE 입력 전용) ---
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Attributes")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UAOSAttributeSet, Damage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);

	UFUNCTION()
	void OnRep_AttackRange(const FGameplayAttributeData& OldAttackRange);

	UFUNCTION()
	void OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);
};
