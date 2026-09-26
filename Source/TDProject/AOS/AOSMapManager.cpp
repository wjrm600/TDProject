#include "AOSMapManager.h"
#include "AOSStructure.h"
#include "AOSSpawnPoint.h"
#include "AOSGameMode.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

static TAutoConsoleVariable<int32> CVarShowStructureBoxes(
    TEXT("AOS.Debug.ShowStructureBoxes"), 0,
    TEXT("1=타워/커맨드센터 디버그 박스 표시, 0=숨김 (기본). 콘솔 명령으로 활성화: AOS.Debug.ShowStructureBoxes 1"));

static TAutoConsoleVariable<int32> CVarShowAttackRange(
    TEXT("AOS.Debug.ShowAttackRange"), 0,
    TEXT("1=모든 구조물·캐릭터 공격 범위 표시, 0=숨김"));

AAOSMapManager::AAOSMapManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAOSMapManager::BeginPlay()
{
	Super::BeginPlay();

	// 이전 PIE 세션이나 에디터에서 남은 persistent 라인 제거
	FlushPersistentDebugLines(GetWorld());

	InitializeMap();

	// 구조물은 서버(권한)만 스폰 → 클라는 리플리케이션으로 수신.
	// 가드 없으면 클라가 로컬 중복 구조물을 스폰 → 서버가 못 건드리는 "유령" 메시 +
	// HP바 2개 겹침(깜빡임) 발생. (클라는 SpawnStructures 스킵)
	if (HasAuthority())
	{
		SpawnStructures();
	}
}

void AAOSMapManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GetWorld()) return;

	// Dedicated Server에는 렌더 파이프라인이 없으므로 디버그 그리기 스킵
	// (CVar가 켜져 있어도 클라이언트만 시각화 — DS의 불필요한 액터 순회 방지)
	if (GetNetMode() == NM_DedicatedServer) return;

	// StructureBoxes 토글
	if (CVarShowStructureBoxes.GetValueOnGameThread())
	{
		DrawRuntimeStructureDebug();
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
		// 🟡 MODIFIED - 라인별 색상 (Top=Yellow, Mid=Green, Bottom=Cyan)
		FColor LaneColor;
		switch (LaneInfo.LaneType)
		{
			case EAOSLane::Top:    LaneColor = FColor::Yellow; break;
			case EAOSLane::Mid:    LaneColor = FColor::Green;  break;
			case EAOSLane::Bottom: LaneColor = FColor::Cyan;   break;
			default:               LaneColor = FColor::White;  break;
		}
		// Team1 = 밝은 색, Team2 = 어두운 색
		FColor Team1Color = LaneColor;
		FColor Team2Color = FColor(LaneColor.R / 2, LaneColor.G / 2, LaneColor.B / 2);

		// 라인 경로 그리기 - 스폰→타워1→타워2→타워3→적 커맨드 센터
		if (bShowLanePaths)
		{
			// 시작점: SpawnPoint 액터 매칭 → 없으면 첫 타워 위치 fallback
			FVector Team1Start = ResolveLaneStartForVisualization(LaneInfo, EAOSTeam::Team1);
			FVector Team2Start = ResolveLaneStartForVisualization(LaneInfo, EAOSTeam::Team2);

			// Team1 경로: Team1Start → 각 타워 → Team2 CC
			TArray<FVector> Team1Path;
			Team1Path.Add(Team1Start);
			for (const FVector& TowerPos : LaneInfo.Team1TowerPositions)
				Team1Path.Add(TowerPos);
			Team1Path.Add(Team2CommandCenterPosition);

			for (int32 i = 0; i < Team1Path.Num() - 1; ++i)
			{
				DrawDebugLine(GetWorld(), Team1Path[i], Team1Path[i + 1],
					Team1Color, true, -1.0f, 0, EditorVisualizationThickness);
			}

			// Team2 경로: Team2Start → 각 타워 → Team1 CC
			TArray<FVector> Team2Path;
			Team2Path.Add(Team2Start);
			for (const FVector& TowerPos : LaneInfo.Team2TowerPositions)
				Team2Path.Add(TowerPos);
			Team2Path.Add(Team1CommandCenterPosition);

			for (int32 i = 0; i < Team2Path.Num() - 1; ++i)
			{
				DrawDebugLine(GetWorld(), Team2Path[i], Team2Path[i + 1],
					Team2Color, true, -1.0f, 0, EditorVisualizationThickness);
			}
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

// 에디터 시각화용 라인 시작점 결정 헬퍼
// SpawnPoint 액터 매칭 → 없으면 첫 타워 위치 → 없으면 ZeroVector
FVector AAOSMapManager::ResolveLaneStartForVisualization(const FLaneInfo& LaneInfo, EAOSTeam Team) const
{
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> SpawnPoints;
		UGameplayStatics::GetAllActorsOfClass(World, AAOSSpawnPoint::StaticClass(), SpawnPoints);

		AAOSSpawnPoint* Best = nullptr;
		for (AActor* A : SpawnPoints)
		{
			AAOSSpawnPoint* SP = Cast<AAOSSpawnPoint>(A);
			if (!SP) continue;
			if (SP->GetTeam() == Team && SP->GetLane() == LaneInfo.LaneType)
			{
				if (!Best || SP->GetSpawnIndex() < Best->GetSpawnIndex())
				{
					Best = SP;
				}
			}
		}
		if (Best) return Best->GetActorLocation();
	}

	// Fallback: 첫 타워 위치
	const TArray<FVector>& Towers = (Team == EAOSTeam::Team1)
		? LaneInfo.Team1TowerPositions : LaneInfo.Team2TowerPositions;
	if (Towers.Num() > 0) return Towers[0];

	return FVector::ZeroVector;
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
	// 라인 시작 위치의 단일 진실 공급원은 SpawnPoint 액터.
	// DS 가드: GetAuthGameMode 는 클라이언트에서 nullptr 반환.
	if (UWorld* World = GetWorld())
	{
		if (AAOSGameMode* GM = World->GetAuthGameMode<AAOSGameMode>())
		{
			if (AAOSSpawnPoint* SP = GM->GetNearestSpawnPoint(Team, Lane))
			{
				return SP->GetActorLocation();
			}
		}
	}
	return FVector::ZeroVector;
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
		UClass* T1TowerClass = Team1TowerClass ? Team1TowerClass.Get() : AAOSStructure::StaticClass();
		for (const FVector& TowerPos : LaneInfo.Team1TowerPositions)
		{
			AAOSStructure* Tower = GetWorld()->SpawnActor<AAOSStructure>(
				T1TowerClass,
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
		UClass* T2TowerClass = Team2TowerClass ? Team2TowerClass.Get() : AAOSStructure::StaticClass();
		for (const FVector& TowerPos : LaneInfo.Team2TowerPositions)
		{
			AAOSStructure* Tower = GetWorld()->SpawnActor<AAOSStructure>(
				T2TowerClass,
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
	UClass* T1CCClass = Team1CommandCenterClass ? Team1CommandCenterClass.Get() : AAOSStructure::StaticClass();
	AAOSStructure* Team1Center = GetWorld()->SpawnActor<AAOSStructure>(
		T1CCClass,
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
	UClass* T2CCClass = Team2CommandCenterClass ? Team2CommandCenterClass.Get() : AAOSStructure::StaticClass();
	AAOSStructure* Team2Center = GetWorld()->SpawnActor<AAOSStructure>(
		T2CCClass,
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

	// 지형 4배 스케일 적용 — 캐릭터 공격 범위(500) 대비 라인 길이가 약 12배가 되어
	// MOBA 스타일 라인 푸시 동선이 적정한 비율을 가짐 (이전: ±2200 → ±8800)
	// Command Center 위치 설정 (팀당 1개)
	Team1CommandCenterPosition = FVector(-8800, 0, 0);  // Team1 본진 (왼쪽)
	Team2CommandCenterPosition = FVector(8800, 0, 0);   // Team2 본진 (오른쪽)

	// Top Lane
	{
		FLaneInfo TopLane;
		TopLane.LaneType = EAOSLane::Top;

		// Team1 타워 위치 (스폰 시작 위치는 SpawnPoint 액터가 단일 진실 공급원)
		TopLane.Team1TowerPositions = {
			FVector(5600, 5600, 0),
			FVector(2800, 2800, 0),
			FVector(-1400, -1400, 0)
		};

		// Team2 타워 위치
		TopLane.Team2TowerPositions = {
			FVector(-5600, -5600, 0),
			FVector(-2800, -2800, 0),
			FVector(1400, 1400, 0)
		};

		LanesInfo.Add(TopLane);
	}

	// Mid Lane
	{
		FLaneInfo MidLane;
		MidLane.LaneType = EAOSLane::Mid;

		// Team1 타워 위치
		MidLane.Team1TowerPositions = {
			FVector(5600, 0, 0),
			FVector(2800, 0, 0),
			FVector(-1400, 0, 0)
		};

		// Team2 타워 위치
		MidLane.Team2TowerPositions = {
			FVector(-5600, 0, 0),
			FVector(-2800, 0, 0),
			FVector(1400, 0, 0)
		};

		LanesInfo.Add(MidLane);
	}

	// Bottom Lane
	{
		FLaneInfo BottomLane;
		BottomLane.LaneType = EAOSLane::Bottom;

		// Team1 타워 위치
		BottomLane.Team1TowerPositions = {
			FVector(5600, -5600, 0),
			FVector(2800, -2800, 0),
			FVector(-1400, 1400, 0)
		};

		// Team2 타워 위치
		BottomLane.Team2TowerPositions = {
			FVector(-5600, 5600, 0),
			FVector(-2800, 2800, 0),
			FVector(1400, -1400, 0)
		};

		LanesInfo.Add(BottomLane);
	}
}

// 🟢 NEW - 타워 위치에 디버그 박스 그리기 (DrawRuntimeStructureDebug 로 리다이렉트)
void AAOSMapManager::DrawDebugTowerPositions()
{
	DrawRuntimeStructureDebug();
}

// 🟢 NEW - 런타임 구조물 디버그 시각화 (매 프레임, CVar 토글 가능)
void AAOSMapManager::DrawRuntimeStructureDebug()
{
	if (!GetWorld())
		return;

	// ─────────────────────────────────────────────────────────────────────────
	// AllTowers(코드 스폰 전용) 대신 TActorIterator로 월드의 모든 AOSStructure 순회
	// → 레벨 에디터에 배치된 액터 포함, 실제 화면에 보이는 구조물에 정확히 표시
	// ─────────────────────────────────────────────────────────────────────────
	for (TActorIterator<AAOSStructure> It(GetWorld()); It; ++It)
	{
		AAOSStructure* Structure = *It;
		if (!Structure || !IsValid(Structure) || Structure->IsDestroyed())
			continue;

		FVector Pos = Structure->GetActorLocation();
		EAOSTeam Team         = Structure->GetOwnerTeam();
		EStructureType SType  = Structure->GetStructureType();
		EAOSLane Lane         = Structure->GetLane();

		FString LaneName;
		switch (Lane)
		{
		case EAOSLane::Top:    LaneName = TEXT("Top");    break;
		case EAOSLane::Mid:    LaneName = TEXT("Mid");    break;
		case EAOSLane::Bottom: LaneName = TEXT("Bottom"); break;
		default:               LaneName = TEXT("Unknown"); break;
		}

		const FString TeamStr = (Team == EAOSTeam::Team1) ? TEXT("T1") : TEXT("T2");

		if (SType == EStructureType::CommandCenter)
		{
			// Command Center: 노란(T1) / 주황(T2), 1.5배 박스
			FColor CCColor = (Team == EAOSTeam::Team1) ? FColor::Yellow : FColor::Orange;

			DrawDebugBox(GetWorld(), Pos,
				FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
				CCColor, false, 0.0f, 0, 10.0f);

			DrawDebugString(GetWorld(),
				Pos + FVector(0, 0, DebugBoxSize * 1.5f + 80.0f),
				FString::Printf(TEXT("[%s]\nCommandCenter"), *TeamStr),
				nullptr, CCColor, 0.0f, true);
		}
		else
		{
			// Tower: 빨강(T1) / 파랑(T2) — 캐릭터·HP바·타워 메시 팀 색과 동일
			FColor BoxColor = (Team == EAOSTeam::Team1) ? FColor::Red : FColor::Blue;

			DrawDebugBox(GetWorld(), Pos,
				FVector(DebugBoxSize, DebugBoxSize, DebugBoxSize),
				BoxColor, false, 0.0f, 0, 10.0f);

			DrawDebugString(GetWorld(),
				Pos + FVector(0, 0, DebugBoxSize + 50.0f),
				FString::Printf(TEXT("[%s] Tower\n%s"), *TeamStr, *LaneName),
				nullptr, BoxColor, 0.0f, true);
		}
	}
}
