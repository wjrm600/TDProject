#include "AOSSettlementWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "AOSGameMode.h"
#include "Kismet/GameplayStatics.h"

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

void UAOSSettlementWidget::SetDraw()
{
	if (!ResultText)
	{
		return;
	}

	ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.0f, 1.0f)));
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
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 위젯은 클라이언트 전용 → DS 클라이언트에선 GetAuthGameMode() 가 nullptr 라
	// 기존엔 if(GameMode) 가드에 막혀 "메인 메뉴로" 버튼이 무반응이었다 (CLAUDE.md DS 안티패턴).
	// 서버(리슨서버 호스트)면 GameMode 의 권위 값을, 클라면 기본 메인메뉴 맵을 사용해 ClientTravel 한다.
	// ⚠️ 기본값은 AOSGameMode::MainMenuMapName (AOSGameMode.h) 과 동기 유지할 것.
	FName MapName(TEXT("/Game/AOS/Lvl_MainMenu"));
	if (AAOSGameMode* GameMode = Cast<AAOSGameMode>(World->GetAuthGameMode()))
	{
		MapName = GameMode->GetMainMenuMapName();
	}

	UE_LOG(LogTemp, Warning, TEXT("[Settlement] 메인 메뉴 맵으로 전환: %s"), *MapName.ToString());
	UGameplayStatics::OpenLevel(this, MapName);
}
