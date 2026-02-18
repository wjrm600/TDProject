# AOS (Auto Oriented Strategy) 게임 시스템 개요

**엔진**: Unreal Engine 5.7 / Visual Studio 2026

## 프로젝트 구조

프로젝트는 다음과 같은 핵심 클래스들로 구성되어 있습니다:

### 1. **AOSGameMode** (AOSGameMode.h/cpp)
- 게임의 메인 모드 클래스
- 게임 상태 관리 (준비, 진행, 종료)
- 라운드 시간 관리 (기본값: 10분/600초)
- 승리 조건 확인 (상대 커맨드 센터 파괴)
- CharacterClass를 사용한 AI 캐릭터 스폰 (DefaultPawnClass와 분리)
- 사망한 캐릭터를 팀 배열에서 제거 (OnCharacterDestroyed)

**주요 기능:**
```cpp
- StartGame(): 게임 시작 (SpawnCharactersAtAllSpawnPoints 자동 호출)
- EndGame(WinningTeam): 게임 종료
- SpawnCharactersAtAllSpawnPoints(): 모든 스폰 포인트에서 캐릭터 자동 생성
- CheckVictoryConditions(): 승리 조건 확인
- OnCharacterDestroyed(Character): 사망 캐릭터를 팀 배열에서 제거
```

### 2. **AOSCharacter** (AOSCharacter.h/cpp)
- AI가 자동 제어하는 기본 캐릭터 클래스
- 스폰 포인트에서 자동 생성됨
- 라인 할당 시스템 (탑, 미드, 바텀)
- 사망 시 자동 처리 (메시 숨김 → 2초 후 액터 제거, 리스폰 없음)
- HP 바 위젯 (UWidgetComponent, 캐릭터 상단 Screen Space 표시)

**주요 속성:**
```cpp
- Team: 팀 정보 (Team1/Team2)
- AssignedLane: 할당된 라인
- MaxHealth: 최대 체력 (기본값: 100)
- AttackDamage: 공격 데미지 (기본값: 10)
- AttackRange: 공격 범위 (기본값: 500)
- MovementSpeed: 이동 속도 (기본값: 600)
```

**사망 처리 (OnCharacterDeath):**
```
1. 이동 즉시 정지
2. 콜리전 비활성화
3. 메시 숨기기
4. HP 바 숨기기
5. GameMode에 사망 알림 (팀 배열에서 제거)
6. 2초 후 액터 Destroy
```

### 3. **AOSAIController** (AOSAIController.h/cpp)
- 캐릭터의 자동 행동 관리
- **웨이포인트 큐 시스템** 기반 라인 푸시
- 적 캐릭터 및 적 구조물 탐지 및 자동 공격
- Tick 기반 AI (State Tree 미사용)

**AI 행동 흐름:**
```
1. 배포: BuildWaypointQueue()로 이동 경로 구축
   (아군 타워 → 적 타워 → 적 커맨드 센터 순서)
2. 이동: 웨이포인트 큐를 따라 순차 이동
3. 탐지: 근처 적(EnemyDetectionRange 내)을 감지
4. 전투:
   - 적 캐릭터 발견 → AttackTarget()으로 공격
   - 웨이포인트가 적 구조물 → AttackStructure()로 공격 (이동 + 사거리 내 공격)
5. 복귀: 적 제거 후 다음 웨이포인트로 계속 이동
6. 사망 감지: Tick에서 bAlive 확인 → 이동 중지, 큐 비우기, 레퍼런스 해제
```

**주요 설정값:**
| 설정 | 값 | 설명 |
|------|-----|------|
| EnemyDetectionRange | 1500.0f | 적 캐릭터 감지 범위 |
| AttackRange | 500.0f | 적 캐릭터 공격 가능 범위 |
| ArrivalDistance | 100.0f | 웨이포인트 도착 판정 거리 |
| AttackCooldownDuration | 1.0f | 공격 쿨타임 |

### 4. **AOSStructure** (AOSStructure.h/cpp)
- 맵에 배치되는 구조물 (타워, 커맨드 센터)
- 체력 관리 및 파괴 시스템
- 자동 방어 시스템 (범위 내 적을 자동 공격)
- 충돌 비활성화 (캐릭터가 통과 가능)
- HP 바 위젯 (UWidgetComponent, 구조물 상단 표시)
- 파괴 시: 메시 숨김 + 감지 비활성화 + HP 바 숨기기 + Tick 중지

**구조물 종류:**
- **Tower**: 각 라인 3개씩, 팀당 9개 (체력: 1000)
- **CommandCenter**: 팀당 1개 (체력: 5000, 파괴 시 게임 패배)

**구조물 스탯:**
| 설정 | 값 | 설명 |
|------|-----|------|
| AttackDamage | 20.0f | 공격 데미지 |
| AttackRange | 200.0f | 공격 사거리 |
| AttackCooldown | 2.0f | 공격 쿨타임 |
| DetectionRange | 400.0f | 적 캐릭터 감지 범위 |

### 5. **AOSPlayerController** (AOSPlayerController.h/cpp)
- RTS 스타일 카메라 제어 (WASD 이동, 마우스 휠 줌)
- 마우스 클릭으로 캐릭터 선택
- DefaultPawnClass = None (RTS 모드)

### 6. **AOSMapManager** (AOSMapManager.h/cpp)
- 맵 레이아웃 관리
- 3개 라인 설정 (Top, Mid, Bottom) - LanesInfo 배열
- 팀별 Command Center 위치 (Team1CommandCenterPosition, Team2CommandCenterPosition)
- BeginPlay에서 타워/커맨드 센터 동적 생성 (SpawnStructures)
- 에디터 시각화 (라인 경로, 타워 위치 실시간 표시)
- 런타임 디버그 박스 (Team1: 파란색, Team2: 빨간색)

**중요 구분:**
- `LanesInfo` = 에디터 설정 데이터 (블루프린트에서 편집)
- `AllTowers` = 런타임 스폰된 실제 타워 인스턴스 (UPROPERTY 필수)

### 7. **AOSSpawnPoint** (AOSSpawnPoint.h/cpp)
- 캐릭터 스폰 위치 지정
- 팀/라인/인덱스 정보 저장
- bSpawnEnabled: 개별 스폰 포인트 활성화/비활성화 (디버깅용)
- 12개 필요 (2팀 x 3라인 x 2개)

### 8. **AOSHealthBarWidget** (UI/AOSHealthBarWidget.h/cpp)
- UUserWidget 상속, 3D 월드 위젯으로 HP 바 표시
- BindWidget 메타로 ProgressBar 바인딩 (이름: `HealthProgressBar`)
- 팀별 색상 설정 (Team1: 빨강, Team2: 파랑)
- 캐릭터/구조물 머리 위에 Screen Space로 표시
- Widget Blueprint `WBP_HealthBar` 에디터에서 생성 후 BP_Character 등에 할당

## 게임 플로우

### 현재 구현된 플로우 (자동 스폰)
```
1. 레벨 로드 → AOSMapManager::BeginPlay() → SpawnStructures()
2. AOSSpawnPoint::BeginPlay() → SpawnCharacterAtPoint() (bSpawnEnabled 확인)
3. 캐릭터 생성 → InitializeCharacter() (팀/라인 설정)
4. DeployToLane() → AOSAIController::StartDeployment()
5. BuildWaypointQueue() → 아군 타워 → 적 타워 → 적 커맨드 센터
6. UpdateAIBehavior() 매 Tick 실행 (적 탐지 → 공격 또는 이동)
```

### 게임 종료 (GameEnded)
- 상대 팀의 커맨드 센터 파괴: 즉시 승리
- 시간 만료: 점수로 승자 결정 (미구현)

## 파일 구조

```
Source/TDProject/AOS/
├── AOSGameMode.h/cpp          # 게임 모드 및 캐릭터 스폰 관리
├── AOSCharacter.h/cpp         # AI 캐릭터 (자동 제어)
├── AOSAIController.h/cpp      # AI 컨트롤러 (웨이포인트 큐 시스템)
├── AOSPlayerController.h/cpp  # RTS 카메라 및 선택
├── AOSStructure.h/cpp         # 타워/커맨드 센터
├── AOSMapManager.h/cpp        # 맵 관리 및 구조물 생성
├── AOSSpawnPoint.h/cpp        # 캐릭터 스폰 지점
└── UI/
    └── AOSHealthBarWidget.h/cpp  # HP 바 위젯 (3D 월드 UI)
```

## 다음 단계

### 즉시 필요
- 캐릭터 메시 할당 (SetupMesh)
- ~~캐릭터 사망/리스폰 처리~~ ✅ 완료 (2026-02-18)
- ~~구조물 파괴 이펙트~~ ✅ 완료 (2026-02-18, 간단 버전)
- ~~HP 바 UI~~ ✅ 완료 (2026-02-18, 3D 월드 위젯)

### 단기 목표
- Widget Blueprint 생성 및 캐릭터/구조물 연결 (에디터 작업)
- UI 시스템 확장 (타이머, 팀 정보, 킬 카운트)
- AI 파라미터 밸런싱
- 선택된 캐릭터 하이라이트

### 장기 목표
- 네트워크 멀티플레이
- 캐릭터 클래스 다양화 (탱커, 딜러, 서포터)
- 스킬 시스템

---

**마지막 업데이트**: 2026-02-18
**상태**: 핵심 시스템 완성 (웨이포인트 큐, 자동 스폰, RTS 카메라, 사망/파괴 처리, HP 바 UI), 에디터 연결 및 콘텐츠 추가 필요
