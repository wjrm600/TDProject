#include "AOSMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AOSGameMode.h"

void UAOSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildUI();
}

void UAOSMainMenuWidget::BuildUI()
{
	// Root CanvasPanel
	UCanvasPanel* RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootPanel;

	// VerticalBox (centered)
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ButtonBox"));
	UCanvasPanelSlot* VBoxSlot = RootPanel->AddChildToCanvas(VBox);
	VBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	VBoxSlot->SetAutoSize(true);

	// Title
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("TDProject")));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 48;
	TitleText->SetFont(TitleFont);
	TitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0, 0, 0, 40));
	TitleSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// StartGameButton
	StartGameButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartGameButton"));
	UVerticalBoxSlot* StartSlot = VBox->AddChildToVerticalBox(StartGameButton);
	StartSlot->SetPadding(FMargin(0, 0, 0, 10));
	StartSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	UTextBlock* StartText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartText"));
	StartText->SetText(FText::FromString(TEXT("게임 시작")));
	FSlateFontInfo StartFont = StartText->GetFont();
	StartFont.Size = 24;
	StartText->SetFont(StartFont);
	StartGameButton->AddChild(StartText);
	StartGameButton->OnClicked.AddDynamic(this, &UAOSMainMenuWidget::OnStartGameClicked);

	// ExitGameButton
	ExitGameButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExitGameButton"));
	UVerticalBoxSlot* ExitSlot = VBox->AddChildToVerticalBox(ExitGameButton);
	ExitSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	UTextBlock* ExitText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ExitText"));
	ExitText->SetText(FText::FromString(TEXT("게임 종료")));
	FSlateFontInfo ExitFont = ExitText->GetFont();
	ExitFont.Size = 24;
	ExitText->SetFont(ExitFont);
	ExitGameButton->AddChild(ExitText);
	ExitGameButton->OnClicked.AddDynamic(this, &UAOSMainMenuWidget::OnExitGameClicked);

	UE_LOG(LogTemp, Warning, TEXT("[MainMenu] UI 동적 생성 완료"));
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
