#include "AOSSettlementWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "AOSGameMode.h"

void UAOSSettlementWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.AddDynamic(this, &UAOSSettlementWidget::OnReturnClicked);
	}
}

void UAOSSettlementWidget::SetResult(EAOSTeam WinningTeam)
{
	if (!ResultText)
	{
		return;
	}

	FString ResultString;
	if (WinningTeam == EAOSTeam::Team1)
	{
		ResultString = TEXT("Team 1 승리!");
	}
	else
	{
		ResultString = TEXT("Team 2 승리!");
	}

	ResultText->SetText(FText::FromString(ResultString));
	UE_LOG(LogTemp, Warning, TEXT("[Settlement] 결과 표시: %s"), *ResultString);
}

void UAOSSettlementWidget::OnReturnClicked()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AAOSGameMode* GameMode = Cast<AAOSGameMode>(World->GetAuthGameMode());
	if (GameMode)
	{
		GameMode->TransitionToMainMenu();
		UE_LOG(LogTemp, Warning, TEXT("[Settlement] 메인 메뉴 복귀 요청"));
	}
}
