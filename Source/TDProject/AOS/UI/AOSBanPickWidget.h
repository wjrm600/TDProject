// 벤픽 드래프트 위젯 (순수 C++ — WBP 불필요). League of Legends 챔피언 선택 스타일 레이아웃.
//  레이아웃: 상단바(좌 플레이어명+밴 / 중앙 PICK&BAN 제목+타이머 / 우 플레이어명+밴)
//            + 중앙 행(좌 팀1 픽슬롯 5 / 중앙 챔피언 그리드+상태+확정버튼 / 우 팀2 픽슬롯 5)
//  2단계 선택: 카드 클릭 = 미리보기(PendingIndex 하이라이트) → '확정' 버튼으로 서버 RPC 1회.
//  카드 클릭은 위젯 레벨 NativeOnMouseButtonDown 의 지오메트리 히트테스트로 처리(미니맵 패턴)
//   → 20개 서브위젯 버튼 실현 리스크 회피. 확정 버튼만 단일 UButton(자체 클릭 처리).
//  배경/프레임 텍스처는 경로 자동로드(있으면 적용, 없으면 솔리드 폴백) — 미니맵 프레임 패턴.
//  드래프트 상태는 GameState(OnDraftChanged) 리플리케이션으로 수신.

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

UCLASS()
class TDPROJECT_API UAOSBanPickWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 로스터 주입 + 카드/슬롯 생성 (PlayerController 가 호출)
	void InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	// 팀당 밴/픽 수 (AAOSGameState::GetDraftSequence 와 일치 — 밴2·픽5)
	static constexpr int32 BansPerTeam = 2;
	static constexpr int32 PicksPerTeam = 5;

	// 정적 프레임(루트/상단바/팀패널/그리드/확정버튼) 1회 생성
	void BuildUI();

	// 한 팀의 픽 컬럼(밴 행 헤더 제외, 픽 슬롯 5칸) 생성 — 좌/우 공통
	UVerticalBox* BuildPickColumn(EAOSTeam Team);
	// 한 팀의 밴 행(작은 슬롯 N칸) 생성
	UHorizontalBox* BuildBanRow(EAOSTeam Team);

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

	// ---- 루트/배경 ----
	UPROPERTY()
	UOverlay* RootOverlay = nullptr;

	UPROPERTY()
	UImage* BackdropImage = nullptr;

	// ---- 상단바 ----
	UPROPERTY()
	UTextBlock* TitleText = nullptr;

	UPROPERTY()
	UTextBlock* TimerText = nullptr;

	UPROPERTY()
	UTextBlock* StatusText = nullptr;

	UPROPERTY()
	UTextBlock* Team1PlayerNameText = nullptr;

	UPROPERTY()
	UTextBlock* Team2PlayerNameText = nullptr;

	// ---- 밴 슬롯 (팀당 BansPerTeam) ----
	UPROPERTY()
	TArray<UImage*> Team1BanImages;

	UPROPERTY()
	TArray<UImage*> Team2BanImages;

	// ---- 픽 슬롯 (팀당 PicksPerTeam) ----
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

	// ---- 중앙 챔피언 그리드 ----
	UPROPERTY()
	UWrapBox* CardGrid = nullptr;

	// 카드 보더(히트테스트용) + 이미지(틴트용). 인덱스 = 로스터 인덱스.
	UPROPERTY()
	TArray<UBorder*> CardBorders;

	UPROPERTY()
	TArray<UImage*> CardImages;

	// ---- 확정 버튼 ----
	UPROPERTY()
	UButton* ConfirmButton = nullptr;

	UPROPERTY()
	UTextBlock* ConfirmText = nullptr;

	// 2단계 선택: 클릭으로 미리보기한 로스터 인덱스 (확정 전). INDEX_NONE = 없음.
	int32 PendingIndex = INDEX_NONE;

	TArray<FCharacterRosterEntry> CachedRoster;
	bool bBuilt = false;
	bool bSubscribed = false;
};
