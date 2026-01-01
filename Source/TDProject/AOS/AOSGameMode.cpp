#include "AOSGameMode.h"
#include "AOSCharacter.h"
#include "AOSAIController.h"
#include "AOSStructure.h"
#include "AOSPlayerController.h"
#include "AOSSpawnPoint.h"
#include "AOSMapManager.h"
#include "EngineUtils.h"

AAOSGameMode::AAOSGameMode()
{
	// PlayerStart에서 자동 생성되는 캐릭터를 방지
	// DefaultPawnClass를 nullptr로 명시적으로 설정하여 자동 생성 완전히 비활성화
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AAOSPlayerController::StaticClass();
}

void AAOSGameMode::BeginPlay()
{
	Super::BeginPlay();

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

	AOSGameState = EAOSGameState::Preparation;
	RemainingGameTime = GameDuration;
}

void AAOSGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (AOSGameState == EAOSGameState::GameRunning)
	{
		UpdateGameTime(DeltaTime);
		CheckVictoryConditions();
	}
}

// 🟢 NEW - PlayerStart에서 자동 캐릭터 생성 방지
void AAOSGameMode::RestartPlayer(AController* NewPlayer)
{
	// PlayerStart에서 자동으로 Pawn을 생성하지 않음
	// 모든 캐릭터는 SpawnPoint에서만 생성됨
	// 따라서 이 함수는 아무것도 하지 않음
	UE_LOG(LogTemp, Warning, TEXT("RestartPlayer called but disabled - using SpawnPoint spawning instead"));
}

// 🟡 MODIFIED - 캐릭터 자동 생성 로직 추가
void AAOSGameMode::StartGame()
{
	if (AOSGameState == EAOSGameState::Preparation)
	{
		AOSGameState = EAOSGameState::GameRunning;
		RemainingGameTime = GameDuration;

		// 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
		SpawnCharactersAtAllSpawnPoints();
	}
}

void AAOSGameMode::EndGame(EAOSTeam WinningTeam)
{
	AOSGameState = EAOSGameState::GameEnded;

	// TODO: 승리 팀에 점수 부여, UI 표시 등
}

void AAOSGameMode::DeployCharacters(EAOSTeam Team, const TArray<EAOSLane>& LaneAssignments)
{
	CharacterDeployments.Add(Team, LaneAssignments);

	// 이 함수는 플레이어 준비 단계에서 라인 배치 정보를 받음
	// 실제 캐릭터 스폰은 다른 함수에서 처리
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

// 🔴 REMOVED: SpawnCharacter() 함수는 더 이상 사용되지 않습니다.
// SpawnCharactersAtAllSpawnPoints()로 대체되었습니다.
/*
void AAOSGameMode::SpawnCharacter(AAOSCharacter* Character, EAOSTeam Team, EAOSLane Lane)
{
	// ... 기존 구현 제거됨
}
*/

// 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
void AAOSGameMode::SpawnCharactersAtAllSpawnPoints()
{
	// 🟡 MODIFIED - DefaultPawnClass 대신 CharacterClass 사용 (RTS 모드 지원)
	if (!CharacterClass)
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterClass not set! Please set CharacterClass in GameMode blueprint."));
		return;
	}

	// CharacterClass가 AAOSCharacter 파생 클래스인지 확인
	if (!CharacterClass->IsChildOf(AAOSCharacter::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterClass is not a valid AAOSCharacter class!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Spawning characters at %d spawn points..."), AllSpawnPoints.Num());

	for (AAOSSpawnPoint* SpawnPoint : AllSpawnPoints)
	{
		if (SpawnPoint)
		{
			// 스폰 포인트에서 캐릭터 생성
			AAOSCharacter* NewCharacter = SpawnPoint->SpawnCharacterAtPoint(CharacterClass);

			if (NewCharacter)
			{
				// 팀별 리스트에 추가
				if (NewCharacter->GetTeam() == EAOSTeam::Team1)
				{
					Team1Characters.Add(NewCharacter);
				}
				else
				{
					Team2Characters.Add(NewCharacter);
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Character spawn complete - Team1: %d, Team2: %d"),
		Team1Characters.Num(), Team2Characters.Num());
}

// 🟡 MODIFIED - 맵 매니저를 통한 타워 및 커맨드 센터 생성
void AAOSGameMode::InitializeStructures()
{
	InitializeMapManager();
	CacheTowerReferences();
}

// 🟢 NEW - 맵 매니저 초기화 및 구조물 생성
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

// 🟢 NEW - 생성된 타워와 커맨드 센터를 참조로 캐시
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

// 🟡 MODIFIED - 특정 라인의 팀별 타워 목록 반환
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

void AAOSGameMode::UpdateGameTime(float DeltaTime)
{
	RemainingGameTime -= DeltaTime;

	if (RemainingGameTime <= 0.0f)
	{
		RemainingGameTime = 0.0f;
		// 게임 시간 만료 - 점수로 승자 결정
		// TODO: 점수 계산 및 승자 결정
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
