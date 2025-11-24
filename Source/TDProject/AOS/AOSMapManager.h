#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AOSGameMode.h"
#include "AOSMapManager.generated.h"

class AAOSStructure;
class AAOSCharacter;

USTRUCT(BlueprintType)
struct FLaneInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAOSLane LaneType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team1StartPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team1EndPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team2StartPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team2EndPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> Team1TowerPositions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> Team2TowerPositions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team1CommandCenterPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team2CommandCenterPosition;
};

/**
 * AOS 게임의 맵 관리자
 * 3개 라인 구성 (탑, 미드, 바텀)
 * 각 라인에 타워 3개씩 배치
 */
UCLASS()
class TDPROJECT_API AAOSMapManager : public AActor
{
	GENERATED_BODY()

public:
	AAOSMapManager();

	virtual void BeginPlay() override;

	// 맵 초기화
	UFUNCTION(BlueprintCallable, Category = "AOS|Map")
	void InitializeMap();

	// 라인 정보 접근
	UFUNCTION(BlueprintCallable, Category = "AOS|Map")
	FLaneInfo GetLaneInfo(EAOSLane Lane) const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Map")
	FVector GetLaneStartPosition(EAOSLane Lane, EAOSTeam Team) const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Map")
	FVector GetLaneEndPosition(EAOSLane Lane, EAOSTeam Team) const;

	// 구조물 관리
	UFUNCTION(BlueprintCallable, Category = "AOS|Map")
	void SpawnStructures();

	UFUNCTION(BlueprintCallable, Category = "AOS|Map")
	AAOSStructure* GetCommandCenter(EAOSTeam Team) const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Map")
	TArray<AAOSStructure*> GetTowersInLane(EAOSLane Lane, EAOSTeam Team) const;

protected:
	// 라인 정보
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Map")
	TArray<FLaneInfo> LanesInfo;

	// 스폰된 구조물들
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Map")
	TMap<EAOSTeam, AAOSStructure*> CommandCenters;

	// 라인별 타워들 (런타임용, UPROPERTY 없음)
	TArray<AAOSStructure*> AllTowers;

private:
	void SetupDefaultLaneInfo();
};
