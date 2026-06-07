#include "AOSBanPickWidget.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"
#include "AOSPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
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
#include "Styling/CoreStyle.h"

namespace
{
	// 위젯이 자동로드할 텍스처 경로 (asset-gen 이 이 경로에 생성). 없으면 솔리드 폴백.
	const TCHAR* kBackdropPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_Backdrop.T_BanPick_Backdrop");
	// 카드 장식 프레임(9-slice). 없으면 컬러 림 폴백.
	const TCHAR* kCardFramePath = TEXT("/Game/AOS/UI/Assets/T_BanPick_CardFrame.T_BanPick_CardFrame");

	// 팀 패널/구분선 색 (깊이감)
	FLinearColor PanelColor(EAOSTeam Team)
	{
		return (Team == EAOSTeam::Team1)
			? FLinearColor(0.18f, 0.05f, 0.06f, 0.45f)   // 팀1 = 어두운 레드 반투명
			: FLinearColor(0.05f, 0.09f, 0.18f, 0.45f);  // 팀2 = 어두운 블루 반투명
	}

	FSlateFontInfo MakeFont(int32 Size)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", Size);
		return F;
	}

	// 초상화 없는(플레이스홀더) 유닛용 고유 컬러 타일 — 인덱스로 황금비 색상 분산.
	FLinearColor PlaceholderColor(int32 Index)
	{
		const float Hue01 = FMath::Frac(static_cast<float>(Index) * 0.61803398875f);
		return FLinearColor::MakeFromHSV8(
			static_cast<uint8>(Hue01 * 255.f), /*S*/ 130, /*V*/ 135);
	}
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
	return (Team == EAOSTeam::Team1)
		? FLinearColor(0.85f, 0.27f, 0.27f, 1.f)   // 팀1 = 레드
		: FLinearColor(0.30f, 0.55f, 0.95f, 1.f);  // 팀2 = 블루
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
// UI 빌드
// ============================================================
void UAOSBanPickWidget::BuildUI()
{
	if (bBuilt || !WidgetTree) return;
	bBuilt = true;

	// 루트: 전체화면 오버레이
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BPRoot"));
	WidgetTree->RootWidget = RootOverlay;

	// [0] 배경 이미지 (전체화면, Visible → 카드 클릭 히트테스트 버블링 + 모달 차단)
	BackdropImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BPBackdrop"));
	if (UTexture2D* BgTex = TryLoadTexture(kBackdropPath))
	{
		BackdropImage->SetBrushFromTexture(BgTex, false);
	}
	else
	{
		BackdropImage->SetBrush(FSlateColorBrush(FLinearColor(0.02f, 0.02f, 0.05f, 0.97f)));
	}
	BackdropImage->SetVisibility(ESlateVisibility::Visible);
	if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(BackdropImage))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	// [1] 메인 세로 박스
	UVerticalBox* MainVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPMain"));
	if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(MainVB))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
		OS->SetPadding(FMargin(28.f, 18.f, 28.f, 18.f));
	}

	// ---- 상단바 ----
	UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BPTop"));
	if (UVerticalBoxSlot* VS = MainVB->AddChildToVerticalBox(TopBar))
	{
		VS->SetHorizontalAlignment(HAlign_Fill);
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	// 상단바/본문 구분선 (옅은 골드 라인)
	{
		USizeBox* DivBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BPDivBox"));
		DivBox->SetHeightOverride(2.f);
		UBorder* DivLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPDivLine"));
		DivLine->SetBrushColor(FLinearColor(0.55f, 0.48f, 0.30f, 0.55f));
		DivBox->SetContent(DivLine);
		if (UVerticalBoxSlot* VS = MainVB->AddChildToVerticalBox(DivBox))
		{
			VS->SetHorizontalAlignment(HAlign_Fill);
			VS->SetPadding(FMargin(40.f, 0.f, 40.f, 12.f));
		}
	}

	// 좌: 팀1 플레이어명 + 밴 행
	{
		UVerticalBox* L = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Team1PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPT1Name"));
		Team1PlayerNameText->SetText(FText::FromString(TEXT("Player 1")));
		Team1PlayerNameText->SetFont(MakeFont(18));
		Team1PlayerNameText->SetColorAndOpacity(FSlateColor(TeamColor(EAOSTeam::Team1)));
		L->AddChildToVerticalBox(Team1PlayerNameText);
		L->AddChildToVerticalBox(BuildBanRow(EAOSTeam::Team1));
		if (UHorizontalBoxSlot* HS = TopBar->AddChildToHorizontalBox(L))
		{
			HS->SetHorizontalAlignment(HAlign_Left);
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	// 중앙: 제목 + 타이머
	{
		UVerticalBox* C = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPTitle"));
		TitleText->SetText(FText::FromString(TEXT("PICK & BAN")));
		{
			FSlateFontInfo TitleFont = MakeFont(34);
			TitleFont.OutlineSettings.OutlineSize = 1;
			TitleFont.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
			TitleText->SetFont(TitleFont);
		}
		TitleText->SetJustification(ETextJustify::Center);
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.88f, 0.62f, 1.f))); // 골드
		if (UVerticalBoxSlot* S = C->AddChildToVerticalBox(TitleText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPTimer"));
		TimerText->SetText(FText::FromString(TEXT("--")));
		{
			FSlateFontInfo TimerFont = MakeFont(40);
			TimerFont.OutlineSettings.OutlineSize = 1;
			TimerFont.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
			TimerText->SetFont(TimerFont);
		}
		TimerText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = C->AddChildToVerticalBox(TimerText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		if (UHorizontalBoxSlot* HS = TopBar->AddChildToHorizontalBox(C))
		{
			HS->SetHorizontalAlignment(HAlign_Center);
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	// 우: 팀2 플레이어명 + 밴 행
	{
		UVerticalBox* R = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Team2PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPT2Name"));
		Team2PlayerNameText->SetText(FText::FromString(TEXT("Player 2")));
		Team2PlayerNameText->SetFont(MakeFont(18));
		Team2PlayerNameText->SetJustification(ETextJustify::Right);
		Team2PlayerNameText->SetColorAndOpacity(FSlateColor(TeamColor(EAOSTeam::Team2)));
		if (UVerticalBoxSlot* S = R->AddChildToVerticalBox(Team2PlayerNameText))
		{
			S->SetHorizontalAlignment(HAlign_Right);
		}
		if (UVerticalBoxSlot* S = R->AddChildToVerticalBox(BuildBanRow(EAOSTeam::Team2)))
		{
			S->SetHorizontalAlignment(HAlign_Right);
		}
		if (UHorizontalBoxSlot* HS = TopBar->AddChildToHorizontalBox(R))
		{
			HS->SetHorizontalAlignment(HAlign_Right);
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	// ---- 중앙 행: 좌 픽 / 중앙 그리드 / 우 픽 ----
	UHorizontalBox* MidHB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BPMid"));
	if (UVerticalBoxSlot* VS = MainVB->AddChildToVerticalBox(MidHB))
	{
		VS->SetHorizontalAlignment(HAlign_Fill);
		VS->SetVerticalAlignment(VAlign_Fill);
		VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 좌 팀1 픽 컬럼 (반투명 팀 패널로 감싸 깊이감)
	{
		UBorder* T1Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPT1Panel"));
		T1Panel->SetBrushColor(PanelColor(EAOSTeam::Team1));
		T1Panel->SetPadding(FMargin(10.f));
		T1Panel->SetContent(BuildPickColumn(EAOSTeam::Team1));
		if (UHorizontalBoxSlot* HS = MidHB->AddChildToHorizontalBox(T1Panel))
		{
			HS->SetVerticalAlignment(VAlign_Fill);   // 전체 높이로 패널 확장
			HS->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}
	}

	// 중앙 패널
	{
		UVerticalBox* CenterVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPCenter"));

		UTextBlock* SelectLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPSelectLabel"));
		SelectLabel->SetText(FText::FromString(TEXT("CHAMPION SELECT")));
		SelectLabel->SetFont(MakeFont(18));
		SelectLabel->SetJustification(ETextJustify::Center);
		SelectLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.82f, 1.f)));
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(SelectLabel))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}

		// 그리드 위 신축 여백 (수직 중앙 정렬용)
		{
			USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(TopSpacer))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}

		CardGrid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("BPGrid"));
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(CardGrid))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 그리드 아래 신축 여백
		{
			USpacer* BotSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(BotSpacer))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPStatus"));
		StatusText->SetText(FText::FromString(TEXT("드래프트 준비 중...")));
		StatusText->SetFont(MakeFont(16));
		StatusText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(StatusText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));
		}

		// 확정 버튼
		ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BPConfirm"));
		ConfirmButton->OnClicked.AddDynamic(this, &UAOSBanPickWidget::OnConfirmClicked);
		ConfirmButton->SetBackgroundColor(FLinearColor(0.30f, 0.30f, 0.33f, 1.f)); // 기본(차례 아님)
		ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPConfirmText"));
		ConfirmText->SetText(FText::FromString(TEXT("확정  (CONFIRM)")));
		ConfirmText->SetFont(MakeFont(18));
		ConfirmText->SetJustification(ETextJustify::Center);
		ConfirmButton->SetContent(ConfirmText);
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(ConfirmButton))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}

		if (UHorizontalBoxSlot* HS = MidHB->AddChildToHorizontalBox(CenterVB))
		{
			HS->SetHorizontalAlignment(HAlign_Fill);
			HS->SetVerticalAlignment(VAlign_Fill);
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	// 우 팀2 픽 컬럼 (반투명 팀 패널)
	{
		UBorder* T2Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPT2Panel"));
		T2Panel->SetBrushColor(PanelColor(EAOSTeam::Team2));
		T2Panel->SetPadding(FMargin(10.f));
		T2Panel->SetContent(BuildPickColumn(EAOSTeam::Team2));
		if (UHorizontalBoxSlot* HS = MidHB->AddChildToHorizontalBox(T2Panel))
		{
			HS->SetVerticalAlignment(VAlign_Fill);   // 전체 높이로 패널 확장
			HS->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
	}
}

UHorizontalBox* UAOSBanPickWidget::BuildBanRow(EAOSTeam Team)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TArray<UImage*>& BanImages = (Team == EAOSTeam::Team1) ? Team1BanImages : Team2BanImages;
	BanImages.Reset();

	for (int32 i = 0; i < BansPerTeam; ++i)
	{
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.08f, 1.f));
		Frame->SetPadding(FMargin(2.f));

		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrush(FSlateColorBrush(FLinearColor(0.12f, 0.10f, 0.10f, 1.f)));
		Img->SetDesiredSizeOverride(FVector2D(38.f, 38.f));
		Frame->SetContent(Img);
		BanImages.Add(Img);

		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Frame))
		{
			HS->SetPadding(FMargin(3.f, 4.f, 3.f, 0.f));
		}
	}
	return Row;
}

UVerticalBox* UAOSBanPickWidget::BuildPickColumn(EAOSTeam Team)
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	TArray<UBorder*>& Borders = (Team == EAOSTeam::Team1) ? Team1PickBorders : Team2PickBorders;
	TArray<UImage*>& Images = (Team == EAOSTeam::Team1) ? Team1PickImages : Team2PickImages;
	TArray<UTextBlock*>& Names = (Team == EAOSTeam::Team1) ? Team1PickNames : Team2PickNames;
	Borders.Reset(); Images.Reset(); Names.Reset();

	const FLinearColor TC = TeamColor(Team);

	for (int32 i = 0; i < PicksPerTeam; ++i)
	{
		UBorder* PickSlot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PickSlot->SetBrushColor(FLinearColor(TC.R * 0.45f, TC.G * 0.45f, TC.B * 0.45f, 0.95f)); // 슬롯 팀색 프레임
		PickSlot->SetPadding(FMargin(4.f));

		UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		PickSlot->SetContent(Ov);

		// 초상화: 슬롯 전체 채움 (빈 칸도 색으로 꽉 참 — 작은 사각형 떠보임 해소)
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrush(FSlateColorBrush(FLinearColor(0.08f, 0.08f, 0.11f, 1.f)));
		if (UOverlaySlot* OS = Ov->AddChildToOverlay(Img))
		{
			OS->SetHorizontalAlignment(HAlign_Fill);
			OS->SetVerticalAlignment(VAlign_Fill);
		}

		// 이름: 하단 중앙 (초상화 위에 외곽선으로 가독성)
		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(FText::FromString(TEXT("")));
		{
			FSlateFontInfo NF = MakeFont(15);
			NF.OutlineSettings.OutlineSize = 1;
			NF.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
			Name->SetFont(NF);
		}
		Name->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* OS = Ov->AddChildToOverlay(Name))
		{
			OS->SetHorizontalAlignment(HAlign_Center);
			OS->SetVerticalAlignment(VAlign_Bottom);
			OS->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		// 폭 고정 SizeBox 로 감싸 패널 collapse 방지 (초상화가 폭 없는 컬러 브러시일 때 대비), 높이는 Fill 균등 분배
		USizeBox* SlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotBox->SetWidthOverride(108.f);
		SlotBox->SetContent(PickSlot);
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(SlotBox))
		{
			VS->SetHorizontalAlignment(HAlign_Center);
			VS->SetVerticalAlignment(VAlign_Fill);
			VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));  // 5칸 균등 분배(패널 높이 채움)
			VS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}

		Borders.Add(PickSlot);
		Images.Add(Img);
		Names.Add(Name);
	}
	return Col;
}

// ============================================================
// 로스터 주입 → 그리드 카드 생성
// ============================================================
void UAOSBanPickWidget::InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster)
{
	CachedRoster = Roster;
	BuildUI();
	if (!CardGrid) return;

	CardGrid->ClearChildren();
	CardBorders.Reset();
	CardImages.Reset();

	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("BPCard_%d"), i));
		Card->SetVisibility(ESlateVisibility::HitTestInvisible); // 클릭은 루트가 히트테스트로 처리

		// 장식 프레임: 텍스처 있으면 9-slice(Box), 없으면 금속 컬러 림 폴백.
		// 어느 쪽이든 RefreshCards 가 SetBrushColor 로 상태별 틴트(골드/팀색).
		if (UTexture2D* FrameTex = TryLoadTexture(kCardFramePath))
		{
			FSlateBrush FB;
			FB.SetResourceObject(FrameTex);
			FB.DrawAs = ESlateBrushDrawType::Box;
			FB.ImageSize = FVector2D(32.f, 32.f);       // 렌더 테두리 = Margin*ImageSize ≈ 4px
			FB.Margin = FMargin(0.125f);                // 새 텍스처 테두리 12.5%(64px 텍스처의 8px) 와 일치
			Card->SetBrush(FB);
			Card->SetPadding(FMargin(4.f));             // 초상화 인셋 = 프레임 두께와 정렬
		}
		else
		{
			Card->SetBrushColor(FLinearColor(0.55f, 0.50f, 0.38f, 1.f)); // 금속 림 폴백
			Card->SetPadding(FMargin(5.f));
		}

		// 고정 크기 박스 — 초상화 유무와 무관하게 동일한 카드/클릭 영역 보장
		// (SetDesiredSizeOverride 는 컬러 브러시에서 안정적이지 않아 SizeBox 로 강제)
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(64.f);
		CardSize->SetHeightOverride(64.f);
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

		if (UWrapBoxSlot* WS = CardGrid->AddChildToWrapBox(Card))
		{
			WS->SetPadding(FMargin(4.f));
		}

		CardBorders.Add(Card);
		CardImages.Add(Img);
	}

	PendingIndex = INDEX_NONE;
	RefreshCards();
	RefreshSlots();
	RefreshStatus();
}

// ============================================================
// 라이프사이클
// ============================================================
void UAOSBanPickWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildUI();
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
	// 타이머 표시 갱신 (NativeTick 미호출 시엔 DraftTurnTimeRemaining 의 OnRep_Draft 가 RefreshStatus 트리거)
	RefreshStatus();
}

FReply UAOSBanPickWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Abs = InMouseEvent.GetScreenSpacePosition();
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

		// 프레임 틴트: 상태별 (텍스처 프레임/컬러 림 공통)
		if (CardBorders[i])
		{
			FLinearColor FrameTint(0.60f, 0.56f, 0.45f, 1.f);                      // 기본 = 금속 톤
			if (i == PendingIndex)                               FrameTint = FLinearColor(1.00f, 0.84f, 0.38f, 1.f); // 미리보기 = 밝은 골드
			else if (GS->IsUnitBanned(i))                        FrameTint = FLinearColor(0.42f, 0.16f, 0.16f, 1.f); // 밴 = 암적
			else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team1)) FrameTint = FLinearColor(0.95f, 0.35f, 0.35f, 1.f); // T1 픽 = 적
			else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team2)) FrameTint = FLinearColor(0.40f, 0.60f, 1.00f, 1.f); // T2 픽 = 청
			CardBorders[i]->SetBrushColor(FrameTint);
		}
	}
}

void UAOSBanPickWidget::RefreshSlots()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS) return;

	auto FillTeam = [this](const TArray<int32>& Bans, const TArray<int32>& Picks,
		TArray<UImage*>& BanImgs, TArray<UImage*>& PickImgs, TArray<UTextBlock*>& PickNames)
	{
		// 밴 슬롯
		for (int32 i = 0; i < BanImgs.Num(); ++i)
		{
			if (!BanImgs[i]) continue;
			if (Bans.IsValidIndex(i))
			{
				if (UTexture2D* P = GetPortrait(Bans[i]))
				{
					BanImgs[i]->SetBrushFromTexture(P, false);
				}
				else
				{
					BanImgs[i]->SetBrush(FSlateColorBrush(FLinearColor(0.25f, 0.10f, 0.10f, 1.f)));
				}
				BanImgs[i]->SetColorAndOpacity(FLinearColor(0.45f, 0.30f, 0.30f, 1.f)); // 밴 = 어둡게
				BanImgs[i]->SetDesiredSizeOverride(FVector2D(38.f, 38.f));
			}
			else
			{
				BanImgs[i]->SetBrush(FSlateColorBrush(FLinearColor(0.12f, 0.10f, 0.10f, 1.f)));
				BanImgs[i]->SetColorAndOpacity(FLinearColor::White);
				BanImgs[i]->SetDesiredSizeOverride(FVector2D(38.f, 38.f));
			}
		}

		// 픽 슬롯
		for (int32 i = 0; i < PickImgs.Num(); ++i)
		{
			const bool bFilled = Picks.IsValidIndex(i);
			if (PickImgs[i])
			{
				if (bFilled)
				{
					if (UTexture2D* P = GetPortrait(Picks[i]))
					{
						PickImgs[i]->SetBrushFromTexture(P, false);
					}
					else
					{
						PickImgs[i]->SetBrush(FSlateColorBrush(PlaceholderColor(Picks[i])));  // 컬러 타일
					}
				}
				else
				{
					PickImgs[i]->SetBrush(FSlateColorBrush(FLinearColor(0.10f, 0.10f, 0.13f, 1.f)));
				}
				PickImgs[i]->SetColorAndOpacity(FLinearColor::White);
			}
			if (PickNames[i])
			{
				PickNames[i]->SetText(bFilled
					? (CachedRoster.IsValidIndex(Picks[i]) ? CachedRoster[Picks[i]].DisplayName : FText::FromString(FString::FromInt(Picks[i])))
					: FText::GetEmpty());
			}
		}
	};

	FillTeam(GS->Team1BannedUnitIds, GS->Team1PickedUnitIds, Team1BanImages, Team1PickImages, Team1PickNames);
	FillTeam(GS->Team2BannedUnitIds, GS->Team2PickedUnitIds, Team2BanImages, Team2PickImages, Team2PickNames);

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
			(!bComplete && Secs <= 5) ? FLinearColor(1.0f, 0.30f, 0.30f, 1.f) : FLinearColor(0.95f, 0.95f, 0.95f, 1.f)));
	}

	// 상태 텍스트
	if (StatusText)
	{
		if (bComplete)
		{
			StatusText->SetText(FText::FromString(TEXT("드래프트 완료 — 라운드 준비로 이동합니다")));
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
				: FLinearColor(0.7f, 0.7f, 0.7f, 1.f)));
		}
	}

	// 확정 버튼: 내 턴 + 미리보기 유효 시만 활성
	if (ConfirmButton)
	{
		const bool bCanConfirm = bMyTurn && (PendingIndex != INDEX_NONE) && GS->IsUnitAvailableForDraft(PendingIndex);
		ConfirmButton->SetIsEnabled(bCanConfirm);
		ConfirmButton->SetBackgroundColor(bCanConfirm
			? TeamColor(Local)
			: FLinearColor(0.28f, 0.28f, 0.31f, 1.f));
	}
	if (ConfirmText)
	{
		FString Label = TEXT("확정  (CONFIRM)");
		if (!bComplete && bMyTurn && CachedRoster.IsValidIndex(PendingIndex))
		{
			const FString Phase = GS->IsCurrentStepBan() ? TEXT("밴") : TEXT("픽");
			Label = FString::Printf(TEXT("%s 확정: %s"), *Phase, *CachedRoster[PendingIndex].DisplayName.ToString());
		}
		ConfirmText->SetText(FText::FromString(Label));
	}
}
