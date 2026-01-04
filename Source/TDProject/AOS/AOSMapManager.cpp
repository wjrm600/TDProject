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

	// 시각화 업데이트
	UpdateEditorVisualization();
}

// 🟢 NEW - 프로퍼티 변경 시 시각화 갱신
void AAOSMapManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// LanesInfo 또는 시각화 설정이 변경되면 시각화 갱신
	FName PropertyName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, LanesInfo) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, Team1CommandCenterPosition) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, Team2CommandCenterPosition) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, bShowEditorVisualization) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, bShowLanePaths) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, bShowTowerPositions) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, bShowCommandCenters) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, EditorVisualizationThickness))
	{
		UpdateEditorVisualization();
	}
}

// 🟢 NEW - 에디터 시각화 업데이트 헬퍼 함수
void AAOSMapManager::UpdateEditorVisualization()
{
	if (!bShowEditorVisualization || !GetWorld())
		return;

	// 이전 디버그 라인 완전히 제거
	FlushPersistentDebugLines(GetWorld());

	for (const FLaneInfo& LaneInfo : LanesInfo)
	{
		FColor Team1Color = FColor::Cyan;    // Team1: 청록색
		FColor Team2Color = FColor::Magenta; // Team2: 마젠타색

		// 라인 경로 그리기 (스폰 지점 → 적 본진)
		if (bShowLanePaths)
		{
			// Team1 라인 경로: Team1 스폰 → Team2 본진
			DrawDebugLine(
				GetWorld(),
				LaneInfo.Team1StartPosition,
				Team2CommandCenterPosition,
				Team1Color,
				true,
				-1.0f,
				0,
				EditorVisualizationThickness
			);

			// Team2 라인 경로: Team2 스폰 → Team1 본진
			DrawDebugLine(
				GetWorld(),
				LaneInfo.Team2StartPosition,
				Team1CommandCenterPosition,
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

				// 추가: X 마커로 타워 위치를 더 명확하게 표시
				DrawDebugLine(GetWorld(),
					TowerPos + FVector(-100, -100, 0),
					TowerPos + FVector(100, 100, 0),
					Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
				DrawDebugLine(GetWorld(),
					TowerPos + FVector(100, -100, 0),
					TowerPos + FVector(-100, 100, 0),
					Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
				DrawDebugLine(GetWorld(),
					TowerPos + FVector(0, 0, 0),
					TowerPos + FVector(0, 0, 200),
					Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
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

				// 추가: X 마커로 타워 위치를 더 명확하게 표시
				DrawDebugLine(GetWorld(),
					TowerPos + FVector(-100, -100, 0),
					TowerPos + FVector(100, 100, 0),
					Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
				DrawDebugLine(GetWorld(),
					TowerPos + FVector(100, -100, 0),
					TowerPos + FVector(-100, 100, 0),
					Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
				DrawDebugLine(GetWorld(),
					TowerPos + FVector(0, 0, 0),
					TowerPos + FVector(0, 0, 200),
					Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
			}
		}
	}

	// Command Center 위치 표시 (팀당 1개씩)
	if (bShowCommandCenters)
	{
		FColor Team1Color = FColor::Cyan;
		FColor Team2Color = FColor::Magenta;
		float Size = 150.0f;

		// Team1 Command Center - 박스 프레임으로 그리기
		FVector CC1 = Team1CommandCenterPosition;

		// 밑면
		DrawDebugLine(GetWorld(), CC1 + FVector(-Size, -Size, -Size), CC1 + FVector(Size, -Size, -Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(Size, -Size, -Size), CC1 + FVector(Size, Size, -Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(Size, Size, -Size), CC1 + FVector(-Size, Size, -Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(-Size, Size, -Size), CC1 + FVector(-Size, -Size, -Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);

		// 윗면
		DrawDebugLine(GetWorld(), CC1 + FVector(-Size, -Size, Size), CC1 + FVector(Size, -Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(Size, -Size, Size), CC1 + FVector(Size, Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(Size, Size, Size), CC1 + FVector(-Size, Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(-Size, Size, Size), CC1 + FVector(-Size, -Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);

		// 수직 연결선
		DrawDebugLine(GetWorld(), CC1 + FVector(-Size, -Size, -Size), CC1 + FVector(-Size, -Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(Size, -Size, -Size), CC1 + FVector(Size, -Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(Size, Size, -Size), CC1 + FVector(Size, Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC1 + FVector(-Size, Size, -Size), CC1 + FVector(-Size, Size, Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);

		// Team2 Command Center - 박스 프레임으로 그리기
		FVector CC2 = Team2CommandCenterPosition;

		// 밑면
		DrawDebugLine(GetWorld(), CC2 + FVector(-Size, -Size, -Size), CC2 + FVector(Size, -Size, -Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(Size, -Size, -Size), CC2 + FVector(Size, Size, -Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(Size, Size, -Size), CC2 + FVector(-Size, Size, -Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(-Size, Size, -Size), CC2 + FVector(-Size, -Size, -Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);

		// 윗면
		DrawDebugLine(GetWorld(), CC2 + FVector(-Size, -Size, Size), CC2 + FVector(Size, -Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(Size, -Size, Size), CC2 + FVector(Size, Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(Size, Size, Size), CC2 + FVector(-Size, Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(-Size, Size, Size), CC2 + FVector(-Size, -Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);

		// 수직 연결선
		DrawDebugLine(GetWorld(), CC2 + FVector(-Size, -Size, -Size), CC2 + FVector(-Size, -Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(Size, -Size, -Size), CC2 + FVector(Size, -Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(Size, Size, -Size), CC2 + FVector(Size, Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
		DrawDebugLine(GetWorld(), CC2 + FVector(-Size, Size, -Size), CC2 + FVector(-Size, Size, Size), Team2Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
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
	// 적 팀의 Command Center 위치를 반환
	if (Team == EAOSTeam::Team1)
	{
		return Team2CommandCenterPosition;  // Team1은 Team2 본진을 목표로
	}
	else
	{
		return Team1CommandCenterPosition;  // Team2는 Team1 본진을 목표로
	}
}

void AAOSMapManager::SpawnStructures()
{
	// 🟡 MODIFIED - 디버그 로깅 추가
	UE_LOG(LogTemp, Warning, TEXT("=== SpawnStructures Started ==="));
	UE_LOG(LogTemp, Warning, TEXT("Total LanesInfo: %d"), LanesInfo.Num());
	UE_LOG(LogTemp, Warning, TEXT("Team1 CommandCenter Position: (%.1f, %.1f, %.1f)"),
		Team1CommandCenterPosition.X, Team1CommandCenterPosition.Y, Team1CommandCenterPosition.Z);
	UE_LOG(LogTemp, Warning, TEXT("Team2 CommandCenter Position: (%.1f, %.1f, %.1f)"),
		Team2CommandCenterPosition.X, Team2CommandCenterPosition.Y, Team2CommandCenterPosition.Z);

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

	// 커맨드 센터 생성 (팀당 1개)
	UE_LOG(LogTemp, Warning, TEXT("=== Spawning Command Centers ==="));

	// Team1 커맨드 센터
	AAOSStructure* Team1Center = GetWorld()->SpawnActor<AAOSStructure>(
		AAOSStructure::StaticClass(),
		Team1CommandCenterPosition,
		FRotator::ZeroRotator
	);

	if (Team1Center)
	{
		Team1Center->Initialize(EStructureType::CommandCenter, EAOSTeam::Team1, EAOSLane::Mid);
		CommandCenters.Add(EAOSTeam::Team1, Team1Center);
		UE_LOG(LogTemp, Warning, TEXT("Team1 Command Center spawned at (%.1f, %.1f, %.1f)"),
			Team1CommandCenterPosition.X, Team1CommandCenterPosition.Y, Team1CommandCenterPosition.Z);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn Team1 Command Center"));
	}

	// Team2 커맨드 센터
	AAOSStructure* Team2Center = GetWorld()->SpawnActor<AAOSStructure>(
		AAOSStructure::StaticClass(),
		Team2CommandCenterPosition,
		FRotator::ZeroRotator
	);

	if (Team2Center)
	{
		Team2Center->Initialize(EStructureType::CommandCenter, EAOSTeam::Team2, EAOSLane::Mid);
		CommandCenters.Add(EAOSTeam::Team2, Team2Center);
		UE_LOG(LogTemp, Warning, TEXT("Team2 Command Center spawned at (%.1f, %.1f, %.1f)"),
			Team2CommandCenterPosition.X, Team2CommandCenterPosition.Y, Team2CommandCenterPosition.Z);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn Team2 Command Center"));
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

	// Command Center 위치 설정 (팀당 1개)
	Team1CommandCenterPosition = FVector(-2200, 0, 0);  // Team1 본진 (왼쪽)
	Team2CommandCenterPosition = FVector(2200, 0, 0);   // Team2 본진 (오른쪽)

	// Top Lane
	{
		FLaneInfo TopLane;
		TopLane.LaneType = EAOSLane::Top;

		// Team1 (왼쪽/아래) - 스폰 위치
		TopLane.Team1StartPosition = FVector(2000, 2000, 0);
		TopLane.Team1TowerPositions = {
			FVector(1400, 1400, 0),
			FVector(700, 700, 0),
			FVector(-350, -350, 0)
		};

		// Team2 (오른쪽/위) - 스폰 위치
		TopLane.Team2StartPosition = FVector(-2000, -2000, 0);
		TopLane.Team2TowerPositions = {
			FVector(-1400, -1400, 0),
			FVector(-700, -700, 0),
			FVector(350, 350, 0)
		};

		LanesInfo.Add(TopLane);
	}

	// Mid Lane
	{
		FLaneInfo MidLane;
		MidLane.LaneType = EAOSLane::Mid;

		// Team1 - 스폰 위치
		MidLane.Team1StartPosition = FVector(2000, 0, 0);
		MidLane.Team1TowerPositions = {
			FVector(1400, 0, 0),
			FVector(700, 0, 0),
			FVector(-350, 0, 0)
		};

		// Team2 - 스폰 위치
		MidLane.Team2StartPosition = FVector(-2000, 0, 0);
		MidLane.Team2TowerPositions = {
			FVector(-1400, 0, 0),
			FVector(-700, 0, 0),
			FVector(350, 0, 0)
		};

		LanesInfo.Add(MidLane);
	}

	// Bottom Lane
	{
		FLaneInfo BottomLane;
		BottomLane.LaneType = EAOSLane::Bottom;

		// Team1 - 스폰 위치
		BottomLane.Team1StartPosition = FVector(2000, -2000, 0);
		BottomLane.Team1TowerPositions = {
			FVector(1400, -1400, 0),
			FVector(700, -700, 0),
			FVector(-350, 350, 0)
		};

		// Team2 - 스폰 위치
		BottomLane.Team2StartPosition = FVector(-2000, 2000, 0);
		BottomLane.Team2TowerPositions = {
			FVector(-1400, 1400, 0),
			FVector(-700, 700, 0),
			FVector(350, -350, 0)
		};

		LanesInfo.Add(BottomLane);
	}
}

// 🟢 NEW - 타워 위치에 디버그 박스 그리기
void AAOSMapManager::DrawDebugTowerPositions()
{
	if (!GetWorld())
		return;

	UE_LOG(LogTemp, Warning, TEXT("=== Drawing Debug Tower Boxes ==="));

	// 실제 스폰된 타워들의 위치에 디버그 박스 표시
	for (AAOSStructure* Tower : AllTowers)
	{
		if (!Tower)
			continue;

		FVector TowerPos = Tower->GetActorLocation();
		EAOSTeam Team = Tower->GetOwnerTeam();
		EAOSLane Lane = Tower->GetLane();

		FString LaneName;
		switch (Lane)
		{
		case EAOSLane::Top: LaneName = TEXT("Top"); break;
		case EAOSLane::Mid: LaneName = TEXT("Mid"); break;
		case EAOSLane::Bottom: LaneName = TEXT("Bottom"); break;
		default: LaneName = TEXT("Unknown"); break;
		}

		FColor BoxColor = (Team == EAOSTeam::Team1) ? FColor::Blue : FColor::Red;
		FString TeamName = (Team == EAOSTeam::Team1) ? TEXT("Team1") : TEXT("Team2");

		DrawDebugBox(
			GetWorld(),
			TowerPos,
			FVector(DebugBoxSize, DebugBoxSize, DebugBoxSize),
			BoxColor,
			true,  // bPersistentLines
			-1.0f, // LifeTime (영구)
			0,     // DepthPriority
			10.0f  // Thickness
		);

		UE_LOG(LogTemp, Warning, TEXT("[%s Lane] %s Tower: (%.1f, %.1f, %.1f) - %s BOX"),
			*LaneName, *TeamName, TowerPos.X, TowerPos.Y, TowerPos.Z,
			(Team == EAOSTeam::Team1) ? TEXT("BLUE") : TEXT("RED"));
	}

	// Command Center - 노란색/주황색 박스 (더 큰 사이즈, 팀당 1개)
	DrawDebugBox(
		GetWorld(),
		Team1CommandCenterPosition,
		FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
		FColor::Yellow,
		true,
		-1.0f,
		0,
		10.0f
	);

	DrawDebugBox(
		GetWorld(),
		Team2CommandCenterPosition,
		FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
		FColor::Orange,
		true,
		-1.0f,
		0,
		10.0f
	);

	UE_LOG(LogTemp, Warning, TEXT("Team1 Command Center: (%.1f, %.1f, %.1f) - YELLOW BOX"),
		Team1CommandCenterPosition.X, Team1CommandCenterPosition.Y, Team1CommandCenterPosition.Z);
	UE_LOG(LogTemp, Warning, TEXT("Team2 Command Center: (%.1f, %.1f, %.1f) - ORANGE BOX"),
		Team2CommandCenterPosition.X, Team2CommandCenterPosition.Y, Team2CommandCenterPosition.Z);

	UE_LOG(LogTemp, Warning, TEXT("=== Debug Tower Boxes Drawn ==="));
}
