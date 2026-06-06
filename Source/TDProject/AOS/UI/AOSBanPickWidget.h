// 벤픽 드래프트 위젯 (순수 C++ — WBP 불필요).
//  로스터 카드 그리드 + 현재 턴/타이머 + 팀별 밴/픽 목록.
//  클릭은 위젯 레벨 NativeOnMouseButtonDown 에서 카드 지오메트리 히트테스트로 처리(미니맵 패턴)
//  → 서브위젯(UUserWidget) 실현/WidgetTree 리스크 회피.
//  드래프트 상태는 GameState(OnDraftChanged) 리플리케이션으로 수신, 선택은 PC RPC 로 서버 전송.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSGameMode.h"   // FCharacterRosterEntry, EAOSTeam
#include "AOSBanPickWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UWrapBox;
class AAOSGameState;

UCLASS()
class TDPROJECT_API UAOSBanPickWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 로스터 주입 + 카드 생성 (PlayerController 가 호출)
	void InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	// 정적 프레임(루트/상태텍스트/그리드/팀목록) 1회 생성
	void BuildUI();

	// GameState 드래프트 변경 → 카드/상태 갱신
	UFUNCTION()
	void OnDraftChanged();

	// 카드 클릭 → 서버 선택 RPC
	void HandleCardClicked(int32 RosterIndex);

	void RefreshCards();
	void RefreshStatus();

	AAOSGameState* GetAOSGameState() const;
	EAOSTeam GetLocalTeam() const;

	UPROPERTY()
	UBorder* RootBorder = nullptr;

	UPROPERTY()
	UTextBlock* StatusText = nullptr;

	UPROPERTY()
	UTextBlock* Team1Text = nullptr;

	UPROPERTY()
	UTextBlock* Team2Text = nullptr;

	UPROPERTY()
	UWrapBox* CardGrid = nullptr;

	// 카드 보더(히트테스트용) + 이미지(틴트용). 인덱스 = 로스터 인덱스.
	UPROPERTY()
	TArray<UBorder*> CardBorders;

	UPROPERTY()
	TArray<UImage*> CardImages;

	TArray<FCharacterRosterEntry> CachedRoster;
	bool bBuilt = false;
	bool bSubscribed = false;
};
