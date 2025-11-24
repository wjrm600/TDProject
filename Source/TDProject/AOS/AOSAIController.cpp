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

void AAOSAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// OnPossess는 Pawn::BeginPlay() 중에 호출됨
	// 이 시점에서 ControlledCharacter 초기화
	ControlledCharacter = Cast<AAOSCharacter>(InPawn);

	if (!ControlledCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("[AI Controller] Failed to possess character - InPawn is not AAOSCharacter!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Possessed character in lane: %d"),
		static_cast<int32>(ControlledCharacter->GetLane()));

	// 배포 시작 (이곳에서 바로 호출)
	StartDeployment(ControlledCharacter->GetLane());
}

void AAOSAIController::BeginPlay()
{
	Super::BeginPlay();

	// OnPossess에서 이미 모든 초기화 및 배포 시작됨
}

void AAOSAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ControlledCharacter || !ControlledCharacter->IsAlive())
	{
		return;
	}

	UpdateAIBehavior(DeltaTime);
}

void AAOSAIController::StartDeployment(EAOSLane Lane)
{
	DeployedLane = Lane;
	CacheLaneInfo();

	// 첫번째 목표 위치 설정
	CurrentMoveTarget = GetNextTargetLocation();

	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Deployment started on lane: %d"), static_cast<int32>(Lane));
}

FVector AAOSAIController::GetNextTargetLocation()
{
	if (!ControlledCharacter)
	{
		return FVector::ZeroVector;
	}

	// MapManager 찾기 (월드에 있는 AAOSMapManager 액터 검색)
	AAOSMapManager* MapManager = nullptr;
	for (TActorIterator<AAOSMapManager> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		MapManager = *ActorItr;
		break;
	}

	if (!MapManager)
	{
		return LaneEndPosition;
	}

	EAOSTeam CharacterTeam = ControlledCharacter->GetTeam();
	EAOSTeam EnemyTeam = (CharacterTeam == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;

	// 다음 목표 결정 (타워 > 커맨드 센터)
	TArray<AAOSStructure*> TowersInLane = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);

	if (TowersInLane.Num() > 0 && NextTowerIndex < TowersInLane.Num())
	{
		AAOSStructure* TargetTower = TowersInLane[NextTowerIndex];
		if (TargetTower && !TargetTower->IsDestroyed())
		{
			return TargetTower->GetActorLocation();
		}
		else
		{
			NextTowerIndex++;
			return GetNextTargetLocation(); // 다음 타워로
		}
	}

	// 모든 타워 파괴 완료 - 커맨드 센터로 이동
	bAllTowersDestroyed = true;
	AAOSStructure* CommandCenter = MapManager->GetCommandCenter(EnemyTeam);
	if (CommandCenter)
	{
		return CommandCenter->GetActorLocation();
	}

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
		return;
	}

	LaneStartPosition = MapManager->GetLaneStartPosition(DeployedLane, ControlledCharacter->GetTeam());
	LaneEndPosition = MapManager->GetLaneEndPosition(DeployedLane, ControlledCharacter->GetTeam());

	UE_LOG(LogTemp, Warning, TEXT("[AI Controller] Cached lane info - Start: (%.1f, %.1f, %.1f), End: (%.1f, %.1f, %.1f)"),
		LaneStartPosition.X, LaneStartPosition.Y, LaneStartPosition.Z,
		LaneEndPosition.X, LaneEndPosition.Y, LaneEndPosition.Z);
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
		// 타워 방향으로 이동
		CurrentMoveTarget = NearestTower->GetActorLocation();
	}
	else
	{
		// 다음 목표로 이동
		CurrentMoveTarget = GetNextTargetLocation();
	}

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
		UE_LOG(LogTemp, Warning, TEXT("[AI] Arrived at target: (%.1f, %.1f, %.1f)"),
			CurrentMoveTarget.X, CurrentMoveTarget.Y, CurrentMoveTarget.Z);

		// 다음 목표 업데이트
		if (!bAllTowersDestroyed)
		{
			NextTowerIndex++;
		}
		CurrentMoveTarget = GetNextTargetLocation();
		return;
	}

	// 목표 방향으로 이동
	ControlledCharacter->AddMovementInput(Direction, 1.0f);
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
		CurrentTarget->ReceiveDamage(10.0f);
		CurrentAttackCooldown = AttackCooldownDuration;
		UE_LOG(LogTemp, Warning, TEXT("[AI] Attacking enemy! Distance: %.1f"), Distance);
	}
}
