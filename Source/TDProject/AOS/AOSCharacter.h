#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AOSGameMode.h"
#include "Net/UnrealNetwork.h"
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	// 공격력 조회
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	float GetAttackDamage() const { return AttackDamage; }

	// 선택 하이라이트 (커스텀 뎁스 스텐실)
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	void SetHighlighted(bool bHighlight);

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

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, BlueprintReadOnly, Category = "AOS|Character")
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

	// Phase 3B: 체력 리플리케이션 콜백
	UFUNCTION()
	void OnRep_CurrentHealth();

	// Phase 3B: 사망 시각 효과 멀티캐스트 (서버 → 모든 클라이언트)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDeath();

private:
	float CurrentAttackCooldown = 0.0f;

	void SetupCharacterDefaults();
};
