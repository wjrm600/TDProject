#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AOSGameMode.h"
#include "AOSPlayerState.generated.h"

/**
 * Phase 3A: AOS 플레이어 상태 (팀/준비/배치 계획 리플리케이션)
 *
 * 각 플레이어(컨트롤러)는 자신의 PlayerState를 보유합니다.
 * 서버는 접속 순서대로 Team1/Team2를 할당하고, 클라이언트는 리플리케이션으로 자신의 팀을 받습니다.
 *
 * 주의: DeployPlan 고정 배열은 UHT 지원 제한이 있으므로 Top/Mid/Bottom 개별 프로퍼티로 분리
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerTeamChanged, EAOSTeam, NewTeam);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerReadyChanged, bool, bReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeployPlanChanged);

UCLASS()
class TDPROJECT_API AAOSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AAOSPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 팀 할당 (서버가 PostLogin에서 설정)
	UPROPERTY(ReplicatedUsing = OnRep_Team, BlueprintReadOnly, Category = "AOS|Player")
	EAOSTeam Team = EAOSTeam::Team1;

	// 준비 여부 (클라이언트가 Server RPC로 요청 → 서버가 업데이트)
	UPROPERTY(ReplicatedUsing = OnRep_Ready, BlueprintReadOnly, Category = "AOS|Player")
	bool bIsReady = false;

	// 라인별 배치 수 (0~2, UHT 호환 위해 개별 프로퍼티로 분리)
	UPROPERTY(ReplicatedUsing = OnRep_DeployPlan, BlueprintReadOnly, Category = "AOS|Player")
	int32 DeployCountTop = 2;

	UPROPERTY(ReplicatedUsing = OnRep_DeployPlan, BlueprintReadOnly, Category = "AOS|Player")
	int32 DeployCountMid = 2;

	UPROPERTY(ReplicatedUsing = OnRep_DeployPlan, BlueprintReadOnly, Category = "AOS|Player")
	int32 DeployCountBottom = 2;

	// 서버 전용 Setter (AOSGameMode / AOSPlayerController Server RPC에서 호출)
	void ServerSetTeam(EAOSTeam NewTeam);
	void ServerSetReady(bool bReady);
	void ServerSetDeployCount(EAOSLane Lane, int32 Count);

	// Getter
	UFUNCTION(BlueprintCallable, Category = "AOS|Player")
	EAOSTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Player")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Player")
	int32 GetDeployCount(EAOSLane Lane) const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Player")
	int32 GetTotalDeployCount() const { return DeployCountTop + DeployCountMid + DeployCountBottom; }

	// 클라이언트 UI 바인딩용 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "AOS|Player")
	FOnPlayerTeamChanged OnPlayerTeamChanged;

	UPROPERTY(BlueprintAssignable, Category = "AOS|Player")
	FOnPlayerReadyChanged OnPlayerReadyChanged;

	UPROPERTY(BlueprintAssignable, Category = "AOS|Player")
	FOnDeployPlanChanged OnDeployPlanChanged;

protected:
	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_Ready();

	UFUNCTION()
	void OnRep_DeployPlan();
};
