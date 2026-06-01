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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerCountChanged, int32, Count);
// Slice 0: 팀 골드 변경 알림 (UI 바인딩용). 두 팀 중 어느 팀이 얼마가 됐는지.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamGoldChanged, EAOSTeam, Team, int32, NewGold);

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

	// 현재 접속 인원 (로비 UI 갱신용)
	UPROPERTY(ReplicatedUsing = OnRep_ConnectedCount, BlueprintReadOnly, Category = "AOS|Game")
	int32 ConnectedCount = 0;

	// 라운드 준비 남은 시간 (클라이언트 UI 표시용 — 1초 단위로 갱신)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AOS|Game")
	float PreparationTimeRemaining = 30.0f;

	// Draw 결과 플래그 (양쪽 배치 0명 시 Settlement Draw 표시용)
	UPROPERTY(ReplicatedUsing = OnRep_IsDraw, BlueprintReadOnly, Category = "AOS|Game")
	bool bIsDraw = false;

	// Slice 0: 팀별 글로벌 골드 (상점에서 아이템 구매에 사용). 라운드 간 누적.
	// 기존 bTeam1Ready/bTeam2Ready 패턴 답습 — 배열 대신 팀별 개별 프로퍼티 (UHT 친화).
	UPROPERTY(ReplicatedUsing = OnRep_Gold, BlueprintReadOnly, Category = "AOS|Economy")
	int32 Team1Gold = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Gold, BlueprintReadOnly, Category = "AOS|Economy")
	int32 Team2Gold = 0;

	// 서버 전용: 상태 업데이트 (AOSGameMode에서 호출)
	void ServerSetCurrentState(EAOSGameState NewState);
	void ServerSetCurrentRound(int32 NewRound);
	void ServerSetTeamReady(EAOSTeam Team, bool bReady);
	void ServerSetConnectedCount(int32 Count);
	void ServerSetPreparationTime(float Time);
	void SetIsDraw(bool bDraw);

	// Slice 0: 골드 — 서버 전용 가감/설정 + 조회
	void ServerAddGold(EAOSTeam Team, int32 Amount);   // Amount 음수면 차감 (구매)
	void ServerSetGold(EAOSTeam Team, int32 NewGold);

	UFUNCTION(BlueprintCallable, Category = "AOS|Economy")
	int32 GetGold(EAOSTeam Team) const;

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

	UPROPERTY(BlueprintAssignable, Category = "AOS|Game")
	FOnPlayerCountChanged OnPlayerCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "AOS|Economy")
	FOnTeamGoldChanged OnTeamGoldChanged;

protected:
	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	void OnRep_Gold();

	UFUNCTION()
	void OnRep_CurrentRound();

	UFUNCTION()
	void OnRep_TeamReady();

	UFUNCTION()
	void OnRep_ConnectedCount();

	UFUNCTION()
	void OnRep_IsDraw();
};
