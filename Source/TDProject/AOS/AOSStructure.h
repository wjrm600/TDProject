#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AOSGameMode.h"
#include "AOSStructure.generated.h"

class UWidgetComponent;
class UAOSHealthBarWidget;

UENUM(BlueprintType)
enum class EStructureType : uint8
{
	CommandCenter UMETA(DisplayName = "Command Center"),
	Tower UMETA(DisplayName = "Tower")
};

/**
 * AOS 게임의 구조물 (타워, 커맨드 센터)
 * 건물 체력 관리, 대미지 처리, 파괴 상태 등을 담당
 */
UCLASS()
class TDPROJECT_API AAOSStructure : public AActor
{
	GENERATED_BODY()

public:
	AAOSStructure();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 초기 설정
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	void Initialize(EStructureType Type, EAOSTeam OwnerTeam, EAOSLane Lane = EAOSLane::Mid);

	// 데미지 및 체력 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	void ReceiveDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	float GetMaxHealth() const { return MaxHealth; }

	// 상태 확인
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	bool IsDestroyed() const { return CurrentHealth <= 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	EStructureType GetStructureType() const { return StructureType; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	EAOSTeam GetOwnerTeam() const { return OwnerTeam; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	EAOSLane GetLane() const { return Lane; }

	// 방어 능력
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	void FireAtTarget(class AAOSCharacter* Target);

	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	AAOSCharacter* FindNearestEnemy();

protected:
	// 기본 설정
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Structure")
	EStructureType StructureType;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Structure")
	EAOSTeam OwnerTeam;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Structure")
	EAOSLane Lane;

	// 체력 시스템
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure")
	float MaxHealth = 1000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Structure")
	float CurrentHealth;

	// 공격 속성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure")
	float AttackDamage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure")
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure")
	float AttackCooldown = 2.0f;

	// 메시 및 콜리전
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|Structure")
	class UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|Structure")
	class USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|Structure")
	class USphereComponent* DetectionRange;

	// HP 바 위젯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|UI")
	UWidgetComponent* HealthBarComponent;

	UPROPERTY()
	UAOSHealthBarWidget* HealthBarWidget;

	void UpdateHealthBar();
	void InitializeHealthBar();

	// HP 바 위젯 클래스 (에디터에서 설정 또는 코드에서 자동 로드)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|UI")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

private:
	float CurrentAttackCooldown = 0.0f;
	class AAOSCharacter* CurrentTarget = nullptr;

	void UpdateAttackTarget();

	// 메시 설정 함수
	void SetupTowerMesh();
	void SetupCommandCenterMesh();

	// 파괴 처리
	void OnStructureDestroyed();
};
