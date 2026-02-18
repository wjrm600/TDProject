#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSHealthBarWidget.generated.h"

class UProgressBar;

/**
 * AOS HP 바 위젯
 * 캐릭터/구조물 머리 위에 표시되는 체력 바
 */
UCLASS()
class TDPROJECT_API UAOSHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HP 퍼센트 업데이트 (0.0 ~ 1.0)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdateHealthPercent(float Percent);

	// 바 색상 설정 (팀 색상)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetBarColor(FLinearColor Color);

protected:
	// Widget Blueprint에서 ProgressBar 이름을 "HealthProgressBar"로 설정해야 함
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthProgressBar;
};
