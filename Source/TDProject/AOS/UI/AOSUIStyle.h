// 공유 UI 디자인 토큰 — 화이트+파스텔(A+B 블렌드). 벤픽/라운드 준비/상점 위젯이 공유한다.
//
// ⚠️ 헤더-only(inline) 설계: 모든 토큰/헬퍼가 inline 이라 단일 정의 → 유니티/Jumbo 빌드에서
//    심볼 충돌이 원천적으로 없다. (과거: 각 위젯 .cpp 익명 네임스페이스에 팔레트 복붙 → CS-접두로
//    충돌 회피하던 잔재를 근본 해소.) 색/폰트 신규 상수는 전부 여기에 모으고, 위젯 .cpp 에 다시
//    흩뿌리지 않는다.
//
// 팀색은 게임 타입 의존을 피하려 bool bTeam1 로 받는다 (호출부: Team == EAOSTeam::Team1).
#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/UObjectGlobals.h"

namespace AOSUIStyle
{
	// ── 팔레트 (화이트 베이스 + 파스텔) ──
	inline const FLinearColor BgBase     = FLinearColor(0.957f, 0.965f, 0.973f, 1.f); // 전체 배경(near-white)
	inline const FLinearColor PanelSoft  = FLinearColor(0.933f, 0.941f, 0.953f, 1.f); // 옅은 패널 톤(층 분리)
	inline const FLinearColor CardWhite  = FLinearColor(1.000f, 1.000f, 1.000f, 1.f); // 카드 바탕
	inline const FLinearColor BorderSoft = FLinearColor(0.890f, 0.902f, 0.922f, 1.f); // 얇은 테두리
	inline const FLinearColor TextSlate  = FLinearColor(0.224f, 0.255f, 0.310f, 1.f); // 본문/헤딩
	inline const FLinearColor TextMuted  = FLinearColor(0.604f, 0.627f, 0.675f, 1.f); // 보조 텍스트
	inline const FLinearColor Accent     = FLinearColor(0.361f, 0.561f, 0.839f, 1.f); // LOCK IN 활성(블루)
	inline const FLinearColor AccentIdle = FLinearColor(0.882f, 0.894f, 0.914f, 1.f); // LOCK IN 비활성 바탕
	inline const FLinearColor BanRed     = FLinearColor(0.753f, 0.314f, 0.290f, 1.f); // 밴 X

	// 팀 파스텔 림 / 진한 텍스트 (bTeam1 ? 코랄 : 블루)
	inline FLinearColor TeamAccent(bool bTeam1)
	{
		return bTeam1 ? FLinearColor(0.957f, 0.659f, 0.627f, 1.f)   // 코랄 파스텔(림/라인)
		              : FLinearColor(0.682f, 0.784f, 0.918f, 1.f);  // 블루 파스텔(림/라인)
	}
	inline FLinearColor TeamDeep(bool bTeam1)
	{
		return bTeam1 ? FLinearColor(0.690f, 0.314f, 0.235f, 1.f)   // 진한 코랄(텍스트)
		              : FLinearColor(0.184f, 0.435f, 0.722f, 1.f);  // 진한 블루(텍스트)
	}

	// 초상화 없는(플레이스홀더) 유닛 타일 — 파스텔(저채도/고명도)로 인덱스 분산
	inline FLinearColor PlaceholderColor(int32 Index)
	{
		const float Hue01 = FMath::Frac(static_cast<float>(Index) * 0.61803398875f);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue01 * 255.f), /*S*/ 70, /*V*/ 210);
	}

	// ── 폰트 (Roboto 기본 — Regular/Bold 는 항상 존재) ──
	inline FSlateFontInfo Heading(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Bold", Size); }
	inline FSlateFontInfo Body(int32 Size)    { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }

	// ── 메트릭 ──
	inline constexpr float CardRadius       = 9.f;
	inline constexpr float SlotRadius       = 8.f;
	inline constexpr float DividerThickness = 3.f;
	inline constexpr float HoverLerpSpeed   = 12.f;  // hover/선택 트랜지션 보간 속도(1/s)

	// ── 텍스처 키트 (Mcp_Tools/Asset_Pipeline/ui_kit_manifest.json 계약) ──
	// 텍스처가 있으면 DrawAs=Box(9-slice, Margin=slice_margin) 브러시, 없으면 솔리드
	// 폴백 — 텍스처 미존재 상태에서도 PIE 가 항상 동작해야 한다 (BanPick 패턴).
	inline FSlateBrush KitBrush(const TCHAR* TexPath, float SliceMargin, const FLinearColor& Fallback)
	{
		FSlateBrush Brush;
		if (UTexture2D* Tex = Cast<UTexture2D>(
				StaticLoadObject(UTexture2D::StaticClass(), nullptr, TexPath)))
		{
			Brush.SetResourceObject(Tex);
			Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
			Brush.DrawAs = SliceMargin > 0.f ? ESlateBrushDrawType::Box
			                                 : ESlateBrushDrawType::Image;
			Brush.Margin = FMargin(SliceMargin);
			Brush.TintColor = FLinearColor::White;
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.TintColor = Fallback;
		}
		return Brush;
	}

	// 버튼 3상태 (BasePath / BasePath_Hover / BasePath_Pressed) → FButtonStyle
	inline FButtonStyle KitButtonStyle(const FString& BasePath, float SliceMargin, const FLinearColor& Fallback)
	{
		FButtonStyle Style;
		Style.SetNormal(KitBrush(*BasePath, SliceMargin, Fallback));
		Style.SetHovered(KitBrush(*(BasePath + TEXT("_Hover")), SliceMargin,
		                          Fallback * 1.1f));
		Style.SetPressed(KitBrush(*(BasePath + TEXT("_Pressed")), SliceMargin,
		                          Fallback * 0.85f));
		Style.SetNormalPadding(FMargin(14.f, 7.f));
		Style.SetPressedPadding(FMargin(14.f, 9.f, 14.f, 5.f));
		return Style;
	}
}
