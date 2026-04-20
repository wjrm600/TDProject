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

	// RTS용 캐릭터 클래스 (DefaultPawnClass와 분리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AOS|Spawn")
	TSubclassOf<AAOSCharacter> CharacterClass;

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

	// Getter
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	float GetRemainingTime() const { return RemainingGameTime; }

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

private:
	void SetGameState(EAOSGameState NewState);
	void UpdateGameTime(float DeltaTime);
	void CheckVictoryConditions();

	// 배치 계획 저장 (TMap<EAOSTeam, TMap<>> 은 UHT 미지원이므로 배열로 관리)
	// DeployPlan[TeamIndex][LaneIndex] = Count
	int32 DeployPlan[2][3];
};
