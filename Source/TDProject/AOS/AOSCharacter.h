#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AOSGameMode.h"
#include "Net/UnrealNetwork.h"
#include "AOSCharacter.generated.h"

class UAOSAttributeComponent;
class UAOSAbilitySystemComponent;
class UAOSAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UDataTable;
class UWidgetComponent;
class UAOSHealthBarWidget;
struct FOnAttributeChangeData;

/**
 * AOS 게임의 기본 캐릭터 클래스
 * 플레이어가 관리하는 4마리의 캐릭터를 위한 기본 속성 포함
 *
 * GAS Phase 1: ASC/AttributeSet 부착 (병행 운영 — 게임 로직은 아직 float 멤버 사용)
 */
UCLASS()
class TDPROJECT_API AAOSCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAOSCharacter();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// AttributeSet 접근자 (BlueprintReadOnly 도 가능하지만 함수형으로 일관)
	UFUNCTION(BlueprintCallable, Category = "AOS|GAS")
	UAOSAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 컨트롤러 빙의 시점 (서버) — ASC ActorInfo 초기화
	virtual void PossessedBy(AController* NewController) override;

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

	// Phase 2: AttributeSet wrapper — Health 의 진짜 소스는 AttributeSet
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	float GetMaxHealth() const;

	// 라인 위치 정보
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	FVector GetLaneStartPosition() const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	FVector GetLaneEndPosition() const;

	// 공격력 조회 (Phase 3: AttributeSet wrapper)
	UFUNCTION(BlueprintCallable, Category = "AOS|Character")
	float GetAttackDamage() const;

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

	// 체력 시스템 (Phase 2: AttributeSet 으로 이전)
	// MaxHealth 는 AttributeSet 초기값 시드로만 사용 (Phase 3 에서 완전 제거 예정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Character")
	float MaxHealth = 100.0f;

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

	// --- GAS Phase 1+ ---
	// ASC: 캐릭터 자체 소유 (PlayerState 미사용 — AI 캐릭터 패턴)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|GAS")
	UAOSAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY()
	UAOSAttributeSet* AttributeSet;

	// Phase 2: 데미지 GE 클래스 (BP 에서 override 가능, 기본값은 UGE_Damage)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS")
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	// Phase 3: 스폰 시 자동 부여할 능력 목록 (BP 에서 추가 능력 부여 가능)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// Phase 5+: AttributeSet 초기값 DataTable
	// 약속 자산: /Game/AOS/GAS/Data/DT_CharacterAttributes (FAOSAttributeInitRow 사용)
	// nullptr 또는 row 미발견 시 기존 float 멤버 (MaxHealth/AttackDamage 등) 가 fallback
	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS|Init")
	TSoftObjectPtr<UDataTable> AttributeInitTable;

	// 위 DT 에서 사용할 row 이름 (캐릭터별 다른 값을 원하면 변경)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS|Init")
	FName AttributeInitRowName = FName("Default");

	// ASC ActorInfo 초기화 (서버: PossessedBy / 클라: BeginPlay)
	void InitializeAbilitySystem();

	// Phase 3: 서버에서 StartupAbilities 부여 (PossessedBy 후)
	void GiveStartupAbilities();

	void UpdateHealthBar();

	// Phase 2: AttributeSet Health 변경 콜백 (HP 바 자동 갱신)
	void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

	// Phase 3: AttributeSet MoveSpeed 변경 콜백 (CharacterMovement->MaxWalkSpeed 동기화)
	void OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data);

	// Phase 3B: 사망 시각 효과 멀티캐스트 (서버 → 모든 클라이언트)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDeath();

private:
	// Phase 3: 쿨타임 카운터 제거 — ASC 의 "Cooldown.Attack.Basic" 태그가 단일 진실 공급원

	void SetupCharacterDefaults();
};
