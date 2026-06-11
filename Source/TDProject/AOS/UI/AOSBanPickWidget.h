// 벤픽 드래프트 위젯 — UMG(WBP) 하이브리드. League of Legends 챔피언 선택 스타일 레이아웃.
//  ▸ 정적 프레임(BackdropImage/Title/Timer/Status/플레이어명/Confirm)은 BindWidgetOptional →
//    WBP_BanPick 가 있으면 디자이너 트리에 바인딩, 없으면 RebuildWidget 이 C++ 폴백 프레임 생성.
//  ▸ 동적 자식 컨테이너(CardGrid / Team1·2BanRow / Team1·2PickRow)도 BindWidgetOptional →
//    C++ 가 이 (바인딩되거나 폴백 생성된) 컨테이너에 카드/슬롯을 채운다.
//    (배열 자체는 BindWidget 불가라 plain UPROPERTY 로 GC 보호만.)
//  레이아웃: 상단바(좌 플레이어명+밴 / 중앙 PICK&BAN 제목+타이머 / 우 플레이어명+밴)
//            + 중앙 행(좌 팀1 픽슬롯 5 / 중앙 챔피언 그리드+상태+확정버튼 / 우 팀2 픽슬롯 5)
//  2단계 선택: 카드 클릭 = 미리보기(PendingIndex 하이라이트) → '확정' 버튼으로 서버 RPC 1회.
//  카드 클릭은 위젯 레벨 NativeOnMouseButtonDown 의 지오메트리 히트테스트로 처리(미니맵 패턴)
//   → 20개 서브위젯 버튼 실현 리스크 회피. 확정 버튼만 단일 UButton(자체 클릭 처리).
//  드래프트 상태는 GameState(OnDraftChanged) 리플리케이션으로 수신.
//
//  [WBP 저작 계약] WBP_BanPick(Parent=UAOSBanPickWidget) 에서 아래 이름과 정확히 일치하는 위젯을
//    배치하면 자동 바인딩된다(전부 선택 — 누락 시 해당 부분만 폴백/스킵):
//      BackdropImage(Image), TitleText/TimerText/StatusText/Team1PlayerNameText/Team2PlayerNameText(TextBlock),
//      CardGrid(WrapBox), Team1BanRow/Team2BanRow(HorizontalBox),
//      Team1PickRow/Team2PickRow(HorizontalBox), ConfirmButton(Button), ConfirmText(TextBlock),
//      MyPreviewImage/EnemyPreviewImage(Image — 3D 프리뷰).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSGameMode.h"   // FCharacterRosterEntry, EAOSTeam
#include "AOSBanPickWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UOverlay;
class UTextBlock;
class UWrapBox;
class UHorizontalBox;
class UVerticalBox;
class AAOSGameState;
class AAOSCharacterPreviewStage;

UCLASS()
class TDPROJECT_API UAOSBanPickWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 로스터 주입 + 카드/슬롯 채움 (PlayerController 가 호출)
	void InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster);

	// 3D 프리뷰 스테이지 연결 (PlayerController 가 클라에서 스폰 후 전달).
	//  Mine = 우하단(내 팀) / Enemy = 좌상단(상대 팀). RT 를 프리뷰 Image 브러시로 연결.
	void SetPreviewStages(AAOSCharacterPreviewStage* Mine, AAOSCharacterPreviewStage* Enemy);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// WBP 미사용 시 C++ 폴백 트리 생성 (MainMenu/Settlement/Lobby 패턴)
	virtual TSharedRef<SWidget> RebuildWidget() override;

protected:
	// 팀당 밴/픽 수 (AAOSGameState::GetDraftSequence 와 일치 — 밴2·픽5)
	static constexpr int32 BansPerTeam = 2;
	static constexpr int32 PicksPerTeam = 5;

	// WBP 없을 때 정적 프레임(루트/상단바/팀패널/그리드 + 빈 동적 컨테이너) 생성.
	// RootWidget 미설정일 때만 동작(WBP 존재 시 skip).
	void BuildFallbackFrame();

	// (바인딩되거나 폴백 생성된) 컨테이너에 동적 자식 채우기 — 좌/우 공통, 멱등(clear 후 refill)
	void PopulatePickRow(EAOSTeam Team);      // 픽 슬롯 5칸 (가로 행 — 레퍼런스 레이아웃)
	void PopulateBanRow(EAOSTeam Team);       // 밴 슬롯 N칸

	// GameState 드래프트 변경 → 카드/슬롯/상태 갱신
	UFUNCTION()
	void OnDraftChanged();

	// 확정 버튼 클릭 → PendingIndex 를 서버로 전송
	UFUNCTION()
	void OnConfirmClicked();

	// 카드 클릭 → 미리보기 선택(PendingIndex)
	void HandleCardClicked(int32 RosterIndex);

	void RefreshCards();    // 그리드 카드 틴트 + 미리보기 하이라이트
	void RefreshSlots();    // 팀별 밴/픽 슬롯 채우기
	void RefreshStatus();   // 제목/타이머/상태 텍스트 + 확정 버튼 활성

	// 3D 프리뷰 갱신: 내 팀(우하단)=내 미리보기/최신픽, 상대(좌상단)=상대 최신픽 → 스테이지 메시 스왑
	void UpdatePreviewSelections();

	// 텍스처 경로 자동로드 (없으면 nullptr)
	static UTexture2D* TryLoadTexture(const TCHAR* AssetPath);
	// 로스터 초상화 (없으면 nullptr)
	UTexture2D* GetPortrait(int32 RosterIndex) const;
	// 팀 대표 색 (Team1=레드 / Team2=블루)
	static FLinearColor TeamColor(EAOSTeam Team);

	AAOSGameState* GetAOSGameState() const;
	EAOSTeam GetLocalTeam() const;
	// 자기/상대 플레이어 이름 (PlayerState->GetPlayerName)
	FString GetPlayerNameForTeam(EAOSTeam Team) const;

	// ---- 루트(폴백 전용 — WBP 경로에선 미사용) ----
	UPROPERTY()
	UOverlay* RootOverlay = nullptr;

	// ---- 정적 프레임 (WBP 바인딩 / 폴백 C++ 생성) ----
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* BackdropImage = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TimerText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Team1PlayerNameText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Team2PlayerNameText = nullptr;

	// ---- 확정 버튼 (WBP 바인딩 / 폴백 C++ 생성) ----
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ConfirmText = nullptr;

	// ---- 3D 캐릭터 프리뷰 (WBP 바인딩 / 폴백 코너 배치 — 스테이지 RT 를 브러시로 표시) ----
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* MyPreviewImage = nullptr;       // 우하단 = 내 팀

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* EnemyPreviewImage = nullptr;    // 좌상단 = 상대 팀

	// ---- 동적 자식 컨테이너 (WBP 바인딩 / 폴백 C++ 생성 — C++ 가 채움) ----
	UPROPERTY(meta = (BindWidgetOptional))
	UWrapBox* CardGrid = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* Team1BanRow = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* Team2BanRow = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* Team1PickRow = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* Team2PickRow = nullptr;

	// ---- 밴 슬롯 이미지 (팀당 BansPerTeam, C++ 생성 — 배열은 BindWidget 불가) ----
	UPROPERTY()
	TArray<UImage*> Team1BanImages;

	UPROPERTY()
	TArray<UImage*> Team2BanImages;

	// ---- 픽 슬롯 (팀당 PicksPerTeam, C++ 생성) ----
	UPROPERTY()
	TArray<UBorder*> Team1PickBorders;

	UPROPERTY()
	TArray<UImage*> Team1PickImages;

	UPROPERTY()
	TArray<UTextBlock*> Team1PickNames;

	UPROPERTY()
	TArray<UBorder*> Team2PickBorders;

	UPROPERTY()
	TArray<UImage*> Team2PickImages;

	UPROPERTY()
	TArray<UTextBlock*> Team2PickNames;

	// 카드 보더(히트테스트용) + 이미지(틴트용). 인덱스 = 로스터 인덱스.
	UPROPERTY()
	TArray<UBorder*> CardBorders;

	UPROPERTY()
	TArray<UImage*> CardImages;

	// 3D 프리뷰 스테이지 (PlayerController 소유 — 위젯은 비소유 참조. 액터 destroy 시 UPROPERTY 자동 null)
	UPROPERTY()
	AAOSCharacterPreviewStage* MyPreviewStage = nullptr;

	UPROPERTY()
	AAOSCharacterPreviewStage* EnemyPreviewStage = nullptr;

	// 2단계 선택: 클릭으로 미리보기한 로스터 인덱스 (확정 전). INDEX_NONE = 없음.
	int32 PendingIndex = INDEX_NONE;

	TArray<FCharacterRosterEntry> CachedRoster;
	bool bSubscribed = false;
};
