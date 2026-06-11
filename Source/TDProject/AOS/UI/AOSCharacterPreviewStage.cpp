#include "AOSCharacterPreviewStage.h"
#include "AOSCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"

AAOSCharacterPreviewStage::AAOSCharacterPreviewStage()
{
	PrimaryActorTick.bCanEverTick = false;   // 컴포넌트(메시 포즈/캡처)가 자체 틱

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(SceneRoot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetVisibility(false);
	PreviewMesh->bReceivesDecals = false;
	// 메인 뷰에 안 보여도(캡처 전용) 포즈를 항상 갱신 → 오프스크린 idle 애니 재생
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(180.f, 120.f, 170.f));
	KeyLight->SetAttenuationRadius(1500.f);
	KeyLight->CastShadows = false;   // 격리 캡처 — 바닥 없음, 그림자 불필요

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetRelativeLocation(FVector(180.f, -170.f, 60.f));
	FillLight->SetAttenuationRadius(1500.f);
	FillLight->CastShadows = false;

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(SceneRoot);
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;  // 이 스테이지만 렌더
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;   // 메시 표시될 때만 켬(SetCapturing)
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
}

void AAOSCharacterPreviewStage::BeginPlay()
{
	Super::BeginPlay();

	if (Capture)
	{
		Capture->ShowOnlyActors.Empty();
		Capture->ShowOnlyActors.Add(this);   // 게임 월드 차단 — 이 스테이지 메시만 캡처
		Capture->SetRelativeLocationAndRotation(CaptureRelativeLocation, CaptureRelativeRotation);
		Capture->FOVAngle = CaptureFOV;
	}
	if (PreviewMesh)
	{
		PreviewMesh->SetRelativeLocationAndRotation(MeshRelativeLocation, MeshRelativeRotation);
	}
	if (KeyLight)  KeyLight->SetIntensity(KeyLightIntensity);
	if (FillLight) FillLight->SetIntensity(FillLightIntensity);
}

UTextureRenderTarget2D* AAOSCharacterPreviewStage::InitRenderTarget(int32 Width, int32 Height)
{
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	RenderTarget->ClearColor = BackgroundColor;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(FMath::Max(4, Width), FMath::Max(4, Height));
	RenderTarget->UpdateResourceImmediate(true);
	if (Capture)
	{
		Capture->TextureTarget = RenderTarget;
	}
	return RenderTarget;
}

void AAOSCharacterPreviewStage::SetPreviewCharacter(TSubclassOf<AAOSCharacter> CharacterClass)
{
	if (!PreviewMesh) return;

	USkeletalMesh* SkelMesh = nullptr;
	UClass* AnimClass = nullptr;
	FVector RelLoc = MeshRelativeLocation;
	FRotator RelRot = MeshRelativeRotation;

	if (CharacterClass)
	{
		if (const AAOSCharacter* CDO = CharacterClass->GetDefaultObject<AAOSCharacter>())
		{
			// GetMesh() const 는 비-const 컴포넌트 포인터 반환 → GetAnimClass()(비-const) 호출 가능
			if (USkeletalMeshComponent* SrcMesh = CDO->GetMesh())
			{
				SkelMesh = SrcMesh->GetSkeletalMeshAsset();
				AnimClass = SrcMesh->GetAnimClass();
				// 캐릭터 BP 메시 오프셋(보통 z-90, yaw-90) 그대로 → 자연스러운 스탠딩
				RelLoc = SrcMesh->GetRelativeLocation();
				RelRot = SrcMesh->GetRelativeRotation();
			}
		}
	}

	if (SkelMesh)
	{
		PreviewMesh->SetSkeletalMeshAsset(SkelMesh);
		if (AnimClass)
		{
			// AnimBP(UAOSAnimInstance)는 OwningCharacter null 시 조기반환 → 크래시 없이 idle 포즈
			PreviewMesh->SetAnimInstanceClass(AnimClass);
		}
		PreviewMesh->SetRelativeLocationAndRotation(RelLoc, RelRot);
		PreviewMesh->SetVisibility(true);
		SetCapturing(true);
	}
	else
	{
		PreviewMesh->SetVisibility(false);
		PreviewMesh->SetSkeletalMeshAsset(nullptr);
		SetCapturing(false);
	}
}

void AAOSCharacterPreviewStage::SetCapturing(bool bEnable)
{
	if (!Capture) return;
	Capture->bCaptureEveryFrame = bEnable;   // idle 애니 갱신 위해 매 프레임 (메시 보일 때만)
	if (bEnable)
	{
		Capture->CaptureScene();             // 즉시 1프레임 반영
	}
}
