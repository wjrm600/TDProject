#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "AOSMapManager.h"
#include "GAS/AOSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
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

	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Possessed character - waiting for deployment command"));
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

	UpdateAIBehavior(DeltaTime);

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

void AAOSAIController::UpdateAIBehavior(float DeltaTime)
{
	// ServerTravel 직후 MapManager 가 늦게 등록되는 race condition 대응:
	// Deployment 는 시작됐지만 WaypointQueue 가 아직 비어있으면 매 tick 재시도.
	if (bDeploymentStarted && WaypointQueue.Num() == 0)
	{
		CacheLaneInfo();
		BuildWaypointQueue();
		if (WaypointQueue.Num() == 0)
		{
			// MapManager 가 아직 준비 안 됨 — 이번 tick 스킵
			return;
		}
		// 큐가 새로 만들어졌으면 첫 목표 위치도 설정
		CurrentMoveTarget = GetNextTargetLocation();
	}

	// Phase 3: CurrentAttackCooldown 멤버 제거 — 쿨타임은 ASC 의 "Cooldown.Attack.Basic" 태그로 관리

	// 가장 가까운 적군 찾기 (우선순위: 캐릭터 > 타워)
	AAOSCharacter* NearestEnemy = FindNearestEnemy();
	if (NearestEnemy)
	{
		CurrentTarget = NearestEnemy;
		AttackTarget(DeltaTime);
		return;
	}

	AAOSStructure* NearestTower = FindNearestEnemyTower();
	if (NearestTower)
	{
		// 적 구조물 공격
		AttackStructure(NearestTower, DeltaTime);
		return;
	}

	// 다음 웨이포인트로 이동
	CurrentMoveTarget = GetNextTargetLocation();
	CurrentTarget = nullptr;
	MoveTowardsTarget(DeltaTime);
}

void AAOSAIController::MoveTowardsTarget(float DeltaTime)
{
	if (!ControlledCharacter)
	{
		return;
	}

	// Z(높이)를 무시한 2D 거리로 도착 판정 — 지형 높낮이 차이 허용
	float Distance2D = FVector::Dist2D(ControlledCharacter->GetActorLocation(), CurrentMoveTarget);

	if (Distance2D <= ArrivalDistance)
	{
		StopMovement();
		LastNavMoveTarget = FVector::ZeroVector;

		if (bAllTowersDestroyed)
		{
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[AI] Arrived at waypoint! 2D Distance: %.1f, Index: %d"),
			Distance2D, CurrentWaypointIndex);

		CurrentWaypointIndex++;
		UE_LOG(LogTemp, Warning, TEXT("[AI] Moving to next waypoint. New index: %d/%d"),
			CurrentWaypointIndex, WaypointQueue.Num() - 1);

		CurrentMoveTarget = GetNextTargetLocation();
		return;
	}

	// NavMesh 이동 요청 — 목표가 50 유닛 이상 바뀔 때만 재요청(매 틱 방지)
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

void AAOSAIController::AttackStructure(AAOSStructure* Structure, float DeltaTime)
{
	if (!ControlledCharacter || !Structure || Structure->IsDestroyed())
	{
		return;
	}

	float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(), Structure->GetActorLocation());
	const float EffectiveRange = GetEffectiveAttackRange();

	if (Distance > EffectiveRange)
	{
		// NavMesh로 구조물 접근
		FVector StructurePos = Structure->GetActorLocation();
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

	// 공격 범위 내 - 멈추고 공격
	StopMovement();
	LastNavMoveTarget = FVector::ZeroVector;

	FVector DirectionToStructure = (Structure->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
	ControlledCharacter->SetActorRotation(DirectionToStructure.Rotation());

	// Phase 3: GA_Attack 트리거 (Cooldown.Attack.Basic 태그가 ASC 에 없을 때만)
	UAbilitySystemComponent* ASC = ControlledCharacter->GetAbilitySystemComponent();
	if (ASC && !ASC->HasMatchingGameplayTag(
		FGameplayTag::RequestGameplayTag(FName("Cooldown.Attack.Basic"))))
	{
		FGameplayEventData EventData;
		EventData.Target = Structure;
		EventData.Instigator = ControlledCharacter;
		ASC->HandleGameplayEvent(
			FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic")),
			&EventData);

		const float NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		UE_LOG(LogTemp, Warning, TEXT("[AI %.3fs] GA_Attack → Structure! HP: %.0f/%.0f"),
			NowSec, Structure->GetCurrentHealth(), Structure->GetMaxHealth());
	}
}

void AAOSAIController::AttackTarget(float DeltaTime)
{
	if (!ControlledCharacter || !CurrentTarget)
	{
		return;
	}

	float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(), CurrentTarget->GetActorLocation());
	const float EffectiveRange = GetEffectiveAttackRange();

	if (Distance > EffectiveRange)
	{
		// MoveToActor: 목표 캐릭터가 움직여도 경로 자동 갱신
		if (LastNavMoveActor != CurrentTarget)
		{
			LastNavMoveTarget = FVector::ZeroVector;
			MoveToActor(CurrentTarget, EffectiveRange * 0.8f,
				/*bStopOnOverlap=*/true,
				/*bUsePathfinding=*/true,
				/*bCanStrafe=*/false);
			LastNavMoveActor = CurrentTarget;
		}
		return;
	}

	// 공격 범위 내 - 멈추고 공격
	StopMovement();
	LastNavMoveActor = nullptr;

	FVector DirectionToEnemy = (CurrentTarget->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
	ControlledCharacter->SetActorRotation(DirectionToEnemy.Rotation());

	// Phase 3: GA_Attack 트리거 (Cooldown.Attack.Basic 태그가 ASC 에 없을 때만)
	UAbilitySystemComponent* ASC = ControlledCharacter->GetAbilitySystemComponent();
	if (ASC && !ASC->HasMatchingGameplayTag(
		FGameplayTag::RequestGameplayTag(FName("Cooldown.Attack.Basic"))))
	{
		FGameplayEventData EventData;
		EventData.Target = CurrentTarget;
		EventData.Instigator = ControlledCharacter;
		ASC->HandleGameplayEvent(
			FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic")),
			&EventData);

		const float NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		UE_LOG(LogTemp, Warning, TEXT("[AI %.3fs] GA_Attack → Enemy! Distance: %.1f"),
			NowSec, Distance);
	}
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
