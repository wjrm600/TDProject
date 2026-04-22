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
	EAOSLane LaneType = EAOSLane::Mid;

	// 각 팀의 스폰 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team1StartPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Team2StartPosition = FVector::ZeroVector;

	// 라인의 타워 위치들
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> Team1TowerPositions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> Team2TowerPositions;
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
	virtual void Tick(float DeltaTime) override;

	// 🟢 NEW - 에디터에서 시각화
#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

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

	// 🟢 NEW - 디버그 기능
	UFUNCTION(BlueprintCallable, Category = "AOS|Debug")
	void DrawDebugTowerPositions();

	void DrawRuntimeStructureDebug();

protected:
	// 라인 정보
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Map", meta = (AllowPrivateAccess = "true"))
	TArray<FLaneInfo> LanesInfo;

	// 팀별 Command Center 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Map", meta = (AllowPrivateAccess = "true"))
	FVector Team1CommandCenterPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Map", meta = (AllowPrivateAccess = "true"))
	FVector Team2CommandCenterPosition;

	// 스폰된 구조물들
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Map")
	TMap<EAOSTeam, AAOSStructure*> CommandCenters;

	// 라인별 타워들 (런타임용)
	UPROPERTY()
	TArray<AAOSStructure*> AllTowers;

	// 팀별 구조물 Blueprint 클래스 (미설정 시 기본 C++ 클래스로 스폰)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure Classes")
	TSubclassOf<AAOSStructure> Team1TowerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure Classes")
	TSubclassOf<AAOSStructure> Team2TowerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure Classes")
	TSubclassOf<AAOSStructure> Team1CommandCenterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Structure Classes")
	TSubclassOf<AAOSStructure> Team2CommandCenterClass;

	// 🟢 NEW - 디버그 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Debug")
	bool bShowDebugTowerBoxes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Debug")
	float DebugBoxSize = 200.0f;

	// 🟢 NEW - 에디터 시각화 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
	bool bShowEditorVisualization = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
	bool bShowLanePaths = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
	bool bShowTowerPositions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
	bool bShowCommandCenters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
	float EditorVisualizationThickness = 5.0f;

private:
	void SetupDefaultLaneInfo();

#if WITH_EDITOR
	void UpdateEditorVisualization();
#endif
};
