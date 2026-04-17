#include "AOSPlayerState.h"
#include "Net/UnrealNetwork.h"

AAOSPlayerState::AAOSPlayerState()
{
	// PlayerState는 기본적으로 리플리케이션됨
	bReplicates = true;
}

void AAOSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAOSPlayerState, Team);
	DOREPLIFETIME(AAOSPlayerState, bIsReady);
	DOREPLIFETIME(AAOSPlayerState, DeployCountTop);
	DOREPLIFETIME(AAOSPlayerState, DeployCountMid);
	DOREPLIFETIME(AAOSPlayerState, DeployCountBottom);
}

void AAOSPlayerState::ServerSetTeam(EAOSTeam NewTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Team == NewTeam)
	{
		return;
	}

	Team = NewTeam;
	OnPlayerTeamChanged.Broadcast(Team);
}

void AAOSPlayerState::ServerSetReady(bool bReady)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsReady == bReady)
	{
		return;
	}

	bIsReady = bReady;
	OnPlayerReadyChanged.Broadcast(bIsReady);
}

void AAOSPlayerState::ServerSetDeployCount(EAOSLane Lane, int32 Count)
{
	if (!HasAuthority())
	{
		return;
	}

	// 라인당 0~2 제한
	Count = FMath::Clamp(Count, 0, 2);

	bool bChanged = false;
	switch (Lane)
	{
	case EAOSLane::Top:
		if (DeployCountTop != Count)
		{
			DeployCountTop = Count;
			bChanged = true;
		}
		break;
	case EAOSLane::Mid:
		if (DeployCountMid != Count)
		{
			DeployCountMid = Count;
			bChanged = true;
		}
		break;
	case EAOSLane::Bottom:
		if (DeployCountBottom != Count)
		{
			DeployCountBottom = Count;
			bChanged = true;
		}
		break;
	}

	if (bChanged)
	{
		OnDeployPlanChanged.Broadcast();
	}
}

int32 AAOSPlayerState::GetDeployCount(EAOSLane Lane) const
{
	switch (Lane)
	{
	case EAOSLane::Top: return DeployCountTop;
	case EAOSLane::Mid: return DeployCountMid;
	case EAOSLane::Bottom: return DeployCountBottom;
	}
	return 0;
}

void AAOSPlayerState::OnRep_Team()
{
	OnPlayerTeamChanged.Broadcast(Team);
}

void AAOSPlayerState::OnRep_Ready()
{
	OnPlayerReadyChanged.Broadcast(bIsReady);
}

void AAOSPlayerState::OnRep_DeployPlan()
{
	OnDeployPlanChanged.Broadcast();
}
