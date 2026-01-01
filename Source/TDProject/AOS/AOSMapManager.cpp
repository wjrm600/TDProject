#include "AOSMapManager.h"
#include "AOSStructure.h"
#include "DrawDebugHelpers.h"

AAOSMapManager::AAOSMapManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAOSMapManager::BeginPlay()
{
	Super::BeginPlay();

	InitializeMap();
	SpawnStructures();

	// 🟢 NEW - 타워 위치 디버그 박스 표시
	if (bShowDebugTowerBoxes)
	{
		DrawDebugTowerPositions();
	}
}

// 🟢 NEW - 에디터에서 액터 선택 시 시각화
#if WITH_EDITOR
void AAOSMapManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// LanesInfo가 비어있으면 기본값으로 초기화
	if (LanesInfo.Num() == 0)
	{
		SetupDefaultLaneInfo();
	}

	// 에디터 시각화가 활성화되어 있으면 라인 경로 그리기
	if (!bShowEditorVisualization || !GetWorld())
		return;

	// 이전 디버그 라인 제거
	FlushPersistentDebugLines(GetWorld());

	for (const FLaneInfo& LaneInfo : LanesInfo)
	{
		FColor Team1Color = FColor::Cyan;    // Team1: 청록색
		FColor Team2Color = FColor::Magenta; // Team2: 마젠타색

		// 라인 경로 그리기
		if (bShowLanePaths)
		{
			// Team1 라인 경로
			DrawDebugLine(
				GetWorld(),
				LaneInfo.Team1StartPosition,
				LaneInfo.Team1EndPosition,
				Team1Color,
				true,
				-1.0f,
				0,
				EditorVisualizationThickness
			);

			// Team2 라인 경로
			DrawDebugLine(
				GetWorld(),
				LaneInfo.Team2StartPosition,
				LaneInfo.Team2EndPosition,
				Team2Color,
				true,
				-1.0f,
				0,
				EditorVisualizationThickness
			);
		}

		// 타워 위치 표시
		if (bShowTowerPositions)
		{
			// Team1 타워
			for (int32 i = 0; i < LaneInfo.Team1TowerPositions.Num(); ++i)
			{
				const FVector& TowerPos = LaneInfo.Team1TowerPositions[i];

				// 타워 위치에 구체
				DrawDebugSphere(
					GetWorld(),
					TowerPos,
					100.0f,
					12,
					Team1Color,
					true,
					-1.0f,
					0,
					EditorVisualizationThickness
				);

				// 타워 번호 표시 (위쪽 화살표)
				DrawDebugDirectionalArrow(
					GetWorld(),
					TowerPos,
					TowerPos + FVector(0, 0, 300),
					50.0f,
					Team1Color,
					true,
					-1.0f,
					0,
					EditorVisualizationThickness
				);
			}

			// Team2 타워
			for (int32 i = 0; i < LaneInfo.Team2TowerPositions.Num(); ++i)
			{
				const FVector& TowerPos = LaneInfo.Team2TowerPositions[i];

				DrawDebugSphere(
					GetWorld(),
					TowerPos,
					100.0f,
					12,
					Team2Color,
					true,
					-1.0f,
					0,
					EditorVisualizationThickness
				);

				DrawDebugDirectionalArrow(
					GetWorld(),
					TowerPos,
					TowerPos + FVector(0, 0, 300),
					50.0f,
					Team2Color,
					true,
					-1.0f,
					0,
					EditorVisualizationThickness
				);
			}
		}

		// Command Center 위치 표시
		if (bShowCommandCenters)
		{
			// Team1 Command Center - 큰 박스
			DrawDebugBox(
				GetWorld(),
				LaneInfo.Team1CommandCenterPosition,
				FVector(150, 150, 150),
				Team1Color,
				true,
				-1.0f,
				0,
				EditorVisualizationThickness * 2.0f
			);

			// Team2 Command Center - 큰 박스
			DrawDebugBox(
				GetWorld(),
				LaneInfo.Team2CommandCenterPosition,
				FVector(150, 150, 150),
				Team2Color,
				true,
				-1.0f,
				0,
				EditorVisualizationThickness * 2.0f
			);
		}
	}
}
#endif

void AAOSMapManager::InitializeMap()
{
	if (LanesInfo.Num() == 0)
	{
		SetupDefaultLaneInfo();
	}
}

FLaneInfo AAOSMapManager::GetLaneInfo(EAOSLane Lane) const
{
	for (const FLaneInfo& Info : LanesInfo)
	{
		if (Info.LaneType == Lane)
		{
			return Info;
		}
	}

	return FLaneInfo();
}

FVector AAOSMapManager::GetLaneStartPosition(EAOSLane Lane, EAOSTeam Team) const
{
	FLaneInfo Info = GetLaneInfo(Lane);

	if (Team == EAOSTeam::Team1)
	{
		return Info.Team1StartPosition;
	}
	else
	{
		return Info.Team2StartPosition;
	}
}

FVector AAOSMapManager::GetLaneEndPosition(EAOSLane Lane, EAOSTeam Team) const
{
	FLaneInfo Info = GetLaneInfo(Lane);

	if (Team == EAOSTeam::Team1)
	{
		return Info.Team1EndPosition;
	}
	else
	{
		return Info.Team2EndPosition;
	}
}

void AAOSMapManager::SpawnStructures()
{
	// 🟡 MODIFIED - 디버그 로깅 추가
	UE_LOG(LogTemp, Warning, TEXT("=== SpawnStructures Started ==="));
	UE_LOG(LogTemp, Warning, TEXT("Total LanesInfo: %d"), LanesInfo.Num());

	if (LanesInfo.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("LanesInfo is empty! No towers will be spawned."));
		return;
	}

	// 각 라인별로 타워 생성
	for (const FLaneInfo& LaneInfo : LanesInfo)
	{
		UE_LOG(LogTemp, Warning, TEXT("Processing Lane: %d"), static_cast<int32>(LaneInfo.LaneType));
		UE_LOG(LogTemp, Warning, TEXT("  Team1 Towers: %d"), LaneInfo.Team1TowerPositions.Num());
		UE_LOG(LogTemp, Warning, TEXT("  Team2 Towers: %d"), LaneInfo.Team2TowerPositions.Num());

		// Team1 타워 생성
		for (const FVector& TowerPos : LaneInfo.Team1TowerPositions)
		{
			AAOSStructure* Tower = GetWorld()->SpawnActor<AAOSStructure>(
				AAOSStructure::StaticClass(),
				TowerPos,
				FRotator::ZeroRotator
			);

			if (Tower)
			{
				Tower->Initialize(EStructureType::Tower, EAOSTeam::Team1, LaneInfo.LaneType);
				AllTowers.Add(Tower);
				UE_LOG(LogTemp, Warning, TEXT("Team1 Tower spawned at (%.1f, %.1f, %.1f)"), TowerPos.X, TowerPos.Y, TowerPos.Z);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to spawn Team1 Tower at (%.1f, %.1f, %.1f)"), TowerPos.X, TowerPos.Y, TowerPos.Z);
			}
		}

		// Team2 타워 생성
		for (const FVector& TowerPos : LaneInfo.Team2TowerPositions)
		{
			AAOSStructure* Tower = GetWorld()->SpawnActor<AAOSStructure>(
				AAOSStructure::StaticClass(),
				TowerPos,
				FRotator::ZeroRotator
			);

			if (Tower)
			{
				Tower->Initialize(EStructureType::Tower, EAOSTeam::Team2, LaneInfo.LaneType);
				AllTowers.Add(Tower);
				UE_LOG(LogTemp, Warning, TEXT("Team2 Tower spawned at (%.1f, %.1f, %.1f)"), TowerPos.X, TowerPos.Y, TowerPos.Z);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to spawn Team2 Tower at (%.1f, %.1f, %.1f)"), TowerPos.X, TowerPos.Y, TowerPos.Z);
			}
		}
	}

	// 커맨드 센터 생성
	UE_LOG(LogTemp, Warning, TEXT("=== Spawning Command Centers ==="));
	bool bMidLaneFound = false;
	for (const FLaneInfo& LaneInfo : LanesInfo)
	{
		if (LaneInfo.LaneType == EAOSLane::Mid)
		{
			bMidLaneFound = true;
			UE_LOG(LogTemp, Warning, TEXT("Mid Lane found, spawning command centers"));

			// Team1 커맨드 센터
			AAOSStructure* Team1Center = GetWorld()->SpawnActor<AAOSStructure>(
				AAOSStructure::StaticClass(),
				LaneInfo.Team1CommandCenterPosition,
				FRotator::ZeroRotator
			);

			if (Team1Center)
			{
				Team1Center->Initialize(EStructureType::CommandCenter, EAOSTeam::Team1, EAOSLane::Mid);
				CommandCenters.Add(EAOSTeam::Team1, Team1Center);
				UE_LOG(LogTemp, Warning, TEXT("Team1 Command Center spawned at (%.1f, %.1f, %.1f)"),
					LaneInfo.Team1CommandCenterPosition.X, LaneInfo.Team1CommandCenterPosition.Y, LaneInfo.Team1CommandCenterPosition.Z);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to spawn Team1 Command Center"));
			}

			// Team2 커맨드 센터
			AAOSStructure* Team2Center = GetWorld()->SpawnActor<AAOSStructure>(
				AAOSStructure::StaticClass(),
				LaneInfo.Team2CommandCenterPosition,
				FRotator::ZeroRotator
			);

			if (Team2Center)
			{
				Team2Center->Initialize(EStructureType::CommandCenter, EAOSTeam::Team2, EAOSLane::Mid);
				CommandCenters.Add(EAOSTeam::Team2, Team2Center);
				UE_LOG(LogTemp, Warning, TEXT("Team2 Command Center spawned at (%.1f, %.1f, %.1f)"),
					LaneInfo.Team2CommandCenterPosition.X, LaneInfo.Team2CommandCenterPosition.Y, LaneInfo.Team2CommandCenterPosition.Z);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to spawn Team2 Command Center"));
			}

			break;
		}
	}

	if (!bMidLaneFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Mid Lane not found! Cannot spawn command centers."));
	}

	UE_LOG(LogTemp, Warning, TEXT("=== SpawnStructures Complete ==="));
}

AAOSStructure* AAOSMapManager::GetCommandCenter(EAOSTeam Team) const
{
	return CommandCenters.FindRef(Team);
}

TArray<AAOSStructure*> AAOSMapManager::GetTowersInLane(EAOSLane Lane, EAOSTeam Team) const
{
	TArray<AAOSStructure*> Result;

	for (AAOSStructure* Tower : AllTowers)
	{
		if (Tower && Tower->GetLane() == Lane && Tower->GetOwnerTeam() == Team)
		{
			Result.Add(Tower);
		}
	}

	return Result;
}

void AAOSMapManager::SetupDefaultLaneInfo()
{
	// 기본 맵 레이아웃 설정
	// 3x3 라인 구조 (각 팀이 마주보는 구조)

	LanesInfo.Empty();

	// Top Lane
	{
		FLaneInfo TopLane;
		TopLane.LaneType = EAOSLane::Top;

		// Team1 (왼쪽/아래)
		TopLane.Team1StartPosition = FVector(2000, 2000, 0);
		TopLane.Team1EndPosition = FVector(-2000, -2000, 0);
		TopLane.Team1TowerPositions = {
			FVector(1400, 1400, 0),
			FVector(700, 700, 0),
			FVector(-350, -350, 0)
		};
		TopLane.Team1CommandCenterPosition = FVector(-2200, -2200, 0);

		// Team2 (오른쪽/위)
		TopLane.Team2StartPosition = FVector(-2000, -2000, 0);
		TopLane.Team2EndPosition = FVector(2000, 2000, 0);
		TopLane.Team2TowerPositions = {
			FVector(-1400, -1400, 0),
			FVector(-700, -700, 0),
			FVector(350, 350, 0)
		};
		TopLane.Team2CommandCenterPosition = FVector(2200, 2200, 0);

		LanesInfo.Add(TopLane);
	}

	// Mid Lane
	{
		FLaneInfo MidLane;
		MidLane.LaneType = EAOSLane::Mid;

		// Team1
		MidLane.Team1StartPosition = FVector(2000, 0, 0);
		MidLane.Team1EndPosition = FVector(-2000, 0, 0);
		MidLane.Team1TowerPositions = {
			FVector(1400, 0, 0),
			FVector(700, 0, 0),
			FVector(-350, 0, 0)
		};
		MidLane.Team1CommandCenterPosition = FVector(-2200, 0, 0);

		// Team2
		MidLane.Team2StartPosition = FVector(-2000, 0, 0);
		MidLane.Team2EndPosition = FVector(2000, 0, 0);
		MidLane.Team2TowerPositions = {
			FVector(-1400, 0, 0),
			FVector(-700, 0, 0),
			FVector(350, 0, 0)
		};
		MidLane.Team2CommandCenterPosition = FVector(2200, 0, 0);

		LanesInfo.Add(MidLane);
	}

	// Bottom Lane
	{
		FLaneInfo BottomLane;
		BottomLane.LaneType = EAOSLane::Bottom;

		// Team1
		BottomLane.Team1StartPosition = FVector(2000, -2000, 0);
		BottomLane.Team1EndPosition = FVector(-2000, 2000, 0);
		BottomLane.Team1TowerPositions = {
			FVector(1400, -1400, 0),
			FVector(700, -700, 0),
			FVector(-350, 350, 0)
		};
		BottomLane.Team1CommandCenterPosition = FVector(-2200, 2200, 0);

		// Team2
		BottomLane.Team2StartPosition = FVector(-2000, 2000, 0);
		BottomLane.Team2EndPosition = FVector(2000, -2000, 0);
		BottomLane.Team2TowerPositions = {
			FVector(-1400, 1400, 0),
			FVector(-700, 700, 0),
			FVector(350, -350, 0)
		};
		BottomLane.Team2CommandCenterPosition = FVector(2200, -2200, 0);

		LanesInfo.Add(BottomLane);
	}
}

// 🟢 NEW - 타워 위치에 디버그 박스 그리기
void AAOSMapManager::DrawDebugTowerPositions()
{
	if (!GetWorld())
		return;

	UE_LOG(LogTemp, Warning, TEXT("=== Drawing Debug Tower Boxes ==="));

	// 각 라인별로 타워 위치에 디버그 박스 표시
	for (const FLaneInfo& LaneInfo : LanesInfo)
	{
		FString LaneName;
		switch (LaneInfo.LaneType)
		{
		case EAOSLane::Top: LaneName = TEXT("Top"); break;
		case EAOSLane::Mid: LaneName = TEXT("Mid"); break;
		case EAOSLane::Bottom: LaneName = TEXT("Bottom"); break;
		default: LaneName = TEXT("Unknown"); break;
		}

		// Team1 타워 위치 - 파란색 박스
		for (int32 i = 0; i < LaneInfo.Team1TowerPositions.Num(); ++i)
		{
			const FVector& TowerPos = LaneInfo.Team1TowerPositions[i];
			DrawDebugBox(
				GetWorld(),
				TowerPos,
				FVector(DebugBoxSize, DebugBoxSize, DebugBoxSize),
				FColor::Blue,
				true,  // bPersistentLines
				-1.0f, // LifeTime (영구)
				0,     // DepthPriority
				10.0f  // Thickness
			);

			UE_LOG(LogTemp, Warning, TEXT("[%s Lane] Team1 Tower %d: (%.1f, %.1f, %.1f) - BLUE BOX"),
				*LaneName, i + 1, TowerPos.X, TowerPos.Y, TowerPos.Z);
		}

		// Team2 타워 위치 - 빨간색 박스
		for (int32 i = 0; i < LaneInfo.Team2TowerPositions.Num(); ++i)
		{
			const FVector& TowerPos = LaneInfo.Team2TowerPositions[i];
			DrawDebugBox(
				GetWorld(),
				TowerPos,
				FVector(DebugBoxSize, DebugBoxSize, DebugBoxSize),
				FColor::Red,
				true,  // bPersistentLines
				-1.0f, // LifeTime (영구)
				0,     // DepthPriority
				10.0f  // Thickness
			);

			UE_LOG(LogTemp, Warning, TEXT("[%s Lane] Team2 Tower %d: (%.1f, %.1f, %.1f) - RED BOX"),
				*LaneName, i + 1, TowerPos.X, TowerPos.Y, TowerPos.Z);
		}

		// Command Center - 노란색 박스 (더 큰 사이즈)
		DrawDebugBox(
			GetWorld(),
			LaneInfo.Team1CommandCenterPosition,
			FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
			FColor::Yellow,
			true,
			-1.0f,
			0,
			10.0f
		);

		DrawDebugBox(
			GetWorld(),
			LaneInfo.Team2CommandCenterPosition,
			FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
			FColor::Orange,
			true,
			-1.0f,
			0,
			10.0f
		);

		UE_LOG(LogTemp, Warning, TEXT("[%s Lane] Team1 Command Center: (%.1f, %.1f, %.1f) - YELLOW BOX"),
			*LaneName, LaneInfo.Team1CommandCenterPosition.X, LaneInfo.Team1CommandCenterPosition.Y, LaneInfo.Team1CommandCenterPosition.Z);
		UE_LOG(LogTemp, Warning, TEXT("[%s Lane] Team2 Command Center: (%.1f, %.1f, %.1f) - ORANGE BOX"),
			*LaneName, LaneInfo.Team2CommandCenterPosition.X, LaneInfo.Team2CommandCenterPosition.Y, LaneInfo.Team2CommandCenterPosition.Z);
	}

	UE_LOG(LogTemp, Warning, TEXT("=== Debug Tower Boxes Drawn ==="));
}
