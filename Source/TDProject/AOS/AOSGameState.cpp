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
