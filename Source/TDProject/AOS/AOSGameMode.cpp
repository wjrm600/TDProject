#include "AOSGameMode.h"
#include "AOSCharacter.h"
#include "AOSAIController.h"
#include "AOSStructure.h"
#include "AOSPlayerController.h"
#include "AOSSpawnPoint.h"
#include "AOSMapManager.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"

AAOSGameMode::AAOSGameMode()
{
	// Tick 활성화 (준비 단계 타이머 + 라운드 시간 카운트다운 용)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// PlayerStart에서 자동 생성되는 캐릭터를 방지
	// DefaultPawnClass를 nullptr로 명시적으로 설정하여 자동 생성 완전히 비활성화
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AAOSPlayerController::StaticClass();

	// 기본 캐릭터 클래스 설정 (블루프린트 없이도 동작)
	CharacterClass = AAOSCharacter::StaticClass();

	// Phase 3A: 리플리케이션용 GameState / PlayerState 클래스 지정
	GameStateClass = AAOSGameState::StaticClass();
	PlayerStateClass = AAOSPlayerState::StaticClass();

	// DeployPlan은 FAOSLaneDeployPlan 구조체 배열 — 기본값은 빈 Classes 배열
	// InitializeDefaultDeployPlan()에서 TransitionToRoundPreparation 시 채워짐
}

// Phase 3A: 접속 시 팀 자동 할당
void AAOSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	AAOSPlayerState* PS = NewPlayer->GetPlayerState<AAOSPlayerState>();
	if (!PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] PostLogin: PlayerState가 AAOSPlayerState 아님"));
		return;
	}

	// 접속 순서로 팀 할당 (첫 번째 → Team1, 두 번째 → Team2)
	const int32 NumPlayers = GetNumPlayers();
	const EAOSTeam AssignedTeam = (NumPlayers <= 1) ? EAOSTeam::Team1 : EAOSTeam::Team2;
	PS->ServerSetTeam(AssignedTeam);

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] PostLogin: 플레이어 %d명, %s 할당"),
		NumPlayers,
		AssignedTeam == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"));

	// 로비 유지 — 양쪽이 준비 버튼을 눌러야 RoundPreparation으로 전이
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] %d명 접속 중. 로비 대기."), NumPlayers);

	// 접속 인원 리플리케이션 갱신
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetConnectedCount(NumPlayers);
	}
}

// Phase 3A: 퇴장 시 처리
void AAOSGameMode::Logout(AController* Exiting)
{
	// PIE 종료 등 월드 해제 중에는 EndGame 로직을 실행하지 않음
	if (GetWorld() && GetWorld()->bIsTearingDown)
	{
		Super::Logout(Exiting);
		return;
	}

	if (Exiting)
	{
		APlayerController* PC = Cast<APlayerController>(Exiting);
		AAOSPlayerState* PS = PC ? PC->GetPlayerState<AAOSPlayerState>() : nullptr;
		if (PS)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] Logout: %s 퇴장"),
				PS->Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"));
		}

		// 게임 도중 한쪽이 나가면 남은 팀 승리 처리 (라운드 진행 중일 때만)
		if (AOSGameState == EAOSGameState::RoundRunning || AOSGameState == EAOSGameState::RoundPreparation)
		{
			if (PS)
			{
				const EAOSTeam WinningTeam = (PS->Team == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;
				EndGame(WinningTeam);
			}
		}
	}

	Super::Logout(Exiting);

	// 퇴장 후 접속 인원 갱신 (Super 이후에 호출해야 정확한 카운트)
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetConnectedCount(GetNumPlayers());
	}
}

void AAOSGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 레벨 이름으로 메인메뉴 여부 판단 (bAutoStartGame 오버라이드 가능)
	FString MapName = GetWorld()->GetMapName();
	MapName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);

	bool bIsMainMenuLevel = MapName.Contains(TEXT("MainMenu"));

	if (!bAutoStartGame && bIsMainMenuLevel)
	{
		// 메인 메뉴 레벨: 메뉴 상태로 시작
		SetGameState(EAOSGameState::MainMenu);
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] MainMenu 레벨 감지 → 메뉴 상태"));
	}
	else
	{
		// 게임 레벨: Lobby 상태로 대기 (2명 접속 완료 시 PostLogin → TransitionToRoundPreparation)
		TransitionToLobby();
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 게임 레벨 감지 → Lobby 대기 (Map: %s)"), *MapName);
	}
}

void AAOSGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (AOSGameState == EAOSGameState::RoundPreparation)
	{
		int32 PrevSecond = FMath::CeilToInt(PreparationTimeRemaining);
		PreparationTimeRemaining = FMath::Max(0.0f, PreparationTimeRemaining - DeltaTime);
		int32 CurrSecond = FMath::CeilToInt(PreparationTimeRemaining);

		// 1초 변경 시마다 GameState를 통해 클라이언트로 리플리케이션
		if (PrevSecond != CurrSecond)
		{
			if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
			{
				AOSGS->ServerSetPreparationTime(PreparationTimeRemaining);
			}
		}
	}
	else if (AOSGameState == EAOSGameState::RoundRunning)
	{
		UpdateGameTime(DeltaTime);
		CheckVictoryConditions();
	}
}

// PlayerStart에서 자동 캐릭터 생성 방지
void AAOSGameMode::RestartPlayer(AController* NewPlayer)
{
	// PlayerStart에서 자동으로 Pawn을 생성하지 않음
	// 모든 캐릭터는 SpawnPoint에서만 생성됨
	// 따라서 이 함수는 아무것도 하지 않음
	UE_LOG(LogTemp, Warning, TEXT("RestartPlayer called but disabled - using SpawnPoint spawning instead"));
}

// RoundPreparation 상태에서만 라운드 시작 가능
void AAOSGameMode::StartRound()
{
	// Phase 3A: 서버 권한 체크
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] StartRound(): 클라이언트에서 호출됨. 무시."));
		return;
	}

	if (AOSGameState != EAOSGameState::RoundPreparation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] StartRound() 호출되었으나 현재 상태가 RoundPreparation이 아님. 무시."));
		return;
	}

	// 자동 시작 타이머 취소 (수동 시작 시)
	if (GetWorldTimerManager().IsTimerActive(RoundPreparationTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(RoundPreparationTimerHandle);
	}

	// 라운드 번호 증가
	CurrentRound++;

	// Phase 3A: AOSGameState에 라운드 번호 리플리케이션
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetCurrentRound(CurrentRound);
	}

	// 이전 라운드 캐릭터 참조 정리
	Team1Characters.Empty();
	Team2Characters.Empty();

	// 게임 시간 초기화
	RemainingGameTime = GameDuration;

	// 배치 계획에 따라 캐릭터 생성 (먼저 스폰 후 0명 여부 확인)
	SpawnCharactersForRound();

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] ===== 라운드 %d 시작 시도 ====="), CurrentRound);
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] Team1: %d명, Team2: %d명 배치"),
		Team1Characters.Num(), Team2Characters.Num());

	// 양쪽 배치 0명 → Draw 처리 (RoundRunning 진입 안 함)
	if (Team1Characters.Num() == 0 && Team2Characters.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 양쪽 배치 0명 → 무승부 처리. 5초 후 다음 라운드 준비."));

		if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
		{
			AOSGS->SetIsDraw(true);
		}
		SetGameState(EAOSGameState::Settlement);

		GetWorldTimerManager().SetTimer(DrawTransitionHandle, this, &AAOSGameMode::HandleDrawRound, 5.0f, false);
		return;
	}

	// 정상 라운드 시작
	SetGameState(EAOSGameState::RoundRunning);
}

void AAOSGameMode::HandleDrawRound()
{
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->SetIsDraw(false);
	}
	TransitionToRoundPreparation();
}

void AAOSGameMode::EndRound()
{
	// Phase 3A: 서버 권한 체크
	if (!HasAuthority())
	{
		return;
	}

	if (AOSGameState != EAOSGameState::RoundRunning)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] ===== 라운드 %d 종료 ====="), CurrentRound);

	// 라운드 종료 델리게이트 브로드캐스트
	OnRoundEnded.Broadcast(CurrentRound);

	// 커맨드 센터 파괴 여부 확인
	bool bTeam1CCDestroyed = false;
	bool bTeam2CCDestroyed = false;

	if (AAOSStructure* Team1CC = CommandCenters.FindRef(EAOSTeam::Team1))
	{
		bTeam1CCDestroyed = Team1CC->IsDestroyed();
	}
	if (AAOSStructure* Team2CC = CommandCenters.FindRef(EAOSTeam::Team2))
	{
		bTeam2CCDestroyed = Team2CC->IsDestroyed();
	}

	if (bTeam1CCDestroyed)
	{
		EndGame(EAOSTeam::Team2);
	}
	else if (bTeam2CCDestroyed)
	{
		EndGame(EAOSTeam::Team1);
	}
	else
	{
		// 커맨드 센터가 아직 파괴되지 않음 → 다음 라운드 준비
		TransitionToRoundPreparation();
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 커맨드 센터 미파괴 → 다음 라운드 준비"));
	}
}

void AAOSGameMode::EndGame(EAOSTeam WinningTeam)
{
	// Phase 3A: 서버 권한 체크
	if (!HasAuthority())
	{
		return;
	}

	// 라운드 종료 타이머가 남아있으면 정리
	if (GetWorldTimerManager().IsTimerActive(RoundEndTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	}

	SetGameState(EAOSGameState::Settlement);

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 게임 종료! 승리 팀: %s (라운드 %d)"),
		WinningTeam == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"), CurrentRound);
}

// 배치 계획 설정
void AAOSGameMode::SetLaneDeployCount(EAOSTeam Team, EAOSLane Lane, int32 Count)
{
	// Phase 3A: 서버 권한 체크 (단, 로컬 싱글 플레이 시에도 동작하도록 ListenServer/Standalone 허용)
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] SetLaneDeployCount(): 클라이언트에서 호출됨. RPC 경로 사용 필요."));
		return;
	}

	// 0 ~ MaxCharactersPerLane 범위로 제한
	int32 ClampedCount = FMath::Clamp(Count, 0, MaxCharactersPerLane);
	SetDeployCount(Team, Lane, ClampedCount);

	UE_LOG(LogTemp, Log, TEXT("[GameMode] 배치 계획 설정: %s %s 라인 = %d명"),
		Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
		Lane == EAOSLane::Top ? TEXT("Top") : Lane == EAOSLane::Mid ? TEXT("Mid") : TEXT("Bottom"),
		ClampedCount);
}

int32 AAOSGameMode::GetLaneDeployCount(EAOSTeam Team, EAOSLane Lane) const
{
	return GetDeployCount(Team, Lane);
}

int32 AAOSGameMode::GetTotalDeployCount(EAOSTeam Team) const
{
	int32 Total = 0;
	Total += GetDeployCount(Team, EAOSLane::Top);
	Total += GetDeployCount(Team, EAOSLane::Mid);
	Total += GetDeployCount(Team, EAOSLane::Bottom);
	return Total;
}

// 내부 배치 계획 접근 함수 (하위 호환: Count 기반)
void AAOSGameMode::SetDeployCount(EAOSTeam Team, EAOSLane Lane, int32 Count)
{
	int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	int32 L = static_cast<int32>(Lane);
	if (L < 0 || L >= 3) return;

	TSubclassOf<AAOSCharacter> FB =
		(CharacterRoster.Num() > 0) ? CharacterRoster[0].CharacterClass : CharacterClass;

	DeployPlan[T][L].Classes.SetNum(Count);
	for (auto& Cls : DeployPlan[T][L].Classes)
		if (!Cls) Cls = FB;
}

int32 AAOSGameMode::GetDeployCount(EAOSTeam Team, EAOSLane Lane) const
{
	int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	int32 L = static_cast<int32>(Lane);
	return (L >= 0 && L < 3) ? DeployPlan[T][L].Classes.Num() : 0;
}

// 기본 배치 계획 초기화 (로스터 첫 번째 캐릭터 또는 폴백으로 Top2/Mid2/Bottom1)
void AAOSGameMode::InitializeDefaultDeployPlan()
{
	TSubclassOf<AAOSCharacter> DefaultClass =
		(CharacterRoster.Num() > 0) ? CharacterRoster[0].CharacterClass : CharacterClass;

	for (int32 T = 0; T < 2; ++T)
	{
		for (int32 L = 0; L < 3; ++L)
			DeployPlan[T][L].Classes.Empty();

		// 기본: Top 2, Mid 2, Bottom 1
		DeployPlan[T][0].Classes = { DefaultClass, DefaultClass };
		DeployPlan[T][1].Classes = { DefaultClass, DefaultClass };
		DeployPlan[T][2].Classes = { DefaultClass };
	}
}

// 레인 배치 클래스 목록 설정 (신규 API)
void AAOSGameMode::SetLaneDeployClasses(EAOSTeam Team, EAOSLane Lane,
	const TArray<TSubclassOf<AAOSCharacter>>& Classes)
{
	if (!HasAuthority()) return;
	int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	int32 L = static_cast<int32>(Lane);
	if (L < 0 || L >= 3) return;

	DeployPlan[T][L].Classes.Empty();
	for (int32 i = 0; i < FMath::Min(Classes.Num(), MaxCharactersPerLane); ++i)
		if (Classes[i]) DeployPlan[T][L].Classes.Add(Classes[i]);
}

const TArray<TSubclassOf<AAOSCharacter>>& AAOSGameMode::GetLaneDeployClasses(
	EAOSTeam Team, EAOSLane Lane) const
{
	static TArray<TSubclassOf<AAOSCharacter>> Empty;
	int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	int32 L = static_cast<int32>(Lane);
	return (L >= 0 && L < 3) ? DeployPlan[T][L].Classes : Empty;
}

void AAOSGameMode::ServerSetLaneDeployClassesForPlayer(AAOSPlayerState* PlayerState,
	EAOSLane Lane, const TArray<TSubclassOf<AAOSCharacter>>& Classes)
{
	if (!HasAuthority() || !PlayerState) return;
	if (AOSGameState != EAOSGameState::RoundPreparation) return;

	int32 T = (PlayerState->GetTeam() == EAOSTeam::Team1) ? 0 : 1;
	int32 NewCount = FMath::Min(Classes.Num(), MaxCharactersPerLane);

	// 총합 5명 초과 방지
	int32 Others = 0;
	for (int32 OtherL = 0; OtherL < 3; ++OtherL)
		if (OtherL != static_cast<int32>(Lane))
			Others += DeployPlan[T][OtherL].Classes.Num();
	if (Others + NewCount > 5) return;

	SetLaneDeployClasses(PlayerState->GetTeam(), Lane, Classes);
	PlayerState->ServerSetDeployCount(Lane, NewCount);
}

void AAOSGameMode::DeployCharacters(EAOSTeam Team, const TArray<EAOSLane>& LaneAssignments)
{
	CharacterDeployments.Add(Team, LaneAssignments);

	// 이 함수는 플레이어 준비 단계에서 라인 배치 정보를 받음
	// 실제 캐릭터 스폰은 StartRound()에서 처리
}

void AAOSGameMode::RegisterSpawnPoint(AAOSSpawnPoint* SpawnPoint)
{
	if (!SpawnPoint)
	{
		return;
	}

	AllSpawnPoints.Add(SpawnPoint);

	// 팀별로 분류
	EAOSTeam Team = SpawnPoint->GetTeam();
	if (!TeamSpawnPoints.Contains(Team))
	{
		TeamSpawnPoints.Add(Team, TArray<AAOSSpawnPoint*>());
	}
	TeamSpawnPoints[Team].Add(SpawnPoint);

	// 팀 내에서 라인/인덱스 순으로 정렬
	TeamSpawnPoints[Team].Sort([](const AAOSSpawnPoint& A, const AAOSSpawnPoint& B)
	{
		if (A.GetLane() != B.GetLane())
		{
			return static_cast<int32>(A.GetLane()) < static_cast<int32>(B.GetLane());
		}
		return A.GetSpawnIndex() < B.GetSpawnIndex();
	});
}

AAOSSpawnPoint* AAOSGameMode::GetNearestSpawnPoint(EAOSTeam Team, EAOSLane Lane)
{
	if (!TeamSpawnPoints.Contains(Team))
	{
		return nullptr;
	}

	TArray<AAOSSpawnPoint*>& SpawnPoints = TeamSpawnPoints[Team];

	// 해당 팀의 해당 라인에서 사용 가능한 첫 번째 스폰 포인트 찾기
	for (AAOSSpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (SpawnPoint && SpawnPoint->GetLane() == Lane && !SpawnPoint->IsOccupied())
		{
			return SpawnPoint;
		}
	}

	// 해당 라인에 사용 가능한 포인트가 없으면 nullptr 반환
	return nullptr;
}

// 라운드 배치 계획에 따라 캐릭터 생성
void AAOSGameMode::SpawnCharactersForRound()
{
	// 스폰 포인트의 Occupied 상태 초기화
	for (AAOSSpawnPoint* SP : AllSpawnPoints)
	{
		if (SP) SP->ReleaseCharacter();
	}

	const EAOSTeam Teams[] = { EAOSTeam::Team1, EAOSTeam::Team2 };
	const EAOSLane Lanes[] = { EAOSLane::Top, EAOSLane::Mid, EAOSLane::Bottom };

	for (EAOSTeam CurrentTeam : Teams)
	{
		for (EAOSLane CurrentLane : Lanes)
		{
			int32 T = (CurrentTeam == EAOSTeam::Team1) ? 0 : 1;
			int32 L = static_cast<int32>(CurrentLane);
			const TArray<TSubclassOf<AAOSCharacter>>& PlannedClasses = DeployPlan[T][L].Classes;

			if (PlannedClasses.Num() == 0) continue;

			// 해당 팀/라인의 스폰 포인트 수집
			TArray<AAOSSpawnPoint*> LaneSpawnPoints;
			if (TeamSpawnPoints.Contains(CurrentTeam))
			{
				for (AAOSSpawnPoint* SP : TeamSpawnPoints[CurrentTeam])
				{
					if (SP && SP->GetLane() == CurrentLane)
						LaneSpawnPoints.Add(SP);
				}
			}

			int32 SpawnCount = FMath::Min(PlannedClasses.Num(), LaneSpawnPoints.Num());

			for (int32 i = 0; i < SpawnCount; ++i)
			{
				TSubclassOf<AAOSCharacter> ClassToSpawn = PlannedClasses[i];
				if (!ClassToSpawn) ClassToSpawn = CharacterClass; // 폴백
				if (!ClassToSpawn) continue;

				AAOSCharacter* NewCharacter = LaneSpawnPoints[i]->SpawnCharacterAtPoint(ClassToSpawn);
				if (NewCharacter)
				{
					if (CurrentTeam == EAOSTeam::Team1)
						Team1Characters.Add(NewCharacter);
					else
						Team2Characters.Add(NewCharacter);
				}
			}

			if (SpawnCount < PlannedClasses.Num())
			{
				UE_LOG(LogTemp, Warning, TEXT("[GameMode] %s %s 라인: 스폰 포인트 부족 (%d/%d)"),
					CurrentTeam == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
					CurrentLane == EAOSLane::Top ? TEXT("Top") : CurrentLane == EAOSLane::Mid ? TEXT("Mid") : TEXT("Bottom"),
					SpawnCount, PlannedClasses.Num());
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 라운드 %d 캐릭터 생성 완료 - Team1: %d, Team2: %d"),
		CurrentRound, Team1Characters.Num(), Team2Characters.Num());
}

// 맵 매니저를 통한 타워 및 커맨드 센터 생성
void AAOSGameMode::InitializeStructures()
{
	InitializeMapManager();
	CacheTowerReferences();
}

// 맵 매니저 초기화 및 구조물 생성
void AAOSGameMode::InitializeMapManager()
{
	// 월드에서 MapManager 찾기
	for (TActorIterator<AAOSMapManager> MapItr(GetWorld()); MapItr; ++MapItr)
	{
		MapManager = *MapItr;
		if (MapManager)
		{
			break;
		}
	}

	// MapManager가 없으면 새로 생성
	if (!MapManager && GetWorld())
	{
		MapManager = GetWorld()->SpawnActor<AAOSMapManager>(
			AAOSMapManager::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator
		);

		if (MapManager)
		{
			MapManager->InitializeMap();
			MapManager->SpawnStructures();
			UE_LOG(LogTemp, Warning, TEXT("AOSMapManager created and structures spawned"));
		}
	}
	else if (MapManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("AOSMapManager found in world"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to initialize MapManager"));
	}
}

// 생성된 타워와 커맨드 센터를 참조로 캐시
void AAOSGameMode::CacheTowerReferences()
{
	if (!MapManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("MapManager not available for caching tower references"));
		return;
	}

	// 각 라인의 타워들을 캐시
	for (int32 Lane = 0; Lane < 3; ++Lane)
	{
		EAOSLane LaneType = static_cast<EAOSLane>(Lane);

		// Team1 타워
		TArray<AAOSStructure*> Team1Towers = MapManager->GetTowersInLane(LaneType, EAOSTeam::Team1);
		if (Team1Towers.Num() > 0)
		{
			LaneTowers.Add(LaneType, Team1Towers);
		}

		// Team2 타워
		TArray<AAOSStructure*> Team2Towers = MapManager->GetTowersInLane(LaneType, EAOSTeam::Team2);
		for (AAOSStructure* Tower : Team2Towers)
		{
			if (LaneTowers.Contains(LaneType))
			{
				LaneTowers[LaneType].Add(Tower);
			}
		}
	}

	// 커맨드 센터 캐시
	AAOSStructure* Team1Center = MapManager->GetCommandCenter(EAOSTeam::Team1);
	AAOSStructure* Team2Center = MapManager->GetCommandCenter(EAOSTeam::Team2);

	if (Team1Center)
	{
		CommandCenters.Add(EAOSTeam::Team1, Team1Center);
		UE_LOG(LogTemp, Warning, TEXT("Team1 CommandCenter cached"));
	}

	if (Team2Center)
	{
		CommandCenters.Add(EAOSTeam::Team2, Team2Center);
		UE_LOG(LogTemp, Warning, TEXT("Team2 CommandCenter cached"));
	}

	if (Team1Center && Team2Center)
	{
		UE_LOG(LogTemp, Warning, TEXT("All structures initialized successfully"));
	}
}

AAOSStructure* AAOSGameMode::GetCommandCenter(EAOSTeam Team)
{
	return CommandCenters.FindRef(Team);
}

// 특정 라인의 팀별 타워 목록 반환
TArray<AAOSStructure*> AAOSGameMode::GetTowersByLane(EAOSLane Lane, EAOSTeam Team)
{
	TArray<AAOSStructure*> Result;

	if (!LaneTowers.Contains(Lane))
	{
		return Result;
	}

	// 해당 라인의 모든 타워 중 팀에 맞는 것만 필터링
	for (AAOSStructure* Tower : LaneTowers[Lane])
	{
		if (Tower && Tower->GetOwnerTeam() == Team)
		{
			Result.Add(Tower);
		}
	}

	return Result;
}

void AAOSGameMode::OnCharacterDestroyed(AAOSCharacter* DestroyedCharacter)
{
	if (!DestroyedCharacter)
	{
		return;
	}

	EAOSTeam CharacterTeam = DestroyedCharacter->GetTeam();

	if (CharacterTeam == EAOSTeam::Team1)
	{
		Team1Characters.Remove(DestroyedCharacter);
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] Team1 캐릭터 사망. 남은: %d"), Team1Characters.Num());
	}
	else
	{
		Team2Characters.Remove(DestroyedCharacter);
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] Team2 캐릭터 사망. 남은: %d"), Team2Characters.Num());
	}

	// 양 팀 모두 캐릭터가 없으면 라운드 종료 (3초 딜레이)
	if (AOSGameState == EAOSGameState::RoundRunning &&
		Team1Characters.Num() == 0 && Team2Characters.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 양 팀 모든 캐릭터 사망 → 3초 후 라운드 종료"));

		// 이미 타이머가 설정되어 있으면 중복 방지
		if (!GetWorldTimerManager().IsTimerActive(RoundEndTimerHandle))
		{
			GetWorldTimerManager().SetTimer(
				RoundEndTimerHandle,
				this,
				&AAOSGameMode::EndRound,
				3.0f,
				false
			);
		}
	}
}

// 게임 상태 설정 및 델리게이트 브로드캐스트 헬퍼
void AAOSGameMode::SetGameState(EAOSGameState NewState)
{
	AOSGameState = NewState;

	// Phase 3A: AOSGameState에 상태 리플리케이션
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetCurrentState(NewState);
	}

	OnGameStateChanged.Broadcast(NewState);
}

// MainMenu 상태로 전이
void AAOSGameMode::TransitionToMainMenu()
{
	SetGameState(EAOSGameState::MainMenu);
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] MainMenu 상태로 전이"));
}

// Lobby 상태로 전이
void AAOSGameMode::TransitionToLobby()
{
	SetGameState(EAOSGameState::Lobby);
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] Lobby 상태로 전이"));
}

// RoundPreparation 상태로 전이: 스폰 포인트 등록 + 구조물 초기화
void AAOSGameMode::TransitionToRoundPreparation()
{
	// 이전 라운드 캐릭터 참조 정리
	Team1Characters.Empty();
	Team2Characters.Empty();

	// 라운드 종료 타이머 정리
	if (GetWorldTimerManager().IsTimerActive(RoundEndTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	}

	// 첫 라운드인 경우만 스폰 포인트 등록 및 구조물 초기화
	if (CurrentRound == 0)
	{
		// 월드의 모든 스폰 포인트 등록
		for (TActorIterator<AAOSSpawnPoint> SpawnPointItr(GetWorld()); SpawnPointItr; ++SpawnPointItr)
		{
			AAOSSpawnPoint* SpawnPoint = *SpawnPointItr;
			if (SpawnPoint)
			{
				RegisterSpawnPoint(SpawnPoint);
			}
		}

		// 구조물 초기화 (타워, 커맨드 센터)
		InitializeStructures();

		// 스폰 포인트 팀/라인 현황 로그 (레벨 설정 진단용)
		for (auto& KV : TeamSpawnPoints)
		{
			TMap<EAOSLane, int32> LaneCounts;
			for (AAOSSpawnPoint* SP : KV.Value)
			{
				if (SP) LaneCounts.FindOrAdd(SP->GetLane())++;
			}
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] %s 스폰 포인트 — Top:%d Mid:%d Bottom:%d"),
				KV.Key == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
				LaneCounts.FindRef(EAOSLane::Top),
				LaneCounts.FindRef(EAOSLane::Mid),
				LaneCounts.FindRef(EAOSLane::Bottom));
		}
	}

	// 기본 배치 계획 초기화 (라인당 MaxCharactersPerLane명)
	InitializeDefaultDeployPlan();

	// 준비 상태 초기화 (로비/이전 라운드의 ready 상태가 남아있으면 첫 번째 플레이어가 시작 버튼을 눌렀을 때 바로 StartRound가 실행됨)
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetTeamReady(EAOSTeam::Team1, false);
		AOSGS->ServerSetTeamReady(EAOSTeam::Team2, false);
	}
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AAOSPlayerState* AOSPS = Cast<AAOSPlayerState>(PS))
		{
			AOSPS->ServerSetReady(false);
		}
	}

	RemainingGameTime = GameDuration;
	SetGameState(EAOSGameState::RoundPreparation);

	// ── 모든 PlayerController에 로스터 RPC 전송 (원격 클라이언트 포함) ──────────────
	// ShowCharacterSelect()는 IsLocalPlayerController()==true 인 PC에만 바인딩되므로,
	// 원격 클라이언트의 서버사이드 PC는 OnGameStateChanged가 실행되지 않아
	// Client_ReceiveCharacterRoster RPC가 전달되지 않는다.
	// GameMode에서 직접 모든 PC를 순회하여 RPC를 전송하면 이 문제가 해결된다.
	{
		const TArray<FCharacterRosterEntry>& Roster = GetCharacterRoster();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (AAOSPlayerController* PC = Cast<AAOSPlayerController>(It->Get()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[GameMode] 로스터 RPC → %s (IsLocal:%d, 로스터:%d개)"),
					*PC->GetName(), PC->IsLocalPlayerController() ? 1 : 0, Roster.Num());
				PC->Client_ReceiveCharacterRoster(Roster);
			}
		}
	}
	// ──────────────────────────────────────────────────────────────────────────────────

	PreparationTimeRemaining = 30.0f;
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetPreparationTime(PreparationTimeRemaining);
	}

	// 30초 후 자동 라운드 시작 (양쪽이 준비 버튼을 누르지 않아도 자동 시작)
	GetWorldTimerManager().SetTimer(RoundPreparationTimerHandle, this, &AAOSGameMode::StartRound, 30.0f, false);

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] RoundPreparation 상태로 전이 (다음 라운드: %d). SpawnPoints: %d. 30초 자동 시작 타이머 시작"),
		CurrentRound + 1, AllSpawnPoints.Num());
}

void AAOSGameMode::UpdateGameTime(float DeltaTime)
{
	RemainingGameTime -= DeltaTime;

	if (RemainingGameTime <= 0.0f)
	{
		RemainingGameTime = 0.0f;
		// 커맨드센터 잔여 체력 비교 — 체력이 높은 팀(피해를 덜 받은 팀)이 승리
		AAOSStructure* Team1CC = CommandCenters.FindRef(EAOSTeam::Team1);
		AAOSStructure* Team2CC = CommandCenters.FindRef(EAOSTeam::Team2);

		float Team1HP = Team1CC ? Team1CC->GetCurrentHealth() : 0.0f;
		float Team2HP = Team2CC ? Team2CC->GetCurrentHealth() : 0.0f;

		EAOSTeam Winner = (Team1HP >= Team2HP) ? EAOSTeam::Team1 : EAOSTeam::Team2;
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 시간 만료 — Team1HP=%.0f Team2HP=%.0f → 승리팀=%d"),
			Team1HP, Team2HP, (int32)Winner);
		EndGame(Winner);
	}
}

void AAOSGameMode::CheckVictoryConditions()
{
	// 각 팀의 커맨드 센터 상태 확인
	if (CommandCenters.Num() >= 2)
	{
		if (AAOSStructure* Team1Center = CommandCenters.FindRef(EAOSTeam::Team1))
		{
			if (Team1Center->IsDestroyed())
			{
				EndGame(EAOSTeam::Team2);
			}
		}

		if (AAOSStructure* Team2Center = CommandCenters.FindRef(EAOSTeam::Team2))
		{
			if (Team2Center->IsDestroyed())
			{
				EndGame(EAOSTeam::Team1);
			}
		}
	}
}

// Phase 3A: 서버에서 플레이어 준비 상태 업데이트 + GameState 리플리케이션
void AAOSGameMode::ServerSetPlayerReady(AAOSPlayerState* PlayerState, bool bReady)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	PlayerState->ServerSetReady(bReady);

	// GameState의 팀별 준비 플래그도 업데이트
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetTeamReady(PlayerState->GetTeam(), bReady);
	}

	UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 준비 상태: %s"),
		PlayerState->GetTeam() == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
		bReady ? TEXT("TRUE") : TEXT("FALSE"));

	// 양쪽 준비 완료 시 상태에 따라 전이
	if (AreAllPlayersReady())
	{
		if (AOSGameState == EAOSGameState::MainMenu)
		{
			// 양쪽이 모두 메인 메뉴에서 "게임 시작" 클릭 → 게임 레벨로 ServerTravel
			// (bTeam1Ready/bTeam2Ready는 ServerTravel 후 새 레벨에서 자동으로 false 초기화됨)
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] 양쪽 시작 완료 → ServerTravel"));
			FString MapURL = GameMapName.ToString();
			GetWorld()->ServerTravel(MapURL, false /*bAbsolute*/);
		}
		else if (AOSGameState == EAOSGameState::Lobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] 양쪽 준비 완료 → RoundPreparation"));
			TransitionToRoundPreparation();
		}
		else if (AOSGameState == EAOSGameState::RoundPreparation)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] 양쪽 준비 완료 → 라운드 시작"));
			StartRound();
		}
	}
}

bool AAOSGameMode::AreAllPlayersReady()
{
	const AAOSGameState* AOSGS = Cast<AAOSGameState>(GameState);
	if (!AOSGS)
	{
		return false;
	}

	// 양쪽 팀 접속 + 양쪽 준비
	return GetNumPlayers() >= 2 && AOSGS->AreBothTeamsReady();
}

void AAOSGameMode::ServerSetLaneDeployCountForPlayer(AAOSPlayerState* PlayerState, EAOSLane Lane, int32 Count)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	// RoundPreparation 상태에서만 허용
	if (AOSGameState != EAOSGameState::RoundPreparation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] ServerSetLaneDeployCountForPlayer: RoundPreparation 아님. 무시."));
		return;
	}

	// 0 ~ MaxCharactersPerLane 범위로 제한
	const int32 ClampedCount = FMath::Clamp(Count, 0, MaxCharactersPerLane);

	// PlayerState에 저장 (리플리케이션 용)
	PlayerState->ServerSetDeployCount(Lane, ClampedCount);

	// GameMode의 DeployPlan에도 반영 (SpawnCharactersForRound가 읽는 곳)
	SetDeployCount(PlayerState->GetTeam(), Lane, ClampedCount);

	UE_LOG(LogTemp, Log, TEXT("[GameMode] %s %s 라인 배치 수: %d (리플리케이션)"),
		PlayerState->GetTeam() == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
		Lane == EAOSLane::Top ? TEXT("Top") : Lane == EAOSLane::Mid ? TEXT("Mid") : TEXT("Bottom"),
		ClampedCount);
}
