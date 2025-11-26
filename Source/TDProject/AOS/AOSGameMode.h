#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AOSGameMode.generated.h"

class AAOSCharacter;
class AAOSStructure;
class AAOSSpawnPoint;
class AAOSMapManager;

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
	Preparation UMETA(DisplayName = "Preparation"),
	GameRunning UMETA(DisplayName = "Game Running"),
	GameEnded UMETA(DisplayName = "Game Ended")
};

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

	// 🟢 NEW - PlayerStart에서 자동 캐릭터 생성 방지
	virtual void RestartPlayer(AController* NewPlayer) override;

	// 게임 시간 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void EndGame(EAOSTeam WinningTeam);

	// 캐릭터 배치 관련
	UFUNCTION(BlueprintCallable, Category = "AOS|Characters")
	void DeployCharacters(EAOSTeam Team, const TArray<EAOSLane>& LaneAssignments);

	// 스폰 포인트 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	void RegisterSpawnPoint(AAOSSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	AAOSSpawnPoint* GetNearestSpawnPoint(EAOSTeam Team, EAOSLane Lane);

	// 🔴 REMOVED: void SpawnCharacter(AAOSCharacter* Character, EAOSTeam Team, EAOSLane Lane);
	// ↑ 더 이상 필요 없음 (자동 생성으로 변경)

	// 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	void SpawnCharactersAtAllSpawnPoints();

	// 구조물 생성 및 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Structures")
	void InitializeStructures();

	UFUNCTION(BlueprintCallable, Category = "AOS|Structures")
	AAOSStructure* GetCommandCenter(EAOSTeam Team);

	UFUNCTION(BlueprintCallable, Category = "AOS|Structures")
	TArray<AAOSStructure*> GetTowersByLane(EAOSLane Lane, EAOSTeam Team);

	// Getter
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	float GetRemainingTime() const { return RemainingGameTime; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	EAOSGameState GetAOSGameState() const { return AOSGameState; }

protected:
	// 게임 시간 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Settings")
	float GameDuration = 600.0f; // 10분

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	float RemainingGameTime;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	EAOSGameState AOSGameState = EAOSGameState::Preparation;

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

	// 🟢 NEW - 맵 매니저 참조
	AAOSMapManager* MapManager;

	// 🟢 NEW - 타워 생성 함수
	void InitializeMapManager();
	void CacheTowerReferences();

private:
	void UpdateGameTime(float DeltaTime);
	void CheckVictoryConditions();
};
