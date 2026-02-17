#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AOSGameMode.h"
#include "AOSSpawnPoint.generated.h"

/**
 * 캐릭터 스폰 포인트
 * 플레이어가 캐릭터를 배치할 위치 지정
 */
UCLASS()
class TDPROJECT_API AAOSSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AAOSSpawnPoint();

	virtual void BeginPlay() override;

	// 초기 설정
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	void Initialize(EAOSTeam InTeam, EAOSLane InLane, int32 InSpawnIndex);

	// Getter
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	EAOSTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	EAOSLane GetLane() const { return Lane; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	int32 GetSpawnIndex() const { return SpawnIndex; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	FVector GetSpawnLocation() const { return GetActorLocation(); }

	// 스폰 포인트가 사용 중인지 확인
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	bool IsOccupied() const { return OccupiedCharacter != nullptr; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	class AAOSCharacter* GetOccupiedCharacter() const { return OccupiedCharacter; }

	// 캐릭터 할당/해제
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	void SetOccupiedCharacter(class AAOSCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	void ReleaseCharacter();

	// 🟢 NEW - 스폰 포인트에서 캐릭터 생성
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	class AAOSCharacter* SpawnCharacterAtPoint(TSubclassOf<class AAOSCharacter> CharacterClass);

	// 🟢 NEW - 생성된 캐릭터 초기화
	UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
	void InitializeCharacter(class AAOSCharacter* Character);

protected:
	// 팀 정보
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Spawn")
	EAOSTeam Team = EAOSTeam::Team1;

	// 라인 정보 (새로 추가!)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Spawn")
	EAOSLane Lane = EAOSLane::Mid;

	// 스폰 포인트 인덱스 (같은 팀/라인 내에서의 순서)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Spawn")
	int32 SpawnIndex = 0;

	// 스폰 활성화 여부 (false면 캐릭터를 스폰하지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Spawn")
	bool bSpawnEnabled = true;

	// 현재 이 스폰 포인트에 있는 캐릭터
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Spawn")
	AAOSCharacter* OccupiedCharacter = nullptr;

	// 시각적 표시 (에디터에서 보기 위한 컴포넌트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|Spawn")
	class UBillboardComponent* BillboardComponent;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
