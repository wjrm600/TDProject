# 2026-01-01 작업 로그: RTS 카메라 시스템 완성 및 라인 이동 버그 수정

## 작업 개요
RTS 스타일 자유 카메라 시스템을 완성하고, 캐릭터들이 중앙으로 몰리는 라인 이동 버그를 해결했습니다.

---

## 1. RTS 카메라 4방향 이동 구현

### 문제
- 초기 구현에서 WASD 키 입력 시 월드 축 기준으로 이동
- W를 누르면 오른쪽으로, D를 누르면 위쪽으로 움직이는 문제 발생

### 해결 방법
**파일**: [AOSPlayerController.cpp:119-131](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSPlayerController.cpp#L119-L131)

카메라의 Yaw(수평 회전)만 사용하여 방향 벡터를 계산하도록 수정:

```cpp
// Yaw만 사용 - Pitch는 무시하고 수평면에서만 이동
FRotator CameraRotation = RTSCamera->GetActorRotation();
FRotator YawOnlyRotation(0.0f, CameraRotation.Yaw, 0.0f);

// 수평면 기준 Forward/Right 방향 벡터
FVector ForwardDirection = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::X);
FVector RightDirection = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::Y);

// 입력에 따라 이동
FVector MovementDelta = FVector::ZeroVector;
MovementDelta += ForwardDirection * CameraMoveForward * CameraMoveSpeed * DeltaTime;
MovementDelta += RightDirection * CameraMoveRight * CameraMoveSpeed * DeltaTime;
```

### 핵심 개선 사항
- 카메라가 수직(-90° Pitch)을 향해도 NaN 발생하지 않음
- Yaw만 사용하여 안정적인 수평면 이동 보장
- 맵 경계 체크로 카메라가 맵 밖으로 나가지 않도록 제한

---

## 2. 카메라 각도 변수화

### 문제
- 카메라 각도가 하드코딩되어 있어 조정이 어려움

### 해결 방법
**파일**: [AOSPlayerController.h:86-111](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSPlayerController.h#L86-L111)

EditAnywhere 속성으로 카메라 관련 변수들을 노출:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
float CameraHeight = 3000.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
float CameraPitch = -70.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
float CameraYaw = 0.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
float CameraMoveSpeed = 2000.0f;
```

### 블루프린트에서 조정 가능한 값들
- `CameraHeight`: 카메라 높이 (기본값: 3000)
- `CameraPitch`: 카메라 피치 각도 (기본값: -70°)
- `CameraYaw`: 카메라 요 각도 (기본값: 0°)
- `CameraMoveSpeed`: 카메라 이동 속도 (기본값: 2000)
- `MapBoundaryX/Y`: 맵 경계 제한 (기본값: 10000)

---

## 3. DefaultPawnClass vs CharacterClass 분리

### 문제
- RTS 게임은 플레이어가 캐릭터를 직접 조종하지 않아야 함
- DefaultPawnClass를 None으로 설정하니 캐릭터가 아예 스폰되지 않음

### 해결 방법
**파일**:
- [AOSGameMode.h:32-34](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSGameMode.h#L32-L34)
- [AOSGameMode.cpp:172-178](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSGameMode.cpp#L172-L178)

GameMode에 별도의 CharacterClass 변수 추가:

```cpp
// AOSGameMode.h
// 🟢 NEW - RTS용 캐릭터 클래스 (DefaultPawnClass와 분리)
UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AOS|Spawn")
TSubclassOf<AAOSCharacter> CharacterClass;

// AOSGameMode.cpp - SpawnCharactersAtAllSpawnPoints()
if (!CharacterClass)
{
    UE_LOG(LogTemp, Error, TEXT("CharacterClass not set!"));
    return;
}

AAOSCharacter* NewCharacter = SpawnPoint->SpawnCharacterAtPoint(CharacterClass);
```

### 블루프린트 설정
1. `BP_AOSGameMode`에서 **DefaultPawnClass = None** (플레이어가 캐릭터를 빙의하지 않음)
2. **CharacterClass = BP_AOSCharacter** (AI 캐릭터 스폰에 사용)

---

## 4. 카메라 줌 인/아웃 기능

### 구현 내용
**파일**: [AOSPlayerController.cpp:208-228](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSPlayerController.cpp#L208-L228)

마우스 휠로 카메라 높이를 조절:

```cpp
void AAOSPlayerController::ZoomCamera(float AxisValue)
{
    if (!RTSCamera || AxisValue == 0.0f)
        return;

    FVector CurrentLocation = RTSCamera->GetActorLocation();

    // 마우스 휠 위로 = 줌 인, 아래로 = 줌 아웃
    float ZoomDelta = -AxisValue * ZoomSpeed;

    // 높이 범위 제한
    float NewHeight = FMath::Clamp(CurrentLocation.Z + ZoomDelta,
                                   MinZoomHeight, MaxZoomHeight);

    CurrentLocation.Z = NewHeight;
    RTSCamera->SetActorLocation(CurrentLocation);
    CameraHeight = NewHeight;
}
```

### 줌 관련 설정값
- `ZoomSpeed`: 줌 속도 (기본값: 500)
- `MinZoomHeight`: 최소 높이 (기본값: 1000)
- `MaxZoomHeight`: 최대 높이 (기본값: 5000)

---

## 5. 라인별 캐릭터 이동 버그 수정

### 문제
- 테스트 결과 모든 캐릭터가 중앙(0, 0, 0)으로 몰림
- 라인별로 분산되지 않고 한 곳에 집중

### 원인 분석
**파일**: [AOSMapManager.cpp:223-237 (수정 전)](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSMapManager.cpp#L223-L237)

모든 라인의 **세 번째 타워 위치가 (0, 0, 0)으로 겹쳐 있었음**:

```cpp
// 수정 전 - 모든 라인의 3번째 타워가 중앙에!
TopLane.Team1TowerPositions = {
    FVector(1200, 1200, 0),
    FVector(600, 600, 0),
    FVector(0, 0, 0)  // ❌ 중앙!
};

MidLane.Team1TowerPositions = {
    FVector(1200, 0, 0),
    FVector(600, 0, 0),
    FVector(0, 0, 0)  // ❌ 중앙!
};

BottomLane.Team1TowerPositions = {
    FVector(1200, -1200, 0),
    FVector(600, -600, 0),
    FVector(0, 0, 0)  // ❌ 중앙!
};
```

### 해결 방법
**파일**: [AOSMapManager.cpp:215-297](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSMapManager.cpp#L215-L297)

각 라인의 타워 위치를 독립적으로 분산:

```cpp
// Top Lane
TopLane.Team1TowerPositions = {
    FVector(1400, 1400, 0),
    FVector(700, 700, 0),
    FVector(-350, -350, 0)  // ✅ 라인 경로상에 위치
};
TopLane.Team1CommandCenterPosition = FVector(-2200, -2200, 0);

// Mid Lane
MidLane.Team1TowerPositions = {
    FVector(1400, 0, 0),
    FVector(700, 0, 0),
    FVector(-350, 0, 0)  // ✅ 라인 경로상에 위치
};
MidLane.Team1CommandCenterPosition = FVector(-2200, 0, 0);

// Bottom Lane
BottomLane.Team1TowerPositions = {
    FVector(1400, -1400, 0),
    FVector(700, -700, 0),
    FVector(-350, 350, 0)  // ✅ 라인 경로상에 위치
};
BottomLane.Team1CommandCenterPosition = FVector(-2200, 2200, 0);
```

### 수정 결과
- 각 라인이 독립적인 경로를 가짐
- 타워 위치가 라인을 따라 균등하게 분포
- Command Center 위치도 라인 끝에 맞게 조정
- 캐릭터들이 각자 할당된 라인을 따라 이동

---

## 6. 타워 위치 디버그 박스 표시

### 구현 내용
**파일**: [AOSMapManager.cpp:307-395](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSMapManager.cpp#L307-L395)

런타임에 타워 위치를 시각적으로 확인할 수 있도록 디버그 박스 표시:

```cpp
void AAOSMapManager::DrawDebugTowerPositions()
{
    // Team1 타워 - 파란색 박스
    DrawDebugBox(GetWorld(), TowerPos, FVector(DebugBoxSize, DebugBoxSize, DebugBoxSize),
                 FColor::Blue, true, -1.0f, 0, 10.0f);

    // Team2 타워 - 빨간색 박스
    DrawDebugBox(GetWorld(), TowerPos, FVector(DebugBoxSize, DebugBoxSize, DebugBoxSize),
                 FColor::Red, true, -1.0f, 0, 10.0f);

    // Command Center - 노란색/주황색 박스 (1.5배 크기)
    DrawDebugBox(GetWorld(), CommandCenterPos,
                 FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
                 FColor::Yellow, true, -1.0f, 0, 10.0f);
}
```

### 디버그 박스 색상 구분
- **Team1 타워**: 파란색 박스
- **Team2 타워**: 빨간색 박스
- **Team1 Command Center**: 노란색 박스 (1.5배 크기)
- **Team2 Command Center**: 주황색 박스 (1.5배 크기)

### 블루프린트 설정
**파일**: [AOSMapManager.h:100-109](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSMapManager.h#L100-L109)

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Debug")
bool bShowDebugTowerBoxes = true;  // 디버그 박스 표시 ON/OFF

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Debug")
float DebugBoxSize = 200.0f;  // 박스 크기 조절
```

---

## 7. 에디터에서 라인 경로 시각화

### 구현 내용
**파일**: [AOSMapManager.cpp:25-171](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSMapManager.cpp#L25-L171)

에디터에서 MapManager 액터를 선택하면 라인 정보를 3D 공간에 시각화:

```cpp
#if WITH_EDITOR
void AAOSMapManager::OnConstruction(const FTransform& Transform)
{
    // LanesInfo가 비어있으면 기본값으로 초기화
    if (LanesInfo.Num() == 0)
        SetupDefaultLaneInfo();

    // 라인 경로 그리기 (Team1: 청록색, Team2: 마젠타색)
    DrawDebugLine(GetWorld(), StartPos, EndPos, Team1Color, true, -1.0f, 0, Thickness);

    // 타워 위치 표시 (구체 + 화살표)
    DrawDebugSphere(GetWorld(), TowerPos, 100.0f, 12, TeamColor, true, -1.0f, 0, Thickness);
    DrawDebugDirectionalArrow(GetWorld(), TowerPos, TowerPos + FVector(0, 0, 300),
                              50.0f, TeamColor, true, -1.0f, 0, Thickness);

    // Command Center 표시 (큰 박스)
    DrawDebugBox(GetWorld(), CommandCenterPos, FVector(150, 150, 150),
                 TeamColor, true, -1.0f, 0, Thickness * 2.0f);
}
#endif
```

### 시각화 요소
1. **라인 경로**:
   - Team1: 청록색(Cyan) 라인
   - Team2: 마젠타색(Magenta) 라인
   - 시작점에서 끝점까지 직선

2. **타워 위치**:
   - 구체(Sphere)로 표시
   - 위쪽 화살표로 번호 구분
   - Team1: 청록색, Team2: 마젠타색

3. **Command Center**:
   - 큰 박스로 표시
   - 일반 타워보다 2배 두꺼운 선

### 블루프린트 설정 옵션
**파일**: [AOSMapManager.h:111-125](c:\UnrealProject\TDProject\Source\TDProject\AOS\AOSMapManager.h#L111-L125)

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
bool bShowEditorVisualization = true;  // 전체 시각화 ON/OFF

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
bool bShowLanePaths = true;  // 라인 경로 표시

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
bool bShowTowerPositions = true;  // 타워 위치 표시

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
bool bShowCommandCenters = true;  // 커맨드 센터 표시

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Editor Visualization")
float EditorVisualizationThickness = 5.0f;  // 선 두께 조절
```

### 사용 방법
1. **에디터에서 BP_AOSMapManager를 레벨에 배치**
2. **액터를 선택하면 자동으로 시각화 표시**
3. **Details 패널**에서:
   - `LanesInfo` 배열을 직접 편집하여 위치 조정
   - 값을 변경할 때마다 실시간으로 시각화 업데이트
   - `Editor Visualization` 카테고리에서 표시 옵션 조정

### 주요 특징
- ✅ **에디터 전용**: 런타임에는 영향 없음 (`#if WITH_EDITOR`)
- ✅ **실시간 업데이트**: LanesInfo 값 변경 시 즉시 반영
- ✅ **하드코딩 해결**: 블루프린트에서 직접 편집 가능
- ✅ **시각적 피드백**: 타워와 라인 경로를 한눈에 확인

---

## 입력 바인딩 설정

프로젝트 설정에서 다음 입력 바인딩 필요:

### Axis Mappings
- **MoveForward**: W (Scale: 1.0), S (Scale: -1.0), Up Arrow (Scale: 1.0), Down Arrow (Scale: -1.0)
- **MoveRight**: D (Scale: 1.0), A (Scale: -1.0), Right Arrow (Scale: 1.0), Left Arrow (Scale: -1.0)
- **CameraZoom**: Mouse Wheel Up (Scale: 1.0), Mouse Wheel Down (Scale: -1.0)

### Action Mappings
- **LeftMouseClick**: Left Mouse Button

---

## 완성된 기능 요약

### RTS 카메라 시스템
✅ 4방향 자유 이동 (WASD / 화살표 키)
✅ 카메라 방향 기준 이동 (직관적인 조작)
✅ 마우스 휠 줌 인/아웃
✅ 블루프린트에서 카메라 각도/높이/속도 조정 가능
✅ 맵 경계 제한
✅ 마우스 클릭으로 캐릭터 선택

### 게임 시스템
✅ DefaultPawnClass와 CharacterClass 분리 (RTS 모드)
✅ 라인별 독립적인 경로 시스템
✅ AI 캐릭터 자동 스폰 및 라인 이동
✅ 타워 위치 정상 배치

### 디버그 & 시각화 도구
✅ 런타임 타워 위치 디버그 박스 (색상별 팀 구분)
✅ 에디터에서 라인 경로 실시간 시각화
✅ 블루프린트에서 LanesInfo 직접 편집 가능
✅ 타워/Command Center 위치 에디터 미리보기

---

## 테스트 방법

### 에디터 테스트
1. **BP_AOSMapManager를 레벨에 배치**
2. **액터 선택 시**:
   - 라인 경로가 청록색/마젠타색 라인으로 표시되는지 확인
   - 타워 위치에 구체와 화살표가 표시되는지 확인
   - Command Center 위치에 큰 박스가 표시되는지 확인
3. **LanesInfo 편집**:
   - Details 패널에서 타워 위치 변경 시 실시간 반영 확인

### 런타임 테스트
1. **에디터에서 플레이**
2. **카메라 동작 확인**:
   - WASD로 카메라 4방향 이동
   - 마우스 휠로 줌 인/아웃
   - 맵 경계 제한 동작 확인
3. **AI 동작 확인**:
   - 캐릭터들이 Top, Mid, Bottom 라인을 따라 분산되는지 확인
   - 타워 위치에 파란색/빨간색/노란색 디버그 박스 표시 확인
4. **선택 기능**:
   - 마우스 클릭으로 캐릭터 선택 가능한지 확인

---

## 다음 작업 예정

- [ ] 선택된 캐릭터 하이라이트 표시 (현재 TODO 항목)
- [ ] 미니맵 UI 추가
- [ ] 캐릭터 상태 UI 표시
- [ ] 다중 선택 기능 (드래그 박스)

---

**작업 완료일**: 2026-01-01
**빌드 상태**: ✅ 성공
**테스트 상태**: 에디터 테스트 필요
