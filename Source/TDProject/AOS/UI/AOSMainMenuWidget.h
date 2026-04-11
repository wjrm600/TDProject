#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSMainMenuWidget.generated.h"

class UButton;

/**
 * AOS 메인 메뉴 위젯
 * 게임 시작 및 종료 버튼을 제공하는 메인 메뉴 UI
 */
UCLASS()
class TDPROJECT_API UAOSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	// Widget Blueprint에서 버튼 이름을 정확히 맞춰야 함
	UPROPERTY(meta = (BindWidget))
	UButton* StartGameButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ExitGameButton;

	// 버튼 클릭 이벤트 핸들러
	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnExitGameClicked();
};
