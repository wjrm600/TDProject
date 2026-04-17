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
