// 투척 무기 액터 — 손에서 날아가 땅에 꽂히며 범위 피해, 잠시 뒤 주인에게 돌아온다 (Kwang Q 검 던지기).
// UGA_SkillBase::ThrownWeaponClass 로 지정하면 GA 가 ThrowReleaseDelay 뒤 서버에서 스폰한다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AOSThrownWeapon.generated.h"

class AAOSCharacter;
class UGA_SkillBase;
class USceneComponent;
class USkeletalMeshComponent;
class UParticleSystem;
class UParticleSystemComponent;
class UAnimMontage;

UENUM()
enum class EAOSThrownWeaponPhase : uint8
{
	None,
	Flying,     // 손 → 착지점 포물선 비행
	Stuck,      // 땅에 꽂혀 대기 (착지 순간 범위 피해)
	Returning,  // 주인 손으로 귀환
	Returned    // 회수 완료 — 메시 숨김, 잠시 뒤 파괴
};

/**
 * AAOSThrownWeapon — 서버 권한 투척 무기.
 *
 * 네트워크 (DS 정합):
 *   - 서버만 단계(Phase)를 진행하고 데미지를 준다. 단계 + 단계 시작 서버 시각만 복제(ReplicateMovement 끔).
 *   - 각 머신은 그 값으로 위치를 직접 계산한다: 비행 = 손 → ImpactLocation 포물선, 귀환 = 주인 손 추적.
 *   - 비주얼(메시·이펙트·받기 몽타주)은 NetMode != DedicatedServer 에서만.
 *
 * 무기 메시:
 *   - 별도 무기 메시가 없는 Paragon 히어로용 — 주인 스켈레탈 메시를 그대로 쓰고 WeaponMaterialSlotName
 *     섹션만 보이게 한다. 레퍼런스 포즈의 TipSocket/BaseSocket 으로 칼끝이 Pivot 원점, 칼날이 +X 가 되게 정렬.
 *
 * 손의 무기:
 *   - 스폰~회수 동안 주인 ASC 에 State.WeaponThrown(복제 loose 태그) → AAOSCharacter 가 무기 본을 숨기고
 *     GA_Attack / GA_SkillBase 가 활성화를 막는다.
 */
UCLASS()
class TDPROJECT_API AAOSThrownWeapon : public AActor
{
	GENERATED_BODY()

public:
	AAOSThrownWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** 서버 전용 — SpawnActorDeferred 직후, FinishSpawning 전에 호출. */
	void InitThrow(AAOSCharacter* InOwner, UGA_SkillBase* InSkill, const FVector& InStart, const FVector& InImpact);

	// ============================================================
	// 비행 / 착지 / 귀환
	// ============================================================

	/** 비행 속도 (cm/s). 비행 시간 = 거리 / 속도 를 Min~Max 로 클램프. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "100.0"))
	float FlightSpeed = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "0.05"))
	float MinFlightTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "0.05"))
	float MaxFlightTime = 0.7f;

	/** 포물선 최고점 높이 (직선 대비 cm). 클수록 위에서 내리꽂힌다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "0.0"))
	float ArcHeight = 200.0f;

	/** 착지 순간 범위 피해 반경 (cm). 데미지 양·부수효과는 던진 GA 의 BaseDamage / TargetAppliedEffects. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "0.0"))
	float ImpactRadius = 250.0f;

	/** 칼끝이 땅에 박히는 깊이 (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "0.0"))
	float StuckDepth = 25.0f;

	/** 꽂힌 칼날의 최소 하향 각도 (도). 낮게 날아와도 이 각도 이상으로 세워 꽂는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MinStuckPitch = 55.0f;

	/** 꽂혀 있는 시간 (초) — 이 동안 주인은 무기가 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "0.0"))
	float StuckDuration = 1.5f;

	/** 귀환 속도 (cm/s). 주인을 추적한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "100.0"))
	float ReturnSpeed = 2500.0f;

	/** 서버가 회수로 판정하는 거리 (주인 위치 기준, cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "10.0"))
	float CatchDistance = 120.0f;

	/** 안전장치 — 이 시간이 지나면 무조건 회수 처리 (무기가 영원히 사라지지 않게). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon", meta = (ClampMin = "1.0"))
	float MaxLifetime = 8.0f;

	// ============================================================
	// 무기 메시 (주인 스켈레탈 메시에서 무기 섹션만)
	// ============================================================

	/** 보이게 둘 머티리얼 슬롯 (Kwang = M05_Weapon). 나머지 섹션은 숨긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|Mesh")
	FName WeaponMaterialSlotName = FName("M05_Weapon");

	/** 칼끝 / 칼자루 쪽 소켓(또는 본) — 정렬 기준. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|Mesh")
	FName TipSocketName = FName("FX_weapon_tip");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|Mesh")
	FName BaseSocketName = FName("FX_weapon_base");

	// ============================================================
	// 이펙트 (Cascade — Paragon 팩) / 받기 몽타주
	// ============================================================

	/** 비행 중 무기에 붙는 이펙트 (P_SwordThrow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX")
	TObjectPtr<UParticleSystem> FlightFX;

	/** 비행 중 칼날 궤적 — BaseSocket~TipSocket 애님 트레일 (P_Kwang_Primary_Trail). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX")
	TObjectPtr<UParticleSystem> FlightTrailFX;

	/** 착지 순간 그 자리 (P_Kwang_Sword_Impact). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX")
	TObjectPtr<UParticleSystem> ImpactFX;

	/** 꽂혀 있는 동안 무기에 붙는 반복 이펙트 (P_Kwang_Sword_Attach). 귀환 시작 시 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX")
	TObjectPtr<UParticleSystem> StuckFX;

	/** 귀환 중 무기에 붙는 이펙트 (P_SwordReturn1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX")
	TObjectPtr<UParticleSystem> ReturnFX;

	/** 회수 순간 주인 손 (P_SwordReturnToHand). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX")
	TObjectPtr<UParticleSystem> CatchFX;

	/** 회수 순간 주인이 재생할 받기 몽타주 (AM_Kwang_Q_Catch). 다른 몽타주 재생 중이거나 이동 중이면 생략. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX")
	TObjectPtr<UAnimMontage> CatchMontage;

	/** 받기 몽타주 시작 위치 (초) — 손을 뻗는 앞부분을 건너뛰어 무기 도착 직후 손에 잡히게. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Thrown Weapon|FX", meta = (ClampMin = "0.0"))
	float CatchMontageStartTime = 0.2f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "AOS|Thrown Weapon")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, Category = "AOS|Thrown Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	// --- 복제 상태 ---
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EAOSThrownWeaponPhase Phase = EAOSThrownWeaponPhase::None;

	UPROPERTY(Replicated)
	TObjectPtr<AAOSCharacter> OwnerCharacter;

	UPROPERTY(Replicated)
	FVector_NetQuantize StartLocation;

	UPROPERTY(Replicated)
	FVector_NetQuantize ImpactLocation;

	UPROPERTY(Replicated)
	float FlightDuration = 0.5f;

	/** 현재 단계가 시작된 서버 월드 시각 (GameState::GetServerWorldTimeSeconds 기준). */
	UPROPERTY(Replicated)
	float PhaseStartServerTime = 0.0f;

	UFUNCTION()
	void OnRep_Phase();

	// --- 서버 ---
	void SetPhase(EAOSThrownWeaponPhase NewPhase);
	void OnImpact();
	void StartReturn();
	void FinishReturn();
	void RemoveOwnerThrownTag();

	TWeakObjectPtr<UGA_SkillBase> OwningSkill;
	FTimerHandle PhaseTimerHandle;
	FTimerHandle SafetyTimerHandle;
	bool bOwnerTagApplied = false;

	// --- 공통 / 비주얼 ---
	float GetServerTime() const;
	FVector GetHandLocation() const;          // 렌더 머신 = 주인 손 소켓, DS = 주인 위치 근사
	FVector EvaluateFlight(float Alpha, FVector& OutDirection) const;
	void ApplyStuckTransform();
	void HandlePhaseVisuals();                // 단계 진입 시 이펙트/메시 (NetMode != DS)
	bool SetupWeaponVisual();                 // 주인 메시 복사 + 무기 섹션만 표시 + 정렬
	void StopFX(TObjectPtr<UParticleSystemComponent>& FX);

	/** 렌더 머신의 비행 시작점 (주인 손). 서버는 StartLocation. */
	FVector VisualStart = FVector::ZeroVector;
	bool bVisualReady = false;
	EAOSThrownWeaponPhase VisualPhase = EAOSThrownWeaponPhase::None;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> FlightFXComp;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> FlightTrailComp;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> StuckFXComp;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> ReturnFXComp;
};
