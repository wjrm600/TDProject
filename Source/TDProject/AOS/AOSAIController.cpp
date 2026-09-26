#include "AOSAIController.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "AOSMapManager.h"
#include "GAS/AOSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "Navigation/PathFollowingComponent.h"	// EPathFollowingRequestResult 값 정의 (AIController.h 는 전방 선언만)
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

	// Phase 6 + 아군오사(FF) 수정: StateTree 시작(StartLogic)을 OnPossess 에서
	// StartDeployment 로 이동했다.
	//   OnPossess 는 SpawnActor 중 auto-possess 로 호출되어 SetTeam 보다 먼저 실행됨.
	//   여기서 StartLogic 을 하면 StateTree 가 팀 기본값(Team1) 상태로 첫 평가를 수행 →
	//   두번째로 스폰되는 Team2 캐릭터가 이미 Team2 로 설정된 동료를 적으로 오인하고
	//   타겟을 락 → 스폰 직후 자기 진영에서 아군끼리 기본공격(데미지=공격자 AP).
	//   StartDeployment 는 SetTeam → DeployToLane 이후에 호출되므로 팀이 확정된 뒤 안전하게 시작.
	if (!StateTreeComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[AI Controller] Possessed %s — StateTreeComponent is NULL!"),
			*InPawn->GetName());
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[AI Controller] Possessed %s — ControlledCharacter 캐시됨 (StartLogic 은 StartDeployment 에서 호출)"),
		*InPawn->GetName());

	// 선호 행동 메모리: 기본공격 시작/종료를 ASC 태그 이벤트로 기록 (공격을 어디서 발동하든 한 곳에서 집계).
	// GA_Attack 의 ActivationOwnedTags(Ability.Attack.Basic) 가 붙으면 시작(+1), 떨어지면 종료 시각 기록.
	if (UAbilitySystemComponent* ASC = ControlledCharacter->GetAbilitySystemComponent())
	{
		static const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"));
		BasicAttackTagHandle = ASC->RegisterGameplayTagEvent(AttackTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AAOSAIController::OnBasicAttackTagChanged);
		BoundAbilitySystem = ASC;
	}
}

void AAOSAIController::OnUnPossess()
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
	{
		static const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"));
		ASC->RegisterGameplayTagEvent(AttackTag, EGameplayTagEventType::NewOrRemoved).Remove(BasicAttackTagHandle);
	}
	BoundAbilitySystem.Reset();
	BasicAttackTagHandle.Reset();
	EndStrafe();

	Super::OnUnPossess();
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
		// 주의: StopMovement() 는 PathFollowing 의 AbortMove 를 트리거하여
		//       CMC 의 root motion 적용을 방해할 수 있음. Brain.StopLogic 이 이미
		//       OnCharacterDeath 에서 StateTree 를 정지시키므로 추가 StopMovement 불필요.
		if (ControlledCharacter && !ControlledCharacter->IsAlive())
		{
			// StopMovement();  // 의도적으로 호출하지 않음 — root motion 적용 보장
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

	// ─── 디버그: 감지 범위 (AOS.Debug.ShowAttackRange — AOSMapManager.cpp 정의) ───
	// 공격 범위 원은 AOSCharacter::Tick 이 AttributeSet 값(팀 색)으로 그린다. 여기서 멤버 AttackRange
	// (GetEffectiveAttackRange 의 fallback 일 뿐)를 또 그리면 실제 사거리와 다른 원이 겹쳐 보여 제거함.
	// DS 모드에서는 렌더 파이프라인이 없으므로 스킵
	if (GetNetMode() != NM_DedicatedServer)
	{
		IConsoleVariable* ShowAttackRangeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowAttackRange"));
		if (ControlledCharacter && GetWorld() && ShowAttackRangeCVar && ShowAttackRangeCVar->GetInt())
		{
			FVector CharPos = ControlledCharacter->GetActorLocation();

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

	// 아군오사(FF) 수정: 팀(SetTeam)·라인·웨이포인트가 모두 설정된 지금 StateTree 시작.
	// (OnPossess 가 아니라 여기서 시작해야 팀 기본값으로 인한 아군 오인공격이 없음.
	//  이 시점엔 pawn 이 possess 된 상태라 schema 의 context actor(AOSCharacter) binding 도 정상.)
	if (StateTreeComponent && !StateTreeComponent->IsRunning())
	{
		// 캐릭터별 AI 개성: BP_Char_* 에 AIStateTreeOverride 가 있으면 공용 트리 대신 그 트리를 실행.
		// SetStateTree 는 실행 중엔 거부되므로 반드시 StartLogic 직전(여기)에서 교체한다.
		if (UStateTree* OverrideTree = ControlledCharacter ? ControlledCharacter->GetAIStateTreeOverride() : nullptr)
		{
			StateTreeComponent->SetStateTree(OverrideTree);
			UE_LOG(LogTemp, Warning, TEXT("[AI Controller] %s → 전용 StateTree '%s' 사용"),
				*ControlledCharacter->GetName(), *OverrideTree->GetName());
		}

		StateTreeComponent->StartLogic();
		UE_LOG(LogTemp, Warning,
			TEXT("[AI Controller] StartDeployment → StateTree StartLogic() 호출, IsRunning=%s"),
			StateTreeComponent->IsRunning() ? TEXT("true") : TEXT("false"));
	}
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

// =============================================================================
// 선호 행동(캐릭터별 AI 개성) 헬퍼 — AI/AOSStateTreeBehaviorNodes 가 사용
// =============================================================================

void AAOSAIController::OnBasicAttackTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// 공격 시작 (GA_Attack 활성화) — 1회로 집계
		++BasicAttackCount;
	}
	else if (const UWorld* World = GetWorld())
	{
		// 공격 종료 (몽타주 끝 → EndAbility) — 옆걸음 창의 시작점
		LastBasicAttackEndTime = World->GetTimeSeconds();
	}
}

bool AAOSAIController::IsBasicAttackActive() const
{
	const UAbilitySystemComponent* ASC = BoundAbilitySystem.Get();
	if (!ASC) return false;
	static const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"));
	return ASC->HasMatchingGameplayTag(AttackTag);
}

bool AAOSAIController::IsInPostAttackWindow(float WindowSeconds) const
{
	if (LastBasicAttackEndTime < 0.0 || IsBasicAttackActive()) return false;
	const UWorld* World = GetWorld();
	if (!World) return false;
	return (World->GetTimeSeconds() - LastBasicAttackEndTime) < WindowSeconds;
}

int32 AAOSAIController::CountEnemiesInRadius(float Radius) const
{
	if (!ControlledCharacter) return 0;

	const FVector MyLoc = ControlledCharacter->GetActorLocation();
	const EAOSTeam MyTeam = ControlledCharacter->GetTeam();
	int32 Count = 0;
	for (TActorIterator<AAOSCharacter> It(GetWorld()); It; ++It)
	{
		const AAOSCharacter* Other = *It;
		if (!Other || !Other->IsAlive() || Other->GetTeam() == MyTeam) continue;
		if (FVector::Dist(MyLoc, Other->GetActorLocation()) <= Radius)
		{
			++Count;
		}
	}
	return Count;
}

AAOSCharacter* AAOSAIController::FindLowestMaxHealthEnemyInRadius(float Radius) const
{
	if (!ControlledCharacter) return nullptr;

	const FVector MyLoc = ControlledCharacter->GetActorLocation();
	const EAOSTeam MyTeam = ControlledCharacter->GetTeam();
	AAOSCharacter* Best = nullptr;
	float BestMaxHealth = FLT_MAX;
	float BestDist = FLT_MAX;
	for (TActorIterator<AAOSCharacter> It(GetWorld()); It; ++It)
	{
		AAOSCharacter* Other = *It;
		if (!Other || !Other->IsAlive() || Other->GetTeam() == MyTeam) continue;

		const float Dist = FVector::Dist(MyLoc, Other->GetActorLocation());
		if (Dist > Radius) continue;

		// 최대 체력 오름차순, 동률(0.5 이내)이면 가까운 쪽 — 매 tick 재선택해도 타겟이 흔들리지 않게
		const float OtherMax = Other->GetMaxHealth();
		const bool bLower = OtherMax < BestMaxHealth - 0.5f;
		const bool bTieCloser = FMath::Abs(OtherMax - BestMaxHealth) <= 0.5f && Dist < BestDist;
		if (bLower || bTieCloser)
		{
			Best = Other;
			BestMaxHealth = OtherMax;
			BestDist = Dist;
		}
	}
	return Best;
}

AAOSStructure* AAOSAIController::FindNearestFriendlyStructure() const
{
	if (!ControlledCharacter) return nullptr;

	const FVector MyLoc = ControlledCharacter->GetActorLocation();
	const EAOSTeam MyTeam = ControlledCharacter->GetTeam();
	AAOSStructure* NearestTower = nullptr;
	AAOSStructure* CommandCenter = nullptr;
	float NearestDist = FLT_MAX;
	for (TActorIterator<AAOSStructure> It(GetWorld()); It; ++It)
	{
		AAOSStructure* S = *It;
		if (!S || S->IsDestroyed() || S->GetOwnerTeam() != MyTeam) continue;

		if (S->GetStructureType() == EStructureType::CommandCenter)
		{
			CommandCenter = S;
			continue;
		}
		const float Dist = FVector::Dist(MyLoc, S->GetActorLocation());
		if (Dist < NearestDist)
		{
			NearestDist = Dist;
			NearestTower = S;
		}
	}
	return NearestTower ? NearestTower : CommandCenter;
}

void AAOSAIController::BeginStrafe(AActor* FocusActor)
{
	ACharacter* Char = Cast<ACharacter>(GetPawn());
	if (!Char || !FocusActor) return;

	if (!bStrafing)
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			// 이동 방향이 아니라 컨트롤러(= 포커스 타겟) 방향을 바라보게 → BlendSpace 가 Jog_Left/Right 로 옆걸음
			bSavedOrientRotationToMovement = CMC->bOrientRotationToMovement;
			bSavedUseControllerDesiredRotation = CMC->bUseControllerDesiredRotation;
			CMC->bOrientRotationToMovement = false;
			CMC->bUseControllerDesiredRotation = true;
		}
		bStrafing = true;
	}
	// 타겟이 바뀌었을 수 있으므로 매번 포커스 갱신 (Gameplay 우선순위 > 경로추종의 Move 포커스)
	SetFocus(FocusActor, EAIFocusPriority::Gameplay);
}

void AAOSAIController::UpdateStrafe(AActor* Target, float StepDistance)
{
	if (!ControlledCharacter || !Target) return;
	BeginStrafe(Target);

	const FVector MyLoc = ControlledCharacter->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const float Range = GetEffectiveAttackRange();

	// 사거리 밖으로 벌어졌으면 옆걸음 대신 접근 (타겟을 바라본 채)
	if (FVector::Dist(MyLoc, TargetLoc) > Range)
	{
		if (FVector::Dist(LastNavMoveTarget, TargetLoc) > 50.0f)
		{
			LastNavMoveActor = nullptr;
			MoveToLocation(TargetLoc, Range * 0.8f,
				/*bStopOnOverlap=*/true,
				/*bUsePathfinding=*/true,
				/*bProjectDestinationToNavigation=*/true,
				/*bCanStrafe=*/true);
			LastNavMoveTarget = TargetLoc;
		}
		bHasStrafeGoal = false;
		return;
	}

	// 진행 중인 걸음이 있으면 도착(30 이내)하거나 막힐(1.5초) 때까지 그대로 둔다.
	// 끝나면 방향을 뒤집어(좌↔우) 다음 걸음 → 호출되는 동안 좌우로 계속 오간다.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (bHasStrafeGoal)
	{
		const bool bReached = FVector::Dist2D(MyLoc, StrafeGoal) <= 30.0f;
		const bool bStuck = (Now - StrafeStepStartTime) > 1.5;
		if (!bReached && !bStuck) return;
	}
	StrafeSign = -StrafeSign;

	FVector Offset = MyLoc - TargetLoc;
	Offset.Z = 0.0f;
	if (Offset.IsNearlyZero())
	{
		Offset = -ControlledCharacter->GetActorForwardVector();
		Offset.Z = 0.0f;
	}
	// 타겟 중심 원호 위로 이동 → 거리(=사거리 안)를 유지한 채 옆으로 비킨다
	const float OrbitRadius = FMath::Clamp(static_cast<float>(Offset.Size()), 100.0f, FMath::Max(100.0f, Range * 0.9f));
	const float AngleRad = (StepDistance / OrbitRadius) * StrafeSign;
	const FVector NewOffset = Offset.GetSafeNormal().RotateAngleAxisRad(AngleRad, FVector::UpVector) * OrbitRadius;
	FVector Goal = TargetLoc + NewOffset;
	Goal.Z = MyLoc.Z;

	StrafeGoal = Goal;
	StrafeStepStartTime = Now;
	bHasStrafeGoal = true;

	LastNavMoveActor = nullptr;
	LastNavMoveTarget = FVector::ZeroVector;
	MoveToLocation(Goal, 10.0f,
		/*bStopOnOverlap=*/false,
		/*bUsePathfinding=*/true,
		/*bProjectDestinationToNavigation=*/true,
		/*bCanStrafe=*/true);
}

void AAOSAIController::EndStrafe()
{
	bHasStrafeGoal = false;
	if (!bStrafing) return;
	bStrafing = false;

	if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			CMC->bOrientRotationToMovement = bSavedOrientRotationToMovement;
			CMC->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		}
	}
	ClearFocus(EAIFocusPriority::Gameplay);
}

bool AAOSAIController::RequestMoveStep(const FVector& Goal, float AcceptanceRadius)
{
	if (!ControlledCharacter) return false;

	LastNavMoveActor = nullptr;
	LastNavMoveTarget = FVector::ZeroVector;
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Goal, AcceptanceRadius,
		/*bStopOnOverlap=*/false,
		/*bUsePathfinding=*/true,
		/*bProjectDestinationToNavigation=*/true,
		/*bCanStrafe=*/false);
	return Result != EPathFollowingRequestResult::Failed;
}

void AAOSAIController::MarkBehaviorUsed(FName Key)
{
	if (Key.IsNone()) return;
	if (const UWorld* World = GetWorld())
	{
		BehaviorLastUsedTime.Add(Key, World->GetTimeSeconds());
	}
}

bool AAOSAIController::IsBehaviorReady(FName Key, float CooldownSeconds) const
{
	const double* LastUsed = BehaviorLastUsedTime.Find(Key);
	if (!LastUsed) return true;
	const UWorld* World = GetWorld();
	return !World || (World->GetTimeSeconds() - *LastUsed) >= CooldownSeconds;
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
