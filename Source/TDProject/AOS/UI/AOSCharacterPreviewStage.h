// 벤픽 3D 캐릭터 프리뷰 스테이지 (클라이언트 전용).
//  화면 밖 먼 좌표에 스폰되는 액터로, SkeletalMeshComponent + SceneCaptureComponent2D + 라이트를 들고
//  로스터 캐릭터의 메시/애님을 렌더 타깃에 캡처한다. 위젯이 그 RT 를 Slate 브러시로 직접 표시
//  (머티리얼/RT 에셋 불필요 — `FSlateBrush::SetResourceObject(RenderTarget)`).
//
//  DS: 서버는 렌더 파이프라인이 없으므로 **클라에서만 스폰**(PlayerController 가 IsLocalController +
//      비 DedicatedServer 가드). 선택 데이터(UnitId)는 GameState 리플리케이션으로 받고, 3D 렌더는 각 클라 로컬.
//  성능: 캡처 2개. 메시가 보일 때만 매 프레임 캡처(애님 갱신), 비었을 땐 캡처 끔.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AOSCharacterPreviewStage.generated.h"

class AAOSCharacter;
class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;

UCLASS()
class TDPROJECT_API AAOSCharacterPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	AAOSCharacterPreviewStage();

	// 런타임 렌더 타깃 생성 + 캡처에 연결 (에셋 불필요). 스폰 직후 PlayerController 가 1회 호출.
	UTextureRenderTarget2D* InitRenderTarget(int32 Width, int32 Height);

	// 표시할 캐릭터 설정 — 클래스 CDO 의 스켈레탈 메시 + AnimClass 를 프리뷰 메시에 적용.
	// nullptr 이면 메시 숨김 + 캡처 정지.
	void SetPreviewCharacter(TSubclassOf<AAOSCharacter> CharacterClass);

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	USkeletalMeshComponent* PreviewMesh;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	USceneCaptureComponent2D* Capture;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	UPointLightComponent* KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	UPointLightComponent* FillLight;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget = nullptr;

	// ── 프레이밍/라이팅 (빌드 없이 에디터에서 튜닝) ──
	// 캡처 카메라의 메시 기준 상대 위치/회전 (캐릭터 정면을 바라보게)
	UPROPERTY(EditAnywhere, Category = "Preview|Framing")
	FVector CaptureRelativeLocation = FVector(240.f, 0.f, 10.f);

	UPROPERTY(EditAnywhere, Category = "Preview|Framing")
	FRotator CaptureRelativeRotation = FRotator(0.f, 180.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Preview|Framing", meta = (ClampMin = "10.0", ClampMax = "120.0"))
	float CaptureFOV = 32.f;

	// 메시(발)가 프레임 하단에 오도록 내림 — 캐릭터 스탠딩 프레이밍
	UPROPERTY(EditAnywhere, Category = "Preview|Framing")
	FVector MeshRelativeLocation = FVector(0.f, 0.f, -90.f);

	UPROPERTY(EditAnywhere, Category = "Preview|Framing")
	FRotator MeshRelativeRotation = FRotator(0.f, -90.f, 0.f);

	// 렌더 타깃 배경색 (ShowOnlyList 라 메시 외 영역은 이 클리어 컬러)
	UPROPERTY(EditAnywhere, Category = "Preview|Lighting")
	FLinearColor BackgroundColor = FLinearColor(0.015f, 0.02f, 0.04f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Preview|Lighting")
	float KeyLightIntensity = 9000.f;

	UPROPERTY(EditAnywhere, Category = "Preview|Lighting")
	float FillLightIntensity = 3000.f;

private:
	// 현재 메시가 유효(표시 중)할 때만 캡처 활성
	void SetCapturing(bool bEnable);
};
