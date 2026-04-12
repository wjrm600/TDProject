#include "AOSSettlementWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "AOSGameMode.h"

void UAOSSettlementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildUI();
}

void UAOSSettlementWidget::BuildUI()
{
	// Root CanvasPanel
	UCanvasPanel* RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootPanel;

	// VerticalBox (centered)
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	UCanvasPanelSlot* VBoxSlot = RootPanel->AddChildToCanvas(VBox);
	VBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	VBoxSlot->SetAutoSize(true);

	// Result title
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettlementTitle"));
	TitleText->SetText(FText::FromString(TEXT("게임 결과")));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 36;
	TitleText->SetFont(TitleFont);
	TitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0, 0, 0, 20));
	TitleSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// ResultText
	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultText"));
	ResultText->SetText(FText::FromString(TEXT("")));
	FSlateFontInfo ResultFont = ResultText->GetFont();
	ResultFont.Size = 48;
	ResultText->SetFont(ResultFont);
	ResultText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* ResultSlot = VBox->AddChildToVerticalBox(ResultText);
	ResultSlot->SetPadding(FMargin(0, 0, 0, 40));
	ResultSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// ReturnToMainMenuButton
	ReturnToMainMenuButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ReturnButton"));
	UVerticalBoxSlot* BtnSlot = VBox->AddChildToVerticalBox(ReturnToMainMenuButton);
	BtnSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	UTextBlock* BtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReturnText"));
	BtnText->SetText(FText::FromString(TEXT("메인 메뉴로")));
	FSlateFontInfo BtnFont = BtnText->GetFont();
	BtnFont.Size = 24;
	BtnText->SetFont(BtnFont);
	ReturnToMainMenuButton->AddChild(BtnText);
	ReturnToMainMenuButton->OnClicked.AddDynamic(this, &UAOSSettlementWidget::OnReturnClicked);

	UE_LOG(LogTemp, Warning, TEXT("[Settlement] UI 동적 생성 완료"));
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
