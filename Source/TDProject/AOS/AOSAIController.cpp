#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "AOSMapManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

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
}

void AAOSAIController::StartDeployment(EAOSLane Lane)
{
	DeployedLane = Lane;
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

	// MapManager 찾기
	AAOSMapManager* MapManager = nullptr;
	for (TActorIterator<AAOSMapManager> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		MapManager = *ActorItr;
		break;
	}

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

	// MapManager 찾기
	AAOSMapManager* MapManager = nullptr;
	for (TActorIterator<AAOSMapManager> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		MapManager = *ActorItr;
		break;
	}

	if (!MapManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[AI Controller] MapManager not found!"));
		return;
	}

	EAOSTeam CharacterTeam = ControlledCharacter->GetTeam();
	LaneStartPosition = MapManager->GetLaneStartPosition(DeployedLane, CharacterTeam);
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

	// MapManager 찾기
	AAOSMapManager* MapManager = nullptr;
	for (TActorIterator<AAOSMapManager> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		MapManager = *ActorItr;
		break;
	}

	if (!MapManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[AI Controller] MapManager not found while building waypoint queue!"));
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
	// 쿨타임 업데이트
	if (CurrentAttackCooldown > 0.0f)
	{
		CurrentAttackCooldown -= DeltaTime;
	}

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

	FVector Direction = (CurrentMoveTarget - ControlledCharacter->GetActorLocation()).GetSafeNormal();
	float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(), CurrentMoveTarget);

	// 도착 판정
	if (Distance <= ArrivalDistance)
	{
		ControlledCharacter->GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// 모든 웨이포인트 완료 시 더 이상 진행하지 않음
		if (bAllTowersDestroyed)
		{
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[AI] Arrived at waypoint! Distance: %.1f, CurrentWaypointIndex: %d"),
			Distance, CurrentWaypointIndex);

		// 다음 웨이포인트로 이동
		CurrentWaypointIndex++;
		UE_LOG(LogTemp, Warning, TEXT("[AI] Moving to next waypoint. New index: %d/%d"),
			CurrentWaypointIndex, WaypointQueue.Num() - 1);

		CurrentMoveTarget = GetNextTargetLocation();
		return;
	}

	// 목표 방향으로 이동
	ControlledCharacter->AddMovementInput(Direction, 1.0f);
}

void AAOSAIController::AttackStructure(AAOSStructure* Structure, float DeltaTime)
{
	if (!ControlledCharacter || !Structure || Structure->IsDestroyed())
	{
		return;
	}

	float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(), Structure->GetActorLocation());

	if (Distance > AttackRange)
	{
		// 구조물 방향으로 이동
		FVector Direction = (Structure->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
		ControlledCharacter->AddMovementInput(Direction, 1.0f);
		return;
	}

	// 공격 범위 내 - 멈추고 공격
	ControlledCharacter->GetCharacterMovement()->Velocity = FVector::ZeroVector;

	// 구조물 방향으로 회전
	FVector DirectionToStructure = (Structure->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
	ControlledCharacter->SetActorRotation(DirectionToStructure.Rotation());

	if (CurrentAttackCooldown <= 0.0f)
	{
		Structure->ReceiveDamage(ControlledCharacter->GetAttackDamage());
		CurrentAttackCooldown = AttackCooldownDuration;
		UE_LOG(LogTemp, Warning, TEXT("[AI] Attacking structure! HP: %.0f/%.0f"),
			Structure->GetCurrentHealth(), Structure->GetMaxHealth());
	}
}

void AAOSAIController::AttackTarget(float DeltaTime)
{
	if (!ControlledCharacter || !CurrentTarget)
	{
		return;
	}

	float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(), CurrentTarget->GetActorLocation());

	// 공격 범위 확인
	if (Distance > AttackRange)
	{
		// 적을 추격
		FVector Direction = (CurrentTarget->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
		ControlledCharacter->AddMovementInput(Direction, 1.0f);
		return;
	}

	// 공격 범위 내 - 멈추고 공격
	ControlledCharacter->GetCharacterMovement()->Velocity = FVector::ZeroVector;

	// 적 방향으로 회전
	FVector DirectionToEnemy = (CurrentTarget->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
	FRotator LookAtRotation = DirectionToEnemy.Rotation();
	ControlledCharacter->SetActorRotation(LookAtRotation);

	// 공격 실행
	if (CurrentAttackCooldown <= 0.0f)
	{
		CurrentTarget->ReceiveDamage(ControlledCharacter->GetAttackDamage());
		CurrentAttackCooldown = AttackCooldownDuration;
		UE_LOG(LogTemp, Warning, TEXT("[AI] Attacking enemy! Distance: %.1f"), Distance);
	}
}
