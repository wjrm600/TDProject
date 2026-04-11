#include "AOSMainMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AOSGameMode.h"

void UAOSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartGameButton)
	{
		StartGameButton->OnClicked.AddDynamic(this, &UAOSMainMenuWidget::OnStartGameClicked);
	}

	if (ExitGameButton)
	{
		ExitGameButton->OnClicked.AddDynamic(this, &UAOSMainMenuWidget::OnExitGameClicked);
	}
}

void UAOSMainMenuWidget::OnStartGameClicked()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AAOSGameMode* GameMode = Cast<AAOSGameMode>(World->GetAuthGameMode());
	if (GameMode)
	{
		GameMode->TransitionToPreparation();
		GameMode->StartGame();
		UE_LOG(LogTemp, Warning, TEXT("[MainMenu] 게임 시작 요청"));
	}
}

void UAOSMainMenuWidget::OnExitGameClicked()
{
	UKismetSystemLibrary::QuitGame(
		GetWorld(),
		GetOwningPlayer(),
		EQuitPreference::Quit,
		false
	);
}
