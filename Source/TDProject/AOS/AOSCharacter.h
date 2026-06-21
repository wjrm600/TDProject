#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AOSGameMode.h"
#include "Net/UnrealNetwork.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimMontage.h"
#include "AOSCharacter.generated.h"

class UAOSAttributeComponent;
class UAOSAbilitySystemComponent;
class UAOSAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UDataTable;
class UWidgetComponent;
class UAOSHealthBarWidget;
class UStaticMeshComponent;
class UStaticMesh;
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

	// === Animation Slots (Phase 3.5) ===
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Animation", meta = (ToolTip = "GameplayTag 기반 스킬 몽타주 매핑 (Phase 4)"))
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> SkillMontages;

	// 사망 몽타주 종료 후 ragdoll 상태로 유지할 시간 (초). 이 시간 후 액터 destroy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Animation", meta = (ClampMin = "0.0", ToolTip = "사망 몽타주 종료 후 ragdoll 정착 시간 (초)"))
	float RagdollSettleDuration = 2.0f;

	// 접근자 (GA_Attack, GA_Skill 등에서 사용)
	UFUNCTION(BlueprintPure, Category = "AOS|Animation")
	UAnimMontage* GetAttackMontage() const { return AttackMontage; }

	UFUNCTION(BlueprintPure, Category = "AOS|Animation")
	UAnimMontage* GetHitReactMontage() const { return HitReactMontage; }

	UFUNCTION(BlueprintPure, Category = "AOS|Animation")
	UAnimMontage* GetDeathMontage() const { return DeathMontage; }

	UFUNCTION(BlueprintPure, Category = "AOS|Animation")
	UAnimMontage* GetSkillMontage(FGameplayTag SkillTag) const;

	// === Multicast RPCs (Phase 3.5) ===
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathMontage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayHitReact();

	// Slice 1: 플로팅 데미지 숫자 (서버 → 모든 클라, 시각 전용).
	// Unreliable — 비주얼이라 패킷 유실 허용(놓친 숫자 1개는 무해).
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShowDamageNumber(float DamageAmount);

	// 사망 몽타주 종료 시점에 호출 — 메시를 ragdoll 시뮬레이션으로 전환
	// PhysicsAsset 가 없는 경우 fallback 으로 메시 hide.
	UFUNCTION(BlueprintCallable, Category = "AOS|Animation")
	void StartRagdoll();

	// Phase 4+: 시전 중 이동 불가 스킬 root — GE_Rooted(Duration) 적용 + 이동 즉시 정지.
	// 스킬 GA 가 bAllowMovementDuringCast=false 일 때 호출 (서버 전용).
	// State.Rooted 태그가 Duration 동안 부여되어 StateTree task 가 RUNNING 유지 → AI 홀드.
	void ApplyCastRoot(float Duration);

	// === Weapon Attachment (무기 부착 시스템 — 데이터 주도 소켓) ===
	// 무기는 별도 StaticMesh 로 캐릭터 손 소켓(WeaponSocketName)에 부착된다 (몸만 만든 메시 + 무기 분리 전제).
	// 서버가 EquipWeapon() 으로 메시 지정 → EquippedWeaponMesh 복제 → 클라가 OnRep 으로 시각화 (DS 정합).

	// 캐릭터 기본 무기 (예: 알렉스의 검). BP CDO 에서 지정. 스폰 시 자동 장착(서버).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AOS|Weapon")
	TObjectPtr<UStaticMesh> DefaultWeaponMesh;

	// 무기 부착 소켓 이름 (캐릭터별 손 소켓; 기본 "weapon_r"). 스켈레톤에 해당 소켓이 있어야 부착됨.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AOS|Weapon")
	FName WeaponSocketName = FName("weapon_r");

	// 서버 전용: 무기 메시 장착(nullptr 전달 시 해제). EquippedWeaponMesh 복제 → 클라 OnRep.
	UFUNCTION(BlueprintCallable, Category = "AOS|Weapon")
	void EquipWeapon(UStaticMesh* WeaponMesh);

	UFUNCTION(BlueprintPure, Category = "AOS|Weapon")
	UStaticMeshComponent* GetWeaponMeshComponent() const { return WeaponMeshComponent; }

protected:
	// 팀 및 라인 정보 — DS 클라가 팀을 알아야 HP 바 색상을 칠할 수 있으므로 Replicated
	UPROPERTY(ReplicatedUsing = OnRep_Team, BlueprintReadWrite, Category = "AOS|Character")
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

	// 무기 메시 컴포넌트 — WeaponSocketName 소켓에 부착 (코스메틱, NoCollision).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMeshComponent;

	// 현재 장착 무기 메시 — 복제. 서버 EquipWeapon → 클라 OnRep_EquippedWeapon.
	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, BlueprintReadOnly, Category = "AOS|Weapon")
	TObjectPtr<UStaticMesh> EquippedWeaponMesh;

	// Weapon 리플리케이션 콜백 (클라) — 복제된 무기 메시를 시각화
	UFUNCTION()
	void OnRep_EquippedWeapon();

	// 무기 메시/소켓 부착을 WeaponMeshComponent 에 실제 반영 (서버+클라 공통)
	void RefreshWeaponMesh();

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

	// HP 바를 팀 색상(Team1=Red, Team2=Blue)으로 칠함. 위젯 미생성 시 안전하게 skip.
	// 렌더링 머신(클라/리슨서버)에서 호출되어야 함 — 위젯은 그쪽에만 존재.
	void RefreshHealthBarTeamColor();

	// Team 리플리케이션 콜백 (클라) — 팀 도착 시 HP 바 색상 갱신
	UFUNCTION()
	void OnRep_Team();

	// HP 바 팀 색상이 1회 적용됐는지 (Tick 의 lazy 적용 가드)
	bool bHealthBarColorApplied = false;

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

	// HitReact ↔ Attack 충돌 방지 (Rule B) — 서버 전용.
	// GE_HitReact_State 를 ASC 에 적용해 HitReactMontage 재생 시간 동안
	// "State.HitReact" 태그를 부여 → GA_Attack::ActivationBlockedTags 가 공격 차단.
	// Duration = HitReactMontage->GetPlayLength() via SetByCaller(Data.Duration).
	void ApplyHitReactStateGE();

	// Phase 3.5: 사망 몽타주 종료 후 ragdoll 전환 타이머 핸들 (각 클라이언트 로컬)
	FTimerHandle RagdollTimerHandle;
};
