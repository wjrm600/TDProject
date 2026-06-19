#include "AOSGameMode.h"
#include "AOSCharacter.h"
#include "AOSAIController.h"
#include "AOSStructure.h"
#include "AOSPlayerController.h"
#include "AOSSpawnPoint.h"
#include "AOSMapManager.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"
#include "GAS/Data/AOSItemData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/DataTable.h"
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

	// CharacterClass(폴백)는 의도적으로 nullptr로 둠 — 실제 스폰은 CharacterRoster에서 결정
	// BP_AOSGameMode에서 CharacterRoster를 비워두면 Roster 미설정 경고 후 스폰 스킵
	CharacterClass = nullptr;

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
	else if (AOSGameState == EAOSGameState::BanPick)
	{
		// 벤픽 턴 카운트다운 — 1초마다 GameState 리플리케이션 (위젯 표시용)
		int32 PrevSecond = FMath::CeilToInt(DraftTurnTimeRemaining);
		DraftTurnTimeRemaining = FMath::Max(0.0f, DraftTurnTimeRemaining - DeltaTime);
		int32 CurrSecond = FMath::CeilToInt(DraftTurnTimeRemaining);
		if (PrevSecond != CurrSecond)
		{
			if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
			{
				AOSGS->ServerSetDraftTurnTime(DraftTurnTimeRemaining);
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

	// Slice 0: 라운드 패시브 수입 — 양 팀에 매 라운드 시작 시 골드 지급 (경제 보장).
	AwardGold(EAOSTeam::Team1, GoldPerRoundIncome);
	AwardGold(EAOSTeam::Team2, GoldPerRoundIncome);

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
	InitLaneTracking();
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

	// Slice 1: 이번 라운드의 라인 승패 결과를 GameState 에 push (다음 준비 화면 패널이 표시)
	PushRoundResultToGameState();

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

	// Roster의 첫 번째 유효 클래스 → CharacterClass 순으로 폴백
	// (Roster가 단일 진실 공급원, CharacterClass는 Roster 미설정 시의 최후 안전망)
	TSubclassOf<AAOSCharacter> FB = nullptr;
	for (const FCharacterRosterEntry& Entry : CharacterRoster)
	{
		if (Entry.CharacterClass) { FB = Entry.CharacterClass; break; }
	}
	if (!FB) FB = CharacterClass;

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

// 기본 배치 계획 초기화 (Roster 첫 번째 유효 캐릭터 → CharacterClass 폴백, Top2/Mid2/Bottom1)
void AAOSGameMode::InitializeDefaultDeployPlan()
{
	// Roster의 첫 번째 유효 클래스를 기본 클래스로 사용
	// (Roster가 단일 진실 공급원, CharacterClass는 최후 안전망)
	TSubclassOf<AAOSCharacter> DefaultClass = nullptr;
	for (const FCharacterRosterEntry& Entry : CharacterRoster)
	{
		if (Entry.CharacterClass) { DefaultClass = Entry.CharacterClass; break; }
	}
	if (!DefaultClass) DefaultClass = CharacterClass;

	if (!DefaultClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] InitializeDefaultDeployPlan: CharacterRoster와 CharacterClass 모두 미설정 — 기본 배치 계획 비워둠. BP_AOSGameMode의 Roster를 설정하세요."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[GameMode] InitializeDefaultDeployPlan: 기본 클래스=%s (%s)"),
			*DefaultClass->GetName(),
			(CharacterRoster.Num() > 0 && CharacterRoster[0].CharacterClass == DefaultClass) ? TEXT("Roster") : TEXT("CharacterClass 폴백"));
	}

	// DefaultClass 의 로스터 인덱스 = 기본 배치 유닛 id (없으면 -1 = 아이템 미적용)
	int32 DefaultUnitId = -1;
	for (int32 i = 0; i < CharacterRoster.Num(); ++i)
	{
		if (CharacterRoster[i].CharacterClass && CharacterRoster[i].CharacterClass == DefaultClass)
		{
			DefaultUnitId = i;
			break;
		}
	}

	for (int32 T = 0; T < 2; ++T)
	{
		for (int32 L = 0; L < 3; ++L)
		{
			DeployPlan[T][L].Classes.Empty();
			DeployPlan[T][L].UnitIds.Empty();
		}

		if (DefaultClass)
		{
			// 기본: Top 2, Mid 2, Bottom 1
			DeployPlan[T][0].Classes = { DefaultClass, DefaultClass };
			DeployPlan[T][0].UnitIds = { DefaultUnitId, DefaultUnitId };
			DeployPlan[T][1].Classes = { DefaultClass, DefaultClass };
			DeployPlan[T][1].UnitIds = { DefaultUnitId, DefaultUnitId };
			DeployPlan[T][2].Classes = { DefaultClass };
			DeployPlan[T][2].UnitIds = { DefaultUnitId };
		}
	}
}

// 레인 배치 클래스 목록 설정 (신규 API)
void AAOSGameMode::SetLaneDeployClasses(EAOSTeam Team, EAOSLane Lane,
	const TArray<TSubclassOf<AAOSCharacter>>& Classes,
	const TArray<int32>& UnitIds)
{
	if (!HasAuthority()) return;
	int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	int32 L = static_cast<int32>(Lane);
	if (L < 0 || L >= 3) return;

	DeployPlan[T][L].Classes.Empty();
	DeployPlan[T][L].UnitIds.Empty();
	const int32 Count = FMath::Min(Classes.Num(), MaxCharactersPerLane);
	for (int32 i = 0; i < Count; ++i)
	{
		if (!Classes[i]) continue;
		DeployPlan[T][L].Classes.Add(Classes[i]);
		// UnitIds 가 부족/누락이면 -1 (유닛 미식별 → 아이템 미적용)
		DeployPlan[T][L].UnitIds.Add(UnitIds.IsValidIndex(i) ? UnitIds[i] : -1);
	}
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
	EAOSLane Lane, const TArray<TSubclassOf<AAOSCharacter>>& Classes,
	const TArray<int32>& UnitIds)
{
	if (!HasAuthority() || !PlayerState) return;
	if (AOSGameState != EAOSGameState::RoundPreparation) return;

	const EAOSTeam DeployTeam = PlayerState->GetTeam();
	int32 T = (DeployTeam == EAOSTeam::Team1) ? 0 : 1;

	// 벤픽 픽 필터: 해당 팀이 픽한 유닛만 배치 허용 (UI 가 막지만 서버에서 강제)
	const AAOSGameState* AOSGS = GetGameState<AAOSGameState>();
	TArray<TSubclassOf<AAOSCharacter>> FilteredClasses;
	TArray<int32> FilteredUnitIds;
	for (int32 i = 0; i < Classes.Num(); ++i)
	{
		const int32 Uid = UnitIds.IsValidIndex(i) ? UnitIds[i] : -1;
		if (Classes[i] && Uid >= 0 && AOSGS && AOSGS->IsUnitPickedByTeam(Uid, DeployTeam))
		{
			FilteredClasses.Add(Classes[i]);
			FilteredUnitIds.Add(Uid);
		}
	}

	int32 NewCount = FMath::Min(FilteredClasses.Num(), MaxCharactersPerLane);

	// 총합 5명 초과 방지
	int32 Others = 0;
	for (int32 OtherL = 0; OtherL < 3; ++OtherL)
		if (OtherL != static_cast<int32>(Lane))
			Others += DeployPlan[T][OtherL].Classes.Num();
	if (Others + NewCount > 5) return;

	SetLaneDeployClasses(DeployTeam, Lane, FilteredClasses, FilteredUnitIds);
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
			const TArray<int32>& PlannedUnitIds = DeployPlan[T][L].UnitIds;

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
				if (!ClassToSpawn)
				{
					// Roster → CharacterClass 폴백 (BP 데이터 보존을 위해 C++ base 클래스는 사용하지 않음)
					for (const FCharacterRosterEntry& Entry : CharacterRoster)
					{
						if (Entry.CharacterClass) { ClassToSpawn = Entry.CharacterClass; break; }
					}
					if (!ClassToSpawn) ClassToSpawn = CharacterClass;
				}
				if (!ClassToSpawn)
				{
					UE_LOG(LogTemp, Warning, TEXT("[GameMode] %s %s 라인 [%d]: 스폰할 클래스 없음 (Roster/CharacterClass 모두 미설정). 스킵."),
						CurrentTeam == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
						CurrentLane == EAOSLane::Top ? TEXT("Top") : CurrentLane == EAOSLane::Mid ? TEXT("Mid") : TEXT("Bottom"),
						i);
					continue;
				}

				AAOSCharacter* NewCharacter = LaneSpawnPoints[i]->SpawnCharacterAtPoint(ClassToSpawn);
				if (NewCharacter)
				{
					if (CurrentTeam == EAOSTeam::Team1)
						Team1Characters.Add(NewCharacter);
					else
						Team2Characters.Add(NewCharacter);

					// 이 유닛이 구매한 아이템 GE 를 재적용 (라운드 리셋돼도 유닛에 누적)
					const int32 SpawnedUnitId = PlannedUnitIds.IsValidIndex(i) ? PlannedUnitIds[i] : -1;
					ApplyUnitItemsToCharacter(CurrentTeam, SpawnedUnitId, NewCharacter);
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

	// Slice 1: 라인 승패 추적 (라운드 진행 중에만 — 정산/정리 중 사망은 무시)
	if (AOSGameState == EAOSGameState::RoundRunning)
	{
		RecordLaneDeath(CharacterTeam, DestroyedCharacter->GetLane());
	}

	// Slice 0: 처치한 팀(= 죽은 캐릭터의 반대 팀)에 골드 지급.
	// killer 추적 없이 단순화 — 팀 대 팀 구도라 반대 팀이 처치자.
	const EAOSTeam KillerTeam = GetOpposingTeam(CharacterTeam);
	AwardGold(KillerTeam, GoldPerCharacterKill);

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

// ============================================================
// Slice 0: 골드 적립
// ============================================================

EAOSTeam AAOSGameMode::GetOpposingTeam(EAOSTeam Team)
{
	return (Team == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;
}

void AAOSGameMode::AwardGold(EAOSTeam Team, int32 Amount)
{
	if (!HasAuthority() || Amount == 0)
	{
		return;
	}

	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerAddGold(Team, Amount);
		UE_LOG(LogTemp, Log, TEXT("[Economy] %s 골드 %+d → 총 %d"),
			Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
			Amount, AOSGS->GetGold(Team));
	}
}

void AAOSGameMode::OnStructureDestroyedAwardGold(AAOSStructure* DestroyedStructure)
{
	if (!HasAuthority() || !DestroyedStructure)
	{
		return;
	}

	// 파괴한 팀 = 구조물 소유 팀의 반대.
	const EAOSTeam StructureTeam = DestroyedStructure->GetOwnerTeam();
	const EAOSTeam DestroyerTeam = GetOpposingTeam(StructureTeam);
	AwardGold(DestroyerTeam, GoldPerStructureKill);
}

// ============================================================
// Slice 0: 아이템 구매 / 소유 / 적용
// ============================================================

bool AAOSGameMode::ServerBuyItemForUnit(EAOSTeam Team, int32 UnitId, FName ItemRowName)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!ItemTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Economy] ItemTable 미설정 — 구매 불가. BP_AOSGameMode 에 DT_Items 지정 필요."));
		return false;
	}

	if (UnitId < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Economy] 유효하지 않은 UnitId(%d) — 구매 거부"), UnitId);
		return false;
	}

	// 라운드 준비/정산 단계에서만 구매 허용 (전투 중 구매 방지)
	if (AOSGameState != EAOSGameState::RoundPreparation && AOSGameState != EAOSGameState::Settlement)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Economy] 준비/정산 단계가 아님 — 구매 거부 (현재 상태=%d)"), (int32)AOSGameState);
		return false;
	}

	const FAOSItemRow* Row = ItemTable->FindRow<FAOSItemRow>(ItemRowName, TEXT("ServerBuyItemForUnit"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Economy] 아이템 Row '%s' 없음"), *ItemRowName.ToString());
		return false;
	}

	AAOSGameState* AOSGS = GetGameState<AAOSGameState>();
	if (!AOSGS)
	{
		return false;
	}

	// 골드 검증
	if (AOSGS->GetGold(Team) < Row->Cost)
	{
		UE_LOG(LogTemp, Log, TEXT("[Economy] %s 골드 부족 (보유 %d < 비용 %d) — '%s' 구매 실패"),
			Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
			AOSGS->GetGold(Team), Row->Cost, *ItemRowName.ToString());
		return false;
	}

	// 차감 + 유닛 인벤토리 추가 (라운드 간 누적, 유닛 귀속)
	AwardGold(Team, -Row->Cost);

	const int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	TArray<FName>& UnitInv = UnitItemInventory[T].FindOrAdd(UnitId);
	UnitInv.Add(ItemRowName);

	UE_LOG(LogTemp, Log, TEXT("[Economy] %s 유닛#%d '%s' 구매 완료 (-%d골드). 유닛 보유 아이템 %d개"),
		Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
		UnitId, *ItemRowName.ToString(), Row->Cost, UnitInv.Num());

	return true;
}

TArray<FName> AAOSGameMode::GetUnitItems(EAOSTeam Team, int32 UnitId) const
{
	const int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	return UnitItemInventory[T].FindRef(UnitId);
}

void AAOSGameMode::ApplyUnitItemsToCharacter(EAOSTeam Team, int32 UnitId, AAOSCharacter* Character)
{
	if (!HasAuthority() || !Character || !ItemTable || UnitId < 0)
	{
		return;
	}

	const int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	const TArray<FName>* OwnedPtr = UnitItemInventory[T].Find(UnitId);
	if (!OwnedPtr || OwnedPtr->Num() == 0)
	{
		return;
	}
	const TArray<FName>& Owned = *OwnedPtr;

	IAbilitySystemInterface* AsiChar = Cast<IAbilitySystemInterface>(Character);
	UAbilitySystemComponent* ASC = AsiChar ? AsiChar->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	int32 Applied = 0;
	for (const FName& RowName : Owned)
	{
		const FAOSItemRow* Row = ItemTable->FindRow<FAOSItemRow>(RowName, TEXT("ApplyUnitItems"));
		if (!Row || !Row->StatEffect)
		{
			continue;
		}

		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Row->StatEffect, 1.0f, Ctx);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			Applied++;
		}
	}

	if (Applied > 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Economy] %s 유닛#%d 캐릭터에 아이템 %d개 적용"),
			Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"), UnitId, Applied);
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

// ============================================================
// 벤픽 드래프트 (Lobby → BanPick → RoundPreparation)
// ============================================================

void AAOSGameMode::TransitionToBanPick()
{
	if (!HasAuthority()) return;
	AAOSGameState* AOSGS = GetGameState<AAOSGameState>();
	if (!AOSGS) return;

	AOSGS->ServerResetDraft();
	SetGameState(EAOSGameState::BanPick);

	// 모든 PlayerController 에 로스터 RPC 전송 (원격 클라 포함 — TransitionToRoundPreparation 패턴)
	const TArray<FCharacterRosterEntry>& Roster = GetCharacterRoster();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AAOSPlayerController* PC = Cast<AAOSPlayerController>(It->Get()))
		{
			PC->Client_ReceiveCharacterRoster(Roster);
		}
	}

	StartDraftTurnTimer();
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] BanPick 상태로 전이 — 드래프트 시작 (로스터 %d종, 시퀀스 %d스텝)"),
		Roster.Num(), AAOSGameState::GetDraftSequence().Num());
}

void AAOSGameMode::StartDraftTurnTimer()
{
	DraftTurnTimeRemaining = DraftTurnDuration;
	if (AAOSGameState* AOSGS = GetGameState<AAOSGameState>())
	{
		AOSGS->ServerSetDraftTurnTime(DraftTurnTimeRemaining);
	}
	GetWorldTimerManager().SetTimer(DraftTurnTimerHandle, this, &AAOSGameMode::OnDraftTurnTimeout, DraftTurnDuration, false);
}

void AAOSGameMode::OnDraftTurnTimeout()
{
	AAOSGameState* AOSGS = GetGameState<AAOSGameState>();
	if (!AOSGS || AOSGameState != EAOSGameState::BanPick || AOSGS->IsDraftComplete()) return;

	// 시간 초과 → 활성 팀에 가용 유닛 중 랜덤 자동 선택
	const int32 RosterNum = GetCharacterRoster().Num();
	TArray<int32> Available;
	for (int32 i = 0; i < RosterNum; ++i)
	{
		if (AOSGS->IsUnitAvailableForDraft(i)) Available.Add(i);
	}
	const int32 Choice = (Available.Num() > 0) ? Available[FMath::RandRange(0, Available.Num() - 1)] : -1;
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 벤픽 턴 타임아웃 → 자동선택 유닛#%d (팀%d)"),
		Choice, (int32)AOSGS->GetActiveDraftTeam());
	ServerApplyDraftSelection(AOSGS->GetActiveDraftTeam(), Choice);
}

void AAOSGameMode::ServerAutoCompleteDraft()
{
	if (!HasAuthority()) return;
	AAOSGameState* AOSGS = GetGameState<AAOSGameState>();
	if (!AOSGS || AOSGameState != EAOSGameState::BanPick || AOSGS->IsDraftComplete()) return;

	const int32 RosterNum = GetCharacterRoster().Num();
	// 남은 모든 스텝을 활성 팀별 랜덤 가용 유닛으로 채움. ServerApplyDraftSelection 이
	// 스텝 진행 + 마지막에 RoundPreparation 전환까지 처리. Guard 는 무한루프 방지(시퀀스=14).
	int32 Guard = 0;
	while (!AOSGS->IsDraftComplete() && Guard++ < 64)
	{
		TArray<int32> Available;
		for (int32 i = 0; i < RosterNum; ++i)
		{
			if (AOSGS->IsUnitAvailableForDraft(i)) Available.Add(i);
		}
		const int32 Choice = (Available.Num() > 0) ? Available[FMath::RandRange(0, Available.Num() - 1)] : -1;
		ServerApplyDraftSelection(AOSGS->GetActiveDraftTeam(), Choice);
	}
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 치트: 드래프트 자동 완성 (%d 스텝 채움)"), Guard);
}

void AAOSGameMode::ServerApplyDraftSelection(EAOSTeam Team, int32 UnitId)
{
	if (!HasAuthority()) return;
	AAOSGameState* AOSGS = GetGameState<AAOSGameState>();
	if (!AOSGS || AOSGameState != EAOSGameState::BanPick || AOSGS->IsDraftComplete()) return;

	// 활성 팀의 선택만 유효
	if (Team != AOSGS->GetActiveDraftTeam()) return;

	// 유효 유닛이면 밴/픽 기록 (UnitId<0 = 자동선택 실패 → 기록 없이 스텝만 진행)
	const bool bValidUnit = (UnitId >= 0) && (UnitId < GetCharacterRoster().Num())
		&& AOSGS->IsUnitAvailableForDraft(UnitId);
	if (UnitId >= 0 && !bValidUnit) return;  // 이미 밴/픽된 잘못된 선택 → 거부

	if (bValidUnit)
	{
		if (AOSGS->IsCurrentStepBan())
			AOSGS->ServerRecordBan(Team, UnitId);
		else
			AOSGS->ServerRecordPick(Team, UnitId);
	}

	AOSGS->ServerSetDraftStep(AOSGS->CurrentDraftStep + 1);

	if (AOSGS->IsDraftComplete())
	{
		GetWorldTimerManager().ClearTimer(DraftTurnTimerHandle);
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 드래프트 완료 → RoundPreparation"));
		TransitionToRoundPreparation();
	}
	else
	{
		StartDraftTurnTimer();
	}
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

// ============================================================
// Slice 1: 라인 승패 추적 → 라운드 결과 요약
// ============================================================

void AAOSGameMode::InitLaneTracking()
{
	for (int32 L = 0; L < 3; ++L)
	{
		LaneAlive[0][L] = 0;
		LaneAlive[1][L] = 0;
		LaneWinner[L] = 0;
		LaneWinnerSurvivors[L] = 0;
		bLaneDecided[L] = false;
	}

	for (AAOSCharacter* C : Team1Characters)
	{
		if (!C) continue;
		const int32 L = static_cast<int32>(C->GetLane());
		if (L >= 0 && L < 3) ++LaneAlive[0][L];
	}
	for (AAOSCharacter* C : Team2Characters)
	{
		if (!C) continue;
		const int32 L = static_cast<int32>(C->GetLane());
		if (L >= 0 && L < 3) ++LaneAlive[1][L];
	}

	// 한 팀이 0명으로 시작한 라인은 즉시 확정
	for (int32 L = 0; L < 3; ++L)
	{
		CheckLaneDecided(L);
	}
}

void AAOSGameMode::RecordLaneDeath(EAOSTeam Team, EAOSLane Lane)
{
	const int32 T = (Team == EAOSTeam::Team1) ? 0 : 1;
	const int32 L = static_cast<int32>(Lane);
	if (L < 0 || L >= 3) return;
	LaneAlive[T][L] = FMath::Max(0, LaneAlive[T][L] - 1);
	CheckLaneDecided(L);
}

void AAOSGameMode::CheckLaneDecided(int32 LaneIdx)
{
	if (LaneIdx < 0 || LaneIdx >= 3 || bLaneDecided[LaneIdx]) return;

	const int32 A1 = LaneAlive[0][LaneIdx];
	const int32 A2 = LaneAlive[1][LaneIdx];
	if (A1 > 0 && A2 > 0) return; // 양 팀 모두 생존 → 미확정

	bLaneDecided[LaneIdx] = true;
	if (A1 == 0 && A2 == 0)
	{
		LaneWinner[LaneIdx] = 0;           // 무승부 (동시 전멸 또는 양 팀 미배치)
		LaneWinnerSurvivors[LaneIdx] = 0;
	}
	else if (A1 == 0)
	{
		LaneWinner[LaneIdx] = 2;           // Team2 승 (Team1 이 먼저 전멸)
		LaneWinnerSurvivors[LaneIdx] = A2;
	}
	else // A2 == 0
	{
		LaneWinner[LaneIdx] = 1;           // Team1 승
		LaneWinnerSurvivors[LaneIdx] = A1;
	}

	UE_LOG(LogTemp, Log, TEXT("[Round] 라인 %d 확정: 승자=%d (잔존 %d)"),
		LaneIdx, LaneWinner[LaneIdx], LaneWinnerSurvivors[LaneIdx]);
}

void AAOSGameMode::PushRoundResultToGameState()
{
	AAOSGameState* AOSGS = GetGameState<AAOSGameState>();
	if (!AOSGS) return;

	// 미확정 라인은 무승부로 마감 (안전망)
	for (int32 L = 0; L < 3; ++L)
	{
		if (!bLaneDecided[L]) { LaneWinner[L] = 0; LaneWinnerSurvivors[L] = 0; }
	}

	FAOSRoundResult Result;
	Result.RoundNumber = CurrentRound;
	Result.bValid = true;
	for (int32 L = 0; L < 3; ++L)
	{
		Result.LaneWinners.Add(LaneWinner[L]);
		Result.LaneWinnerSurvivors.Add(LaneWinnerSurvivors[L]);
	}
	AOSGS->ServerSetRoundResult(Result);
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
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] 양쪽 준비 완료 → BanPick(드래프트)"));
			TransitionToBanPick();
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
