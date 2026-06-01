#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AOSGameMode.generated.h"

class AAOSCharacter;
class AAOSStructure;
class AAOSSpawnPoint;
class AAOSMapManager;
class AAOSGameState;
class AAOSPlayerState;

// 레인 배치 계획: TArray 2중 중첩을 UHT가 지원 안 하므로 구조체로 래핑
USTRUCT()
struct FAOSLaneDeployPlan
{
	GENERATED_BODY()
	TArray<TSubclassOf<AAOSCharacter>> Classes;
};

// Slice 0: 라인별 구매 아이템 인벤토리 (DeployPlan 과 동일한 [2][3] 래핑 패턴)
USTRUCT()
struct FAOSLaneItemInventory
{
	GENERATED_BODY()
	// DT_Items 의 RowName 목록 — 같은 아이템 중복 구매 시 중복 추가(스택)
	TArray<FName> ItemRowNames;
};

// 에디터에서 등록하는 캐릭터 Blueprint 정보
USTRUCT(BlueprintType)
struct FCharacterRosterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Roster")
	TSubclassOf<AAOSCharacter> CharacterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Roster")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AOS|Roster")
	UTexture2D* Portrait = nullptr;
};

UENUM(BlueprintType)
enum class EAOSLane : uint8
{
	Top UMETA(DisplayName = "Top Lane"),
	Mid UMETA(DisplayName = "Mid Lane"),
	Bottom UMETA(DisplayName = "Bottom Lane")
};

UENUM(BlueprintType)
enum class EAOSTeam : uint8
{
	Team1 UMETA(DisplayName = "Team 1"),
	Team2 UMETA(DisplayName = "Team 2")
};

UENUM(BlueprintType)
enum class EAOSGameState : uint8
{
	MainMenu UMETA(DisplayName = "Main Menu"),
	Lobby UMETA(DisplayName = "Lobby"),
	RoundPreparation UMETA(DisplayName = "Round Preparation"),
	RoundRunning UMETA(DisplayName = "Round Running"),
	Settlement UMETA(DisplayName = "Settlement")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameStateChanged, EAOSGameState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundEnded, int32, RoundNumber);

/**
 * AOS 게임 모드 메인 클래스
 * 게임 플로우, 라운드 시간, 승리 조건 등을 관리
 */
UCLASS()
class TDPROJECT_API AAOSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAOSGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// PlayerStart에서 자동 캐릭터 생성 방지
	virtual void RestartPlayer(AController* NewPlayer) override;

	// Phase 3A: 네트워킹 - 접속/퇴장 핸들러
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// 라운드 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void StartRound();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void EndRound();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void EndGame(EAOSTeam WinningTeam);

	// 하위 호환: StartGame()은 StartRound()로 위임
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void StartGame() { StartRound(); }

	// 배치 계획 설정/조회
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void SetLaneDeployCount(EAOSTeam Team, EAOSLane Lane, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	int32 GetLaneDeployCount(EAOSTeam Team, EAOSLane Lane) const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	int32 GetTotalDeployCount(EAOSTeam Team) const;

	// 캐릭터 배치 관련
	UFUNCTION(BlueprintCallable, Category = "AOS|Characters")
	void DeployCharacters(EAOSTeam Team, const TArray<EAOSLane>& LaneAssignments);

	// 스폰 포인트 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	void RegisterSpawnPoint(AAOSSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	AAOSSpawnPoint* GetNearestSpawnPoint(EAOSTeam Team, EAOSLane Lane);

	// [Deprecated 폴백] CharacterRoster가 비어있을 때만 사용되는 최후 안전망.
	// 정상 사용 시에는 CharacterRoster에 BP_Character류 Blueprint를 등록할 것.
	// 비워두면 Roster가 단일 진실 공급원이 되어 BP CDO 데이터가 보존됨.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AOS|Spawn", meta = (DisplayName = "Character Class (Fallback Only)"))
	TSubclassOf<AAOSCharacter> CharacterClass;

	// 실제 스폰에 사용되는 캐릭터 Blueprint 목록 (단일 진실 공급원)
	// BP_AOSGameMode에서 BP_Character류 Blueprint를 등록할 것
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AOS|Roster")
	TArray<FCharacterRosterEntry> CharacterRoster;

	// 로스터 조회
	UFUNCTION(BlueprintCallable, Category = "AOS|Roster")
	const TArray<FCharacterRosterEntry>& GetCharacterRoster() const { return CharacterRoster; }

	// 레인 배치 클래스 목록 설정/조회 (신규 API)
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void SetLaneDeployClasses(EAOSTeam Team, EAOSLane Lane,
		const TArray<TSubclassOf<AAOSCharacter>>& Classes);

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	const TArray<TSubclassOf<AAOSCharacter>>& GetLaneDeployClasses(EAOSTeam Team, EAOSLane Lane) const;

	// 네트워크 경유 버전
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void ServerSetLaneDeployClassesForPlayer(AAOSPlayerState* PlayerState, EAOSLane Lane,
		const TArray<TSubclassOf<AAOSCharacter>>& Classes);

	// 구조물 생성 및 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Structures")
	void InitializeStructures();

	UFUNCTION(BlueprintCallable, Category = "AOS|Structures")
	AAOSStructure* GetCommandCenter(EAOSTeam Team);

	UFUNCTION(BlueprintCallable, Category = "AOS|Structures")
	TArray<AAOSStructure*> GetTowersByLane(EAOSLane Lane, EAOSTeam Team);

	// 캐릭터 사망 시 호출
	UFUNCTION(BlueprintCallable, Category = "AOS|Characters")
	void OnCharacterDestroyed(AAOSCharacter* DestroyedCharacter);

	// Slice 0: 구조물(타워/CC) 파괴 시 호출 — 파괴한 쪽(= 구조물 반대 팀)에 골드 지급
	UFUNCTION(BlueprintCallable, Category = "AOS|Structures")
	void OnStructureDestroyedAwardGold(AAOSStructure* DestroyedStructure);

	// Slice 0: 팀에 골드 지급/차감 (서버 권한). GameState 경유.
	UFUNCTION(BlueprintCallable, Category = "AOS|Economy")
	void AwardGold(EAOSTeam Team, int32 Amount);

	// Slice 0: 라인 아이템 구매 (서버 권한). 골드 검증 → 차감 → 인벤토리 추가. 성공 시 true.
	UFUNCTION(BlueprintCallable, Category = "AOS|Economy")
	bool ServerBuyLaneItem(EAOSTeam Team, EAOSLane Lane, FName ItemRowName);

	// Slice 0: 특정 (팀,라인) 이 소유한 아이템 RowName 목록 (상점 UI/디버그용)
	UFUNCTION(BlueprintCallable, Category = "AOS|Economy")
	TArray<FName> GetLaneItems(EAOSTeam Team, EAOSLane Lane) const;

	// Getter

	/** Returns the MapManager instance owned/discovered by this GameMode (server-side only). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AOS|Map")
	AAOSMapManager* GetMapManager() const { return MapManager; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	float GetRemainingTime() const { return RemainingGameTime; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	float GetRemainingPreparationTime() const { return PreparationTimeRemaining; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	EAOSGameState GetAOSGameState() const { return AOSGameState; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	bool ShouldAutoStartGame() const { return bAutoStartGame; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	FName GetGameMapName() const { return GameMapName; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	FName GetMainMenuMapName() const { return MainMenuMapName; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	int32 GetCurrentRound() const { return CurrentRound; }

	// 게임 상태 전이 함수
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void TransitionToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void TransitionToLobby();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void TransitionToRoundPreparation();

	// Draw 라운드 처리 — 5초 타이머 후 다음 라운드 준비로 전환
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void HandleDrawRound();

	// 게임 상태 변경 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "AOS|Game")
	FOnGameStateChanged OnGameStateChanged;

	// 라운드 종료 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "AOS|Game")
	FOnRoundEnded OnRoundEnded;

	// 라인 당 최대 캐릭터 수
	static const int32 MaxCharactersPerLane = 2;

	// Phase 3A: 네트워킹 - 팀별 플레이어 준비 상태 체크 + 라운드 자동 시작
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void ServerSetPlayerReady(AAOSPlayerState* PlayerState, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	bool AreAllPlayersReady();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void ServerSetLaneDeployCountForPlayer(AAOSPlayerState* PlayerState, EAOSLane Lane, int32 Count);

protected:
	// 레벨 분리 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Settings")
	bool bAutoStartGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Settings")
	FName GameMapName = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Settings")
	FName MainMenuMapName = TEXT("/Game/AOS/Lvl_MainMenu");

	// 게임 시간 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Settings")
	float GameDuration = 600.0f; // 10분

	// Slice 0: 골드 보상 수치 (design-balance 튜닝 대상)
	// 적 캐릭터 처치 시 처치한 팀에 지급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Economy")
	int32 GoldPerCharacterKill = 50;

	// 적 타워/CC 파괴 시 파괴한 팀에 지급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Economy")
	int32 GoldPerStructureKill = 150;

	// 라운드 시작 시 양 팀에 지급되는 패시브 수입 (라운드 보장 경제)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Economy")
	int32 GoldPerRoundIncome = 100;

	// Slice 0: 아이템 정의 DataTable (Row 타입 = FAOSItemRow). BP_AOSGameMode 에서 DT_Items 지정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Economy")
	TObjectPtr<UDataTable> ItemTable = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	float RemainingGameTime;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	EAOSGameState AOSGameState = EAOSGameState::MainMenu;

	// 라운드 정보
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	int32 CurrentRound = 0;

	// 라운드 종료 타이머
	FTimerHandle RoundEndTimerHandle;

	// 라운드 준비 자동 시작 타이머 (30초)
	FTimerHandle RoundPreparationTimerHandle;

	// Draw 라운드 자동 전환 타이머 (5초 후 다음 라운드 준비)
	FTimerHandle DrawTransitionHandle;

	// 라운드 준비 남은 시간 (UI 표시용)
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	float PreparationTimeRemaining = 30.0f;

	// 팀 별 구조물 참조
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Structures")
	TMap<EAOSTeam, AAOSStructure*> CommandCenters;

	// 팀 별 캐릭터들
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Characters")
	TArray<AAOSCharacter*> Team1Characters;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Characters")
	TArray<AAOSCharacter*> Team2Characters;

	// 라인별 배치 정보 (런타임용, UPROPERTY 없음)
	TMap<EAOSTeam, TArray<EAOSLane>> CharacterDeployments;

	// 라인별 타워들 (런타임용, UPROPERTY 없음)
	TMap<EAOSLane, TArray<AAOSStructure*>> LaneTowers;

	// 스폰 포인트 (런타임용, UPROPERTY 없음)
	TArray<AAOSSpawnPoint*> AllSpawnPoints;
	TMap<EAOSTeam, TArray<AAOSSpawnPoint*>> TeamSpawnPoints;

	// 맵 매니저 참조
	AAOSMapManager* MapManager;

	// 타워 생성 함수
	void InitializeMapManager();
	void CacheTowerReferences();

	// 배치 계획 내부 관리
	void InitializeDefaultDeployPlan();
	int32 GetDeployCount(EAOSTeam Team, EAOSLane Lane) const;
	void SetDeployCount(EAOSTeam Team, EAOSLane Lane, int32 Count);

	// 라운드 기반 캐릭터 생성
	void SpawnCharactersForRound();

	// Slice 0: 스폰된 캐릭터에 그 (팀,라인) 이 소유한 아이템 GE 들을 재적용 (라운드 누적)
	void ApplyLaneItemsToCharacter(EAOSTeam Team, EAOSLane Lane, AAOSCharacter* Character);

private:
	void SetGameState(EAOSGameState NewState);
	void UpdateGameTime(float DeltaTime);
	void CheckVictoryConditions();

	// 배치 계획 저장 (TMap<EAOSTeam, TMap<>> 은 UHT 미지원이므로 배열로 관리)
	// DeployPlan[TeamIndex][LaneIndex].Classes = 배치할 캐릭터 클래스 목록
	FAOSLaneDeployPlan DeployPlan[2][3];

	// Slice 0: 라인별 구매 아이템 인벤토리 (서버 전용, 라운드 간 누적)
	// ItemInventory[TeamIndex][LaneIndex].ItemRowNames
	FAOSLaneItemInventory ItemInventory[2][3];
};
