// 공유 UI 디자인 토큰 — 다크·프리미엄(v2). 벤픽/라운드 준비/상점 위젯이 공유한다.
//
// ⚠️ 헤더-only(inline) 설계: 모든 토큰/헬퍼가 inline 이라 단일 정의 → 유니티/Jumbo 빌드에서
//    심볼 충돌이 원천적으로 없다. 색/폰트 신규 상수는 전부 여기에 모으고, 위젯 .cpp 에 다시
//    흩뿌리지 않는다.
//
// ── v2 리테마 (다크·프리미엄) ────────────────────────────────────────────────
//   • 지반 = 진한 청색 편향 차콜(3단 패널 계층), 텍스트 = 밝은 라이트.
//   • 액센트 역할 분리(중요): Gold = 경제/상점 CTA, Info(파랑) = 시간/타이머,
//     RedCTA(빨강) = 진행/실행(라운드 준비). 팀 코랄/애저 = 팩션, Success/Danger = 상태.
//   • 하위호환: 기존 심볼(BgBase/PanelSoft/CardWhite/TextSlate/Accent/BanRed/PreviewRing…)은
//     이름을 유지하되 값만 다크로 갱신 → 모든 다운스트림이 재컴파일만으로 즉시 다크가 된다.
//     신규 시맨틱 토큰(Gold/Info/RedCTA/Success/Danger/Panel*/Border*…)은 아래에 추가했다.
//     (일부 옛 이름은 다크에서 의미가 어긋난다 — 예: "CardWhite" 는 이제 어두운 카드 바탕.
//      각 위젯을 하이브리드 마이그레이션할 때 신규 시맨틱 이름으로 점진 교체한다.)
//
// 팀색은 게임 타입 의존을 피하려 bool bTeam1 로 받는다 (호출부: Team == EAOSTeam::Team1).
// 값은 sRGB decimal(hex/255) 관례 — 이 코드베이스의 기존 FLinearColor 저작 방식과 동일.
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
	// ── 지반 / 패널 계층 (진한 청색 편향 차콜) ──
	inline const FLinearColor BgDeep      = FLinearColor(0.016f, 0.027f, 0.047f, 1.f); // 최심층(dim/오버레이 베이스)
	inline const FLinearColor BgBase      = FLinearColor(0.031f, 0.047f, 0.075f, 1.f); // 화면 베이스
	inline const FLinearColor PanelBase   = FLinearColor(0.055f, 0.078f, 0.125f, 1.f); // 패널 1단
	inline const FLinearColor PanelRaised = FLinearColor(0.078f, 0.110f, 0.169f, 1.f); // 카드/버튼 2단
	inline const FLinearColor PanelHi     = FLinearColor(0.110f, 0.145f, 0.212f, 1.f); // hover/선택 3단
	inline const FLinearColor Border      = FLinearColor(0.129f, 0.176f, 0.263f, 1.f); // 구획선
	inline const FLinearColor BorderHi    = FLinearColor(0.216f, 0.290f, 0.416f, 1.f); // 강조 테두리/코너 브래킷

	// ── 텍스트 ──
	inline const FLinearColor TextPrimary = FLinearColor(0.906f, 0.929f, 0.976f, 1.f); // 본문/헤딩
	inline const FLinearColor TextMuted   = FLinearColor(0.541f, 0.588f, 0.682f, 1.f); // 보조
	inline const FLinearColor TextFaint   = FLinearColor(0.322f, 0.369f, 0.467f, 1.f); // 최약(placeholder/캡션)

	// ── 액센트 (역할 분리) ──
	inline const FLinearColor Gold        = FLinearColor(1.000f, 0.788f, 0.302f, 1.f); // 경제/상점 CTA/골드
	inline const FLinearColor GoldDeep    = FLinearColor(0.690f, 0.514f, 0.157f, 1.f); // 골드 테두리/그림자
	inline const FLinearColor GoldSoft    = FLinearColor(1.000f, 0.788f, 0.302f, 0.12f);// 골드 반투명 배경
	inline const FLinearColor Info        = FLinearColor(0.310f, 0.651f, 0.949f, 1.f); // 시간/타이머(파랑)
	inline const FLinearColor InfoDeep    = FLinearColor(0.118f, 0.353f, 0.612f, 1.f); // 타이머 칩 테두리
	inline const FLinearColor InfoSoft    = FLinearColor(0.310f, 0.651f, 0.949f, 0.12f);// 타이머 칩 배경
	inline const FLinearColor RedCTA      = FLinearColor(0.886f, 0.290f, 0.235f, 1.f); // 진행/실행(라운드 준비)
	inline const FLinearColor RedCTADeep  = FLinearColor(0.478f, 0.129f, 0.098f, 1.f); // RedCTA 그라디언트 저단

	// ── 상태 ──
	inline const FLinearColor Success     = FLinearColor(0.306f, 0.796f, 0.518f, 1.f); // 유효/구매가능/배치완료
	inline const FLinearColor Danger      = FLinearColor(0.878f, 0.357f, 0.329f, 1.f); // 밴/불가/제거

	// ── 하위호환 별칭 (옛 이름 유지 → 재컴파일만으로 다크 적용) ──
	inline const FLinearColor PanelSoft   = PanelBase;    // 옛 "옅은 패널 톤"
	inline const FLinearColor CardWhite   = PanelRaised;  // 옛 "카드 바탕"(이제 다크)
	inline const FLinearColor BorderSoft  = Border;       // 옛 "얇은 테두리"
	inline const FLinearColor TextSlate   = TextPrimary;  // 옛 "본문/헤딩"(라이트→다크에서 밝은 텍스트)
	inline const FLinearColor Accent      = Info;         // 옛 "LOCK IN 활성(블루)" → Info 로 통합
	inline const FLinearColor AccentIdle  = PanelHi;      // 옛 "LOCK IN 비활성 바탕"
	inline const FLinearColor BanRed      = Danger;       // 옛 "밴 X" → Danger
	inline const FLinearColor PreviewRing = Gold;         // 옛 "미리보기/활성 슬롯 골드 링" → Gold

	// 팀 파스텔 림 / 진한 텍스트 (bTeam1 ? 코랄 : 애저)
	inline FLinearColor TeamAccent(bool bTeam1)
	{
		return bTeam1 ? FLinearColor(0.949f, 0.439f, 0.310f, 1.f)   // 코랄(림/라인)
		              : FLinearColor(0.298f, 0.573f, 0.925f, 1.f);  // 애저(림/라인)
	}
	inline FLinearColor TeamDeep(bool bTeam1)
	{
		return bTeam1 ? FLinearColor(0.690f, 0.314f, 0.235f, 1.f)   // 진한 코랄(패널/채움)
		              : FLinearColor(0.184f, 0.435f, 0.722f, 1.f);  // 진한 애저(패널/채움)
	}

	// 초상화 없는(플레이스홀더) 유닛 타일 — 다크에 맞춘 저명도/중채도로 인덱스 분산
	inline FLinearColor PlaceholderColor(int32 Index)
	{
		const float Hue01 = FMath::Frac(static_cast<float>(Index) * 0.61803398875f);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue01 * 255.f), /*S*/ 95, /*V*/ 120);
	}

	// ── 폰트 (Roboto 기본 — Regular/Bold 는 항상 존재) ──
	inline FSlateFontInfo Heading(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Bold", Size); }
	inline FSlateFontInfo Body(int32 Size)    { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }

	// ── 메트릭 ──
	inline constexpr float CardRadius       = 9.f;
	inline constexpr float SlotRadius       = 8.f;
	inline constexpr float DividerThickness = 3.f;
	inline constexpr float HoverLerpSpeed   = 12.f;  // hover/선택 트랜지션 보간 속도(1/s)

	// 간격 스케일(px) — 위젯 패딩/갭에 사용, 매직넘버 대신 참조
	inline constexpr float Space1 = 4.f;
	inline constexpr float Space2 = 8.f;
	inline constexpr float Space3 = 12.f;
	inline constexpr float Space4 = 16.f;
	inline constexpr float Space5 = 24.f;
	inline constexpr float Space6 = 32.f;

	// 타입 스케일(pt) — 목업 기준
	inline constexpr int32 TypeDisplay = 26;
	inline constexpr int32 TypeH1      = 22;
	inline constexpr int32 TypeH2      = 17;
	inline constexpr int32 TypeBody    = 14;
	inline constexpr int32 TypeCaption = 11;

	// ── 텍스처 키트 (Mcp_Tools/Asset_Pipeline/ui_kit_manifest.json 계약) ──
	// 텍스처가 있으면 DrawAs=Box(9-slice, Margin=slice_margin) 브러시, 없으면 솔리드
	// 폴백 — 텍스처 미존재 상태에서도 PIE 가 항상 동작해야 한다 (BanPick 패턴).
	// ⚠️ v2: 상점 장식 프레임 텍스처(T_UI_Shop_*)는 제거 대상 — 콘텐츠(초상화/아이콘) 이외
	//    "크롬"은 텍스처 대신 토큰 솔리드로 그린다. KitBrush 는 콘텐츠 이미지용으로 유지.
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

	// 솔리드 색 브러시 (토큰 기반 플랫 패널/카드) — 텍스처 없이 색만.
	inline FSlateBrush SolidBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = Color;
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

	// 솔리드 3상태 버튼 (텍스처 없이 토큰 색만) — Normal/Hover(약간 밝게)/Pressed(약간 어둡게).
	inline FButtonStyle SolidButtonStyle(const FLinearColor& Base)
	{
		FButtonStyle Style;
		Style.SetNormal(SolidBrush(Base));
		Style.SetHovered(SolidBrush(Base * 1.15f));
		Style.SetPressed(SolidBrush(Base * 0.85f));
		Style.SetNormalPadding(FMargin(14.f, 7.f));
		Style.SetPressedPadding(FMargin(14.f, 9.f, 14.f, 5.f));
		return Style;
	}
}
