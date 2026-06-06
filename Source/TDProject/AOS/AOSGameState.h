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
// Slice 1: 라운드 결과(라인 승패) 갱신 알림 — 준비 화면 결과 패널이 구독.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoundResultChanged);
// 벤픽: 드래프트 상태(밴/픽/스텝) 갱신 알림 — 벤픽 위젯이 구독.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDraftChanged);

// 벤픽 드래프트 시퀀스의 한 스텝 (어느 팀이 밴/픽 하는가). 정적 시퀀스용 — 비리플리케이션.
struct FAOSDraftStep
{
	EAOSTeam Team = EAOSTeam::Team1;
	bool bBan = false;
};

// Slice 1: 직전 라운드의 라인별 승패 요약 (GameMode 가 EndRound 에서 채워 복제).
// 인덱스 0=Top, 1=Mid, 2=Bottom.
USTRUCT(BlueprintType)
struct FAOSRoundResult
{
	GENERATED_BODY()

	// 라인별 승자: 0=무승부/미정, 1=Team1, 2=Team2
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Round")
	TArray<int32> LaneWinners;

	// 라인별 승자가 확정된 시점의 승자 잔존 캐릭터 수 (마진 = "왜" 근거)
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Round")
	TArray<int32> LaneWinnerSurvivors;

	// 이 결과가 속한(종료된) 라운드 번호
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Round")
	int32 RoundNumber = 0;

	// 유효 결과 여부 (첫 라운드 전엔 false → 패널 숨김)
	UPROPERTY(BlueprintReadOnly, Category = "AOS|Round")
	bool bValid = false;
};

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

	// Slice 1: 직전 라운드 결과 (라인 승패 요약) — 준비 화면 패널이 표시
	UPROPERTY(ReplicatedUsing = OnRep_RoundResult, BlueprintReadOnly, Category = "AOS|Game")
	FAOSRoundResult LastRoundResult;

	// ── 벤픽 드래프트 (밴2+픽5, 스네이크, 전체 고유) ──
	// 픽/밴된 유닛(=로스터 인덱스). 전체 고유라 중복 없음. 모두 OnRep_Draft 로 위젯 갱신.
	UPROPERTY(ReplicatedUsing = OnRep_Draft, BlueprintReadOnly, Category = "AOS|BanPick")
	TArray<int32> Team1PickedUnitIds;

	UPROPERTY(ReplicatedUsing = OnRep_Draft, BlueprintReadOnly, Category = "AOS|BanPick")
	TArray<int32> Team2PickedUnitIds;

	UPROPERTY(ReplicatedUsing = OnRep_Draft, BlueprintReadOnly, Category = "AOS|BanPick")
	TArray<int32> Team1BannedUnitIds;

	UPROPERTY(ReplicatedUsing = OnRep_Draft, BlueprintReadOnly, Category = "AOS|BanPick")
	TArray<int32> Team2BannedUnitIds;

	// 현재 드래프트 스텝 (0..N-1 진행, N(=시퀀스 길이)이면 완료)
	UPROPERTY(ReplicatedUsing = OnRep_Draft, BlueprintReadOnly, Category = "AOS|BanPick")
	int32 CurrentDraftStep = 0;

	// 현재 턴 남은 시간. ReplicatedUsing=OnRep_Draft → 매초 갱신 시 위젯 OnDraftChanged 로 타이머 표시 갱신.
	UPROPERTY(ReplicatedUsing = OnRep_Draft, BlueprintReadOnly, Category = "AOS|BanPick")
	float DraftTurnTimeRemaining = 0.f;

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

	// Slice 1: 라운드 결과 — 서버 전용 설정 + 조회
	void ServerSetRoundResult(const FAOSRoundResult& Result);

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	FAOSRoundResult GetLastRoundResult() const { return LastRoundResult; }

	// ── 벤픽 드래프트 — 서버 전용 변경 (GameMode 가 호출) ──
	void ServerResetDraft();
	void ServerRecordBan(EAOSTeam Team, int32 UnitId);
	void ServerRecordPick(EAOSTeam Team, int32 UnitId);
	void ServerSetDraftStep(int32 Step);
	void ServerSetDraftTurnTime(float Time);

	// 드래프트 시퀀스 (정적, 서버/클라 공유 — 턴/밴or픽 판정 단일 진실)
	static const TArray<FAOSDraftStep>& GetDraftSequence();

	// 조회/헬퍼 (위젯·GameMode 공용)
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	const TArray<int32>& GetPickedUnits(EAOSTeam Team) const;
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	bool IsUnitPickedByTeam(int32 UnitId, EAOSTeam Team) const;
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	bool IsUnitPicked(int32 UnitId) const;
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	bool IsUnitBanned(int32 UnitId) const;
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	bool IsUnitAvailableForDraft(int32 UnitId) const;
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	bool IsDraftComplete() const;
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	EAOSTeam GetActiveDraftTeam() const;
	UFUNCTION(BlueprintCallable, Category = "AOS|BanPick")
	bool IsCurrentStepBan() const;

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

	UPROPERTY(BlueprintAssignable, Category = "AOS|Game")
	FOnRoundResultChanged OnRoundResultChanged;

	UPROPERTY(BlueprintAssignable, Category = "AOS|BanPick")
	FOnDraftChanged OnDraftChanged;

protected:
	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	void OnRep_Gold();

	UFUNCTION()
	void OnRep_RoundResult();

	UFUNCTION()
	void OnRep_Draft();

	UFUNCTION()
	void OnRep_CurrentRound();

	UFUNCTION()
	void OnRep_TeamReady();

	UFUNCTION()
	void OnRep_ConnectedCount();

	UFUNCTION()
	void OnRep_IsDraw();
};
