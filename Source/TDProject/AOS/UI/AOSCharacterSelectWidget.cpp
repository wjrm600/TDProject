#include "AOSCharacterSelectWidget.h"
#include "AOSCharacterDragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "AOSShopWidget.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"

// ─────────────────────────────────────────────────────────────
// UAOSLaneSlotWidget
// ─────────────────────────────────────────────────────────────

void UAOSLaneSlotWidget::BuildSlotUI(UWidgetTree* /*unused*/)
{
	// 이 위젯 자신의 WidgetTree를 사용하여 내부 UI 구성
	SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
		*FString::Printf(TEXT("SlotBorder_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	SlotBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.15f, 0.9f));
	SlotBorder->SetPadding(FMargin(14.0f, 22.0f));
	WidgetTree->RootWidget = SlotBorder;

	UHorizontalBox* InnerHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		*FString::Printf(TEXT("SlotHBox_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	SlotBorder->SetContent(InnerHBox);

	// 캐릭터 이름 텍스트
	SlotNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("SlotName_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	SlotNameText->SetText(FText::FromString(TEXT("[ 비어있음 ]")));
	FSlateFontInfo SlotFont = SlotNameText->GetFont();
	SlotFont.Size = 16;
	SlotNameText->SetFont(SlotFont);
	UHorizontalBoxSlot* NameHSlot = InnerHBox->AddChildToHorizontalBox(SlotNameText);
	NameHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NameHSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	// X 버튼 (슬롯 클리어)
	ClearButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		*FString::Printf(TEXT("ClearBtn_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	ClearButton->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* ClearHSlot = InnerHBox->AddChildToHorizontalBox(ClearButton);
	ClearHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	ClearHSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
	ClearHSlot->SetPadding(FMargin(4.0f, 0, 0, 0));

	UTextBlock* ClearText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("ClearText_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	ClearText->SetText(FText::FromString(TEXT(" X ")));
	FSlateFontInfo ClearFont = ClearText->GetFont();
	ClearFont.Size = 14;
	ClearText->SetFont(ClearFont);
	ClearButton->AddChild(ClearText);
	ClearButton->OnClicked.AddDynamic(this, &UAOSLaneSlotWidget::OnClearButtonClicked);
}

void UAOSLaneSlotWidget::SetAssigned(TSubclassOf<AAOSCharacter> InClass, int32 InRosterIndex, const FText& InName)
{
	AssignedClass = InClass;
	AssignedRosterIndex = InRosterIndex;
	if (SlotNameText) SlotNameText->SetText(InName);
	if (ClearButton) ClearButton->SetVisibility(ESlateVisibility::Visible);
	if (SlotBorder) SlotBorder->SetBrushColor(FLinearColor(0.05f, 0.25f, 0.05f, 0.9f));
}

void UAOSLaneSlotWidget::ClearAssignment()
{
	AssignedClass = nullptr;
	AssignedRosterIndex = -1;
	if (SlotNameText) SlotNameText->SetText(FText::FromString(TEXT("[ 비어있음 ]")));
	if (ClearButton) ClearButton->SetVisibility(ESlateVisibility::Collapsed);
	if (SlotBorder) SlotBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.15f, 0.9f));
}

void UAOSLaneSlotWidget::OnClearButtonClicked()
{
	if (OwnerSelectWidget) OwnerSelectWidget->HandleClearSlot(OwnerLane, SlotIndex);
}

FReply UAOSLaneSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (AssignedClass && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UAOSLaneSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!AssignedClass) return;

	UAOSCharacterDragDropOperation* Op = NewObject<UAOSCharacterDragDropOperation>();
	Op->CharacterClass = AssignedClass;
	Op->RosterIndex = AssignedRosterIndex; // 슬롯 간 이동 시 유닛(로스터) 정체성 유지
	Op->bFromLaneSlot = true;
	Op->SourceLane = OwnerLane;
	Op->SourceSlotIndex = SlotIndex;
	Op->DefaultDragVisual = this;
	Op->Pivot = EDragPivot::CenterCenter;
	OutOperation = Op;
}

bool UAOSLaneSlotWidget::NativeOnDragOver(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (!Cast<UAOSCharacterDragDropOperation>(InOperation)) return false;
	if (SlotBorder) SlotBorder->SetBrushColor(FLinearColor(0.1f, 0.4f, 0.1f, 0.95f));
	return true;
}

void UAOSLaneSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		if (AssignedClass) SlotBorder->SetBrushColor(FLinearColor(0.05f, 0.25f, 0.05f, 0.9f));
		else               SlotBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.15f, 0.9f));
	}
}

bool UAOSLaneSlotWidget::NativeOnDrop(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UAOSCharacterDragDropOperation* Op = Cast<UAOSCharacterDragDropOperation>(InOperation);
	if (!Op || !OwnerSelectWidget) return false;

	OwnerSelectWidget->HandleDropOnLaneSlot(OwnerLane, SlotIndex, Op);
	return true;
}

// ─────────────────────────────────────────────────────────────
// UAOSCharacterCardWidget
// ─────────────────────────────────────────────────────────────

void UAOSCharacterCardWidget::SetupCard(int32 InIdx, TSubclassOf<AAOSCharacter> InClass,
	const FText& InName, UTexture2D* InPortrait,
	UAOSCharacterSelectWidget* InOwner, UWidgetTree* /*unused*/)
{
	RosterIndex = InIdx;
	CharacterClass = InClass;
	OwnerSelectWidget = InOwner;

	// 이 카드 위젯 자신의 WidgetTree로 내부 UI 구성
	UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
		*FString::Printf(TEXT("CardBorder_%d"), InIdx));
	CardBorder->SetBrushColor(FLinearColor(0.15f, 0.15f, 0.2f, 0.95f));
	CardBorder->SetPadding(FMargin(8.0f));
	WidgetTree->RootWidget = CardBorder;

	UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		*FString::Printf(TEXT("CardVBox_%d"), InIdx));
	CardBorder->SetContent(CardVBox);

	// 초상화 이미지
	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
		*FString::Printf(TEXT("CardPortrait_%d"), InIdx));
	PortraitImage->SetDesiredSizeOverride(FVector2D(64.0f, 64.0f));
	if (InPortrait)
		PortraitImage->SetBrushFromTexture(InPortrait);
	else
		PortraitImage->SetColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.5f, 1.0f));
	UVerticalBoxSlot* PortraitVSlot = CardVBox->AddChildToVerticalBox(PortraitImage);
	PortraitVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
	PortraitVSlot->SetPadding(FMargin(0, 0, 0, 4));

	// 이름 텍스트
	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("CardName_%d"), InIdx));
	const FText DisplayName = InName.IsEmpty()
		? FText::FromString(FString::Printf(TEXT("캐릭터 %d"), InIdx + 1))
		: InName;
	NameText->SetText(DisplayName);
	FSlateFontInfo NameFont = NameText->GetFont();
	NameFont.Size = 14;
	NameText->SetFont(NameFont);
	NameText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* NameVSlot = CardVBox->AddChildToVerticalBox(NameText);
	NameVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
}

FReply UAOSCharacterCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UAOSCharacterCardWidget::NativeOnDragDetected(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	UAOSCharacterDragDropOperation* Op = NewObject<UAOSCharacterDragDropOperation>();
	Op->CharacterClass = CharacterClass;
	Op->RosterIndex = RosterIndex;
	Op->bFromLaneSlot = false;
	Op->SourceSlotIndex = -1;
	Op->DefaultDragVisual = this;
	Op->Pivot = EDragPivot::CenterCenter;
	OutOperation = Op;
}

// ─────────────────────────────────────────────────────────────
// UAOSCharacterSelectWidget
// ─────────────────────────────────────────────────────────────

bool UAOSCharacterSelectWidget::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (bSuccess)
	{
		LaneSlotWidgets.Init(nullptr, 6);
		BuildUI();
	}
	return bSuccess;
}

void UAOSCharacterSelectWidget::BuildUI()
{
	// Root CanvasPanel
	UCanvasPanel* RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootPanel;

	// ── 전체 화면 불투명 배경 (게임 뷰포트 차단) ──
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Background"));
	BgBorder->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.09f, 0.93f));
	BgBorder->SetPadding(FMargin(0));
	UCanvasPanelSlot* BgSlot = RootPanel->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BgSlot->SetOffsets(FMargin(0));
	BgSlot->SetZOrder(-1);

	// 중앙 VerticalBox
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ContentBox"));
	UCanvasPanelSlot* VBoxSlot = RootPanel->AddChildToCanvas(VBox);
	VBoxSlot->SetAnchors(FAnchors(0.05f, 0.02f, 0.95f, 0.98f));
	VBoxSlot->SetOffsets(FMargin(0));

	// 타이틀
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Round 1 - 캐릭터 배치")));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 32;
	TitleText->SetFont(TitleFont);
	TitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleVSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleVSlot->SetPadding(FMargin(0, 0, 0, 16));
	TitleVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// Slice 1: 지난 라운드 결과 요약 (첫 라운드엔 숨김 — UpdateRoundResult 가 가시성 제어)
	RoundResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("RoundResultText"));
	RoundResultText->SetText(FText::GetEmpty());
	FSlateFontInfo RRFont = RoundResultText->GetFont();
	RRFont.Size = 18;
	RoundResultText->SetFont(RRFont);
	RoundResultText->SetJustification(ETextJustify::Center);
	RoundResultText->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* RRVSlot = VBox->AddChildToVerticalBox(RoundResultText);
	RRVSlot->SetPadding(FMargin(0, 0, 0, 12));
	RRVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// 준비 타이머 텍스트
	TimerText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetText(FText::FromString(TEXT("준비 시간: 30초")));
	FSlateFontInfo TimerFont = TimerText->GetFont();
	TimerFont.Size = 24;
	TimerText->SetFont(TimerFont);
	TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f)));
	TimerText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TimerVSlot = VBox->AddChildToVerticalBox(TimerText);
	TimerVSlot->SetPadding(FMargin(0, 0, 0, 16));
	TimerVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// ── 상단: 3개 레인 슬롯 영역 ──
	UHorizontalBox* UpperPanel = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("UpperPanel"));
	UVerticalBoxSlot* UpperVSlot = VBox->AddChildToVerticalBox(UpperPanel);
	UpperVSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	UpperVSlot->SetPadding(FMargin(0, 0, 0, 12));

	const EAOSLane Lanes[] = { EAOSLane::Top, EAOSLane::Mid, EAOSLane::Bottom };
	const FString LaneNames[] = { TEXT("Top Lane"), TEXT("Mid Lane"), TEXT("Bottom Lane") };

	for (int32 L = 0; L < 3; ++L)
	{
		UVerticalBox* LaneCol = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), *FString::Printf(TEXT("LaneCol_%d"), L));
		UHorizontalBoxSlot* ColHSlot = UpperPanel->AddChildToHorizontalBox(LaneCol);
		ColHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ColHSlot->SetPadding(FMargin(8.0f, 0));

		// 레인 라벨
		UTextBlock* LaneLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("LaneLabel_%d"), L));
		LaneLabel->SetText(FText::FromString(LaneNames[L]));
		FSlateFontInfo LabelFont = LaneLabel->GetFont();
		LabelFont.Size = 20;
		LaneLabel->SetFont(LabelFont);
		LaneLabel->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* LabelVSlot = LaneCol->AddChildToVerticalBox(LaneLabel);
		LabelVSlot->SetPadding(FMargin(0, 0, 0, 8));
		LabelVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

		// 2개 슬롯 생성
		for (int32 S = 0; S < 2; ++S)
		{
			UAOSLaneSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UAOSLaneSlotWidget>(
				UAOSLaneSlotWidget::StaticClass(),
				*FString::Printf(TEXT("LaneSlot_L%d_S%d"), L, S));
			SlotWidget->OwnerLane = Lanes[L];
			SlotWidget->SlotIndex = S;
			SlotWidget->OwnerSelectWidget = this;
			SlotWidget->BuildSlotUI(WidgetTree); // 슬롯 내부 UI 구성

			LaneSlotWidgets[L * 2 + S] = SlotWidget;

			UVerticalBoxSlot* SlotVSlot = LaneCol->AddChildToVerticalBox(SlotWidget);
			SlotVSlot->SetPadding(FMargin(0, 4));
			SlotVSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	// 총 배치 표시
	TotalCountText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TotalCountText"));
	TotalCountText->SetText(FText::FromString(TEXT("총 배치: 0/5")));
	FSlateFontInfo TotalFont = TotalCountText->GetFont();
	TotalFont.Size = 18;
	TotalCountText->SetFont(TotalFont);
	TotalCountText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TotalVSlot = VBox->AddChildToVerticalBox(TotalCountText);
	TotalVSlot->SetPadding(FMargin(0, 0, 0, 10));
	TotalVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// ── 상점 열기 버튼 (클릭 → 상점 팝업) ──
	ShopButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopButton"));
	{
		UTextBlock* ShopBtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ShopButtonLabel"));
		ShopBtnLabel->SetText(FText::FromString(TEXT("상점 열기")));
		FSlateFontInfo ShopBtnFont = ShopBtnLabel->GetFont();
		ShopBtnFont.Size = 18;
		ShopBtnLabel->SetFont(ShopBtnFont);
		ShopButton->AddChild(ShopBtnLabel);
	}
	ShopButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnShopButtonClicked);
	UVerticalBoxSlot* ShopBtnVSlot = VBox->AddChildToVerticalBox(ShopButton);
	ShopBtnVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
	ShopBtnVSlot->SetPadding(FMargin(0, 4, 0, 8));

	// ── 하단: 캐릭터 카드 그리드 ──
	CardGrid = WidgetTree->ConstructWidget<UWrapBox>(
		UWrapBox::StaticClass(), TEXT("CardGrid"));
	CardGrid->SetInnerSlotPadding(FVector2D(8.0f, 8.0f));
	UVerticalBoxSlot* GridVSlot = VBox->AddChildToVerticalBox(CardGrid);
	GridVSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	GridVSlot->SetPadding(FMargin(0, 8, 0, 12));
	GridVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);

	// 라운드 준비 버튼 (이전: "라운드 시작") — 누르면 서버에 준비 신호 전송 + 시각 피드백
	StartRoundButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("StartRoundButton"));
	UVerticalBoxSlot* BtnVSlot = VBox->AddChildToVerticalBox(StartRoundButton);
	BtnVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
	BtnVSlot->SetPadding(FMargin(0, 0, 0, 8));

	StartRoundButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StartRoundText"));
	StartRoundButtonText->SetText(FText::FromString(TEXT("라운드 준비")));
	FSlateFontInfo BtnFont = StartRoundButtonText->GetFont();
	BtnFont.Size = 22;
	StartRoundButtonText->SetFont(BtnFont);
	StartRoundButton->AddChild(StartRoundButtonText);
	StartRoundButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnStartRoundButtonClicked);

	// 양 팀 준비 상태 표시 (버튼 아래) — GameState OnRep_TeamReady 콜백에서 갱신
	TeamReadyStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TeamReadyStatusText"));
	TeamReadyStatusText->SetText(FText::FromString(TEXT("팀1: 대기중 / 팀2: 대기중")));
	FSlateFontInfo StatusFont = TeamReadyStatusText->GetFont();
	StatusFont.Size = 16;
	TeamReadyStatusText->SetFont(StatusFont);
	TeamReadyStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
	UVerticalBoxSlot* StatusVSlot = VBox->AddChildToVerticalBox(TeamReadyStatusText);
	StatusVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
	StatusVSlot->SetPadding(FMargin(0, 0, 0, 4));

	// ── 상점 팝업: 전체화면 오버레이로 RootPanel 에 추가 (평소 Collapsed, "상점 열기"로 표시) ──
	ShopWidget = WidgetTree->ConstructWidget<UAOSShopWidget>(
		UAOSShopWidget::StaticClass(), TEXT("ShopPopup"));
	ShopWidget->BuildShopUI();
	UCanvasPanelSlot* ShopCanvasSlot = RootPanel->AddChildToCanvas(ShopWidget);
	ShopCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ShopCanvasSlot->SetOffsets(FMargin(0));
	ShopCanvasSlot->SetZOrder(50);

	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] UI 동적 생성 완료"));
}

void UAOSCharacterSelectWidget::InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster)
{
	CachedRoster = Roster;

	// 기존 카드 제거
	for (UAOSCharacterCardWidget* Card : CharacterCardWidgets)
	{
		if (Card) Card->RemoveFromParent();
	}
	CharacterCardWidgets.Empty();

	// 슬롯 초기화
	for (UAOSLaneSlotWidget* LaneSlot : LaneSlotWidgets)
	{
		if (LaneSlot) LaneSlot->ClearAssignment();
	}
	TopLaneCount = MidLaneCount = BottomLaneCount = 0;
	RefreshTotalCountDisplay();

	if (!CardGrid) return;

	// 벤픽 픽 필터: 드래프트가 진행됐으면(로컬 팀 픽 집합 비어있지 않음) 픽된 캐릭터만 표시.
	// (픽 집합이 비어있으면 = 벤픽 미수행 → 전체 표시: 하위 호환)
	TSet<int32> AllowedUnits;
	bool bFilterByPick = false;
	{
		EAOSTeam LocalTeam = EAOSTeam::Team1;
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (AAOSPlayerState* PS = PC->GetPlayerState<AAOSPlayerState>())
			{
				LocalTeam = PS->GetTeam();
			}
		}
		if (AAOSGameState* AOSGS = GetWorld() ? GetWorld()->GetGameState<AAOSGameState>() : nullptr)
		{
			const TArray<int32>& Picked = AOSGS->GetPickedUnits(LocalTeam);
			if (Picked.Num() > 0)
			{
				bFilterByPick = true;
				for (int32 U : Picked) { AllowedUnits.Add(U); }
			}
		}
	}

	// 로스터 카드 생성
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		if (bFilterByPick && !AllowedUnits.Contains(i)) { continue; } // 픽 안 된 캐릭터 → 표시 안 함
		UAOSCharacterCardWidget* Card = WidgetTree->ConstructWidget<UAOSCharacterCardWidget>(
			UAOSCharacterCardWidget::StaticClass(),
			*FString::Printf(TEXT("CharCard_%d"), i));
		Card->SetupCard(i, Roster[i].CharacterClass, Roster[i].DisplayName,
			Roster[i].Portrait, this, WidgetTree);

		UWrapBoxSlot* WrapSlot = CardGrid->AddChildToWrapBox(Card);
		if (WrapSlot) WrapSlot->SetPadding(FMargin(4.0f));

		CharacterCardWidgets.Add(Card);
	}

	// 새 라운드(준비 화면) 진입 시 상점 팝업은 닫힌 상태로 시작
	if (ShopWidget)
	{
		ShopWidget->CloseShop();
	}

	// Slice 1: 지난 라운드 결과 요약 갱신
	UpdateRoundResult();

	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] 로스터 초기화 완료 (%d개 캐릭터)"), Roster.Num());
}

void UAOSCharacterSelectWidget::HandleDropOnLaneSlot(EAOSLane Lane, int32 SlotIndex,
	UAOSCharacterDragDropOperation* Op)
{
	if (!Op) return;

	int32 L = static_cast<int32>(Lane);
	int32 FlatIdx = L * 2 + SlotIndex;
	if (FlatIdx < 0 || FlatIdx >= LaneSlotWidgets.Num()) return;

	UAOSLaneSlotWidget* TargetSlot = LaneSlotWidgets[FlatIdx];
	if (!TargetSlot) return;

	// 중복 배치 방지: 같은 유닛(로스터 인덱스)을 두 슬롯에 동시에 둘 수 없음.
	// (이동인 경우 소스 슬롯과 타겟 슬롯은 검사에서 제외 — 소스는 곧 비워짐)
	if (Op->RosterIndex >= 0)
	{
		const int32 SrcFlat = Op->bFromLaneSlot
			? (static_cast<int32>(Op->SourceLane) * 2 + Op->SourceSlotIndex) : -1;
		for (int32 i = 0; i < LaneSlotWidgets.Num(); ++i)
		{
			if (i == FlatIdx || i == SrcFlat) continue;
			const UAOSLaneSlotWidget* S = LaneSlotWidgets[i];
			if (S && S->GetAssignedRosterIndex() == Op->RosterIndex)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] 유닛(로스터 %d) 이미 배치됨 — 중복 배치 거부"), Op->RosterIndex);
				return;
			}
		}
	}

	bool bTargetWasEmpty = (TargetSlot->GetAssignedClass() == nullptr);

	// 빈 슬롯 + 총합 5명 초과면 거부
	if (bTargetWasEmpty && GetTotalAssignedCount() >= MaxTotalCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] 총 배치 수 초과 — 드롭 거부"));
		return;
	}

	// 소스가 레인 슬롯이었다면 원래 슬롯 비우기
	if (Op->bFromLaneSlot)
	{
		int32 SrcFlatIdx = static_cast<int32>(Op->SourceLane) * 2 + Op->SourceSlotIndex;
		if (SrcFlatIdx >= 0 && SrcFlatIdx < LaneSlotWidgets.Num())
		{
			if (UAOSLaneSlotWidget* SrcSlot = LaneSlotWidgets[SrcFlatIdx])
				SrcSlot->ClearAssignment();
		}
	}

	// 대상 슬롯에 배정 (유닛 정체성 = 로스터 인덱스 보존)
	FText CharName = FText::FromString(TEXT("캐릭터"));
	if (Op->RosterIndex >= 0 && Op->RosterIndex < CachedRoster.Num())
		CharName = CachedRoster[Op->RosterIndex].DisplayName;

	TargetSlot->SetAssigned(Op->CharacterClass, Op->RosterIndex, CharName);

	UpdateCountsFromSlots();
	RefreshTotalCountDisplay();
}

void UAOSCharacterSelectWidget::HandleClearSlot(EAOSLane Lane, int32 SlotIndex)
{
	int32 FlatIdx = static_cast<int32>(Lane) * 2 + SlotIndex;
	if (FlatIdx >= 0 && FlatIdx < LaneSlotWidgets.Num())
	{
		if (UAOSLaneSlotWidget* SlotPtr = LaneSlotWidgets[FlatIdx])
			SlotPtr->ClearAssignment();
	}
	UpdateCountsFromSlots();
	RefreshTotalCountDisplay();
}

TArray<TSubclassOf<AAOSCharacter>> UAOSCharacterSelectWidget::GetLaneClasses(EAOSLane Lane) const
{
	TArray<TSubclassOf<AAOSCharacter>> Result;
	int32 L = static_cast<int32>(Lane);
	for (int32 S = 0; S < 2; ++S)
	{
		int32 FlatIdx = L * 2 + S;
		if (FlatIdx < LaneSlotWidgets.Num())
		{
			if (const UAOSLaneSlotWidget* SlotPtr = LaneSlotWidgets[FlatIdx])
			{
				if (TSubclassOf<AAOSCharacter> Cls = SlotPtr->GetAssignedClass())
					Result.Add(Cls);
			}
		}
	}
	return Result;
}

TArray<int32> UAOSCharacterSelectWidget::GetLaneUnitIds(EAOSLane Lane) const
{
	// GetLaneClasses 와 동일 순서/조건 — 배정된 슬롯의 UnitId(로스터 인덱스)만 평행 수집
	TArray<int32> Result;
	int32 L = static_cast<int32>(Lane);
	for (int32 S = 0; S < 2; ++S)
	{
		int32 FlatIdx = L * 2 + S;
		if (FlatIdx < LaneSlotWidgets.Num())
		{
			if (const UAOSLaneSlotWidget* SlotPtr = LaneSlotWidgets[FlatIdx])
			{
				if (SlotPtr->GetAssignedClass())
					Result.Add(SlotPtr->GetAssignedRosterIndex());
			}
		}
	}
	return Result;
}

int32 UAOSCharacterSelectWidget::GetLaneCount(EAOSLane Lane) const
{
	switch (Lane)
	{
	case EAOSLane::Top:    return TopLaneCount;
	case EAOSLane::Mid:    return MidLaneCount;
	case EAOSLane::Bottom: return BottomLaneCount;
	default: return 0;
	}
}

int32 UAOSCharacterSelectWidget::GetTotalCount() const
{
	return TopLaneCount + MidLaneCount + BottomLaneCount;
}

void UAOSCharacterSelectWidget::SetLaneCount(EAOSLane Lane, int32 Count)
{
	Count = FMath::Clamp(Count, 0, MaxPerLane);
	switch (Lane)
	{
	case EAOSLane::Top:    TopLaneCount = Count; break;
	case EAOSLane::Mid:    MidLaneCount = Count; break;
	case EAOSLane::Bottom: BottomLaneCount = Count; break;
	}
	RefreshTotalCountDisplay();
}

void UAOSCharacterSelectWidget::SetRoundNumber(int32 RoundNum)
{
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(
			FString::Printf(TEXT("Round %d - 캐릭터 배치"), RoundNum)));
	}
}

void UAOSCharacterSelectWidget::UpdatePreparationTimer(float RemainingSeconds)
{
	if (!TimerText) return;

	int32 Seconds = FMath::CeilToInt(RemainingSeconds);
	TimerText->SetText(FText::FromString(
		FString::Printf(TEXT("준비 시간: %d초"), Seconds)));

	// 10초 이하면 빨간색으로 강조
	FLinearColor Color = (Seconds <= 10)
		? FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)
		: FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
	TimerText->SetColorAndOpacity(FSlateColor(Color));

	// 상점 팝업이 열려 있으면 상점 헤더 타이머도 갱신 (준비창 남은시간 항상 표시)
	if (ShopWidget && ShopWidget->IsOpen())
	{
		ShopWidget->UpdateTimer(RemainingSeconds);
	}
}

void UAOSCharacterSelectWidget::OnStartRoundButtonClicked()
{
	if (bLocalPressedReady)
	{
		// 이미 준비 완료 — 재클릭 무시
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] 라운드 준비 클릭 (Top:%d, Mid:%d, Bottom:%d, Total:%d)"),
		TopLaneCount, MidLaneCount, BottomLaneCount, GetTotalCount());

	bLocalPressedReady = true;

	// 시각 피드백: 버튼 비활성화 + 텍스트 "준비 완료 ✓" + 그레이아웃
	if (StartRoundButton)
	{
		StartRoundButton->SetIsEnabled(false);
	}
	if (StartRoundButtonText)
	{
		StartRoundButtonText->SetText(FText::FromString(TEXT("준비 완료 ✓")));
		StartRoundButtonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.0f, 0.5f, 1.0f)));
	}

	OnStartRoundClicked.Broadcast();
}

void UAOSCharacterSelectWidget::OnShopButtonClicked()
{
	if (!ShopWidget) return;

	// 현재 배치된 유닛(최대 5) 목록을 구성해 상점 팝업에 전달
	TArray<FAOSShopUnit> PlacedUnits;
	for (UAOSLaneSlotWidget* LaneSlot : LaneSlotWidgets)
	{
		if (!LaneSlot) continue;
		TSubclassOf<AAOSCharacter> Cls = LaneSlot->GetAssignedClass();
		if (!Cls) continue;

		FAOSShopUnit U;
		U.UnitId = LaneSlot->GetAssignedRosterIndex();
		U.CharacterClass = Cls;
		U.DisplayName = (U.UnitId >= 0 && U.UnitId < CachedRoster.Num())
			? CachedRoster[U.UnitId].DisplayName
			: FText::FromString(TEXT("유닛"));
		PlacedUnits.Add(U);
	}

	ShopWidget->OpenForUnits(PlacedUnits);
	UE_LOG(LogTemp, Log, TEXT("[CharacterSelect] 상점 열기 (배치 유닛 %d)"), PlacedUnits.Num());
}

void UAOSCharacterSelectWidget::UpdateTeamReadyStatus(bool bTeam1Ready, bool bTeam2Ready)
{
	if (!TeamReadyStatusText) return;

	auto TeamLabel = [](bool bReady)
	{
		return bReady ? TEXT("준비완료") : TEXT("대기중");
	};

	const FString StatusStr = FString::Printf(TEXT("팀1: %s  /  팀2: %s"),
		TeamLabel(bTeam1Ready), TeamLabel(bTeam2Ready));
	TeamReadyStatusText->SetText(FText::FromString(StatusStr));

	// 양 팀 모두 준비 → 녹색, 한쪽만 → 노랑, 둘 다 미준비 → 회색
	FLinearColor StatusColor;
	if (bTeam1Ready && bTeam2Ready)
	{
		StatusColor = FLinearColor(0.4f, 1.0f, 0.4f, 1.0f);  // 녹색
	}
	else if (bTeam1Ready || bTeam2Ready)
	{
		StatusColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);  // 노랑
	}
	else
	{
		StatusColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);  // 회색
	}
	TeamReadyStatusText->SetColorAndOpacity(FSlateColor(StatusColor));
}

void UAOSCharacterSelectWidget::ResetReadyState()
{
	bLocalPressedReady = false;

	if (StartRoundButton)
	{
		StartRoundButton->SetIsEnabled(true);
	}
	if (StartRoundButtonText)
	{
		StartRoundButtonText->SetText(FText::FromString(TEXT("라운드 준비")));
		StartRoundButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	if (TeamReadyStatusText)
	{
		TeamReadyStatusText->SetText(FText::FromString(TEXT("팀1: 대기중 / 팀2: 대기중")));
		TeamReadyStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
	}
}

void UAOSCharacterSelectWidget::UpdateCountsFromSlots()
{
	TopLaneCount = MidLaneCount = BottomLaneCount = 0;
	for (int32 S = 0; S < 2; ++S)
	{
		if (LaneSlotWidgets.IsValidIndex(0 * 2 + S) && LaneSlotWidgets[0 * 2 + S] && LaneSlotWidgets[0 * 2 + S]->GetAssignedClass()) ++TopLaneCount;
		if (LaneSlotWidgets.IsValidIndex(1 * 2 + S) && LaneSlotWidgets[1 * 2 + S] && LaneSlotWidgets[1 * 2 + S]->GetAssignedClass()) ++MidLaneCount;
		if (LaneSlotWidgets.IsValidIndex(2 * 2 + S) && LaneSlotWidgets[2 * 2 + S] && LaneSlotWidgets[2 * 2 + S]->GetAssignedClass()) ++BottomLaneCount;
	}
}

void UAOSCharacterSelectWidget::RefreshLaneSlotDisplay(EAOSLane /*Lane*/, int32 /*SlotIndex*/)
{
	// SetAssigned/ClearAssignment에서 직접 갱신하므로 현재 별도 처리 불필요
}

void UAOSCharacterSelectWidget::RefreshTotalCountDisplay()
{
	if (TotalCountText)
	{
		TotalCountText->SetText(FText::FromString(
			FString::Printf(TEXT("총 배치: %d/%d"), GetTotalCount(), MaxTotalCount)));
	}
}

int32 UAOSCharacterSelectWidget::GetTotalAssignedCount() const
{
	int32 Total = 0;
	for (const UAOSLaneSlotWidget* LaneSlot : LaneSlotWidgets)
	{
		if (LaneSlot && LaneSlot->GetAssignedClass()) ++Total;
	}
	return Total;
}

void UAOSCharacterSelectWidget::UpdateRoundResult()
{
	if (!RoundResultText) return;

	AAOSGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAOSGameState>() : nullptr;
	const FAOSRoundResult R = GS ? GS->GetLastRoundResult() : FAOSRoundResult();

	// 첫 라운드 전(미유효)이거나 데이터 부족 → 패널 숨김
	if (!GS || !R.bValid || R.LaneWinners.Num() < 3)
	{
		RoundResultText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 로컬 플레이어 팀 (1=Team1, 2=Team2)
	int32 MyTeamNum = 1;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AAOSPlayerState* PS = PC->GetPlayerState<AAOSPlayerState>())
		{
			MyTeamNum = (PS->GetTeam() == EAOSTeam::Team1) ? 1 : 2;
		}
	}

	const TCHAR* LaneNames[3] = { TEXT("Top"), TEXT("Mid"), TEXT("Bottom") };
	int32 WinCount = 0;
	FString Parts;
	for (int32 L = 0; L < 3; ++L)
	{
		const int32 W = R.LaneWinners[L];
		FString Outcome;
		if (W == 0)
		{
			Outcome = TEXT("무");
		}
		else if (W == MyTeamNum)
		{
			const int32 Surv = R.LaneWinnerSurvivors.IsValidIndex(L) ? R.LaneWinnerSurvivors[L] : 0;
			Outcome = FString::Printf(TEXT("승(생존 %d)"), Surv);
			++WinCount;
		}
		else
		{
			Outcome = TEXT("패");
		}
		if (!Parts.IsEmpty()) Parts += TEXT("    ");
		Parts += FString::Printf(TEXT("%s %s"), LaneNames[L], *Outcome);
	}

	RoundResultText->SetText(FText::FromString(FString::Printf(
		TEXT("지난 라운드 %d 결과:    %s    (%d/3 라인 승)"), R.RoundNumber, *Parts, WinCount)));

	// 다수 라인 승=녹색, 1라인=노랑, 0라인=빨강 (한눈에 읽히도록)
	const FLinearColor Color = (WinCount >= 2) ? FLinearColor(0.4f, 1.0f, 0.4f, 1.0f)
		: (WinCount == 1) ? FLinearColor(1.0f, 0.85f, 0.2f, 1.0f)
		: FLinearColor(1.0f, 0.45f, 0.45f, 1.0f);
	RoundResultText->SetColorAndOpacity(FSlateColor(Color));
	RoundResultText->SetVisibility(ESlateVisibility::Visible);
}
