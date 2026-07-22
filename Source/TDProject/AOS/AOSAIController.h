#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AOSGameMode.h"
#include "AOSAIController.generated.h"

class AAOSCharacter;
class AAOSStructure;
class UStateTreeAIComponent;

/**
 * AOS 게임의 AI 컨트롤러
 * 캐릭터의 자동 이동, 라인 순찰, 적 찾기, 공격 등을 관리
 *
 * 행동 패턴:
 * 1. 라인의 첫 번째 타워로 이동
 * 2. 이동 중 타워 또는 적군 발견 → 공격
 * 3. 모든 타워 파괴 후 커맨드 센터로 이동
 * 4. 도중에 적군 만나면 우선 공격
 */
UCLASS()
class TDPROJECT_API AAOSAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAOSAIController();
	virtual ~AAOSAIController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	// 배포 시작
	UFUNCTION(BlueprintCallable, Category = "AOS|AI")
	void StartDeployment(EAOSLane Lane);

	// 현재 배포된 라인 정보
	UFUNCTION(BlueprintCallable, Category = "AOS|AI")
	EAOSLane GetDeployedLane() const { return DeployedLane; }

	// 다음 목표 위치 계산
	UFUNCTION(BlueprintCallable, Category = "AOS|AI")
	FVector GetNextTargetLocation();

	// 감지 범위 내의 적군 찾기
	UFUNCTION(BlueprintCallable, Category = "AOS|AI")
	AAOSCharacter* FindNearestEnemy();

	// 감지 범위 내의 적 타워 찾기
	UFUNCTION(BlueprintCallable, Category = "AOS|AI")
	AAOSStructure* FindNearestEnemyTower();

	/**
	 * 거리 판정용 effective AttackRange.
	 * 우선순위: 캐릭터의 AttributeSet->GetAttackRange() (DT 적용된 단일 진실 공급원)
	 *           → 없으면 AIController.AttackRange 멤버 (fallback)
	 * 디버그 시각화와 실제 공격 거리를 일관되게 유지하기 위해 도입.
	 */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|Attack")
	float GetEffectiveAttackRange() const;

	// =========================================================================
	// Phase 6: State Tree task/condition 이 호출하는 헬퍼들 (public)
	// =========================================================================

	/** 현재 공격 중인 캐릭터 타겟 반환 (없으면 nullptr) */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	AAOSCharacter* GetCurrentTargetCharacter() const { return CurrentTarget; }

	/** 새 공격 타겟 설정 (StateTree 의 FindNearestEnemy task 가 호출) */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	void SetCurrentTarget(AAOSCharacter* InTarget);

	/** 현재 웨이포인트가 적 구조물이면 반환 (이미 파괴됐거나 없으면 nullptr) */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	AAOSStructure* GetCurrentWaypointStructure() const;

	/** 현재 캐릭터 타겟이 effective attack range 내인지 */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	bool IsCurrentTargetInAttackRange() const;

	/** 현재 웨이포인트(이동 목표 위치) 도달 여부 (Z 무시 2D 거리 ≤ ArrivalDistance) */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	bool HasArrivedAtCurrentWaypoint() const;

	/** 현재 캐릭터 타겟으로 NavMesh 이동 요청 (사거리 0.8 배에서 정지) */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	void RequestMoveToCurrentTarget();

	/** 현재 웨이포인트 위치로 NavMesh 이동 요청 */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	void RequestMoveToCurrentWaypoint();

	/** 다음 웨이포인트로 인덱스 증가 + CurrentMoveTarget 갱신 */
	UFUNCTION(BlueprintCallable, Category = "AOS|AI|StateTree")
	void AdvanceToNextWaypoint();

protected:
	// 배포된 라인
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI")
	EAOSLane DeployedLane = EAOSLane::Mid;

	// 현재 제어 중인 캐릭터
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI")
	TObjectPtr<AAOSCharacter> ControlledCharacter;

	// AI 감지 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|AI|Detection")
	float EnemyDetectionRange = 1500.0f;

	// 공격 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|AI|Attack")
	float AttackRange = 500.0f;

	// 라인 정보 캐시
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
	FVector LaneStartPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
	FVector LaneEndPosition = FVector::ZeroVector;

	// 모든 라인 타워 파괴되었는지 확인
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
	bool bAllTowersDestroyed = false;

	// 현재 공격 대상
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
	TObjectPtr<AAOSCharacter> CurrentTarget = nullptr;

	// 현재 이동 목표
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
	FVector CurrentMoveTarget = FVector::ZeroVector;

	// 웨이포인트 큐 (순차적으로 방문할 구조물들)
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
	TArray<AAOSStructure*> WaypointQueue;

	// 현재 웨이포인트 인덱스
	UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
	int32 CurrentWaypointIndex = 0;

	// Phase 6: StateTree AI 컴포넌트 (행동 결정 트리)
	// BP_AOSAIController 의 컴포넌트 디테일 → StateTreeRef 슬롯에 ST_AOSCharacterAI 자산 지정
	// 생성자에서 SetStartLogicAutomatically(false) — StartLogic() 은 StartDeployment() 에서
	// 팀 확정 후 수동 호출 (OnPossess 시점 호출 시 Team2 아군오사 — cpp OnPossess 주석 참고)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|AI|StateTree")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

private:
	// GameMode 캐시 (라운드 상태 확인용)
	UPROPERTY()
	class AAOSGameMode* CachedGameMode = nullptr;

	// StartDeployment 가 호출되었는지 — retry 게이트에서 사용
	bool bDeploymentStarted = false;

	/** GameMode 캐시 우선, TActorIterator fallback 으로 MapManager 반환 (서버 전용). */
	AAOSMapManager* ResolveMapManager() const;

	void CacheLaneInfo();
	void BuildWaypointQueue();

	// Phase 6: UpdateAIBehavior/MoveTowardsTarget/AttackTarget/AttackStructure 제거
	// (StateTree 의 task 가 대체 — 헬퍼는 public 으로 노출)

	// 이동 완료 판정 거리 (캐릭터 충돌 범위 고려)
	const float ArrivalDistance = 100.0f;

	// NavMesh 이동 캐시 — 동일 목표로 중복 요청 방지
	FVector LastNavMoveTarget = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<AAOSCharacter> LastNavMoveActor = nullptr;

	// 디버그 드로우: 캐릭터 이동 경로 (웨이포인트 선/구체/화살표)
	void DrawDebugPath();
};
