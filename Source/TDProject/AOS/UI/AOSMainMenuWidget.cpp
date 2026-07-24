#include "AOSMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AOSUIStyle.h"

TSharedRef<SWidget> UAOSMainMenuWidget::RebuildWidget()
{
	// WBP_MainMenu(Parent=UAOSMainMenuWidget)가 있으면 RootWidget 이 이미 채워짐 → 폴백 skip.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackFrame();
	}
	return Super::RebuildWidget();
}

void UAOSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 바인딩 (WBP/폴백 단일 경로 — IsAlreadyBound 로 중복 방지)
	if (StartGameButton && !StartGameButton->OnClicked.IsAlreadyBound(this, &UAOSMainMenuWidget::OnStartGameClicked))
	{
		StartGameButton->OnClicked.AddDynamic(this, &UAOSMainMenuWidget::OnStartGameClicked);
	}
	if (ExitGameButton && !ExitGameButton->OnClicked.IsAlreadyBound(this, &UAOSMainMenuWidget::OnExitGameClicked))
	{
		ExitGameButton->OnClicked.AddDynamic(this, &UAOSMainMenuWidget::OnExitGameClicked);
	}
}

void UAOSMainMenuWidget::BuildFallbackFrame()
{
	// WBP 가 트리를 저작했으면(RootWidget 존재) 폴백 skip — 멱등.
	if (WidgetTree && WidgetTree->RootWidget) { return; }

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
	TitleText->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextPrimary));
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0, 0, 0, 20));
	TitleSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// StatusText (준비 상태 표시 — "게임 시작을 눌러 주세요" / "Team1: 준비 완료 ✓ ..." 등)
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("게임 시작을 눌러 주세요")));
	FSlateFontInfo StatusFont = StatusText->GetFont();
	StatusFont.Size = 16;
	StatusText->SetFont(StatusFont);
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextMuted));
	UVerticalBoxSlot* StatusSlot = VBox->AddChildToVerticalBox(StatusText);
	StatusSlot->SetPadding(FMargin(0, 0, 0, 20));
	StatusSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// StartGameButton
	StartGameButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartGameButton"));
	StartGameButton->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
	UVerticalBoxSlot* StartSlot = VBox->AddChildToVerticalBox(StartGameButton);
	StartSlot->SetPadding(FMargin(0, 0, 0, 10));
	StartSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	UTextBlock* StartText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartText"));
	StartText->SetText(FText::FromString(TEXT("게임 시작")));
	FSlateFontInfo StartFont = StartText->GetFont();
	StartFont.Size = 24;
	StartText->SetFont(StartFont);
	StartText->SetColorAndOpacity(FSlateColor(AOSUIStyle::Gold));
	StartGameButton->AddChild(StartText);
	// OnClicked 바인딩은 NativeConstruct 에서 (WBP/폴백 공용 단일 경로)

	// ExitGameButton
	ExitGameButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExitGameButton"));
	ExitGameButton->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
	UVerticalBoxSlot* ExitSlot = VBox->AddChildToVerticalBox(ExitGameButton);
	ExitSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	UTextBlock* ExitText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ExitText"));
	ExitText->SetText(FText::FromString(TEXT("게임 종료")));
	FSlateFontInfo ExitFont = ExitText->GetFont();
	ExitFont.Size = 24;
	ExitText->SetFont(ExitFont);
	ExitText->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextMuted));
	ExitGameButton->AddChild(ExitText);
	// OnClicked 바인딩은 NativeConstruct 에서 (WBP/폴백 공용 단일 경로)

	UE_LOG(LogTemp, Warning, TEXT("[MainMenu] 폴백 UI 생성 완료 (WBP_MainMenu 미사용)"));
}

void UAOSMainMenuWidget::OnStartGameClicked()
{
	// ── 독립 시작 흐름 ────────────────────────────────────────────────────────────
	// 서버/클라이언트 모두 이 버튼 클릭 시 PlayerController의 Server_SetReady(true)를
	// 호출하도록 OnStartClicked 델리게이트만 브로드캐스트한다.
	// PlayerController(ShowMainMenu에서 바인딩)가 RPC를 처리하고,
	// GameMode::ServerSetPlayerReady()에서 양쪽 모두 준비 완료 시 ServerTravel을 실행한다.
	// ─────────────────────────────────────────────────────────────────────────────
	UE_LOG(LogTemp, Warning, TEXT("[MainMenu] 시작 버튼 클릭 → OnStartClicked 브로드캐스트"));
	OnStartClicked.Broadcast();
}

void UAOSMainMenuWidget::UpdateReadyState(const FString& LocalName, bool bLocalReady,
                                           const FString& RemoteName, bool bRemoteReady)
{
	if (!StatusText)
	{
		return;
	}

	if (bLocalReady && bRemoteReady)
	{
		StatusText->SetText(FText::FromString(TEXT("양쪽 준비 완료! 이동 중...")));
	}
	else if (bLocalReady)
	{
		// 자신은 준비, 상대방 대기
		const FString WaitingName = RemoteName.IsEmpty() ? TEXT("다른 사용자") : RemoteName;
		StatusText->SetText(FText::FromString(
			FString::Printf(TEXT("%s: 게임 시작 ✓  |  %s: 대기 중..."), *LocalName, *WaitingName)));
	}
	else if (bRemoteReady)
	{
		// 상대방은 준비, 자신 대기
		const FString ReadyName = RemoteName.IsEmpty() ? TEXT("다른 사용자") : RemoteName;
		StatusText->SetText(FText::FromString(
			FString::Printf(TEXT("%s: 대기 중...  |  %s: 게임 시작 ✓"), *LocalName, *ReadyName)));
	}
	else
	{
		StatusText->SetText(FText::FromString(TEXT("게임 시작을 눌러 주세요")));
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
