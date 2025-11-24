#include "AOSMapManager.h"
#include "AOSStructure.h"

AAOSMapManager::AAOSMapManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAOSMapManager::BeginPlay()
{
	Super::BeginPlay();

	InitializeMap();
	SpawnStructures();
}

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
			FVector(1200, 1200, 0),
			FVector(600, 600, 0),
			FVector(0, 0, 0)
		};
		TopLane.Team1CommandCenterPosition = FVector(-1200, -1200, 0);

		// Team2 (오른쪽/위)
		TopLane.Team2StartPosition = FVector(-2000, -2000, 0);
		TopLane.Team2EndPosition = FVector(2000, 2000, 0);
		TopLane.Team2TowerPositions = {
			FVector(-1200, -1200, 0),
			FVector(-600, -600, 0),
			FVector(0, 0, 0)
		};
		TopLane.Team2CommandCenterPosition = FVector(1200, 1200, 0);

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
			FVector(1200, 0, 0),
			FVector(600, 0, 0),
			FVector(0, 0, 0)
		};
		MidLane.Team1CommandCenterPosition = FVector(-1500, 0, 0);

		// Team2
		MidLane.Team2StartPosition = FVector(-2000, 0, 0);
		MidLane.Team2EndPosition = FVector(2000, 0, 0);
		MidLane.Team2TowerPositions = {
			FVector(-1200, 0, 0),
			FVector(-600, 0, 0),
			FVector(0, 0, 0)
		};
		MidLane.Team2CommandCenterPosition = FVector(1500, 0, 0);

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
			FVector(1200, -1200, 0),
			FVector(600, -600, 0),
			FVector(0, 0, 0)
		};
		BottomLane.Team1CommandCenterPosition = FVector(-1200, 1200, 0);

		// Team2
		BottomLane.Team2StartPosition = FVector(-2000, 2000, 0);
		BottomLane.Team2EndPosition = FVector(2000, -2000, 0);
		BottomLane.Team2TowerPositions = {
			FVector(-1200, 1200, 0),
			FVector(-600, 600, 0),
			FVector(0, 0, 0)
		};
		BottomLane.Team2CommandCenterPosition = FVector(1200, -1200, 0);

		LanesInfo.Add(BottomLane);
	}
}
