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
	// ※ WBP_BanPick 사용 시엔 디자이너가 BackdropImage 브러시를 직접 지정 → 이 경로는 폴백 전용.
	const TCHAR* kBackdropPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_Backdrop.T_BanPick_Backdrop");
	// 카드 장식 프레임(9-slice). 없으면 컬러 림 폴백.
	const TCHAR* kCardFramePath = TEXT("/Game/AOS/UI/Assets/T_BanPick_CardFrame.T_BanPick_CardFrame");
	// 중앙 비네팅 글로우(그리드 뒤). 없으면 skip.
	const TCHAR* kCenterGlowPath = TEXT("/Game/AOS/UI/Assets/T_BanPick_CenterGlow.T_BanPick_CenterGlow");

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

	// 루트: 전체화면 오버레이
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BPRoot"));
	WidgetTree->RootWidget = RootOverlay;

	// [0] 배경 이미지 (전체화면, Visible → 카드 클릭 히트테스트 버블링 + 모달 차단)
	BackdropImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackdropImage"));
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

	// [0.5] 중앙 비네팅 글로우 (그리드 뒤 살짝 밝게 — 시선 집중). 텍스처 없으면 skip.
	if (UTexture2D* GlowTex = TryLoadTexture(kCenterGlowPath))
	{
		UImage* CenterGlow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BPCenterGlow"));
		CenterGlow->SetBrushFromTexture(GlowTex, false);
		CenterGlow->SetVisibility(ESlateVisibility::HitTestInvisible);   // 클릭 통과
		CenterGlow->SetDesiredSizeOverride(FVector2D(1120.f, 800.f));
		if (UOverlaySlot* OS = RootOverlay->AddChildToOverlay(CenterGlow))
		{
			OS->SetHorizontalAlignment(HAlign_Center);
			OS->SetVerticalAlignment(VAlign_Center);
		}
	}

	// ── 상단 중앙: 제목 + 타이머 + CHAMPION SELECT (레퍼런스 상단 중앙) ──
	{
		UVerticalBox* TopVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPTopVB"));

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
		TitleText->SetText(FText::FromString(TEXT("PICK & BAN")));
		{
			FSlateFontInfo F = MakeFont(34);
			F.OutlineSettings.OutlineSize = 1;
			F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
			TitleText->SetFont(F);
		}
		TitleText->SetJustification(ETextJustify::Center);
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.88f, 0.62f, 1.f))); // 골드
		if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

		TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
		TimerText->SetText(FText::FromString(TEXT("--")));
		{
			FSlateFontInfo F = MakeFont(40);
			F.OutlineSettings.OutlineSize = 1;
			F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
			TimerText->SetFont(F);
		}
		TimerText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(TimerText)) S->SetHorizontalAlignment(HAlign_Center);

		UTextBlock* SelectLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPSelectLabel"));
		SelectLabel->SetText(FText::FromString(TEXT("CHAMPION SELECT")));
		SelectLabel->SetFont(MakeFont(16));
		SelectLabel->SetJustification(ETextJustify::Center);
		SelectLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.82f, 1.f)));
		if (UVerticalBoxSlot* S = TopVB->AddChildToVerticalBox(SelectLabel)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f)); }

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
		// 챔피언 풀 패널: 얇은 골드 림 + 어두운 반투명 바탕
		UBorder* GridFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPGridFrame"));
		GridFrame->SetBrushColor(FLinearColor(0.50f, 0.42f, 0.24f, 0.55f));   // 골드 림
		GridFrame->SetPadding(FMargin(2.f));
		UBorder* GridPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPGridPanel"));
		GridPanel->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.55f));
		GridPanel->SetPadding(FMargin(16.f, 14.f, 16.f, 14.f));
		GridPanel->SetContent(CardGrid);
		GridFrame->SetContent(GridPanel);
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(GridFrame))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetText(FText::FromString(TEXT("드래프트 준비 중...")));
		StatusText->SetFont(MakeFont(16));
		StatusText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(StatusText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 12.f, 0.f, 8.f));
		}

		// LOCK IN (확정) 버튼 — 크게, 중앙 (OnClicked 바인딩은 InitializeWithRoster 에서)
		ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
		ConfirmButton->SetBackgroundColor(FLinearColor(0.30f, 0.30f, 0.33f, 1.f)); // 기본(차례 아님)
		ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmText"));
		ConfirmText->SetText(FText::FromString(TEXT("LOCK IN")));
		ConfirmText->SetFont(MakeFont(22));
		ConfirmText->SetJustification(ETextJustify::Center);
		ConfirmButton->SetContent(ConfirmText);
		if (UVerticalBoxSlot* S = CenterVB->AddChildToVerticalBox(ConfirmButton))
		{
			S->SetHorizontalAlignment(HAlign_Center);
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
			BanLabel->SetText(FText::FromString(TEXT("BAN")));
			BanLabel->SetFont(MakeFont(12));
			BanLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.5f, 0.5f, 1.f)));
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
			BanLabel->SetText(FText::FromString(TEXT("BAN")));
			BanLabel->SetFont(MakeFont(12));
			BanLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.5f, 0.5f, 1.f)));
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

	for (int32 i = 0; i < PicksPerTeam; ++i)
	{
		UBorder* PickSlot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PickSlot->SetBrushColor(FLinearColor(TC.R * 0.45f, TC.G * 0.45f, TC.B * 0.45f, 0.95f)); // 슬롯 팀색 프레임
		PickSlot->SetPadding(FMargin(3.f));

		UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		PickSlot->SetContent(Ov);

		// 초상화: 슬롯 전체 채움 (빈 칸도 색으로 꽉 참)
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrush(FSlateColorBrush(FLinearColor(0.08f, 0.08f, 0.11f, 1.f)));
		if (UOverlaySlot* OS = Ov->AddChildToOverlay(Img))
		{
			OS->SetHorizontalAlignment(HAlign_Fill);
			OS->SetVerticalAlignment(VAlign_Fill);
		}

		// 이름/슬롯번호: 하단 중앙 (초상화 위에 외곽선으로 가독성)
		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(FText::FromString(TEXT("")));
		{
			FSlateFontInfo NF = MakeFont(13);
			NF.OutlineSettings.OutlineSize = 1;
			NF.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
			Name->SetFont(NF);
		}
		Name->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* OS = Ov->AddChildToOverlay(Name))
		{
			OS->SetHorizontalAlignment(HAlign_Center);
			OS->SetVerticalAlignment(VAlign_Bottom);
			OS->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
		}

		// 세로 카드(레퍼런스): 폭·높이 고정 SizeBox 로 감싸 가로 행에 5칸 배치
		USizeBox* SlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotBox->SetWidthOverride(92.f);
		SlotBox->SetHeightOverride(132.f);
		SlotBox->SetContent(PickSlot);
		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(SlotBox))
		{
			HS->SetVerticalAlignment(VAlign_Top);
			HS->SetPadding(FMargin(3.f, 0.f, 3.f, 0.f));
		}

		Borders.Add(PickSlot);
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

	// 내 쪽(우하단): 미리보기 중이면 그 챔피언, 아니면 내 팀 최신 픽
	int32 MyUnit = INDEX_NONE;
	if (PendingIndex != INDEX_NONE)
	{
		MyUnit = PendingIndex;
	}
	else
	{
		const TArray<int32>& MyPicks = GS->GetPickedUnits(Local);
		if (MyPicks.Num() > 0) MyUnit = MyPicks.Last();
	}

	// 상대 쪽(좌상단): 상대 팀 최신 픽
	int32 EnemyUnit = INDEX_NONE;
	{
		const TArray<int32>& EnemyPicks = GS->GetPickedUnits(Enemy);
		if (EnemyPicks.Num() > 0) EnemyUnit = EnemyPicks.Last();
	}

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

	// 현재 차례(활성 팀 + 밴/픽 단계의 다음 슬롯)에 글로우 하이라이트
	const bool bComplete = GS->IsDraftComplete();
	const EAOSTeam Active = GS->GetActiveDraftTeam();
	const bool bBanStep = GS->IsCurrentStepBan();
	const FLinearColor GlowGold(1.0f, 0.85f, 0.40f, 1.f);

	auto FillTeam = [this, GlowGold](EAOSTeam Team, const TArray<int32>& Bans, const TArray<int32>& Picks,
		TArray<UImage*>& BanImgs, TArray<UImage*>& PickImgs, TArray<UTextBlock*>& PickNames,
		TArray<UBorder*>& PickBorders, int32 ActiveBanSlot, int32 ActivePickSlot)
	{
		const FLinearColor TC = TeamColor(Team);

		// 밴 슬롯
		for (int32 i = 0; i < BanImgs.Num(); ++i)
		{
			if (!BanImgs[i]) continue;
			if (Bans.IsValidIndex(i))
			{
				if (UTexture2D* P = GetPortrait(Bans[i])) BanImgs[i]->SetBrushFromTexture(P, false);
				else BanImgs[i]->SetBrush(FSlateColorBrush(FLinearColor(0.25f, 0.10f, 0.10f, 1.f)));
				BanImgs[i]->SetColorAndOpacity(FLinearColor(0.45f, 0.30f, 0.30f, 1.f)); // 밴 = 어둡게
			}
			else if (i == ActiveBanSlot)
			{
				BanImgs[i]->SetBrush(FSlateColorBrush(FLinearColor(0.28f, 0.14f, 0.14f, 1.f)));
				BanImgs[i]->SetColorAndOpacity(GlowGold);                          // 활성 밴 = 골드 글로우
			}
			else
			{
				BanImgs[i]->SetBrush(FSlateColorBrush(FLinearColor(0.12f, 0.10f, 0.10f, 1.f)));
				BanImgs[i]->SetColorAndOpacity(FLinearColor::White);
			}
			BanImgs[i]->SetDesiredSizeOverride(FVector2D(38.f, 38.f));
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
					PickImgs[i]->SetColorAndOpacity(FLinearColor::White);
				}
				else
				{
					PickImgs[i]->SetBrush(FSlateColorBrush(FLinearColor(0.09f, 0.09f, 0.12f, 1.f)));
					PickImgs[i]->SetColorAndOpacity(bActive
						? FLinearColor(0.85f, 0.85f, 0.90f, 1.f) : FLinearColor(0.50f, 0.50f, 0.55f, 1.f));
				}
			}
			// 이름/상태 (빈 슬롯도 채워보이게)
			if (PickNames[i])
			{
				if (bFilled)
					PickNames[i]->SetText(CachedRoster.IsValidIndex(Picks[i]) ? CachedRoster[Picks[i]].DisplayName : FText::FromString(FString::FromInt(Picks[i])));
				else if (bActive)
					PickNames[i]->SetText(FText::FromString(TEXT("픽 중...")));
				else
					PickNames[i]->SetText(FText::FromString(FString::Printf(TEXT("픽 %d"), i + 1)));
			}
			// 슬롯 프레임 색: 활성=골드 / 채워짐=팀색 진하게 / 빈=팀 기본
			if (PickBorders.IsValidIndex(i) && PickBorders[i])
			{
				FLinearColor BC = bActive ? GlowGold
					: bFilled ? FLinearColor(TC.R * 0.70f, TC.G * 0.70f, TC.B * 0.70f, 1.f)
					          : FLinearColor(TC.R * 0.40f, TC.G * 0.40f, TC.B * 0.40f, 0.9f);
				PickBorders[i]->SetBrushColor(BC);
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
