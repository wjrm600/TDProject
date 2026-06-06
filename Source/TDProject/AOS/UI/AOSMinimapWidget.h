// Slice 1 (가독성): 미니맵 — 렌더 방식 ②(정적 배경 + 아이콘 오버레이).
//  화면 우하단 고정 프레임. 배경은 선택적 정적 텍스처(없으면 반투명 패널).
//  구조물(타워/CC, 파괴 전까지) + 살아있는 캐릭터를 팀 색상 점으로 표시.
//  카메라 yaw 로 회전 정렬 → 화면 뷰(레드 좌하단/블루 우상단)와 방향 일치.
//  현재 카메라 가시 영역을 흰색 박스로 표시. 미니맵 클릭/드래그/터치 시 카메라 이동.
//
//  월드 경계는 배치된 구조물 위치에서 1회 산출(클라이언트가 복제된 액터로 직접 계산).
//  갱신은 월드 타이머로 구동(순수 C++ UUserWidget Auto 틱 게이팅 회피).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSMinimapWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UOverlay;
class UImage;
class UTexture2D;
class AAOSStructure;
class AAOSPlayerController;

UCLASS()
class TDPROJECT_API UAOSMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 최초 빌드 + 갱신 타이머 시작 + 보이기 (라운드 시작 시 PlayerController 가 호출)
	void ShowMinimap();

	// 갱신 타이머 정지 + 숨기기 (라운드 외 상태)
	void HideMinimap();

	virtual void NativeDestruct() override;

	// 클릭/드래그/터치 → 카메라 이동
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

protected:
	// WidgetTree 로 정적 구조(프레임/배경/아이콘 캔버스) 1회 생성
	void BuildUI();

	// 타이머 콜백 — 구조물/캐릭터 아이콘 + 카메라 뷰 박스 재배치
	void UpdateMinimap();

	// 배치된 구조물 위치로 (카메라 yaw 정렬) 월드 경계 1회 산출
	bool ResolveWorldBounds();

	// 월드 XY → 미니맵 로컬 픽셀 (yaw 회전 + 경계 정규화). 로컬 Y 는 아래로 증가.
	FVector2D WorldToLocal(const FVector& World) const;

	// 미니맵 로컬 픽셀 → 월드 지면(z=0) 점 (WorldToLocal 의 역변환, 클릭 이동용)
	FVector LocalToWorldGround(const FVector2D& Local) const;

	// 단색 점 아이콘 (중앙 정렬 정사각형)
	void AddIcon(FLinearColor Color, float Size, const FVector2D& LocalPos);

	// 단색 사각형 (좌상단 정렬) — 뷰 박스 변(edge)용
	void AddRect(const FVector2D& TopLeftLocal, const FVector2D& SizeLocal, FLinearColor Color);

	// 카메라 가시 영역(뷰포트 4모서리 지면 역투영)을 흰 박스로 그림
	void DrawViewBox();

	// 화면 절대좌표 클릭/터치 → 미니맵 내부면 카메라 이동. 처리 여부 반환.
	bool HandleMinimapPress(const FVector2D& ScreenPos);

	// ── 설정 (BP child 에서 조정 가능) ──
	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	FVector2D MinimapSize = FVector2D(260.f, 260.f);

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float ScreenMargin = 24.f;

	// 선택적 정적 배경 이미지 (없으면 BackgroundColor 반투명 패널)
	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	TObjectPtr<UTexture2D> BackgroundTexture = nullptr;

	// 장식 테두리 텍스처 (가운데 투명). 미설정 시 /Game/AOS/UI/Assets/T_MinimapFrame 자동 로드.
	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	TObjectPtr<UTexture2D> FrameTexture = nullptr;

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.04f, 0.6f);

	// 구조물 경계 바깥 여백 (월드 단위)
	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float WorldPadding = 1500.f;

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float StructureIconSize = 11.f;

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float CommandCenterIconSize = 16.f;

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float CharacterIconSize = 6.f;

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	FLinearColor ViewBoxColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float ViewBoxThickness = 2.f;

	// 방향 미세조정 — 카메라 yaw 에 더해지는 오프셋(도). 보통 0.
	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float ExtraYawDeg = 0.f;

	UPROPERTY(EditAnywhere, Category = "AOS|Minimap")
	float UpdateInterval = 0.05f;

	// ── 위젯 참조 ──
	UPROPERTY()
	UBorder* FrameBorder = nullptr;

	UPROPERTY()
	UOverlay* ContentOverlay = nullptr;

	UPROPERTY()
	UImage* BackgroundImage = nullptr;

	UPROPERTY()
	UImage* FrameImage = nullptr;

	UPROPERTY()
	UCanvasPanel* IconCanvas = nullptr;

	// 구조물은 맵 시작 시 1회 스폰 → 목록 캐시 (파괴는 IsDestroyed 로 매 틱 체크)
	UPROPERTY()
	TArray<AAOSStructure*> CachedStructures;

private:
	bool bBuilt = false;
	bool bBoundsValid = false;

	// 화면축 정렬용 월드 2D 기저 (단위 직교) + 그 기저에서의 경계
	FVector2D AxisRight = FVector2D(1.f, 0.f);
	FVector2D AxisUp = FVector2D(0.f, 1.f);
	float RightMin = 0.f, RightMax = 1.f;
	float UpMin = 0.f, UpMax = 1.f;

	TWeakObjectPtr<AAOSPlayerController> OwnerPC;

	FTimerHandle UpdateTimerHandle;
};
