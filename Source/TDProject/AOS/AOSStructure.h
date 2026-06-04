#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AOSGameMode.h"
#include "AOSStructure.generated.h"

class UWidgetComponent;
class UAOSHealthBarWidget;
class UAOSAbilitySystemComponent;
class UAOSAttributeSet;
class UGameplayEffect;
class UDataTable;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EStructureType : uint8
{
	CommandCenter UMETA(DisplayName = "Command Center"),
	Tower UMETA(DisplayName = "Tower")
};

/**
 * AOS 게임의 구조물 (타워, 커맨드 센터)
 * 건물 체력 관리, 대미지 처리, 파괴 상태 등을 담당
 *
 * GAS Phase 5: ASC + AttributeSet 부착 (Health/MaxHealth 가 AttributeSet 으로 이전)
 */
UCLASS()
class TDPROJECT_API AAOSStructure : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAOSStructure();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// AttributeSet 접근자 (BlueprintReadOnly 도 가능하지만 함수형으로 일관)
	UFUNCTION(BlueprintCallable, Category = "AOS|GAS")
	UAOSAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// AttributeSet 사망 콜백에서 호출하므로 public 노출
	void OnStructureDestroyed();

	// 초기 설정
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	void Initialize(EStructureType Type, EAOSTeam OwnerTeam, EAOSLane Lane = EAOSLane::Mid);

	// 데미지 및 체력 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	void ReceiveDamage(float DamageAmount);

	// Phase 5: AttributeSet wrapper — Health 의 진짜 소스는 AttributeSet
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	float GetMaxHealth() const;

	// 상태 확인
	UFUNCTION(BlueprintCallable, Category = "AOS|Structure")
	bool IsDestroyed() const;

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
	// 기본 설정 — 서버에서 Initialize()로 설정, 클라이언트로 리플리케이션 필요
	// ReplicatedUsing — 클라가 타입 수신 시 메시 셋업 (서버 전용 Initialize 를 클라가 못 받으므로)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StructureType, Category = "AOS|Structure")
	EStructureType StructureType = EStructureType::Tower; // 기본값을 Tower로 명시 (enum 첫 값이 CommandCenter여서 미리플리케이션 시 오표시 방지)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OwnerTeam, Category = "AOS|Structure")
	EAOSTeam OwnerTeam = EAOSTeam::Team1;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AOS|Structure")
	EAOSLane Lane = EAOSLane::Mid;

	// #3 클라 신뢰성: 파괴 시각 상태를 복제 (멀티캐스트 relevancy 의존 회피 — #2 Team 패턴과 동일).
	// 서버가 OnStructureDestroyed 에서 true 설정 → OnRep 이 클라에서 메시 숨김.
	UPROPERTY(ReplicatedUsing = OnRep_DestroyedVisual)
	bool bDestroyedVisual = false;

	// 체력 시스템 (Phase 5: AttributeSet 으로 이전)
	// MaxHealth 는 AttributeSet 초기값 시드로 유지 (Initialize 에서 StructureType 별로 갱신)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure")
	float MaxHealth = 1000.0f;

	// 공격 속성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure")
	float AttackDamage = 20.0f;

	// 캐릭터 AttackRange(500)보다 크게 → 타워가 캐릭터를 안정적으로 공격 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure")
	float AttackRange = 600.0f;

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

	// --- GAS Phase 5 ---
	// ASC: 구조물 자체 소유 (캐릭터와 동일 패턴)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|GAS")
	UAOSAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY()
	UAOSAttributeSet* AttributeSet;

	// 데미지 GE 클래스 (BP 에서 override 가능, 기본값은 UGE_Damage)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS")
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	// Phase 5+: AttributeSet 초기값 DataTable
	// 약속 자산:
	//   /Game/AOS/GAS/Data/DT_TowerAttributes        (StructureType=Tower)
	//   /Game/AOS/GAS/Data/DT_CommandCenterAttributes (StructureType=CommandCenter)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS|Init")
	TSoftObjectPtr<UDataTable> TowerAttributeInitTable;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS|Init")
	TSoftObjectPtr<UDataTable> CommandCenterAttributeInitTable;

	// 위 DT 들이 사용할 row 이름 (구조물별 다른 값을 원하면 변경 — 양쪽 DT 공통)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|GAS|Init")
	FName AttributeInitRowName = FName("Default");

	// ASC ActorInfo 초기화 + AttributeSet 시드 + 콜백 등록
	void InitializeAbilitySystem();

	// AttributeSet 초기값 시드 (DT 우선, 없으면 float 멤버 fallback) — Initialize 후 재호출 가능
	void ApplyAttributeSeeds();

	// AttributeSet Health 변경 콜백 (HP 바 자동 갱신)
	void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

	// OwnerTeam 리플리케이션 콜백 — 클라이언트에서 HP 바 색상 갱신
	UFUNCTION()
	void OnRep_OwnerTeam();

	// #3 파괴 시각 상태 복제 콜백 — 클라이언트(+서버)에서 메시/HP바 숨김
	UFUNCTION()
	void OnRep_DestroyedVisual();

	// StructureType 복제 콜백 — 클라에서 타입에 맞는 메시 셋업 (서버는 Initialize 에서 처리)
	UFUNCTION()
	void OnRep_StructureType();

	// 현재 StructureType 에 맞는 메시/머티리얼 셋업 (클라 전용 호출 경로용)
	void SetupMeshForCurrentType();

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

	// Phase 3B: 파괴 시각 효과 멀티캐스트 (서버 → 모든 클라이언트)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDestroyed();

public:
	// Slice 1: 플로팅 데미지 숫자 (서버 → 모든 클라, 시각 전용).
	// AttributeSet::PostGameplayEffectExecute(서버)가 호출하므로 public.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShowDamageNumber(float DamageAmount);
};
