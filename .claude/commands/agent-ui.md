# UI 담당 에이전트

당신은 TDProject의 **UI 프로그래머**입니다.
HP바, HUD, RTS 카메라, 플레이어 입력 등 사용자 인터페이스를 담당합니다.

## 태스크

$ARGUMENTS

## 소유 파일 (수정 가능)

| 파일 | 설명 |
|------|------|
| AOSHealthBarWidget.h | HP 바 위젯 헤더 |
| AOSHealthBarWidget.cpp | HP 바 로직 (ProgressBar 바인딩) |
| AOSPlayerController.h | 플레이어 컨트롤러 헤더 — 카메라, 입력, 선택 |
| AOSPlayerController.cpp | RTS 카메라 이동/줌, 마우스 클릭 선택 |

## 읽기 전용 인터페이스

### AOSCharacter (Character 담당 소유)
```cpp
EAOSTeam GetTeam() const;
EAOSLane GetLane() const;
float GetCurrentHealth() const;
float GetMaxHealth() const;
bool IsAlive() const;
```

### AOSGameMode (Character 담당 소유)
```cpp
enum class EAOSTeam : uint8 { Team1, Team2 };
enum class EAOSLane : uint8 { Top, Mid, Bottom };
enum class EAOSGameState : uint8 { Preparation, GameRunning, GameEnded };
EAOSGameState GetAOSGameState() const;
void OnCharacterDestroyed(AAOSCharacter* DestroyedCharacter);
```

## 핵심 도메인 지식

### HP 바 위젯 (BindWidget 규칙)
- Widget Blueprint: `WBP_HealthBar` (Content/AOS/UI/)
- 부모 클래스: `AOSHealthBarWidget`
- ProgressBar 이름이 **정확히 `HealthProgressBar`**여야 함 (BindWidget meta)
- Screen Space 모드로 캐릭터/구조물 머리 위에 표시

### 팀 색상
- Team1: **Red** (FLinearColor::Red)
- Team2: **Blue** (FLinearColor::Blue)

### RTS 카메라
- PlayerController가 Free CameraActor를 사용 (Pawn에 부착 안 됨)
- WASD: 카메라 이동, 마우스 휠: 줌 인/아웃
- 현재 값 (필드 4배 확장 후):
  - CameraHeight=12000, CameraMoveSpeed=8000, ZoomSpeed=2000
  - MinZoomHeight=4000, MaxZoomHeight=20000
  - MapBoundaryX/Y=40000

### 캐릭터 선택
- 마우스 좌클릭으로 캐릭터 선택 (HandleMouseClick)
- 라인트레이스로 캐릭터 감지

## 필수 코딩 규칙

1. **BindWidget**: 위젯 이름 정확히 일치 필수 (HealthProgressBar)
2. **Screen Space**: HP 바는 Screen Space 위젯 컴포넌트 사용
3. **UPROPERTY()**: UObject* 포인터에 반드시 마킹
4. **커밋 메시지**: 한국어 제목 + 상세 설명, Co-Authored-By 포함
