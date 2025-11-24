#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AOSGameMode.h"
#include "AOSPlayerController.generated.h"

class AAOSCharacter;

/**
 * AOS 게임의 플레이어 컨트롤러
 * 캐릭터 배치, 게임 상태 관리, 네트워크 동기화 등을 담당
 */
UCLASS()
class TDPROJECT_API AAOSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAOSPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// 캐릭터 배치 관련
	UFUNCTION(BlueprintCallable, Category = "AOS|Deployment")
	void SetCharacterDeployment(const TArray<EAOSLane>& LaneAssignments);

	UFUNCTION(BlueprintCallable, Category = "AOS|Deployment")
	void DeployCharactersToLanes();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void StartGameFromPreparation();

	// 플레이어의 캐릭터 목록
	UFUNCTION(BlueprintCallable, Category = "AOS|Characters")
	TArray<AAOSCharacter*> GetPlayerCharacters() const { return PlayerCharacters; }

	// 팀 정보
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void SetPlayerTeam(EAOSTeam Team);

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	EAOSTeam GetPlayerTeam() const { return PlayerTeam; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	EAOSTeam PlayerTeam = EAOSTeam::Team1;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Characters")
	TArray<AAOSCharacter*> PlayerCharacters;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Deployment")
	TArray<EAOSLane> CurrentDeployment;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	class AAOSGameMode* GameMode;

private:
	void SpawnPlayerCharacters();
};
