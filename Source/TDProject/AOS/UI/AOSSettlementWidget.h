#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSGameMode.h"
#include "AOSSettlementWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * AOS 게임 결과(정산) 위젯
 * 승리/패배 결과를 표시하고 메인 메뉴로 복귀하는 UI
 */
UCLASS()
class TDPROJECT_API UAOSSettlementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// 게임 결과 설정 (승리 팀 표시)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetResult(EAOSTeam WinningTeam);

protected:
	// Widget Blueprint에서 이름을 정확히 맞춰야 함
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ResultText;

	UPROPERTY(meta = (BindWidget))
	UButton* ReturnToMainMenuButton;

	// 버튼 클릭 이벤트 핸들러
	UFUNCTION()
	void OnReturnClicked();
};
