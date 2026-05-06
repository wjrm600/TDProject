#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "AOSMapManager.h"
#include "GAS/AOSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarShowCharacterPaths(
    TEXT("AOS.Debug.ShowCharacterPaths"), 0,
    TEXT("1=AI 캐릭터 웨이포인트 경로 표시, 0=숨김"));

AAOSMapManager* AAOSAIController::ResolveMapManager() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// 1) GameMode 가 캐시한 인스턴스 우선 (서버 권한, O(1))
	if (AAOSGameMode* GM = World->GetAuthGameMode<AAOSGameMode>())
	{
		if (AAOSMapManager* MM = GM->GetMapManager())
		{
			return MM;
		}
	}

	// 2) Fallback: TActorIterator 탐색 (GameMode 가 아직 캐시 못 했거나 에디터 환경)
	for (TActorIterator<AAOSMapManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

float AAOSAIController::GetEffectiveAttackRange() const
{
	// 우선: 캐릭터의 AttributeSet (DT 적용된 단일 진실 공급원, 디버그 시각화와 일치)
	if (ControlledCharacter)
	{
		if (UAOSAttributeSet* AttrSet = ControlledCharacter->GetAttributeSet())
		{
			const float AttrRange = AttrSet->GetAttackRange();
			if (AttrRange > 0.0f)
			{
				return AttrRange;
			}
		}
	}
	// Fallback: AIController.AttackRange 멤버 (BP override 가능, AttributeSet 미초기화 시)
	return AttackRange;
}

// AOS.Debug.ShowAttackRange 는 AOSMapManager.cpp 에서 정의됨 — 여기서 재정의 금지

AAOSAIController::AAOSAIController()
{
	bAttachToPawn = true;

	// Phase 6: StateTree AI 컴포넌트 부착
	// (BP_AOSAIController 의 디테일에서 StateTreeRef 슬롯에 ST_AOSCharacterAI 지정)
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));

	// 자동 시작 비활성화 — BeginPlay 시 GetPawn()=null 이라 schema context actor binding 실패
	// OnPossess 가 ControlledCharacter 설정한 직후 수동으로 StartLogic() 호출
	if (StateTreeComponent)
	{
		StateTreeComponent->SetStartLogicAutomatically(false);
	}
}

AAOSAIController::~AAOSAIController()
{
	// 웨이포인트 큐 정리
	WaypointQueue.Empty();

	// 참조 정리
	ControlledCharacter = nullptr;
	CurrentTarget = nullptr;
}

void AAOSAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// OnPossess는 SpawnActor → BeginPlay → SpawnDefaultController 과정에서 호출됨
	// 이 시점에서는 팀/라인이 아직 설정되지 않았으므로 ControlledCharacter만 캐시
	// 실제 배포(StartDeployment)는 InitializeCharacter → DeployToLane에서 호출됨
	ControlledCharacter = Cast<AAOSCharacter>(InPawn);

	if (!ControlledCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("[AI Controller] Failed to possess character - InPawn is not AAOSCharacter!"));
		return;
	}

	// Phase 6: StateTree 수동 시작 — Pawn 이 possess 된 상태에서 schema 가 context actor 를 찾을 수 있음
	if (StateTreeComponent)
	{
		if (!StateTreeComponent->IsRunning())
		{
			StateTreeComponent->StartLogic();
			UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Possessed %s — StateTreeComponent::StartLogic() 호출, IsRunning=%s"),
				*InPawn->GetName(),
				StateTreeComponent->IsRunning() ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Possessed %s — StateTreeComponent 이미 실행 중"),
				*InPawn->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AI Controller] Possessed %s — StateTreeComponent is NULL!"),
			*InPawn->GetName());
	}
}

void AAOSAIController::BeginPlay()
{
	Super::BeginPlay();

	// GameMode 캐시 (라운드 상태 확인용)
	CachedGameMode = Cast<AAOSGameMode>(GetWorld()->GetAuthGameMode());

	// CDO 자동 보정 제거됨 — BP에서 설정한 값을 그대로 존중
	// (BP_AOSAIController CDO를 1500/500으로 수정 완료)
	UE_LOG(LogTemp, Log, TEXT("[AIController] 초기화 완료 - EnemyDetectionRange=%.0f, AttackRange=%.0f (BP CDO 값 사용)"),
		EnemyDetectionRange, AttackRange);
}

void AAOSAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// RoundRunning 상태가 아니면 AI 행동 중지
	if (CachedGameMode && CachedGameMode->GetAOSGameState() != EAOSGameState::RoundRunning)
	{
		return;
	}

	if (!ControlledCharacter || !ControlledCharacter->IsAlive())
	{
		// 캐릭터 사망 시 AI 정리
		if (ControlledCharacter && !ControlledCharacter->IsAlive())
		{
			StopMovement();
			WaypointQueue.Empty();
			CurrentTarget = nullptr;
			ControlledCharacter = nullptr;
		}
		return;
	}

	// Phase 6: 행동 결정은 StateTree 가 담당 (UpdateAIBehavior 호출 제거)
	// ServerTravel 직후 race condition 대응 — WaypointQueue 가 비어있으면 재시도
	if (bDeploymentStarted && WaypointQueue.Num() == 0)
	{
		CacheLaneInfo();
		BuildWaypointQueue();
		if (WaypointQueue.Num() > 0)
		{
			CurrentMoveTarget = GetNextTargetLocation();
		}
	}

	// ─── 디버그: 캐릭터 이동 경로 (탑/미드/바텀 라인별) ───
	if (ControlledCharacter && GetWorld() &&
	    CVarShowCharacterPaths.GetValueOnGameThread())
	{
	    DrawDebugPath();
	}

	// ─── 디버그: 공격 범위 / 감지 범위 (AOS.Debug.ShowAttackRange — AOSMapManager.cpp 정의) ───
	// DS 모드에서는 렌더 파이프라인이 없으므로 스킵
	if (GetNetMode() != NM_DedicatedServer)
	{
		IConsoleVariable* ShowAttackRangeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowAttackRange"));
		if (ControlledCharacter && GetWorld() && ShowAttackRangeCVar && ShowAttackRangeCVar->GetInt())
		{
			FVector CharPos = ControlledCharacter->GetActorLocation();

			// 공격 범위 (AttackRange): 노란색 수평 원 + 라벨
			DrawDebugCircle(GetWorld(), CharPos, AttackRange, 32,
				FColor::Yellow, false, 0.0f, 0, 3.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
			DrawDebugString(GetWorld(), CharPos + FVector(AttackRange, 0, 50.0f),
				FString::Printf(TEXT("[캐릭터] 공격 %.0f"), AttackRange),
				nullptr, FColor::Yellow, 0.0f, true, 1.2f);

			// 감지 범위 (EnemyDetectionRange): 흰색 수평 원 + 라벨 (더 큰 원)
			DrawDebugCircle(GetWorld(), CharPos, EnemyDetectionRange, 32,
				FColor::White, false, 0.0f, 0, 3.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
			DrawDebugString(GetWorld(), CharPos + FVector(EnemyDetectionRange, 0, 50.0f),
				FString::Printf(TEXT("[캐릭터] 감지 %.0f"), EnemyDetectionRange),
				nullptr, FColor::White, 0.0f, true, 1.2f);
		}
	}
}

void AAOSAIController::StartDeployment(EAOSLane Lane)
{
	DeployedLane = Lane;
	bDeploymentStarted = true;
	CacheLaneInfo();

	// 웨이포인트 큐 구축 (아군 타워 → 적 타워 → 적 커맨드 센터)
	BuildWaypointQueue();

	// 첫번째 목표 위치 설정
	CurrentMoveTarget = GetNextTargetLocation();

	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Deployment started on lane: %d, Waypoints: %d"),
		static_cast<int32>(Lane), WaypointQueue.Num());
}

FVector AAOSAIController::GetNextTargetLocation()
{
	if (!ControlledCharacter)
	{
		return FVector::ZeroVector;
	}

	// 웨이포인트 큐가 비어있으면 기본 위치 반환
	if (WaypointQueue.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AI] Waypoint queue is empty!"));
		return LaneEndPosition;
	}

	// 현재 웨이포인트가 유효한지 확인
	while (CurrentWaypointIndex < WaypointQueue.Num())
	{
		AAOSStructure* CurrentWaypoint = WaypointQueue[CurrentWaypointIndex];

		// 웨이포인트가 유효하고 파괴되지 않았으면 해당 위치로 이동
		if (CurrentWaypoint && !CurrentWaypoint->IsDestroyed())
		{
			FString StructureType = CurrentWaypoint->GetStructureType() == EStructureType::Tower ? TEXT("Tower") : TEXT("CommandCenter");
			FString TeamName = CurrentWaypoint->GetOwnerTeam() == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2");

			UE_LOG(LogTemp, Warning, TEXT("[AI] Target: Waypoint[%d/%d] - %s (%s) at (%.0f, %.0f, %.0f)"),
				CurrentWaypointIndex, WaypointQueue.Num() - 1,
				*StructureType, *TeamName,
				CurrentWaypoint->GetActorLocation().X,
				CurrentWaypoint->GetActorLocation().Y,
				CurrentWaypoint->GetActorLocation().Z);

			return CurrentWaypoint->GetActorLocation();
		}

		// 현재 웨이포인트가 파괴되었으면 다음으로 넘어감
		UE_LOG(LogTemp, Warning, TEXT("[AI] Waypoint[%d] destroyed, moving to next waypoint"), CurrentWaypointIndex);
		CurrentWaypointIndex++;
	}

	// 모든 웨이포인트를 통과했으면 마지막 위치 반환
	UE_LOG(LogTemp, Warning, TEXT("[AI] All waypoints completed!"));
	bAllTowersDestroyed = true;
	return LaneEndPosition;
}

AAOSCharacter* AAOSAIController::FindNearestEnemy()
{
	if (!ControlledCharacter)
	{
		return nullptr;
	}

	float NearestDistance = FLT_MAX;
	AAOSCharacter* NearestEnemy = nullptr;

	for (TActorIterator<AAOSCharacter> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		AAOSCharacter* FoundCharacter = *ActorItr;
		if (FoundCharacter && FoundCharacter->IsAlive() && FoundCharacter->GetTeam() != ControlledCharacter->GetTeam())
		{
			float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(), FoundCharacter->GetActorLocation());
			if (Distance < NearestDistance && Distance <= EnemyDetectionRange)
			{
				NearestDistance = Distance;
				NearestEnemy = FoundCharacter;
			}
		}
	}

	return NearestEnemy;
}

AAOSStructure* AAOSAIController::FindNearestEnemyTower()
{
	if (!ControlledCharacter)
	{
		return nullptr;
	}

	AAOSMapManager* MapManager = ResolveMapManager();
	if (!MapManager)
	{
		return nullptr;
	}

	EAOSTeam EnemyTeam = (ControlledCharacter->GetTeam() == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;
	TArray<AAOSStructure*> TowersInLane = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);

	float NearestDistance = FLT_MAX;
	AAOSStructure* NearestTower = nullptr;

	for (AAOSStructure* Tower : TowersInLane)
	{
		if (Tower && !Tower->IsDestroyed())
		{
			float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(), Tower->GetActorLocation());
			if (Distance < NearestDistance && Distance <= EnemyDetectionRange)
			{
				NearestDistance = Distance;
				NearestTower = Tower;
			}
		}
	}

	return NearestTower;
}

void AAOSAIController::CacheLaneInfo()
{
	if (!ControlledCharacter)
	{
		return;
	}

	AAOSMapManager* MapManager = ResolveMapManager();
	if (!MapManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AI Controller] MapManager not yet ready in CacheLaneInfo — will retry on next tick"));
		return;
	}

	EAOSTeam CharacterTeam = ControlledCharacter->GetTeam();

	// 캐릭터가 SpawnPoint 에서 막 스폰된 직후 캐싱되므로
	// 캐릭터 현재 위치 = 라인 시작점. MapManager 우회로 단순화 (O(1), 결합도 감소).
	LaneStartPosition = ControlledCharacter->GetActorLocation();
	LaneEndPosition = MapManager->GetLaneEndPosition(DeployedLane, CharacterTeam);

	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Cached lane info - Team: %d, Lane: %d"),
		static_cast<int32>(CharacterTeam), static_cast<int32>(DeployedLane));
	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Start: (%.1f, %.1f, %.1f), End: (%.1f, %.1f, %.1f)"),
		LaneStartPosition.X, LaneStartPosition.Y, LaneStartPosition.Z,
		LaneEndPosition.X, LaneEndPosition.Y, LaneEndPosition.Z);
}

void AAOSAIController::BuildWaypointQueue()
{
	WaypointQueue.Empty();
	CurrentWaypointIndex = 0;

	if (!ControlledCharacter)
	{
		return;
	}

	AAOSMapManager* MapManager = ResolveMapManager();
	if (!MapManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AI Controller] MapManager not yet ready in BuildWaypointQueue — will retry on next tick"));
		return;
	}

	EAOSTeam MyTeam = ControlledCharacter->GetTeam();
	EAOSTeam EnemyTeam = (MyTeam == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;

	// 1단계: 아군 타워들을 순서대로 추가 (스폰 지점에서 가까운 순)
	TArray<AAOSStructure*> FriendlyTowers = MapManager->GetTowersInLane(DeployedLane, MyTeam);

	// 스폰 지점에서 가까운 순서로 정렬 (역순으로)
	FriendlyTowers.Sort([this](const AAOSStructure& A, const AAOSStructure& B)
	{
		float DistA = FVector::Dist(LaneStartPosition, A.GetActorLocation());
		float DistB = FVector::Dist(LaneStartPosition, B.GetActorLocation());
		return DistA > DistB; // 먼 것부터 (뒤에서부터 추가하기 위해)
	});

	// 역순으로 추가 (가장 가까운 타워가 먼저 오도록)
	for (int32 i = FriendlyTowers.Num() - 1; i >= 0; i--)
	{
		if (FriendlyTowers[i] && !FriendlyTowers[i]->IsDestroyed())
		{
			WaypointQueue.Add(FriendlyTowers[i]);
		}
	}

	// 2단계: 적 타워들을 순서대로 추가 (내 진영에서 가까운 순)
	TArray<AAOSStructure*> EnemyTowers = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);

	// 내 스폰 지점에서 가까운 순서로 정렬
	EnemyTowers.Sort([this](const AAOSStructure& A, const AAOSStructure& B)
	{
		float DistA = FVector::Dist(LaneStartPosition, A.GetActorLocation());
		float DistB = FVector::Dist(LaneStartPosition, B.GetActorLocation());
		return DistA < DistB; // 가까운 것부터
	});

	for (AAOSStructure* Tower : EnemyTowers)
	{
		if (Tower && !Tower->IsDestroyed())
		{
			WaypointQueue.Add(Tower);
		}
	}

	// 3단계: 마지막으로 적 커맨드 센터 추가
	AAOSStructure* EnemyCommandCenter = MapManager->GetCommandCenter(EnemyTeam);
	if (EnemyCommandCenter)
	{
		WaypointQueue.Add(EnemyCommandCenter);
	}

	// 디버그 로그
	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Waypoint Queue Built: %d waypoints"), WaypointQueue.Num());
	for (int32 i = 0; i < WaypointQueue.Num(); i++)
	{
		if (WaypointQueue[i])
		{
			FString StructureType = WaypointQueue[i]->GetStructureType() == EStructureType::Tower ? TEXT("Tower") : TEXT("CommandCenter");
			FString TeamName = WaypointQueue[i]->GetOwnerTeam() == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2");
			UE_LOG(LogTemp, Warning, TEXT("  [%d] %s (%s) at (%.0f, %.0f, %.0f)"),
				i, *StructureType, *TeamName,
				WaypointQueue[i]->GetActorLocation().X,
				WaypointQueue[i]->GetActorLocation().Y,
				WaypointQueue[i]->GetActorLocation().Z);
		}
	}
}

// =============================================================================
// Phase 6: StateTree task/condition 이 호출하는 헬퍼들
// (UpdateAIBehavior / MoveTowardsTarget / AttackTarget / AttackStructure 가
//  StateTree 의 task 로 분리되며 제거됨. 헬퍼는 task 가 사용)
// =============================================================================

void AAOSAIController::SetCurrentTarget(AAOSCharacter* InTarget)
{
	CurrentTarget = InTarget;
}

AAOSStructure* AAOSAIController::GetCurrentWaypointStructure() const
{
	if (CurrentWaypointIndex < 0 || CurrentWaypointIndex >= WaypointQueue.Num())
	{
		return nullptr;
	}

	AAOSStructure* WP = WaypointQueue[CurrentWaypointIndex];
	if (!WP || WP->IsDestroyed())
	{
		return nullptr;
	}

	// 적 구조물만 반환 (아군 웨이포인트는 통과 대상이지 공격 대상 아님)
	if (ControlledCharacter && WP->GetOwnerTeam() == ControlledCharacter->GetTeam())
	{
		return nullptr;
	}
	return WP;
}

bool AAOSAIController::IsCurrentTargetInAttackRange() const
{
	if (!ControlledCharacter || !CurrentTarget) return false;
	const float Distance = FVector::Dist(
		ControlledCharacter->GetActorLocation(),
		CurrentTarget->GetActorLocation());
	return Distance <= GetEffectiveAttackRange();
}

bool AAOSAIController::HasArrivedAtCurrentWaypoint() const
{
	if (!ControlledCharacter) return false;
	const float Distance2D = FVector::Dist2D(
		ControlledCharacter->GetActorLocation(),
		CurrentMoveTarget);
	return Distance2D <= ArrivalDistance;
}

void AAOSAIController::RequestMoveToCurrentTarget()
{
	if (!ControlledCharacter || !CurrentTarget) return;

	const float EffectiveRange = GetEffectiveAttackRange();

	// MoveToActor: 목표가 움직여도 경로 자동 갱신
	if (LastNavMoveActor != CurrentTarget)
	{
		LastNavMoveTarget = FVector::ZeroVector;
		MoveToActor(CurrentTarget, EffectiveRange * 0.8f,
			/*bStopOnOverlap=*/true,
			/*bUsePathfinding=*/true,
			/*bCanStrafe=*/false);
		LastNavMoveActor = CurrentTarget;
	}

	// 사거리 내면 회전 + 정지
	if (IsCurrentTargetInAttackRange())
	{
		StopMovement();
		LastNavMoveActor = nullptr;
		FVector Dir = (CurrentTarget->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
		ControlledCharacter->SetActorRotation(Dir.Rotation());
	}
}

void AAOSAIController::RequestMoveToCurrentWaypoint()
{
	if (!ControlledCharacter) return;

	// 현재 웨이포인트가 적 구조물이면 사거리 내까지만 접근
	AAOSStructure* WPStruct = GetCurrentWaypointStructure();
	if (WPStruct)
	{
		const float EffectiveRange = GetEffectiveAttackRange();
		const float Distance = FVector::Dist(
			ControlledCharacter->GetActorLocation(), WPStruct->GetActorLocation());

		if (Distance <= EffectiveRange)
		{
			StopMovement();
			LastNavMoveTarget = FVector::ZeroVector;
			FVector Dir = (WPStruct->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
			ControlledCharacter->SetActorRotation(Dir.Rotation());
			return;
		}

		FVector StructurePos = WPStruct->GetActorLocation();
		if (FVector::Dist(LastNavMoveTarget, StructurePos) > 50.0f)
		{
			LastNavMoveActor = nullptr;
			MoveToLocation(StructurePos, EffectiveRange * 0.8f,
				/*bStopOnOverlap=*/true,
				/*bUsePathfinding=*/true,
				/*bProjectDestinationToNavigation=*/true,
				/*bCanStrafe=*/false);
			LastNavMoveTarget = StructurePos;
		}
		return;
	}

	// 일반 위치 이동 (CurrentMoveTarget 사용)
	if (FVector::Dist(LastNavMoveTarget, CurrentMoveTarget) > 50.0f)
	{
		LastNavMoveActor = nullptr;
		MoveToLocation(CurrentMoveTarget, ArrivalDistance * 0.5f,
			/*bStopOnOverlap=*/true,
			/*bUsePathfinding=*/true,
			/*bProjectDestinationToNavigation=*/true,
			/*bCanStrafe=*/false);
		LastNavMoveTarget = CurrentMoveTarget;
	}
}

void AAOSAIController::AdvanceToNextWaypoint()
{
	if (bAllTowersDestroyed) return;

	StopMovement();
	LastNavMoveTarget = FVector::ZeroVector;

	CurrentWaypointIndex++;
	UE_LOG(LogTemp, Log, TEXT("[AI] AdvanceToNextWaypoint — index=%d/%d"),
		CurrentWaypointIndex, WaypointQueue.Num() - 1);

	CurrentMoveTarget = GetNextTargetLocation();
}

void AAOSAIController::DrawDebugPath()
{
	if (!ControlledCharacter || WaypointQueue.Num() == 0)
		return;

	// 라인별 색상
	FColor PathColor;
	switch (DeployedLane)
	{
	case EAOSLane::Top:    PathColor = FColor::Yellow; break;
	case EAOSLane::Mid:    PathColor = FColor::Green;  break;
	case EAOSLane::Bottom: PathColor = FColor::Cyan;   break;
	default:               PathColor = FColor::White;  break;
	}

	// 팀2는 어두운 버전
	if (ControlledCharacter->GetTeam() == EAOSTeam::Team2)
	{
		PathColor = FColor(PathColor.R / 2, PathColor.G / 2, PathColor.B / 2);
	}

	FVector CharPos = ControlledCharacter->GetActorLocation();

	// 캐릭터 현재 위치 → 남은 웨이포인트 순서로 선 그리기
	FVector PrevPos = CharPos;
	for (int32 i = CurrentWaypointIndex; i < WaypointQueue.Num(); ++i)
	{
		if (!WaypointQueue[i] || WaypointQueue[i]->IsDestroyed())
			continue;

		FVector WPPos = WaypointQueue[i]->GetActorLocation();
		DrawDebugLine(GetWorld(), PrevPos, WPPos, PathColor, false, 0.0f, 0, 3.0f);

		// 웨이포인트 노드 표시 (작은 구체)
		DrawDebugSphere(GetWorld(), WPPos, 80.0f, 8, PathColor, false, 0.0f);
		PrevPos = WPPos;
	}

	// 캐릭터 위치에 방향 화살표 (현재 이동 목표 방향)
	if (CurrentWaypointIndex < WaypointQueue.Num() && WaypointQueue[CurrentWaypointIndex])
	{
		FVector Dir = (WaypointQueue[CurrentWaypointIndex]->GetActorLocation() - CharPos);
		Dir.Normalize();
		DrawDebugDirectionalArrow(GetWorld(), CharPos, CharPos + Dir * 300.0f,
			60.0f, PathColor, false, 0.0f, 0, 3.0f);
	}
}
