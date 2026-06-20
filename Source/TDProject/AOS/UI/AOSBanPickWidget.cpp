#include "AOSBanPickWidget.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"
#include "AOSPlayerController.h"
#include "AOSCharacterPreviewStage.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScaleBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "AOSUIStyle.h"

namespace
{
	// 위젯이 자동로드할 텍스처 경로 (asset-gen 이 이 경로에 생성). 없으면 솔리드 폴백.
	// ※ WBP_BanPick 사용 시엔 디자이너가 BackdropImage 브러시를 직접 지정 → 이 경로는 폴백 전용.
	const TCHAR* kBackdropPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_Backdrop.T_BanPick_Backdrop");
	// 카드 장식 프레임(9-slice). 없으면 컬러 림 폴백.
	const TCHAR* kCardFramePath = TEXT("/Game/AOS/UI/Assets/T_BanPick_CardFrame.T_BanPick_CardFrame");
	// 중앙 비네팅 글로우(그리드 뒤). 없으면 skip.
	const TCHAR* kCenterGlowPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_CenterGlow.T_BanPick_CenterGlow");
	// 장식: 테이퍼 라인(세로/가로, white-on-alpha → 틴트) + 제목 양옆 필리그리 날개(좌 원본/우 미러)
	const TCHAR* kLineTaperPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_LineTaper.T_BanPick_LineTaper");
	const TCHAR* kLineTaperHPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_LineTaperH.T_BanPick_LineTaperH");
	const TCHAR* kWingSidePath = TEXT("/Game/AOS/UI/Assets/T_BanPick_WingSide.T_BanPick_WingSide");

	// 팀 패널/구분선 색 (깊이감)
	FLinearColor PanelColor(EAOSTeam Team)
	{
		return (Team == EAOSTeam::Team1)
			? FLinearColor(0.45f, 0.10f, 0.12f, 0.55f)   // 팀1 = 레드 (진하게)
			: FLinearColor(0.10f, 0.18f, 0.45f, 0.55f);  // 팀2 = 블루 (진하게)
	}

	FSlateFontInfo MakeFont(int32 Size)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", Size);
		return F;
	}

	// 초상화 없는(플레이스홀더) 유닛용 컬러 타일 — AOSUIStyle 의 파스텔(저채도/고명도) 분산.
	FLinearColor PlaceholderColor(int32 Index)
	{
		return AOSUIStyle::PlaceholderColor(Index);
	}

	// ── 팔레트: 전부 AOSUIStyle(공유 토큰)로 위임 — 화이트+파스텔 블렌드, 값 중앙화(중복 제거).
	//    이름(k*)은 호출부 보존 위해 유지하되 정의는 AOSUIStyle 단일 진실 (CS-접두와 이름이 달라 충돌 없음).
	const FLinearColor kBgLight    = AOSUIStyle::BgBase;      // 전체 배경(near-white)
	const FLinearColor kPanelLight = AOSUIStyle::CardWhite;   // 패널/카드 바탕(흰색)
	const FLinearColor kCardEmpty  = AOSUIStyle::PanelSoft;   // 빈 슬롯
	const FLinearColor kBorderLight= AOSUIStyle::BorderSoft;  // 얇은 테두리
	const FLinearColor kTextDark   = AOSUIStyle::TextSlate;   // 본문 텍스트(슬레이트)
	const FLinearColor kTextGray   = AOSUIStyle::TextMuted;   // 보조 텍스트
	const FLinearColor kLockInBlue = AOSUIStyle::Accent;      // LOCK IN 버튼(활성)
	const FLinearColor kLockInIdle = AOSUIStyle::AccentIdle;  // LOCK IN 버튼(비활성)
	const FLinearColor kBanRed     = AOSUIStyle::BanRed;      // 밴 X / 장식 강조

	// 그리드 최대 표시 높이 = 5행. 행 높이 ≈ 카드(58+보더4)=62 + 이름(11+패딩2)=13 + 랩패딩6 ≈ 81 → 81×5 ≈ 405 + 여유
	constexpr float kGridMaxHeight = 410.f;
}

// ============================================================
// 헬퍼
// ============================================================
UTexture2D* UAOSBanPickWidget::TryLoadTexture(const TCHAR* AssetPath)
{
	return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, AssetPath,
		nullptr, LOAD_NoWarn | LOAD_Quiet));
}

FLinearColor UAOSBanPickWidget::TeamColor(EAOSTeam Team)
{
	return AOSUIStyle::TeamAccent(Team == EAOSTeam::Team1);   // 파스텔 코랄(팀1) / 블루(팀2)
}

UTexture2D* UAOSBanPickWidget::GetPortrait(int32 RosterIndex) const
{
	return CachedRoster.IsValidIndex(RosterIndex) ? CachedRoster[RosterIndex].Portrait : nullptr;
}

AAOSGameState* UAOSBanPickWidget::GetAOSGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AAOSGameState>() : nullptr;
}

EAOSTeam UAOSBanPickWidget::GetLocalTeam() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AAOSPlayerState* PS = PC->GetPlayerState<AAOSPlayerState>())
		{
			return PS->GetTeam();
		}
	}
	return EAOSTeam::Team1;
}

FString UAOSBanPickWidget::GetPlayerNameForTeam(EAOSTeam Team) const
{
	// 자기 팀이면 로컬 PS 우선
	if (APlayerController* MyPC = GetOwningPlayer())
	{
		if (AAOSPlayerState* MyPS = MyPC->GetPlayerState<AAOSPlayerState>())
		{
			if (MyPS->GetTeam() == Team && !MyPS->GetPlayerName().IsEmpty())
			{
				return MyPS->GetPlayerName();
			}
		}
	}
	// GameState PlayerArray 에서 해당 팀 첫 PS
	if (AAOSGameState* GS = GetAOSGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AAOSPlayerState* AOSPS = Cast<AAOSPlayerState>(PS))
			{
				if (AOSPS->GetTeam() == Team && !AOSPS->GetPlayerName().IsEmpty())
				{
					return AOSPS->GetPlayerName();
				}
			}
		}
	}
	return (Team == EAOSTeam::Team1) ? TEXT("Player 1") : TEXT("Player 2");
}

// ============================================================
// WBP 없을 때 C++ 폴백 정적 프레임 (빈 동적 컨테이너 포함)
//   ※ RootWidget 미설정일 때만 동작 — WBP_BanPick 이 트리를 저작했으면 skip.
// ============================================================
TSharedRef<SWidget> UAOSBanPickWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackFrame();
	}
	return Super::RebuildWidget();
}

void UAOSBanPickWidget::BuildFallbackFrame()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	// 루트: 바깥 오버레이(창 전체 배경 + ScaleBox 래퍼)
	//   ⚠ 창 크기/비율이 바뀌어도 코너 블록이 겹치지 않게 — 콘텐츠를 1920x1080 고정 디자인 캔버스에 담고
	//     ScaleBox(ScaleToFit)로 비율 유지 균일 스케일(레터박스). 절대 픽셀 레이아웃의 리사이즈 대응.
	UOverlay* OuterRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BPOuter"));
	WidgetTree->RootWidget = OuterRoot;

	// 바깥 배경 (레터박스 영역까지 라이트로 — 디자인 캔버스 배경과 동색이라 이음새 안 보임)
	UImage* OuterBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BPOuterBg"));
	OuterBg->SetBrush(FSlateColorBrush(kBgLight));
	OuterBg->SetVisibility(ESlateVisibility::Visible);   // 레터박스 영역 클릭도 루트로 버블링
	if (UOverlaySlot* OS = OuterRoot->AddChildToOverlay(OuterBg))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	// ScaleBox(ScaleToFit): 1920x1080 디자인을 비율 유지 균일 스케일(필요 시 레터박스). 안쪽 UI 까지 함께 스케일.
	//   ※ Stretch=Fill 은 고정크기 SizeBox 자식을 스케일 안 함(슬롯만 늘림→안쪽 크기 안 변함),
	//     수동 RenderScale 은 뷰포트/DPI 좌표 계산이 까다로워 한쪽 과도 잘림 → 검증된 ScaleToFit 채택.
	//     실제 플레이(16:9)에선 여백 0. 비-16:9 PIE 창에서만 여백.
	UScaleBox* ScaleRoot = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("BPScale"));
	ScaleRoot->SetStretch(EStretch::ScaleToFit);
	if (UOverlaySlot* OS = OuterRoot->AddChildToOverlay(ScaleRoot))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* DesignCanvas = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BPDesignCanvas"));
	DesignCanvas->SetWidthOverride(1920.f);
	DesignCanvas->SetHeightOverride(1080.f);
	ScaleRoot->SetContent(DesignCanvas);

	// 콘텐츠 오버레이 — 이하 모든 장식/블록/그리드/프리뷰는 1920x1080 기준 절대 배치
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BPRoot"));
	DesignCanvas->SetContent(RootOverlay);

	// [0] 배경 (라이트 테마 — 레퍼런스의 밝은 무채색. Visible → 카드 클릭 히트테스트 버블링)
	BackdropImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackdropImage"));
	BackdropImage->SetBrush(FSlateColorBrush(kBgLight));
	BackdropImage->SetVisibility(ESlateVisibility::Visible);
	if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(BackdropImage))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	// [0.3] 장식: 대각 팀 경계선 (스케치 — 좌 = 블루 코너 경계 / 우 = 레드 코너 경계, 팀색)
	//   콘텐츠 아래 z + HitTestInvisible (클릭은 백드롭으로 버블링). 텍스처 없으면 얇은 솔리드 폴백.
	{
		UTexture2D* LineTex = TryLoadTexture(kLineTaperPath);
		auto MakeDiagonal = [&](const TCHAR* WName, const FLinearColor& Tint, float Angle) -> UWidget*
		{
			UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WName);
			if (LineTex) Line->SetBrushFromTexture(LineTex, false);
			else Line->SetBrush(FSlateColorBrush(FLinearColor::White));
			Line->SetColorAndOpacity(Tint);
			Line->SetVisibility(ESlateVisibility::HitTestInvisible);
			Line->SetRenderTransformAngle(Angle);   // 피벗 중앙 — 세로 라인이 기울어 대각선
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(LineTex ? 46.f : 3.f);
			Box->SetContent(Line);
			Box->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Box;
		};

		// 좌: 블루팀(하단-좌) 경계 — 위가 중앙 쪽, 아래가 블루 코너 쪽 (시계방향 +8°)
		const FLinearColor BlueTint(TeamColor(EAOSTeam::Team2).R, TeamColor(EAOSTeam::Team2).G,
			TeamColor(EAOSTeam::Team2).B, 0.70f);
		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(
			MakeDiagonal(TEXT("BPDiagLeft"), BlueTint, 8.f)))
		{
			OS->SetHorizontalAlignment(HAlign_Left);
			OS->SetVerticalAlignment(VAlign_Fill);
			OS->SetPadding(FMargin(420.f, 30.f, 0.f, 20.f));
		}

		// 우: 레드팀(상단-우) 경계 — 위가 레드 코너 쪽, 아래가 중앙 쪽 (반시계 -8°)
		const FLinearColor RedTint(TeamColor(EAOSTeam::Team1).R, TeamColor(EAOSTeam::Team1).G,
			TeamColor(EAOSTeam::Team1).B, 0.70f);
		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(
			MakeDiagonal(TEXT("BPDiagRight"), RedTint, -8.f)))
		{
			OS->SetHorizontalAlignment(HAlign_Right);
			OS->SetVerticalAlignment(VAlign_Fill);
			OS->SetPadding(FMargin(0.f, 30.f, 420.f, 20.f));
		}
	}

	// ── 상단 중앙: 제목 + 타이머 + CHAMPION SELECT (레퍼런스 상단 중앙) ──
	{
		UVerticalBox* TopVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPTopVB"));

		// 제목 행: [좌 날개] CHARACTER SELECT [우 날개(미러)] — 날개가 제목을 감싸는 형태
		{
			UTexture2D* WingTex = TryLoadTexture(kWingSidePath);
			auto MakeWing = [&](const TCHAR* WName, bool bMirror) -> UWidget*
			{
				UImage* Wing = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WName);
				if (WingTex) Wing->SetBrushFromTexture(WingTex, false);
				Wing->SetColorAndOpacity(FLinearColor(kBanRed.R, kBanRed.G, kBanRed.B, 0.85f));
				Wing->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (bMirror)
				{
					Wing->SetRenderScale(FVector2D(-1.f, 1.f));   // 좌 날개 원본 → 우측 미러
				}
				USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Box->SetWidthOverride(108.f);
				Box->SetHeightOverride(68.f);   // 373x235 비율
				Box->SetContent(Wing);
				Box->SetVisibility(ESlateVisibility::HitTestInvisible);
				return Box;
			};

			TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
			TitleText->SetText(FText::FromString(TEXT("CHARACTER SELECT")));
			TitleText->SetFont(MakeFont(30));
			TitleText->SetJustification(ETextJustify::Center);
			TitleText->SetColorAndOpacity(FSlateColor(kTextDark));

			if (WingTex)
			{
				UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BPTitleRow"));
				if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(MakeWing(TEXT("BPWingL"), false)))
				{
					HS->SetVerticalAlignment(VAlign_Center);
					HS->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
				}
				if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(TitleText))
				{
					HS->SetVerticalAlignment(VAlign_Center);
				}
				if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(MakeWing(TEXT("BPWingR"), true)))
				{
					HS->SetVerticalAlignment(VAlign_Center);
					HS->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
				}
				if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(TitleRow)) S->SetHorizontalAlignment(HAlign_Center);
			}
			else
			{
				if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);
			}
		}

		UTextBlock* SelectLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPSelectLabel"));
		SelectLabel->SetText(FText::FromString(TEXT("SEASON 9 DRAFT")));
		SelectLabel->SetFont(MakeFont(14));
		SelectLabel->SetJustification(ETextJustify::Center);
		SelectLabel->SetColorAndOpacity(FSlateColor(kTextGray));
		if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(SelectLabel)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f)); }

		TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
		TimerText->SetText(FText::FromString(TEXT("--")));
		TimerText->SetFont(MakeFont(34));
		TimerText->SetJustification(ETextJustify::Center);
		TimerText->SetColorAndOpacity(FSlateColor(kTextDark));
		if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(TimerText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f)); }

		// 장식: 타이머 아래 가로 라인 1줄 (LOCK IN 강조선과 동일 스타일, 그리드 폭만큼 — 레퍼런스 레드)
		{
			UTexture2D* DivTex = TryLoadTexture(kLineTaperHPath);
			UImage* Div = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BPTimerDivider"));
			if (DivTex) Div->SetBrushFromTexture(DivTex, false);
			else Div->SetBrush(FSlateColorBrush(FLinearColor::White));
			Div->SetColorAndOpacity(FLinearColor(kBanRed.R, kBanRed.G, kBanRed.B, 0.9f));
			Div->SetVisibility(ESlateVisibility::HitTestInvisible);
			USizeBox* DivBox2 = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			DivBox2->SetWidthOverride(614.f);    // 챔피언 그리드 폭과 일치
			DivBox2->SetHeightOverride(DivTex ? 10.f : 2.f);
			DivBox2->SetContent(Div);
			DivBox2->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(DivBox2))
			{
				S->SetHorizontalAlignment(HAlign_Center);
				S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
		}

		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(TopVB))
		{
			OS->SetHorizontalAlignment(HAlign_Center);
			OS->SetVerticalAlignment(VAlign_Top);
			OS->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
	}

	// ── 중앙: 챔피언 그리드 + 상태 + LOCK IN (화면 중앙) ──
	{
		UVerticalBox* CenterVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPCenter"));

		CardGrid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("CardGrid"));
		// 세로 스크롤: 5행(kGridMaxHeight) 초과 시 스크롤바 (가로 스크롤 없음 — WrapBox 폭이 열 수 고정)
		UScrollBox* GridScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BPGridScroll"));
		GridScroll->SetAnimateWheelScrolling(true);
		GridScroll->AddChild(CardGrid);
		// 그리드 폭 고정 → 레퍼런스처럼 ~8열로 래핑 (+14 = 스크롤바 폭 여유)
		USizeBox* GridWidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BPGridWidthBox"));
		GridWidthBox->SetWidthOverride(614.f);
		GridWidthBox->SetMaxDesiredHeight(kGridMaxHeight);
		GridWidthBox->SetContent(GridScroll);
		// 챔피언 풀 패널: 라이트 바탕 + 얇은 테두리
		UBorder* GridFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPGridFrame"));
		GridFrame->SetBrushColor(kBorderLight);
		GridFrame->SetPadding(FMargin(1.f));
		UBorder* GridPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPGridPanel"));
		GridPanel->SetBrushColor(FLinearColor(0.90f, 0.90f, 0.93f, 1.f));
		GridPanel->SetPadding(FMargin(14.f, 12.f, 14.f, 12.f));
		GridPanel->SetContent(GridWidthBox);
		GridFrame->SetContent(GridPanel);
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(GridFrame))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetText(FText::FromString(TEXT("드래프트 준비 중...")));
		StatusText->SetFont(MakeFont(16));
		StatusText->SetJustification(ETextJustify::Center);
		StatusText->SetColorAndOpacity(FSlateColor(kTextDark));
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(StatusText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 12.f, 0.f, 8.f));
		}

		// LOCK IN (확정) 버튼 — 크게, 중앙 (OnClicked 바인딩은 InitializeWithRoster 에서)
		//   양쪽 강조 라인 (스케치 — 팀 무관 → 레퍼런스 레드. 텍스처 없으면 솔리드 폴백)
		ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
		ConfirmButton->SetBackgroundColor(kLockInIdle); // 기본(차례 아님)
		ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmText"));
		ConfirmText->SetText(FText::FromString(TEXT("LOCK IN")));
		ConfirmText->SetFont(MakeFont(22));
		ConfirmText->SetJustification(ETextJustify::Center);
		ConfirmText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ConfirmButton->SetContent(ConfirmText);

		{
			UTexture2D* AccentTex = TryLoadTexture(kLineTaperHPath);
			auto MakeAccent = [&](const TCHAR* WName) -> UWidget*
			{
				UImage* L = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WName);
				if (AccentTex) L->SetBrushFromTexture(AccentTex, false);
				else L->SetBrush(FSlateColorBrush(FLinearColor::White));
				L->SetColorAndOpacity(FLinearColor(kBanRed.R, kBanRed.G, kBanRed.B, 0.9f));
				L->SetVisibility(ESlateVisibility::HitTestInvisible);
				USizeBox* B = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				B->SetWidthOverride(150.f);
				B->SetHeightOverride(AccentTex ? 10.f : 2.f);
				B->SetContent(L);
				B->SetVisibility(ESlateVisibility::HitTestInvisible);
				return B;
			};

			UHorizontalBox* LockRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BPLockRow"));
			if (UHorizontalBoxSlot* HS = LockRow->AddChildToHorizontalBox(MakeAccent(TEXT("BPLockAccentL"))))
			{
				HS->SetVerticalAlignment(VAlign_Center);
				HS->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
			}
			LockRow->AddChildToHorizontalBox(ConfirmButton);
			if (UHorizontalBoxSlot* HS = LockRow->AddChildToHorizontalBox(MakeAccent(TEXT("BPLockAccentR"))))
			{
				HS->SetVerticalAlignment(VAlign_Center);
				HS->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
			}
			if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(LockRow))
			{
				S->SetHorizontalAlignment(HAlign_Center);
			}
		}

		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(CenterVB))
		{
			OS->SetHorizontalAlignment(HAlign_Center);
			OS->SetVerticalAlignment(VAlign_Center);
		}
	}

	// ── 팀1(레드) 블록: 상단-우 (이름 + 가로 픽 행 + 밴) ──
	{
		UVerticalBox* RedBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPRedBlock"));

		Team1PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Team1PlayerNameText"));
		Team1PlayerNameText->SetText(FText::FromString(TEXT("TEAM 1")));
		Team1PlayerNameText->SetFont(MakeFont(16));
		Team1PlayerNameText->SetJustification(ETextJustify::Right);
		Team1PlayerNameText->SetColorAndOpacity(FSlateColor(TeamColor(EAOSTeam::Team1)));
		if (UVerticalBoxSlot* S = RedBlock->AddChildToVerticalBox(Team1PlayerNameText)) { S->SetHorizontalAlignment(HAlign_Right); S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f)); }

		Team1PickRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Team1PickRow"));
		if (UVerticalBoxSlot* S = RedBlock->AddChildToVerticalBox(Team1PickRow)) S->SetHorizontalAlignment(HAlign_Right);

		// 밴: "BAN" 라벨 + 밴 행
		{
			UHorizontalBox* BanLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* BanLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			BanLabel->SetText(FText::FromString(TEXT("BAN HEROES")));
			BanLabel->SetFont(MakeFont(11));
			BanLabel->SetColorAndOpacity(FSlateColor(kTextGray));
			if (UHorizontalBoxSlot* HS = BanLine->AddChildToHorizontalBox(BanLabel)) HS->SetVerticalAlignment(VAlign_Center);
			Team1BanRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Team1BanRow"));
			BanLine->AddChildToHorizontalBox(Team1BanRow);
			if (UVerticalBoxSlot* S = RedBlock->AddChildToVerticalBox(BanLine)) { S->SetHorizontalAlignment(HAlign_Right); S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }
		}

		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(RedBlock))
		{
			OS->SetHorizontalAlignment(HAlign_Right);
			OS->SetVerticalAlignment(VAlign_Top);
			OS->SetPadding(FMargin(0.f, 24.f, 24.f, 0.f));
		}
	}

	// ── 팀2(블루) 블록: 하단-좌 (이름 + 가로 픽 행 + 밴) ──
	{
		UVerticalBox* BlueBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPBlueBlock"));

		// 밴 행을 최상단(블루 플레이어 이름 위)에 배치 — 레퍼런스. 순서: 밴 → 이름 → 픽
		{
			UHorizontalBox* BanLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Team2BanRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Team2BanRow"));
			BanLine->AddChildToHorizontalBox(Team2BanRow);
			UTextBlock* BanLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			BanLabel->SetText(FText::FromString(TEXT("BAN HEROES")));
			BanLabel->SetFont(MakeFont(11));
			BanLabel->SetColorAndOpacity(FSlateColor(kTextGray));
			if (UHorizontalBoxSlot* HS = BanLine->AddChildToHorizontalBox(BanLabel)) HS->SetVerticalAlignment(VAlign_Center);
			if (UVerticalBoxSlot* S = BlueBlock->AddChildToVerticalBox(BanLine)) { S->SetHorizontalAlignment(HAlign_Left); S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f)); }
		}

		Team2PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Team2PlayerNameText"));
		Team2PlayerNameText->SetText(FText::FromString(TEXT("TEAM 2")));
		Team2PlayerNameText->SetFont(MakeFont(16));
		Team2PlayerNameText->SetColorAndOpacity(FSlateColor(TeamColor(EAOSTeam::Team2)));
		if (UVerticalBoxSlot* S = BlueBlock->AddChildToVerticalBox(Team2PlayerNameText)) { S->SetHorizontalAlignment(HAlign_Left); S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f)); }

		Team2PickRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Team2PickRow"));
		if (UVerticalBoxSlot* S = BlueBlock->AddChildToVerticalBox(Team2PickRow)) S->SetHorizontalAlignment(HAlign_Left);

		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(BlueBlock))
		{
			OS->SetHorizontalAlignment(HAlign_Left);
			OS->SetVerticalAlignment(VAlign_Bottom);
			OS->SetPadding(FMargin(24.f, 0.f, 0.f, 24.f));
		}
	}

	// ── 3D 캐릭터 프리뷰 (폴백: 좌상단=상대 / 우하단=내 팀) ──
	//   최상단 z + (RT 연결 시) HitTestInvisible → 카드 클릭이 루트로 통과. RT 미연결 시 Collapsed.
	{
		EnemyPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EnemyPreviewImage"));
		EnemyPreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		USizeBox* EnemyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BPEnemyPrevBox"));
		EnemyBox->SetWidthOverride(300.f);
		EnemyBox->SetHeightOverride(560.f);
		EnemyBox->SetContent(EnemyPreviewImage);
		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(EnemyBox))
		{
			OS->SetHorizontalAlignment(HAlign_Left);
			OS->SetVerticalAlignment(VAlign_Top);
			OS->SetPadding(FMargin(10.f, 70.f, 0.f, 0.f));
		}

		MyPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MyPreviewImage"));
		MyPreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		USizeBox* MyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BPMyPrevBox"));
		MyBox->SetWidthOverride(300.f);
		MyBox->SetHeightOverride(560.f);
		MyBox->SetContent(MyPreviewImage);
		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(MyBox))
		{
			OS->SetHorizontalAlignment(HAlign_Right);
			OS->SetVerticalAlignment(VAlign_Bottom);
			OS->SetPadding(FMargin(0.f, 0.f, 10.f, 10.f));
		}
	}
}

// ============================================================
// 동적 자식 채우기 (바인딩되거나 폴백 생성된 컨테이너 대상, 멱등)
// ============================================================
void UAOSBanPickWidget::PopulateBanRow(EAOSTeam Team)
{
	UHorizontalBox* Row = (Team == EAOSTeam::Team1) ? Team1BanRow : Team2BanRow;
	if (!Row || !WidgetTree) return;
	Row->ClearChildren();

	TArray<UImage*>& BanImages = (Team == EAOSTeam::Team1) ? Team1BanImages : Team2BanImages;
	BanImages.Reset();

	for (int32 i = 0; i < BansPerTeam; ++i)
	{
		// 라이트 박스 + 얇은 테두리. 빈 상태 = 빨간 ✕ (이미지가 투명해 X 비침), 밴되면 초상화 회색조.
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(40.f);
		Box->SetHeightOverride(40.f);

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(FSlateRoundedBoxBrush(AOSUIStyle::CardWhite, AOSUIStyle::SlotRadius, AOSUIStyle::BorderSoft, 1.f));
		Frame->SetPadding(FMargin(2.f));
		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Inner->SetBrushColor(kCardEmpty);
		Inner->SetPadding(FMargin(0.f));

		UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

		UTextBlock* X = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		X->SetText(FText::FromString(TEXT("✕")));   // ✕
		X->SetFont(MakeFont(20));
		X->SetColorAndOpacity(FSlateColor(kBanRed));
		X->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* OS = Ov->AddChildToOverlay(X)) { OS->SetHorizontalAlignment(HAlign_Center); OS->SetVerticalAlignment(VAlign_Center); }

		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrush(FSlateColorBrush(kCardEmpty));
		Img->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));   // 기본 투명 → X 보임
		if (UOverlaySlot* OS = Ov->AddChildToOverlay(Img)) { OS->SetHorizontalAlignment(HAlign_Fill); OS->SetVerticalAlignment(VAlign_Fill); }

		Inner->SetContent(Ov);
		Frame->SetContent(Inner);
		Box->SetContent(Frame);
		BanImages.Add(Img);

		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Box))
		{
			HS->SetPadding(FMargin(3.f, 4.f, 3.f, 0.f));
		}
	}
}

void UAOSBanPickWidget::PopulatePickRow(EAOSTeam Team)
{
	UHorizontalBox* Row = (Team == EAOSTeam::Team1) ? Team1PickRow : Team2PickRow;
	if (!Row || !WidgetTree) return;
	Row->ClearChildren();

	TArray<UBorder*>& Borders = (Team == EAOSTeam::Team1) ? Team1PickBorders : Team2PickBorders;
	TArray<UImage*>& Images = (Team == EAOSTeam::Team1) ? Team1PickImages : Team2PickImages;
	TArray<UTextBlock*>& Names = (Team == EAOSTeam::Team1) ? Team1PickNames : Team2PickNames;
	Borders.Reset(); Images.Reset(); Names.Reset();

	const FLinearColor TC = TeamColor(Team);
	const FString Prefix = (Team == EAOSTeam::Team1) ? TEXT("R") : TEXT("B");

	for (int32 i = 0; i < PicksPerTeam; ++i)
	{
		// 레퍼런스 카드: [초상화 영역] + "SELECTED HERO" + 이름(CHOOSE HERO) + 슬롯탭(R1/B1)
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());   // 둥근 흰 카드 + 팀 파스텔 보더(RefreshSlots 가 상태별 교체)
		Frame->SetBrush(FSlateRoundedBoxBrush(AOSUIStyle::CardWhite, AOSUIStyle::SlotRadius,
			AOSUIStyle::TeamAccent(Team == EAOSTeam::Team1), 1.5f));
		Frame->SetPadding(FMargin(6.f));
		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Inner->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.f));   // 투명 — 둥근 Frame 의 흰 카드면을 그대로 사용
		Inner->SetPadding(FMargin(4.f));
		UVerticalBox* CardVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Inner->SetContent(CardVB);
		Frame->SetContent(Inner);

		// 초상화 영역 (빈 칸 = kCardEmpty, 픽되면 RefreshSlots 가 초상화)
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrush(FSlateColorBrush(kCardEmpty));
		USizeBox* PortBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PortBox->SetHeightOverride(88.f);
		PortBox->SetContent(Img);
		if (UVerticalBoxSlot* S = CardVB->AddChildToVerticalBox(PortBox)) S->SetHorizontalAlignment(HAlign_Fill);

		// "SELECTED HERO" 헤더
		UTextBlock* Hdr = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Hdr->SetText(FText::FromString(TEXT("SELECTED HERO")));
		Hdr->SetFont(MakeFont(8));
		Hdr->SetColorAndOpacity(FSlateColor(kTextGray));
		Hdr->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = CardVB->AddChildToVerticalBox(Hdr)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f)); }

		// 이름 (빈=CHOOSE HERO / 픽=챔피언명) — RefreshSlots 가 갱신
		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(FText::FromString(TEXT("CHOOSE HERO")));
		Name->SetFont(MakeFont(10));
		Name->SetColorAndOpacity(FSlateColor(kTextDark));
		Name->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = CardVB->AddChildToVerticalBox(Name)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f)); }

		// 슬롯 탭 (R1..R5 / B1..B5) — 팀색 바
		UBorder* Tab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tab->SetBrushColor(TC);
		Tab->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
		UTextBlock* TabT = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TabT->SetText(FText::FromString(FString::Printf(TEXT("%s%d"), *Prefix, i + 1)));
		TabT->SetFont(MakeFont(11));
		TabT->SetColorAndOpacity(FSlateColor(AOSUIStyle::TeamDeep(Team == EAOSTeam::Team1)));   // 파스텔 탭 위 진한 팀색 텍스트(대비)
		TabT->SetJustification(ETextJustify::Center);
		Tab->SetContent(TabT);
		if (UVerticalBoxSlot* S = CardVB->AddChildToVerticalBox(Tab)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }

		// 카드 크기 고정 → 가로 행 5칸
		USizeBox* SlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotBox->SetWidthOverride(108.f);
		SlotBox->SetHeightOverride(168.f);
		SlotBox->SetContent(Frame);
		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(SlotBox))
		{
			HS->SetVerticalAlignment(VAlign_Top);
			HS->SetPadding(FMargin(3.f, 0.f, 3.f, 0.f));
		}

		Borders.Add(Frame);
		Images.Add(Img);
		Names.Add(Name);
	}
}

// ============================================================
// 로스터 주입 → 동적 자식(밴/픽 슬롯 + 그리드 카드) 채움
//   WBP 경로/폴백 경로 공통. PC 가 ShowBanPick 마다 호출(멱등 — clear 후 refill).
// ============================================================
void UAOSBanPickWidget::InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster)
{
	CachedRoster = Roster;

	// 프레임 보장: WBP 미사용(폴백) + 아직 미생성이면 지금 생성.
	// (WBP 사용 시엔 CreateWidget 단계에서 BindWidget 이 이미 컨테이너를 채워줌 → skip)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackFrame();
	}

	// 밴/픽 슬롯 (바인딩되거나 폴백 생성된 컨테이너에)
	PopulateBanRow(EAOSTeam::Team1);
	PopulateBanRow(EAOSTeam::Team2);
	PopulatePickRow(EAOSTeam::Team1);
	PopulatePickRow(EAOSTeam::Team2);

	// 챔피언 그리드 카드
	if (CardGrid)
	{
		CardGrid->ClearChildren();
		CardBorders.Reset();
		CardImages.Reset();

		for (int32 i = 0; i < Roster.Num(); ++i)
		{
			// 셀: [카드(얇은 테두리→초상화)] + [챔피언명 라벨] — 레퍼런스 그리드
			UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

			UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("BPCard_%d"), i));
			Card->SetVisibility(ESlateVisibility::HitTestInvisible); // 클릭은 루트가 히트테스트로 처리
			// 소프트 카드: 둥근 흰 프레임 + 얇은 소프트 보더 (RefreshCards 가 상태별로 보더 색/두께 교체)
			Card->SetBrush(FSlateRoundedBoxBrush(AOSUIStyle::CardWhite, AOSUIStyle::CardRadius,
				AOSUIStyle::BorderSoft, 1.f));
			Card->SetPadding(FMargin(4.f));

			USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CardSize->SetWidthOverride(58.f);
			CardSize->SetHeightOverride(58.f);
			Card->SetContent(CardSize);

			UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			if (UTexture2D* Portrait = Roster[i].Portrait)
			{
				Img->SetBrushFromTexture(Portrait, false);
			}
			else
			{
				Img->SetBrush(FSlateColorBrush(PlaceholderColor(i)));  // 초상화 없으면 고유 컬러 타일
			}
			if (USizeBoxSlot* SBS = Cast<USizeBoxSlot>(CardSize->SetContent(Img)))
			{
				SBS->SetHorizontalAlignment(HAlign_Fill);
				SBS->SetVerticalAlignment(VAlign_Fill);
			}
			if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Card)) S->SetHorizontalAlignment(HAlign_Center);

			// 챔피언명 라벨 (카드 아래, 폭 제한)
			UTextBlock* NameLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			NameLbl->SetText(Roster[i].DisplayName);
			NameLbl->SetFont(MakeFont(8));
			NameLbl->SetColorAndOpacity(FSlateColor(kTextDark));
			NameLbl->SetJustification(ETextJustify::Center);
			NameLbl->SetClipping(EWidgetClipping::ClipToBounds);
			USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			NameBox->SetWidthOverride(64.f);
			NameBox->SetContent(NameLbl);
			if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(NameBox)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f)); }

			if (UWrapBoxSlot* WS = CardGrid->AddChildToWrapBox(Cell))
			{
				WS->SetPadding(FMargin(3.f));
			}

			CardBorders.Add(Card);
			CardImages.Add(Img);
		}
	}

	// 확정 버튼 클릭 바인딩 (WBP 버튼/폴백 버튼 단일 경로 — 중복 add 방지)
	if (ConfirmButton && !ConfirmButton->OnClicked.IsAlreadyBound(this, &UAOSBanPickWidget::OnConfirmClicked))
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UAOSBanPickWidget::OnConfirmClicked);
	}

	PendingIndex = INDEX_NONE;
	RefreshCards();
	RefreshSlots();
	RefreshStatus();
	UpdatePreviewSelections();
}

// ============================================================
// 3D 캐릭터 프리뷰
// ============================================================
void UAOSBanPickWidget::SetPreviewStages(AAOSCharacterPreviewStage* Mine, AAOSCharacterPreviewStage* Enemy)
{
	MyPreviewStage = Mine;
	EnemyPreviewStage = Enemy;

	// 스테이지 렌더 타깃을 프리뷰 Image 브러시에 직접 연결 (머티리얼 에셋 불필요).
	auto BindRT = [](UImage* Img, AAOSCharacterPreviewStage* Stage)
	{
		if (!Img) return;
		UTextureRenderTarget2D* RT = Stage ? Stage->GetRenderTarget() : nullptr;
		if (RT)
		{
			FSlateBrush B;
			B.SetResourceObject(RT);
			B.ImageSize = FVector2D(RT->SizeX, RT->SizeY);
			B.DrawAs = ESlateBrushDrawType::Image;
			Img->SetBrush(B);
			Img->SetVisibility(ESlateVisibility::HitTestInvisible);  // 카드 클릭은 루트로 통과
		}
		else
		{
			Img->SetVisibility(ESlateVisibility::Collapsed);
		}
	};
	BindRT(MyPreviewImage, MyPreviewStage);
	BindRT(EnemyPreviewImage, EnemyPreviewStage);

	UpdatePreviewSelections();
}

void UAOSBanPickWidget::UpdatePreviewSelections()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS) return;

	const EAOSTeam Local = GetLocalTeam();
	const EAOSTeam Enemy = (Local == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;

	// 팀의 "가장 최근 드래프트 유닛"(밴 또는 픽). 드래프트 시퀀스는 밴4를 모두 픽보다
	// 먼저 진행하므로, 픽이 있으면 픽이 더 최신이고 없으면 마지막 밴이 그 팀의 최신 선택.
	// ⚠ 예전 버그: 상대(및 내 폴백)를 GetPickedUnits 만으로 계산 → 상대가 "밴"하면
	//   픽 배열이 비어 프리뷰가 안 떴다. 밴 배열까지 봐야 밴 단계에서도 프리뷰가 갱신됨.
	auto LatestDrafted = [GS](EAOSTeam Team) -> int32
	{
		const TArray<int32>& Picks = GS->GetPickedUnits(Team);
		if (Picks.Num() > 0) return Picks.Last();
		const TArray<int32>& Bans = (Team == EAOSTeam::Team1) ? GS->Team1BannedUnitIds : GS->Team2BannedUnitIds;
		if (Bans.Num() > 0) return Bans.Last();
		return INDEX_NONE;
	};

	// 내 쪽(우하단): 미리보기(PendingIndex) 중이면 그 챔피언, 아니면 내 팀 최신 드래프트(밴/픽)
	const int32 MyUnit = (PendingIndex != INDEX_NONE) ? PendingIndex : LatestDrafted(Local);

	// 상대 쪽(좌상단): 상대 팀 최신 드래프트(밴/픽). 상대 pending 은 리플리케이트 안 되므로
	// 상대 확정(밴/픽 기록) 시점에 갱신된다.
	const int32 EnemyUnit = LatestDrafted(Enemy);

	if (MyPreviewStage)
	{
		MyPreviewStage->SetPreviewCharacter(
			CachedRoster.IsValidIndex(MyUnit) ? CachedRoster[MyUnit].CharacterClass : nullptr);
	}
	if (EnemyPreviewStage)
	{
		EnemyPreviewStage->SetPreviewCharacter(
			CachedRoster.IsValidIndex(EnemyUnit) ? CachedRoster[EnemyUnit].CharacterClass : nullptr);
	}
}

// ============================================================
// 라이프사이클
// ============================================================
void UAOSBanPickWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 안전망: 보통 RebuildWidget 이 이미 프레임을 만들지만, 폴백 미생성 상태면 여기서 보장.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackFrame();
	}
	if (!bSubscribed)
	{
		if (AAOSGameState* GS = GetAOSGameState())
		{
			GS->OnDraftChanged.AddDynamic(this, &UAOSBanPickWidget::OnDraftChanged);
			bSubscribed = true;
		}
	}
}

void UAOSBanPickWidget::NativeDestruct()
{
	if (bSubscribed)
	{
		if (AAOSGameState* GS = GetAOSGameState())
		{
			GS->OnDraftChanged.RemoveDynamic(this, &UAOSBanPickWidget::OnDraftChanged);
		}
		bSubscribed = false;
	}
	Super::NativeDestruct();
}

void UAOSBanPickWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);
	// 반응형 스케일은 ScaleBox(ScaleToFit)가 자동 처리 — 별도 틱 로직 불필요.
	// 타이머 표시 갱신 (NativeTick 미호출 시엔 DraftTurnTimeRemaining 의 OnRep_Draft 가 RefreshStatus 트리거)
	RefreshStatus();
}

FReply UAOSBanPickWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Abs = InMouseEvent.GetScreenSpacePosition();

	// 스크롤 가드: 뷰포트(ScrollBox) 밖으로 스크롤된 카드도 cached geometry 는 화면 좌표를 가짐 →
	// 클릭이 그리드 뷰포트(=CardGrid 부모) 안일 때만 카드 매칭 (그리드 위/아래 영역 오클릭 방지)
	if (CardGrid)
	{
		if (UWidget* Viewport = CardGrid->GetParent())
		{
			if (!Viewport->GetCachedGeometry().IsUnderLocation(Abs))
			{
				return FReply::Unhandled();
			}
		}
	}

	for (int32 i = 0; i < CardBorders.Num(); ++i)
	{
		if (CardBorders[i] && CardBorders[i]->GetCachedGeometry().IsUnderLocation(Abs))
		{
			HandleCardClicked(i);
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

// ============================================================
// 인터랙션 (2단계: 선택 → 확정)
// ============================================================
void UAOSBanPickWidget::OnDraftChanged()
{
	// ⚠ 타이머 갱신(ServerSetDraftTurnTime)도 매 초 OnDraftChanged 를 브로드캐스트한다.
	//   무조건 PendingIndex 를 초기화하면 미리보기가 1초마다 풀려 확정을 못 누른다.
	//   → 미리보기한 유닛이 실제로 밴/픽되어 가용하지 않을 때만 초기화 (내 확정 직후 자연 해제).
	if (AAOSGameState* GS = GetAOSGameState())
	{
		if (PendingIndex != INDEX_NONE && !GS->IsUnitAvailableForDraft(PendingIndex))
		{
			PendingIndex = INDEX_NONE;
		}
	}
	RefreshCards();
	RefreshSlots();
	RefreshStatus();
	UpdatePreviewSelections();   // 픽 확정/변경 시 3D 프리뷰 메시 스왑
}

void UAOSBanPickWidget::HandleCardClicked(int32 RosterIndex)
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS || GS->IsDraftComplete()) return;
	if (GS->GetActiveDraftTeam() != GetLocalTeam()) return;     // 내 턴 아님
	if (!GS->IsUnitAvailableForDraft(RosterIndex)) return;       // 밴/픽 된 유닛

	PendingIndex = RosterIndex;  // 미리보기만 (서버 전송은 확정 버튼에서)
	RefreshCards();
	RefreshStatus();
	UpdatePreviewSelections();   // 카드 클릭 = 내 쪽 3D 프리뷰에 즉시 표시
}

void UAOSBanPickWidget::OnConfirmClicked()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS || GS->IsDraftComplete()) return;
	if (GS->GetActiveDraftTeam() != GetLocalTeam()) return;
	if (PendingIndex == INDEX_NONE || !GS->IsUnitAvailableForDraft(PendingIndex)) return;

	if (AAOSPlayerController* PC = Cast<AAOSPlayerController>(GetOwningPlayer()))
	{
		PC->Server_DraftSelect(PendingIndex);
	}
	PendingIndex = INDEX_NONE; // 서버 확정 후 OnDraftChanged 가 슬롯 갱신
	RefreshStatus();
}

// ============================================================
// 갱신
// ============================================================
void UAOSBanPickWidget::RefreshCards()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS) return;
	const EAOSTeam Local = GetLocalTeam();
	const bool bMyTurn = !GS->IsDraftComplete() && (GS->GetActiveDraftTeam() == Local);

	for (int32 i = 0; i < CardImages.Num(); ++i)
	{
		// 이미지 틴트
		if (CardImages[i])
		{
			FLinearColor Tint(1.f, 1.f, 1.f, 1.f);
			if (GS->IsUnitBanned(i))
			{
				Tint = FLinearColor(0.30f, 0.10f, 0.10f, 1.f);  // 밴 = 어두운 빨강
			}
			else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team1))
			{
				Tint = FLinearColor(1.0f, 0.45f, 0.45f, 1.f);   // 팀1 픽 = 빨강
			}
			else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team2))
			{
				Tint = FLinearColor(0.45f, 0.65f, 1.0f, 1.f);   // 팀2 픽 = 파랑
			}
			else if (!bMyTurn)
			{
				Tint = FLinearColor(0.5f, 0.5f, 0.5f, 1.f);     // 내 턴 아님 = 흐리게
			}
			CardImages[i]->SetColorAndOpacity(Tint);
		}

		// 소프트 카드: 흰 채움 유지 + 상태별 보더(색/두께)로 표현
		if (CardBorders[i])
		{
			FLinearColor Outline = AOSUIStyle::BorderSoft; float Width = 1.f;       // 기본 = 소프트 보더
			if (i == PendingIndex)                               { Outline = FLinearColor(1.00f, 0.78f, 0.20f, 1.f); Width = 2.5f; } // 미리보기 = 골드 ring
			else if (GS->IsUnitBanned(i))                        { Outline = AOSUIStyle::BanRed;            Width = 2.f; }            // 밴
			else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team1)) { Outline = AOSUIStyle::TeamAccent(true);  Width = 2.f; }            // T1 픽 = 코랄
			else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team2)) { Outline = AOSUIStyle::TeamAccent(false); Width = 2.f; }            // T2 픽 = 블루
			CardBorders[i]->SetBrush(FSlateRoundedBoxBrush(AOSUIStyle::CardWhite, AOSUIStyle::CardRadius, Outline, Width));
		}
	}
}

void UAOSBanPickWidget::RefreshSlots()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS) return;

	// 현재 차례(활성 팀 + 밴/픽 단계의 다음 슬롯)에 글로우 하이라이트
	const bool bComplete = GS->IsDraftComplete();
	const EAOSTeam Active = GS->GetActiveDraftTeam();
	const bool bBanStep = GS->IsCurrentStepBan();

	auto FillTeam = [this](EAOSTeam Team, const TArray<int32>& Bans, const TArray<int32>& Picks,
		TArray<UImage*>& BanImgs, TArray<UImage*>& PickImgs, TArray<UTextBlock*>& PickNames,
		TArray<UBorder*>& PickBorders, int32 ActiveBanSlot, int32 ActivePickSlot)
	{
		const FLinearColor TC = TeamColor(Team);

		(void)ActiveBanSlot;   // 라이트 테마 밴 박스는 빈칸=✕ 비침 — 활성 슬롯 별도 강조 없음

		// 밴 슬롯 (빈칸 = 투명 → ✕ 비침 / 밴 = 초상화 회색조)
		for (int32 i = 0; i < BanImgs.Num(); ++i)
		{
			if (!BanImgs[i]) continue;
			if (Bans.IsValidIndex(i))
			{
				if (UTexture2D* P = GetPortrait(Bans[i])) BanImgs[i]->SetBrushFromTexture(P, false);
				else BanImgs[i]->SetBrush(FSlateColorBrush(PlaceholderColor(Bans[i])));
				BanImgs[i]->SetColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.58f, 1.f)); // 밴 = 회색조(불가)
			}
			else
			{
				BanImgs[i]->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));        // 빈칸 → 투명
			}
		}

		// 픽 슬롯
		for (int32 i = 0; i < PickImgs.Num(); ++i)
		{
			const bool bFilled = Picks.IsValidIndex(i);
			const bool bActive = (i == ActivePickSlot);

			if (PickImgs[i])
			{
				if (bFilled)
				{
					if (UTexture2D* P = GetPortrait(Picks[i])) PickImgs[i]->SetBrushFromTexture(P, false);
					else PickImgs[i]->SetBrush(FSlateColorBrush(PlaceholderColor(Picks[i])));
				}
				else
				{
					PickImgs[i]->SetBrush(FSlateColorBrush(kCardEmpty));
				}
				PickImgs[i]->SetColorAndOpacity(FLinearColor::White);
			}
			// 이름: 빈=CHOOSE HERO / 픽=챔피언명
			if (PickNames[i])
			{
				if (bFilled)
				{
					PickNames[i]->SetText(CachedRoster.IsValidIndex(Picks[i]) ? CachedRoster[Picks[i]].DisplayName : FText::FromString(FString::FromInt(Picks[i])));
					PickNames[i]->SetColorAndOpacity(FSlateColor(kTextDark));
				}
				else
				{
					PickNames[i]->SetText(FText::FromString(TEXT("CHOOSE HERO")));
					PickNames[i]->SetColorAndOpacity(FSlateColor(bActive ? TeamColor(Team) : kTextGray));
				}
			}
			// 슬롯 테두리: 둥근 흰 카드 유지 + 상태별 보더(활성=골드 / 채워짐=팀 파스텔 / 빈=소프트)
			if (PickBorders.IsValidIndex(i) && PickBorders[i])
			{
				const FLinearColor Outline = bActive ? FLinearColor(1.0f, 0.78f, 0.20f, 1.f)
					: bFilled ? AOSUIStyle::TeamAccent(Team == EAOSTeam::Team1)
					          : AOSUIStyle::BorderSoft;
				const float Width = bActive ? 2.5f : bFilled ? 2.f : 1.f;
				PickBorders[i]->SetBrush(FSlateRoundedBoxBrush(AOSUIStyle::CardWhite, AOSUIStyle::SlotRadius, Outline, Width));
			}
		}
	};

	auto ActiveBan  = [&](EAOSTeam T, const TArray<int32>& B) { return (!bComplete && Active == T &&  bBanStep) ? B.Num() : -1; };
	auto ActivePick = [&](EAOSTeam T, const TArray<int32>& P) { return (!bComplete && Active == T && !bBanStep) ? P.Num() : -1; };

	FillTeam(EAOSTeam::Team1, GS->Team1BannedUnitIds, GS->Team1PickedUnitIds,
		Team1BanImages, Team1PickImages, Team1PickNames, Team1PickBorders,
		ActiveBan(EAOSTeam::Team1, GS->Team1BannedUnitIds), ActivePick(EAOSTeam::Team1, GS->Team1PickedUnitIds));
	FillTeam(EAOSTeam::Team2, GS->Team2BannedUnitIds, GS->Team2PickedUnitIds,
		Team2BanImages, Team2PickImages, Team2PickNames, Team2PickBorders,
		ActiveBan(EAOSTeam::Team2, GS->Team2BannedUnitIds), ActivePick(EAOSTeam::Team2, GS->Team2PickedUnitIds));

	// 플레이어 이름
	if (Team1PlayerNameText) Team1PlayerNameText->SetText(FText::FromString(GetPlayerNameForTeam(EAOSTeam::Team1)));
	if (Team2PlayerNameText) Team2PlayerNameText->SetText(FText::FromString(GetPlayerNameForTeam(EAOSTeam::Team2)));
}

void UAOSBanPickWidget::RefreshStatus()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS) return;

	const bool bComplete = GS->IsDraftComplete();
	const EAOSTeam Local = GetLocalTeam();
	const bool bMyTurn = !bComplete && (GS->GetActiveDraftTeam() == Local);

	// 타이머 (5초 이하 빨강 긴박감)
	if (TimerText)
	{
		const int32 Secs = FMath::Max(0, FMath::CeilToInt(GS->DraftTurnTimeRemaining));
		TimerText->SetText(bComplete ? FText::FromString(TEXT("--")) : FText::FromString(FString::FromInt(Secs)));
		TimerText->SetColorAndOpacity(FSlateColor(
			(!bComplete && Secs <= 5) ? kBanRed : kTextDark));
	}

	// 상태 텍스트
	if (StatusText)
	{
		if (bComplete)
		{
			StatusText->SetText(FText::FromString(TEXT("드래프트 완료 — 라운드 준비로 이동합니다")));
			StatusText->SetColorAndOpacity(FSlateColor(kTextDark));
		}
		else
		{
			const int32 ActiveTeam = static_cast<int32>(GS->GetActiveDraftTeam()) + 1;
			const FString Phase = GS->IsCurrentStepBan() ? TEXT("밴") : TEXT("픽");
			StatusText->SetText(FText::FromString(FString::Printf(
				TEXT("팀%d %s 차례%s"), ActiveTeam, *Phase,
				bMyTurn ? TEXT("  ← 당신의 차례! 챔피언 선택 후 확정") : TEXT(""))));
			StatusText->SetColorAndOpacity(FSlateColor(bMyTurn
				? TeamColor(Local)
				: kTextGray));
		}
	}

	// 확정 버튼: 내 턴 + 미리보기 유효 시만 활성 (활성=블루 / 비활성=라이트 그레이)
	if (ConfirmButton)
	{
		const bool bCanConfirm = bMyTurn && (PendingIndex != INDEX_NONE) && GS->IsUnitAvailableForDraft(PendingIndex);
		ConfirmButton->SetIsEnabled(bCanConfirm);
		ConfirmButton->SetBackgroundColor(bCanConfirm ? kLockInBlue : kLockInIdle);
	}
	if (ConfirmText)
	{
		FString Label = TEXT("LOCK IN");
		if (!bComplete && bMyTurn && CachedRoster.IsValidIndex(PendingIndex))
		{
			Label = FString::Printf(TEXT("LOCK IN: %s"), *CachedRoster[PendingIndex].DisplayName.ToString());
		}
		ConfirmText->SetText(FText::FromString(Label));
	}
}
