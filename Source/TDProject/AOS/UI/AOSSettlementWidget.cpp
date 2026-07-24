#include "AOSSettlementWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "AOSGameMode.h"
#include "AOSPlayerController.h"
#include "AOSUIStyle.h"

bool UAOSSettlementWidget::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (bSuccess)
	{
		BuildUI();
	}
	return bSuccess;
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
	TitleText->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextPrimary));
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
	ReturnToMainMenuButton->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
	UVerticalBoxSlot* BtnSlot = VBox->AddChildToVerticalBox(ReturnToMainMenuButton);
	BtnSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	UTextBlock* BtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReturnText"));
	BtnText->SetText(FText::FromString(TEXT("메인 메뉴로")));
	FSlateFontInfo BtnFont = BtnText->GetFont();
	BtnFont.Size = 24;
	BtnText->SetFont(BtnFont);
	BtnText->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextPrimary));
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

	ResultText->SetColorAndOpacity(FSlateColor(AOSUIStyle::TeamAccent(WinningTeam == EAOSTeam::Team1)));
	ResultText->SetText(FText::FromString(ResultString));
	UE_LOG(LogTemp, Warning, TEXT("[Settlement] 결과 표시: %s"), *ResultString);
}

void UAOSSettlementWidget::SetDraw()
{
	if (!ResultText)
	{
		return;
	}

	ResultText->SetColorAndOpacity(FSlateColor(AOSUIStyle::Gold));
	ResultText->SetText(FText::FromString(TEXT("무승부\n5초 후 다음 라운드...")));

	// 무승부 시 메인 메뉴 버튼 숨기기 (무승부는 다음 라운드로 자동 전환됨)
	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Settlement] Draw 표시 - 메인 메뉴 버튼 숨김"));
}

void UAOSSettlementWidget::OnReturnClicked()
{
	// 위젯은 클라이언트 전용. 단순 OpenLevel(ClientTravel)이면 그 클라만 서버에서 끊겨 standalone 으로
	// 빠지고, 이후 "게임 시작"이 서버에 닿지 못해 재매칭이 불가능해진다 (매칭 실패 증상).
	// → 소유 PlayerController 의 Server RPC 로 서버에 요청 → 서버가 전원을 메인메뉴맵으로 ServerTravel.
	//   (게임 시작 시 GameMode 의 ServerTravel(GameMapName) 과 대칭 — 모두 서버 연결을 유지해 재매칭 가능.)
	if (AAOSPlayerController* PC = Cast<AAOSPlayerController>(GetOwningPlayer()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Settlement] 메인 메뉴로 → Server_ReturnToMainMenu 요청"));
		PC->Server_ReturnToMainMenu();
	}
}
