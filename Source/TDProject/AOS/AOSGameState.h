#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "AOSGameMode.h"
#include "AOSGameState.generated.h"

/**
 * Phase 3A: AOS 게임 상태 (클라이언트 리플리케이션용)
 *
 * GameMode는 서버에만 존재하므로, 클라이언트가 현재 게임 상태/라운드/팀 준비 상태를 알기 위해
 * GameState를 통해 리플리케이션합니다.
 *
 * 서버: AAOSGameMode가 SetReplicatedState()로 상태를 업데이트
 * 클라이언트: OnRep_CurrentState가 자동 호출되어 UI 갱신
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameStateChangedClient, EAOSGameState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundNumberChanged, int32, NewRound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamReadyChanged);

UCLASS()
class TDPROJECT_API AAOSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AAOSGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 리플리케이션된 게임 상태 (Phase 2의 EAOSGameState와 동일)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentState, BlueprintReadOnly, Category = "AOS|Game")
	EAOSGameState CurrentState = EAOSGameState::MainMenu;

	// 리플리케이션된 라운드 번호
	UPROPERTY(ReplicatedUsing = OnRep_CurrentRound, BlueprintReadOnly, Category = "AOS|Game")
	int32 CurrentRound = 0;

	// 양쪽 팀 준비 여부
	UPROPERTY(ReplicatedUsing = OnRep_TeamReady, BlueprintReadOnly, Category = "AOS|Game")
	bool bTeam1Ready = false;

	UPROPERTY(ReplicatedUsing = OnRep_TeamReady, BlueprintReadOnly, Category = "AOS|Game")
	bool bTeam2Ready = false;

	// 서버 전용: 상태 업데이트 (AOSGameMode에서 호출)
	void ServerSetCurrentState(EAOSGameState NewState);
	void ServerSetCurrentRound(int32 NewRound);
	void ServerSetTeamReady(EAOSTeam Team, bool bReady);

	// Getter
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	EAOSGameState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	int32 GetCurrentRound() const { return CurrentRound; }

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	bool IsTeamReady(EAOSTeam Team) const;

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	bool AreBothTeamsReady() const { return bTeam1Ready && bTeam2Ready; }

	// 클라이언트 UI 바인딩용 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "AOS|Game")
	FOnGameStateChangedClient OnGameStateChangedClient;

	UPROPERTY(BlueprintAssignable, Category = "AOS|Game")
	FOnRoundNumberChanged OnRoundNumberChanged;

	UPROPERTY(BlueprintAssignable, Category = "AOS|Game")
	FOnTeamReadyChanged OnTeamReadyChanged;

protected:
	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	void OnRep_CurrentRound();

	UFUNCTION()
	void OnRep_TeamReady();
};
