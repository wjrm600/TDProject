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
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "AOSShopWidget.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"

// ─────────────────────────────────────────────────────────────
// 라운드 준비 창 디자인 — 벤픽 창(UAOSBanPickWidget)과 동일한 라이트 테마.
//   ※ 색 상수/헬퍼는 AOSBanPickWidget.cpp 의 사본(코드 중복) — cpp-only/저위험/Live Coding 우선.
//     후속 리팩토링으로 공유 헤더(AOSUIStyle.h) 추출 가능(단 신규 파일은 풀 빌드 필요).
// ─────────────────────────────────────────────────────────────
namespace
{
	// 벤픽이 쓰는 장식 텍스처 재사용 (white-on-alpha → 위젯 틴트). 없으면 솔리드 폴백.
	//   ※ 배경은 텍스처 미사용 — 벤픽처럼 솔리드 라이트 그레이(CSBgLight). 장식 라인/날개만 텍스처.
	const TCHAR* CSLineTaperPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_LineTaper.T_BanPick_LineTaper");
	const TCHAR* CSLineTaperHPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_LineTaperH.T_BanPick_LineTaperH");
	const TCHAR* CSWingSidePath  = TEXT("/Game/AOS/UI/Assets/T_BanPick_WingSide.T_BanPick_WingSide");

	// ── 라이트 테마 팔레트 (벤픽과 동일) ──
	const FLinearColor CSBgLight(0.85f, 0.86f, 0.89f, 1.f);      // 전체 배경
	const FLinearColor CSPanelLight(0.95f, 0.95f, 0.97f, 1.f);   // 패널/카드 바탕
	const FLinearColor CSCardEmpty(0.78f, 0.79f, 0.83f, 1.f);    // 빈 슬롯
	const FLinearColor CSBorderLight(0.62f, 0.63f, 0.68f, 1.f);  // 얇은 테두리
	const FLinearColor CSTextDark(0.10f, 0.11f, 0.14f, 1.f);     // 본문 텍스트
	const FLinearColor CSTextGray(0.42f, 0.43f, 0.49f, 1.f);     // 보조 텍스트
	const FLinearColor CSLockInBlue(0.16f, 0.45f, 0.86f, 1.f);   // 준비(LOCK IN) 버튼(활성)
	const FLinearColor CSLockInIdle(0.66f, 0.67f, 0.71f, 1.f);   // 준비 버튼(완료/비활성)
	const FLinearColor CSBanRed(0.82f, 0.22f, 0.22f, 1.f);       // 팀 무관 장식 레드

	// 배치 슬롯 상태색 (라이트)
	const FLinearColor kSlotFilled(0.80f, 0.90f, 0.82f, 1.f);   // 배정됨 — 연한 그린
	const FLinearColor kSlotHover(0.68f, 0.88f, 0.72f, 1.f);    // 드래그 호버 — 밝은 그린

	// 카드 그리드 최대 표시 높이(5행) — 벤픽과 동일 (CS 접두사: 유니티 빌드 충돌 회피)
	constexpr float CSGridMaxHeight = 410.f;

	FSlateFontInfo CSMakeFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	UTexture2D* TryLoadTexture(const TCHAR* AssetPath)
	{
		return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, AssetPath,
			nullptr, LOAD_NoWarn | LOAD_Quiet));
	}

	FLinearColor TeamColor(EAOSTeam Team)
	{
		return (Team == EAOSTeam::Team1)
			? FLinearColor(0.85f, 0.27f, 0.27f, 1.f)   // 팀1 = 레드
			: FLinearColor(0.30f, 0.55f, 0.95f, 1.f);  // 팀2 = 블루
	}

	// 초상화 없는 유닛용 고유 컬러 타일 — 황금비 색상 분산 (벤픽과 동일)
	FLinearColor CSPlaceholderColor(int32 Index)
	{
		const float Hue01 = FMath::Frac(static_cast<float>(Index) * 0.61803398875f);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue01 * 255.f), /*S*/ 130, /*V*/ 135);
	}
}

// ─────────────────────────────────────────────────────────────
// UAOSLaneSlotWidget
// ─────────────────────────────────────────────────────────────

void UAOSLaneSlotWidget::BuildSlotUI(UWidgetTree* /*unused*/)
{
	// 이 위젯 자신의 WidgetTree를 사용하여 내부 UI 구성 — 라이트 테마(벤픽과 동일 팔레트)
	SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
		*FString::Printf(TEXT("SlotBorder_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	SlotBorder->SetBrushColor(CSCardEmpty);
	SlotBorder->SetPadding(FMargin(12.0f, 14.0f));
	WidgetTree->RootWidget = SlotBorder;

	UHorizontalBox* InnerHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		*FString::Printf(TEXT("SlotHBox_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	SlotBorder->SetContent(InnerHBox);

	// 캐릭터 이름 텍스트
	SlotNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("SlotName_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	SlotNameText->SetText(FText::FromString(TEXT("[ 비어있음 ]")));
	SlotNameText->SetFont(CSMakeFont(15));
	SlotNameText->SetColorAndOpacity(FSlateColor(CSTextGray));
	UHorizontalBoxSlot* NameHSlot = InnerHBox->AddChildToHorizontalBox(SlotNameText);
	NameHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NameHSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	// X 버튼 (슬롯 클리어)
	ClearButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		*FString::Printf(TEXT("ClearBtn_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	ClearButton->SetVisibility(ESlateVisibility::Collapsed);
	ClearButton->SetBackgroundColor(FLinearColor(CSBanRed.R, CSBanRed.G, CSBanRed.B, 0.85f));
	UHorizontalBoxSlot* ClearHSlot = InnerHBox->AddChildToHorizontalBox(ClearButton);
	ClearHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	ClearHSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
	ClearHSlot->SetPadding(FMargin(4.0f, 0, 0, 0));

	UTextBlock* ClearText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("ClearText_L%d_S%d"), (int32)OwnerLane, SlotIndex));
	ClearText->SetText(FText::FromString(TEXT(" X ")));
	ClearText->SetFont(CSMakeFont(14));
	ClearText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ClearButton->AddChild(ClearText);
	ClearButton->OnClicked.AddDynamic(this, &UAOSLaneSlotWidget::OnClearButtonClicked);
}

void UAOSLaneSlotWidget::SetAssigned(TSubclassOf<AAOSCharacter> InClass, int32 InRosterIndex, const FText& InName)
{
	AssignedClass = InClass;
	AssignedRosterIndex = InRosterIndex;
	if (SlotNameText)
	{
		SlotNameText->SetText(InName);
		SlotNameText->SetColorAndOpacity(FSlateColor(CSTextDark));
	}
	if (ClearButton) ClearButton->SetVisibility(ESlateVisibility::Visible);
	if (SlotBorder) SlotBorder->SetBrushColor(kSlotFilled);
}

void UAOSLaneSlotWidget::ClearAssignment()
{
	AssignedClass = nullptr;
	AssignedRosterIndex = -1;
	if (SlotNameText)
	{
		SlotNameText->SetText(FText::FromString(TEXT("[ 비어있음 ]")));
		SlotNameText->SetColorAndOpacity(FSlateColor(CSTextGray));
	}
	if (ClearButton) ClearButton->SetVisibility(ESlateVisibility::Collapsed);
	if (SlotBorder) SlotBorder->SetBrushColor(CSCardEmpty);
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
	if (SlotBorder) SlotBorder->SetBrushColor(kSlotHover);
	return true;
}

void UAOSLaneSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		if (AssignedClass) SlotBorder->SetBrushColor(kSlotFilled);
		else               SlotBorder->SetBrushColor(CSCardEmpty);
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

	// 이 카드 위젯 자신의 WidgetTree로 내부 UI 구성 — 라이트 카드(벤픽 그리드 카드와 동일 톤)
	UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
		*FString::Printf(TEXT("CardBorder_%d"), InIdx));
	CardBorder->SetBrushColor(CSBorderLight);   // 얇은 테두리
	CardBorder->SetPadding(FMargin(3.0f));
	WidgetTree->RootWidget = CardBorder;

	UBorder* CardInner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
		*FString::Printf(TEXT("CardInner_%d"), InIdx));
	CardInner->SetBrushColor(CSPanelLight);     // 라이트 바탕
	CardInner->SetPadding(FMargin(5.0f));
	CardBorder->SetContent(CardInner);

	UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		*FString::Printf(TEXT("CardVBox_%d"), InIdx));
	CardInner->SetContent(CardVBox);

	// 초상화 이미지 (초상화 없으면 황금비 컬러 타일)
	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
		*FString::Printf(TEXT("CardPortrait_%d"), InIdx));
	PortraitImage->SetDesiredSizeOverride(FVector2D(60.0f, 60.0f));
	if (InPortrait)
		PortraitImage->SetBrushFromTexture(InPortrait);
	else
		PortraitImage->SetBrush(FSlateColorBrush(CSPlaceholderColor(InIdx)));
	UVerticalBoxSlot* PortraitVSlot = CardVBox->AddChildToVerticalBox(PortraitImage);
	PortraitVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
	PortraitVSlot->SetPadding(FMargin(0, 0, 0, 3));

	// 이름 텍스트
	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("CardName_%d"), InIdx));
	const FText DisplayName = InName.IsEmpty()
		? FText::FromString(FString::Printf(TEXT("캐릭터 %d"), InIdx + 1))
		: InName;
	NameText->SetText(DisplayName);
	NameText->SetFont(CSMakeFont(12));
	NameText->SetColorAndOpacity(FSlateColor(CSTextDark));
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
	// ── 루트: 바깥 오버레이(전체 배경 + ScaleBox 래퍼) — 벤픽 창과 동일한 반응형 캔버스 ──
	//   창 비율이 바뀌어도 레이아웃이 깨지지 않게 콘텐츠를 1920x1080 고정 디자인 캔버스에 담고
	//   ScaleBox(ScaleToFit)로 비율 유지 균일 스케일(레터박스). (Fill/수동 RenderScale 함정 회피.)
	UOverlay* OuterRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CSOuter"));
	WidgetTree->RootWidget = OuterRoot;

	UImage* OuterBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CSOuterBg"));
	OuterBg->SetBrush(FSlateColorBrush(CSBgLight));
	OuterBg->SetVisibility(ESlateVisibility::Visible);
	if (UOverlaySlot* OS = OuterRoot->AddChildToOverlay(OuterBg))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	UScaleBox* ScaleRoot = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("CSScale"));
	ScaleRoot->SetStretch(EStretch::ScaleToFit);
	if (UOverlaySlot* OS = OuterRoot->AddChildToOverlay(ScaleRoot))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* DesignCanvas = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CSDesignCanvas"));
	DesignCanvas->SetWidthOverride(1920.f);
	DesignCanvas->SetHeightOverride(1080.f);
	ScaleRoot->SetContent(DesignCanvas);

	// 콘텐츠 오버레이 — 이하 모든 장식/콘텐츠는 1920x1080 기준 절대 배치 (로컬 변수, 멤버 추가 안 함)
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CSRoot"));
	DesignCanvas->SetContent(RootOverlay);

	// [0] 배경 — 벤픽 창과 동일하게 솔리드 라이트 그레이 (텍스처 백드롭 미사용 = 레퍼런스 라이트 테마).
	//     Visible → 드래그 히트테스트 버블링.
	UImage* BackdropImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CSBackdrop"));
	BackdropImage->SetBrush(FSlateColorBrush(CSBgLight));
	BackdropImage->SetVisibility(ESlateVisibility::Visible);
	if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(BackdropImage))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	// [0.3] 장식: 대각 팀 경계선 (좌 = 블루 코너 / 우 = 레드 코너) — HitTestInvisible, 텍스처 없으면 솔리드
	{
		UTexture2D* LineTex = TryLoadTexture(CSLineTaperPath);
		auto MakeDiagonal = [&](const TCHAR* WName, const FLinearColor& Tint, float Angle) -> UWidget*
		{
			UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WName);
			if (LineTex) Line->SetBrushFromTexture(LineTex, false);
			else Line->SetBrush(FSlateColorBrush(FLinearColor::White));
			Line->SetColorAndOpacity(Tint);
			Line->SetVisibility(ESlateVisibility::HitTestInvisible);
			Line->SetRenderTransformAngle(Angle);
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(LineTex ? 46.f : 3.f);
			Box->SetContent(Line);
			Box->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Box;
		};

		const FLinearColor BlueTint(TeamColor(EAOSTeam::Team2).R, TeamColor(EAOSTeam::Team2).G,
			TeamColor(EAOSTeam::Team2).B, 0.70f);
		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(MakeDiagonal(TEXT("CSDiagLeft"), BlueTint, 8.f)))
		{
			OS->SetHorizontalAlignment(HAlign_Left);
			OS->SetVerticalAlignment(VAlign_Fill);
			OS->SetPadding(FMargin(360.f, 30.f, 0.f, 20.f));
		}
		const FLinearColor RedTint(TeamColor(EAOSTeam::Team1).R, TeamColor(EAOSTeam::Team1).G,
			TeamColor(EAOSTeam::Team1).B, 0.70f);
		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(MakeDiagonal(TEXT("CSDiagRight"), RedTint, -8.f)))
		{
			OS->SetHorizontalAlignment(HAlign_Right);
			OS->SetVerticalAlignment(VAlign_Fill);
			OS->SetPadding(FMargin(0.f, 30.f, 360.f, 20.f));
		}
	}

	// ── 콘텐츠 세로 스택 (중앙 정렬, 상단 기준) ──
	USizeBox* ContentBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CSContentBox"));
	ContentBox->SetWidthOverride(1180.f);
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CSContentVB"));
	ContentBox->SetContent(VBox);
	if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(ContentBox))
	{
		OS->SetHorizontalAlignment(HAlign_Center);
		OS->SetVerticalAlignment(VAlign_Top);
		OS->SetPadding(FMargin(0.f, 26.f, 0.f, 0.f));
	}

	// ── 제목 행: [좌 날개] Round N · 캐릭터 배치 [우 날개(미러)] ──
	{
		UTexture2D* WingTex = TryLoadTexture(CSWingSidePath);
		auto MakeWing = [&](const TCHAR* WName, bool bMirror) -> UWidget*
		{
			UImage* Wing = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WName);
			if (WingTex) Wing->SetBrushFromTexture(WingTex, false);
			Wing->SetColorAndOpacity(FLinearColor(CSBanRed.R, CSBanRed.G, CSBanRed.B, 0.85f));
			Wing->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (bMirror) Wing->SetRenderScale(FVector2D(-1.f, 1.f));
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(100.f);
			Box->SetHeightOverride(63.f);
			Box->SetContent(Wing);
			Box->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Box;
		};

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
		TitleText->SetText(FText::FromString(TEXT("Round 1 - 캐릭터 배치")));
		TitleText->SetFont(CSMakeFont(30));
		TitleText->SetJustification(ETextJustify::Center);
		TitleText->SetColorAndOpacity(FSlateColor(CSTextDark));

		if (WingTex)
		{
			UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CSTitleRow"));
			if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(MakeWing(TEXT("CSWingL"), false)))
			{
				HS->SetVerticalAlignment(VAlign_Center);
				HS->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			}
			if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(TitleText))
			{
				HS->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(MakeWing(TEXT("CSWingR"), true)))
			{
				HS->SetVerticalAlignment(VAlign_Center);
				HS->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
			}
			if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(TitleRow)) S->SetHorizontalAlignment(HAlign_Center);
		}
		else
		{
			if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);
		}
	}

	// 준비 타이머 텍스트 (라이트 — UpdatePreparationTimer 가 ≤10s 빨강으로 갱신)
	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetText(FText::FromString(TEXT("준비 시간: 30초")));
	TimerText->SetFont(CSMakeFont(24));
	TimerText->SetJustification(ETextJustify::Center);
	TimerText->SetColorAndOpacity(FSlateColor(CSTextDark));
	if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(TimerText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	// 장식: 타이머 아래 가로 테이퍼 라인 (벤픽 동일 — 레퍼런스 레드)
	{
		UTexture2D* DivTex = TryLoadTexture(CSLineTaperHPath);
		UImage* Div = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CSTimerDivider"));
		if (DivTex) Div->SetBrushFromTexture(DivTex, false);
		else Div->SetBrush(FSlateColorBrush(FLinearColor::White));
		Div->SetColorAndOpacity(FLinearColor(CSBanRed.R, CSBanRed.G, CSBanRed.B, 0.9f));
		Div->SetVisibility(ESlateVisibility::HitTestInvisible);
		USizeBox* DivBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DivBox->SetWidthOverride(620.f);
		DivBox->SetHeightOverride(DivTex ? 10.f : 2.f);
		DivBox->SetContent(Div);
		DivBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(DivBox))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));
		}
	}

	// Slice 1: 지난 라운드 결과 요약 (첫 라운드엔 숨김 — UpdateRoundResult 가 가시성 제어)
	RoundResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RoundResultText"));
	RoundResultText->SetText(FText::GetEmpty());
	RoundResultText->SetFont(CSMakeFont(17));
	RoundResultText->SetJustification(ETextJustify::Center);
	RoundResultText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(RoundResultText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	// ── 레인 배치 패널: 라이트 프레임(테두리 + 바탕) 안에 3개 레인 슬롯 ──
	{
		UBorder* LaneFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CSLaneFrame"));
		LaneFrame->SetBrushColor(CSBorderLight);
		LaneFrame->SetPadding(FMargin(1.f));
		UBorder* LanePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CSLanePanel"));
		LanePanel->SetBrushColor(CSPanelLight);
		LanePanel->SetPadding(FMargin(14.f, 12.f));
		USizeBox* LaneWidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CSLaneWidthBox"));
		LaneWidthBox->SetWidthOverride(740.f);

		UHorizontalBox* UpperPanel = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("UpperPanel"));
		LaneWidthBox->SetContent(UpperPanel);
		LanePanel->SetContent(LaneWidthBox);
		LaneFrame->SetContent(LanePanel);

		const EAOSLane Lanes[] = { EAOSLane::Top, EAOSLane::Mid, EAOSLane::Bottom };
		const FString LaneNames[] = { TEXT("TOP"), TEXT("MID"), TEXT("BOT") };

		for (int32 L = 0; L < 3; ++L)
		{
			UVerticalBox* LaneCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("LaneCol_%d"), L));
			UHorizontalBoxSlot* ColHSlot = UpperPanel->AddChildToHorizontalBox(LaneCol);
			ColHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColHSlot->SetPadding(FMargin(6.0f, 0));

			UTextBlock* LaneLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("LaneLabel_%d"), L));
			LaneLabel->SetText(FText::FromString(LaneNames[L]));
			LaneLabel->SetFont(CSMakeFont(18));
			LaneLabel->SetColorAndOpacity(FSlateColor(CSTextDark));
			LaneLabel->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* LabelVSlot = LaneCol->AddChildToVerticalBox(LaneLabel);
			LabelVSlot->SetPadding(FMargin(0, 0, 0, 6));
			LabelVSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

			for (int32 S = 0; S < 2; ++S)
			{
				UAOSLaneSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UAOSLaneSlotWidget>(
					UAOSLaneSlotWidget::StaticClass(),
					*FString::Printf(TEXT("LaneSlot_L%d_S%d"), L, S));
				SlotWidget->OwnerLane = Lanes[L];
				SlotWidget->SlotIndex = S;
				SlotWidget->OwnerSelectWidget = this;
				SlotWidget->BuildSlotUI(WidgetTree);

				LaneSlotWidgets[L * 2 + S] = SlotWidget;

				UVerticalBoxSlot* SlotVSlot = LaneCol->AddChildToVerticalBox(SlotWidget);
				SlotVSlot->SetPadding(FMargin(0, 4));
				SlotVSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}

		if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(LaneFrame))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0, 0, 0, 10));
		}
	}

	// 총 배치 + 상점 버튼 행
	{
		UHorizontalBox* InfoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CSInfoRow"));

		TotalCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TotalCountText"));
		TotalCountText->SetText(FText::FromString(TEXT("총 배치: 0/5")));
		TotalCountText->SetFont(CSMakeFont(18));
		TotalCountText->SetColorAndOpacity(FSlateColor(CSTextDark));
		if (UHorizontalBoxSlot* HS = InfoRow->AddChildToHorizontalBox(TotalCountText))
		{
			HS->SetVerticalAlignment(VAlign_Center);
			HS->SetPadding(FMargin(0, 0, 20, 0));
		}

		ShopButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopButton"));
		ShopButton->SetBackgroundColor(FLinearColor(0.55f, 0.57f, 0.62f, 1.f));
		{
			UTextBlock* ShopBtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopButtonLabel"));
			ShopBtnLabel->SetText(FText::FromString(TEXT("상점 열기")));
			ShopBtnLabel->SetFont(CSMakeFont(18));
			ShopBtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			ShopButton->AddChild(ShopBtnLabel);
		}
		ShopButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnShopButtonClicked);
		if (UHorizontalBoxSlot* HS = InfoRow->AddChildToHorizontalBox(ShopButton))
		{
			HS->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(InfoRow))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0, 0, 0, 10));
		}
	}

	// ── 캐릭터 카드 그리드: 라이트 패널 + 세로 스크롤(벤픽과 동일) ──
	{
		CardGrid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("CardGrid"));
		CardGrid->SetInnerSlotPadding(FVector2D(6.0f, 6.0f));

		UScrollBox* GridScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CSGridScroll"));
		GridScroll->SetAnimateWheelScrolling(true);
		GridScroll->AddChild(CardGrid);

		USizeBox* GridWidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CSGridWidthBox"));
		GridWidthBox->SetWidthOverride(740.f);
		GridWidthBox->SetMaxDesiredHeight(CSGridMaxHeight);
		GridWidthBox->SetContent(GridScroll);

		UBorder* GridFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CSGridFrame"));
		GridFrame->SetBrushColor(CSBorderLight);
		GridFrame->SetPadding(FMargin(1.f));
		UBorder* GridPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CSGridPanel"));
		GridPanel->SetBrushColor(FLinearColor(0.90f, 0.90f, 0.93f, 1.f));
		GridPanel->SetPadding(FMargin(14.f, 12.f));
		GridPanel->SetContent(GridWidthBox);
		GridFrame->SetContent(GridPanel);

		if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(GridFrame))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0, 0, 0, 12));
		}
	}

	// ── 라운드 준비 버튼 (LOCK IN 스타일) — 양쪽 강조 라인 ──
	{
		StartRoundButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartRoundButton"));
		StartRoundButton->SetBackgroundColor(CSLockInBlue);
		StartRoundButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartRoundText"));
		StartRoundButtonText->SetText(FText::FromString(TEXT("라운드 준비")));
		StartRoundButtonText->SetFont(CSMakeFont(22));
		StartRoundButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		StartRoundButton->SetContent(StartRoundButtonText);
		StartRoundButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnStartRoundButtonClicked);

		UTexture2D* AccentTex = TryLoadTexture(CSLineTaperHPath);
		auto MakeAccent = [&](const TCHAR* WName) -> UWidget*
		{
			UImage* Ln = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WName);
			if (AccentTex) Ln->SetBrushFromTexture(AccentTex, false);
			else Ln->SetBrush(FSlateColorBrush(FLinearColor::White));
			Ln->SetColorAndOpacity(FLinearColor(CSBanRed.R, CSBanRed.G, CSBanRed.B, 0.9f));
			Ln->SetVisibility(ESlateVisibility::HitTestInvisible);
			USizeBox* B = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			B->SetWidthOverride(150.f);
			B->SetHeightOverride(AccentTex ? 10.f : 2.f);
			B->SetContent(Ln);
			B->SetVisibility(ESlateVisibility::HitTestInvisible);
			return B;
		};

		UHorizontalBox* ReadyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CSReadyRow"));
		if (UHorizontalBoxSlot* HS = ReadyRow->AddChildToHorizontalBox(MakeAccent(TEXT("CSReadyAccentL"))))
		{
			HS->SetVerticalAlignment(VAlign_Center);
			HS->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}
		ReadyRow->AddChildToHorizontalBox(StartRoundButton);
		if (UHorizontalBoxSlot* HS = ReadyRow->AddChildToHorizontalBox(MakeAccent(TEXT("CSReadyAccentR"))))
		{
			HS->SetVerticalAlignment(VAlign_Center);
			HS->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(ReadyRow))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0, 0, 0, 6));
		}
	}

	// 양 팀 준비 상태 표시 — GameState OnRep_TeamReady 콜백에서 갱신
	TeamReadyStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TeamReadyStatusText"));
	TeamReadyStatusText->SetText(FText::FromString(TEXT("팀1: 대기중 / 팀2: 대기중")));
	TeamReadyStatusText->SetFont(CSMakeFont(16));
	TeamReadyStatusText->SetColorAndOpacity(FSlateColor(CSTextGray));
	if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(TeamReadyStatusText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// ── 상점 팝업: OuterRoot(ScaleBox 위)에 전체화면 오버레이로 추가 → 디자인 캔버스 스케일에 안 묶임 ──
	ShopWidget = WidgetTree->ConstructWidget<UAOSShopWidget>(UAOSShopWidget::StaticClass(), TEXT("ShopPopup"));
	ShopWidget->BuildShopUI();
	if (UOverlaySlot* OS = OuterRoot->AddChildToOverlay(ShopWidget))   // 마지막 자식 = 최상위 z
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] UI 동적 생성 완료 (벤픽 디자인 리스킨)"));
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

	// 10초 이하면 빨간색으로 강조 (라이트 배경 — 가독 색)
	FLinearColor Color = (Seconds <= 10) ? CSBanRed : CSTextDark;
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

	// 시각 피드백: 버튼 비활성화 + 텍스트 "준비 완료 ✓" + 그레이아웃(라이트)
	if (StartRoundButton)
	{
		StartRoundButton->SetIsEnabled(false);
		StartRoundButton->SetBackgroundColor(CSLockInIdle);
	}
	if (StartRoundButtonText)
	{
		StartRoundButtonText->SetText(FText::FromString(TEXT("준비 완료 ✓")));
		StartRoundButtonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.16f, 0.42f, 0.18f, 1.0f)));
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

	// 양 팀 모두 준비 → 녹색, 한쪽만 → 호박색, 둘 다 미준비 → 회색 (라이트 배경 가독 색)
	FLinearColor StatusColor;
	if (bTeam1Ready && bTeam2Ready)
	{
		StatusColor = FLinearColor(0.16f, 0.42f, 0.18f, 1.0f);  // 녹색
	}
	else if (bTeam1Ready || bTeam2Ready)
	{
		StatusColor = FLinearColor(0.80f, 0.52f, 0.05f, 1.0f);  // 호박색
	}
	else
	{
		StatusColor = CSTextGray;  // 회색
	}
	TeamReadyStatusText->SetColorAndOpacity(FSlateColor(StatusColor));
}

void UAOSCharacterSelectWidget::ResetReadyState()
{
	bLocalPressedReady = false;

	if (StartRoundButton)
	{
		StartRoundButton->SetIsEnabled(true);
		StartRoundButton->SetBackgroundColor(CSLockInBlue);
	}
	if (StartRoundButtonText)
	{
		StartRoundButtonText->SetText(FText::FromString(TEXT("라운드 준비")));
		StartRoundButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	if (TeamReadyStatusText)
	{
		TeamReadyStatusText->SetText(FText::FromString(TEXT("팀1: 대기중 / 팀2: 대기중")));
		TeamReadyStatusText->SetColorAndOpacity(FSlateColor(CSTextGray));
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

	// 다수 라인 승=녹색, 1라인=호박색, 0라인=빨강 (라이트 배경 가독 색)
	const FLinearColor Color = (WinCount >= 2) ? FLinearColor(0.16f, 0.42f, 0.18f, 1.0f)
		: (WinCount == 1) ? FLinearColor(0.80f, 0.52f, 0.05f, 1.0f)
		: CSBanRed;
	RoundResultText->SetColorAndOpacity(FSlateColor(Color));
	RoundResultText->SetVisibility(ESlateVisibility::Visible);
}
