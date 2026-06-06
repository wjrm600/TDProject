#include "AOSGameState.h"
#include "Net/UnrealNetwork.h"

AAOSGameState::AAOSGameState()
{
	// GameState는 항상 리플리케이션됨 (AGameStateBase가 기본 설정)
	bReplicates = true;
}

void AAOSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAOSGameState, CurrentState);
	DOREPLIFETIME(AAOSGameState, CurrentRound);
	DOREPLIFETIME(AAOSGameState, bTeam1Ready);
	DOREPLIFETIME(AAOSGameState, bTeam2Ready);
	DOREPLIFETIME(AAOSGameState, ConnectedCount);
	DOREPLIFETIME(AAOSGameState, PreparationTimeRemaining);
	DOREPLIFETIME(AAOSGameState, bIsDraw);
	DOREPLIFETIME(AAOSGameState, Team1Gold);
	DOREPLIFETIME(AAOSGameState, Team2Gold);
	DOREPLIFETIME(AAOSGameState, LastRoundResult);

	// 벤픽 드래프트
	DOREPLIFETIME(AAOSGameState, Team1PickedUnitIds);
	DOREPLIFETIME(AAOSGameState, Team2PickedUnitIds);
	DOREPLIFETIME(AAOSGameState, Team1BannedUnitIds);
	DOREPLIFETIME(AAOSGameState, Team2BannedUnitIds);
	DOREPLIFETIME(AAOSGameState, CurrentDraftStep);
	DOREPLIFETIME(AAOSGameState, DraftTurnTimeRemaining);
}

void AAOSGameState::ServerSetCurrentState(EAOSGameState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentState == NewState)
	{
		return;
	}

	CurrentState = NewState;

	// 서버에서도 델리게이트 브로드캐스트 (리스너 서버용)
	OnGameStateChangedClient.Broadcast(CurrentState);
}

void AAOSGameState::ServerSetCurrentRound(int32 NewRound)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentRound == NewRound)
	{
		return;
	}

	CurrentRound = NewRound;
	OnRoundNumberChanged.Broadcast(CurrentRound);
}

void AAOSGameState::ServerSetTeamReady(EAOSTeam Team, bool bReady)
{
	if (!HasAuthority())
	{
		return;
	}

	bool bChanged = false;
	if (Team == EAOSTeam::Team1)
	{
		if (bTeam1Ready != bReady)
		{
			bTeam1Ready = bReady;
			bChanged = true;
		}
	}
	else // Team2
	{
		if (bTeam2Ready != bReady)
		{
			bTeam2Ready = bReady;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnTeamReadyChanged.Broadcast();
	}
}

bool AAOSGameState::IsTeamReady(EAOSTeam Team) const
{
	return (Team == EAOSTeam::Team1) ? bTeam1Ready : bTeam2Ready;
}

void AAOSGameState::OnRep_CurrentState()
{
	OnGameStateChangedClient.Broadcast(CurrentState);
}

void AAOSGameState::OnRep_CurrentRound()
{
	OnRoundNumberChanged.Broadcast(CurrentRound);
}

void AAOSGameState::OnRep_TeamReady()
{
	OnTeamReadyChanged.Broadcast();
}

void AAOSGameState::ServerSetConnectedCount(int32 Count)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ConnectedCount == Count)
	{
		return;
	}

	ConnectedCount = Count;
	// 서버(리스너)에서도 즉시 브로드캐스트
	OnPlayerCountChanged.Broadcast(ConnectedCount);
}

void AAOSGameState::OnRep_ConnectedCount()
{
	// 클라이언트: 리플리케이션 수신 시 UI 갱신
	OnPlayerCountChanged.Broadcast(ConnectedCount);
}

void AAOSGameState::ServerSetPreparationTime(float Time)
{
	if (!HasAuthority()) return;
	PreparationTimeRemaining = Time;
}

void AAOSGameState::SetIsDraw(bool bDraw)
{
	if (!HasAuthority()) return;
	bIsDraw = bDraw;
}

void AAOSGameState::OnRep_IsDraw()
{
	// Settlement 표시 시 bIsDraw를 직접 읽으므로 별도 델리게이트 불필요
}

// ============================================================
// Slice 0: 팀 골드
// ============================================================

void AAOSGameState::ServerAddGold(EAOSTeam Team, int32 Amount)
{
	if (!HasAuthority() || Amount == 0)
	{
		return;
	}

	int32& Gold = (Team == EAOSTeam::Team1) ? Team1Gold : Team2Gold;
	const int32 NewGold = FMath::Max(0, Gold + Amount);
	if (NewGold == Gold)
	{
		return;
	}

	Gold = NewGold;
	// 서버(리스너)에서도 즉시 브로드캐스트 — 클라는 OnRep_Gold 가 처리
	OnTeamGoldChanged.Broadcast(Team, Gold);
}

void AAOSGameState::ServerSetGold(EAOSTeam Team, int32 NewGold)
{
	if (!HasAuthority())
	{
		return;
	}

	NewGold = FMath::Max(0, NewGold);
	int32& Gold = (Team == EAOSTeam::Team1) ? Team1Gold : Team2Gold;
	if (Gold == NewGold)
	{
		return;
	}

	Gold = NewGold;
	OnTeamGoldChanged.Broadcast(Team, Gold);
}

int32 AAOSGameState::GetGold(EAOSTeam Team) const
{
	return (Team == EAOSTeam::Team1) ? Team1Gold : Team2Gold;
}

// ============================================================
// Slice 1: 라운드 결과 (라인 승패 요약)
// ============================================================

void AAOSGameState::ServerSetRoundResult(const FAOSRoundResult& Result)
{
	if (!HasAuthority())
	{
		return;
	}
	LastRoundResult = Result;
	// 서버(리스너 호스트)에서도 즉시 브로드캐스트 — 클라는 OnRep_RoundResult 가 처리
	OnRoundResultChanged.Broadcast();
}

void AAOSGameState::OnRep_RoundResult()
{
	OnRoundResultChanged.Broadcast();
}

void AAOSGameState::OnRep_Gold()
{
	// 클라이언트: 어느 팀이 바뀌었는지 구분 없이 양쪽 모두 브로드캐스트해도 UI 가 자기 팀만 읽으면 됨.
	// 단순화를 위해 두 팀 모두 알림 (구독자가 Team 파라미터로 필터).
	OnTeamGoldChanged.Broadcast(EAOSTeam::Team1, Team1Gold);
	OnTeamGoldChanged.Broadcast(EAOSTeam::Team2, Team2Gold);
}

// ============================================================
// 벤픽 드래프트
// ============================================================

const TArray<FAOSDraftStep>& AAOSGameState::GetDraftSequence()
{
	// 밴 4(교대) + 픽 10(스네이크 1-2-2-1-1-2-2-1-1-2) = 14스텝. 팀당 밴2·픽5.
	static const TArray<FAOSDraftStep> Seq = []()
	{
		const EAOSTeam T1 = EAOSTeam::Team1;
		const EAOSTeam T2 = EAOSTeam::Team2;
		TArray<FAOSDraftStep> S;
		S.Add(FAOSDraftStep{ T1, true });  S.Add(FAOSDraftStep{ T2, true });
		S.Add(FAOSDraftStep{ T1, true });  S.Add(FAOSDraftStep{ T2, true });
		S.Add(FAOSDraftStep{ T1, false }); S.Add(FAOSDraftStep{ T2, false });
		S.Add(FAOSDraftStep{ T2, false }); S.Add(FAOSDraftStep{ T1, false });
		S.Add(FAOSDraftStep{ T1, false }); S.Add(FAOSDraftStep{ T2, false });
		S.Add(FAOSDraftStep{ T2, false }); S.Add(FAOSDraftStep{ T1, false });
		S.Add(FAOSDraftStep{ T1, false }); S.Add(FAOSDraftStep{ T2, false });
		return S;
	}();
	return Seq;
}

void AAOSGameState::ServerResetDraft()
{
	if (!HasAuthority()) return;
	Team1PickedUnitIds.Reset();
	Team2PickedUnitIds.Reset();
	Team1BannedUnitIds.Reset();
	Team2BannedUnitIds.Reset();
	CurrentDraftStep = 0;
	OnDraftChanged.Broadcast();
}

void AAOSGameState::ServerRecordBan(EAOSTeam Team, int32 UnitId)
{
	if (!HasAuthority()) return;
	((Team == EAOSTeam::Team1) ? Team1BannedUnitIds : Team2BannedUnitIds).AddUnique(UnitId);
	OnDraftChanged.Broadcast();
}

void AAOSGameState::ServerRecordPick(EAOSTeam Team, int32 UnitId)
{
	if (!HasAuthority()) return;
	((Team == EAOSTeam::Team1) ? Team1PickedUnitIds : Team2PickedUnitIds).AddUnique(UnitId);
	OnDraftChanged.Broadcast();
}

void AAOSGameState::ServerSetDraftStep(int32 Step)
{
	if (!HasAuthority()) return;
	CurrentDraftStep = Step;
	OnDraftChanged.Broadcast();
}

void AAOSGameState::ServerSetDraftTurnTime(float Time)
{
	if (!HasAuthority()) return;
	DraftTurnTimeRemaining = Time;
	// 리슨서버 호스트 즉시 갱신 (원격 클라는 DraftTurnTimeRemaining 의 OnRep_Draft 가 처리)
	OnDraftChanged.Broadcast();
}

void AAOSGameState::OnRep_Draft()
{
	OnDraftChanged.Broadcast();
}

const TArray<int32>& AAOSGameState::GetPickedUnits(EAOSTeam Team) const
{
	return (Team == EAOSTeam::Team1) ? Team1PickedUnitIds : Team2PickedUnitIds;
}

bool AAOSGameState::IsUnitPickedByTeam(int32 UnitId, EAOSTeam Team) const
{
	return GetPickedUnits(Team).Contains(UnitId);
}

bool AAOSGameState::IsUnitPicked(int32 UnitId) const
{
	return Team1PickedUnitIds.Contains(UnitId) || Team2PickedUnitIds.Contains(UnitId);
}

bool AAOSGameState::IsUnitBanned(int32 UnitId) const
{
	return Team1BannedUnitIds.Contains(UnitId) || Team2BannedUnitIds.Contains(UnitId);
}

bool AAOSGameState::IsUnitAvailableForDraft(int32 UnitId) const
{
	return UnitId >= 0 && !IsUnitPicked(UnitId) && !IsUnitBanned(UnitId);
}

bool AAOSGameState::IsDraftComplete() const
{
	return CurrentDraftStep >= GetDraftSequence().Num();
}

EAOSTeam AAOSGameState::GetActiveDraftTeam() const
{
	const TArray<FAOSDraftStep>& Seq = GetDraftSequence();
	return Seq.IsValidIndex(CurrentDraftStep) ? Seq[CurrentDraftStep].Team : EAOSTeam::Team1;
}

bool AAOSGameState::IsCurrentStepBan() const
{
	const TArray<FAOSDraftStep>& Seq = GetDraftSequence();
	return Seq.IsValidIndex(CurrentDraftStep) ? Seq[CurrentDraftStep].bBan : false;
}
