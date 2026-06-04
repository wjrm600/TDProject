// Slice 1 (가독성): 플로팅 데미지 숫자.
//  서버가 피격 시점에 victim 액터의 Multicast_ShowDamageNumber 로 방송 →
//  각 클라(+리슨 호스트)에서 이 위젯을 1개씩 생성해 뷰포트에 띄움.
//  월드 한 점에서 위로 떠오르며 페이드아웃 후 자동 제거. (DS 는 렌더 없음 → 생성 skip)
//
//  주의: 순수 C++ UUserWidget 은 TickFrequency=Auto 에서 BP Tick/애니메이션이 없으면
//  NativeTick 이 호출되지 않음 → 떠오름/페이드를 월드 타이머로 구동(프레임률 독립).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSDamageNumberWidget.generated.h"

class UCanvasPanel;
class UTextBlock;

UCLASS()
class TDPROJECT_API UAOSDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 클라이언트 전용 진입점: 데미지 숫자 위젯을 생성/초기화해 뷰포트에 추가.
	// DS(렌더 파이프라인 없음)·로컬 PC 부재·Damage<=0 이면 조용히 skip.
	static void SpawnDamageNumber(const UObject* WorldContext, float Damage,
		const FVector& WorldLocation, FLinearColor Color);

	// 데미지/시작 월드 위치/색상 주입 + 애니 타이머 시작 (생성 직후 1회).
	void InitDamageNumber(float InDamage, const FVector& InWorldLocation, FLinearColor InColor);

	virtual void NativeDestruct() override;

protected:
	// WidgetTree 로 정적 구조(캔버스 + 텍스트) 1회 생성.
	void BuildUI();

	// 타이머 콜백 — 떠오름/페이드/화면투영 갱신, 수명 종료 시 제거.
	void UpdateStep();

	UPROPERTY()
	UCanvasPanel* RootCanvas = nullptr;

	UPROPERTY()
	UTextBlock* DamageText = nullptr;

private:
	FVector BaseWorldLocation = FVector::ZeroVector; // 시작 월드 위치
	float StartTimeSeconds = 0.f;                    // 생성 시점 월드 시간
	float Lifetime = 1.1f;                           // 화면 표시 시간(초)
	float RiseWorldSpeed = 90.f;                     // 초당 상승량(cm) — 월드 Z 로 떠오름
	bool bBuilt = false;

	FTimerHandle UpdateTimerHandle;
};
