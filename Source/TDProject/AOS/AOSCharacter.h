#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AOSGameMode.h"
#include "AOSCharacter.generated.h"

class UAOSAttributeComponent;
class UWidgetComponent;
class UAOSHealthBarWidget;

/**
 * AOS 게임의 기본 캐릭터 클래스
 * 플레이어가 관리하는 4마리의 캐릭터를 위한 기본 속성 포함
 */
UCLASS()
class TDPROJECT_API AAOSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAOSCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 팀 및 라인 설정
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	void SetTeam(EAOSTeam NewTeam);

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	void SetLane(EAOSLane NewLane);

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	void DeployToLane();

	// 상태 확인
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	EAOSTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	EAOSLane GetLane() const { return AssignedLane; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	bool IsAlive() const;

	// 데미지 및 체력 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	void ReceiveDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	float GetMaxHealth() const { return MaxHealth; }

	// 라인 위치 정보
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	FVector GetLaneStartPosition() const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	FVector GetLaneEndPosition() const;

	// 사망 처리
	void OnCharacterDeath();

protected:
	// 팀 및 라인 정보
	UPROPERTY(BlueprintReadWrite, Category = "AOS|Character")
	EAOSTeam Team = EAOSTeam::Team1;

	UPROPERTY(BlueprintReadWrite, Category = "AOS|Character")
	EAOSLane AssignedLane = EAOSLane::Mid;

	// 체력 시스템
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Character")
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Character")
	float CurrentHealth;

	// 공격 속성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Character")
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Character")
	float AttackRange = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Character")
	float AttackCooldown = 1.0f;

	// 이동 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Character")
	float MovementSpeed = 600.0f;

	// HP 바 위젯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|UI")
	UWidgetComponent* HealthBarComponent;

	UPROPERTY()
	UAOSHealthBarWidget* HealthBarWidget;

	void UpdateHealthBar();

private:
	float CurrentAttackCooldown = 0.0f;

	void SetupCharacterDefaults();
};
