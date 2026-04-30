# 🏰 타워 및 커맨드 센터 시스템 가이드

**상태**: ✅ 구현 완료 및 컴파일 성공

**작성일**: 2025-11-23

---

## 📚 목차

1. [시스템 개요](#시스템-개요)
2. [구조 및 설계](#구조-및-설계)
3. [타워 생성 프로세스](#타워-생성-프로세스)
4. [각 클래스 설명](#각-클래스-설명)
5. [사용 방법](#사용-방법)
6. [PIE 테스트](#pie-테스트)

---

## 시스템 개요

### 🎯 목표

**전략 게임에 타워 방어 요소 추가**

3개 라인(Top, Mid, Bottom) 각각에:
- **타워**: 팀당 3개씩 (총 18개)
- **커맨드 센터**: 팀당 1개씩 (총 2개)

### 📊 구조

```
AOSMapManager
├─ 맵 레이아웃 정의
├─ 3개 라인 구성
└─ 타워 및 커맨드 센터 생성

↓

AOSGameMode
├─ MapManager 초기화
├─ 생성된 구조물 캐시
└─ 게임 중 구조물 관리

↓

AOSStructure
├─ 타워 (1000 HP)
└─ 커맨드 센터 (5000 HP)
```

---

## 구조 및 설계

### 🏗️ 클래스 계층도

```
AActor
├─ AAOSMapManager (맵 관리)
│  └─ SpawnStructures() → AAOSStructure 생성
│
├─ AAOSGameMode (게임 관리)
│  └─ InitializeStructures()
│     ├─ InitializeMapManager()
│     └─ CacheTowerReferences()
│
└─ AAOSStructure (구조물)
   ├─ Tower (타워)
   └─ CommandCenter (커맨드 센터)
```

### 🔄 데이터 흐름

```
게임 시작 (BeginPlay)
  ↓
AOSGameMode::BeginPlay()
  ├─ RegisterSpawnPoint() (캐릭터 스폰 포인트)
  ├─ InitializeStructures()
  │  ├─ InitializeMapManager()
  │  │  ├─ MapManager 찾기/생성
  │  │  ├─ InitializeMap() (기본값 설정)
  │  │  └─ SpawnStructures() (타워/커맨드 센터 생성)
  │  └─ CacheTowerReferences()
  │     ├─ 타워들을 LaneTowers 맵에 저장
  │     └─ 커맨드 센터를 CommandCenters 맵에 저장
  └─ Preparation 상태로 설정
```

---

## 타워 생성 프로세스

### Step 1️⃣: 맵 정보 설정 (FLaneInfo)

**파일**: `AOSMapManager.h`

```cpp
struct FLaneInfo {
    EAOSLane LaneType;

    // Team1 라인 — 타워 좌표만
    TArray<FVector> Team1TowerPositions;  // 3개

    // Team2 라인 — 타워 좌표만
    TArray<FVector> Team2TowerPositions;  // 3개
};
```

> **NOTE**: 라인 시작 위치(`Team*StartPosition`) 와 종료 위치(`Team*EndPosition`),
> Command Center 위치는 `FLaneInfo` 에서 제거되었음. 라인 시작 위치는 `(Team, Lane)`
> 매칭되는 `AAOSSpawnPoint` 액터의 `GetActorLocation()` 으로 일원화. CC 위치는
> `AAOSMapManager::Team1CommandCenterPosition` / `Team2CommandCenterPosition` 멤버에 보유.

**특징**:
- 3개 라인 각각에 대한 타워 좌표 저장
- 팀별로 대칭된 위치 정의
- 기본값 (SetupDefaultLaneInfo)으로 자동 설정

### Step 2️⃣: 타워 생성 (SpawnStructures)

**파일**: `AOSMapManager.cpp`

```cpp
void AAOSMapManager::SpawnStructures()
{
    for (const FLaneInfo& LaneInfo : LanesInfo) {
        // Team1 타워 3개 생성
        for (const FVector& TowerPos : LaneInfo.Team1TowerPositions) {
            AAOSStructure* Tower = GetWorld()->SpawnActor<AAOSStructure>(...);
            Tower->Initialize(EStructureType::Tower, EAOSTeam::Team1, Lane);
        }

        // Team2 타워 3개 생성
        for (const FVector& TowerPos : LaneInfo.Team2TowerPositions) {
            AAOSStructure* Tower = GetWorld()->SpawnActor<AAOSStructure>(...);
            Tower->Initialize(EStructureType::Tower, EAOSTeam::Team2, Lane);
        }
    }

    // 커맨드 센터 생성 (Mid Lane)
    // Team1, Team2 각각 1개씩
}
```

### Step 3️⃣: 구조물 초기화 (Initialize)

**파일**: `AOSStructure.cpp`

```cpp
void AAOSStructure::Initialize(EStructureType Type, EAOSTeam Team, EAOSLane Lane)
{
    StructureType = Type;
    OwnerTeam = Team;
    Lane = Lane;

    // 타입에 따라 체력 설정
    if (Type == EStructureType::CommandCenter) {
        MaxHealth = 5000.0f;  // 커맨드 센터
    } else {
        MaxHealth = 1000.0f;  // 타워
    }

    CurrentHealth = MaxHealth;
}
```

### Step 4️⃣: 참조 캐싱 (CacheTowerReferences)

**파일**: `AOSGameMode.cpp`

```cpp
void AAOSGameMode::CacheTowerReferences()
{
    // 각 라인별 타워들을 LaneTowers 맵에 저장
    for (int32 Lane = 0; Lane < 3; ++Lane) {
        // Team1 타워 추가
        TArray<AAOSStructure*> Team1Towers =
            MapManager->GetTowersInLane(Lane, Team1);
        LaneTowers.Add(Lane, Team1Towers);

        // Team2 타워 추가
        TArray<AAOSStructure*> Team2Towers =
            MapManager->GetTowersInLane(Lane, Team2);
        for (AAOSStructure* Tower : Team2Towers) {
            LaneTowers[Lane].Add(Tower);
        }
    }

    // 커맨드 센터 캐싱
    CommandCenters.Add(Team1, MapManager->GetCommandCenter(Team1));
    CommandCenters.Add(Team2, MapManager->GetCommandCenter(Team2));
}
```

---

## 각 클래스 설명

### 🗺️ AAOSMapManager (맵 관리자)

**책임**: 맵 레이아웃 정의 및 타워/커맨드 센터 생성

**주요 함수**:

| 함수 | 설명 |
|------|------|
| `InitializeMap()` | 맵 정보 초기화 (기본값 설정) |
| `SpawnStructures()` | 타워 및 커맨드 센터 생성 |
| `GetTowersInLane()` | 특정 라인의 팀별 타워 조회 |
| `GetCommandCenter()` | 팀별 커맨드 센터 조회 |
| `SetupDefaultLaneInfo()` | 기본 라인 정보 설정 |

**기본 타워 배치**:
```
각 라인마다:
Team1 타워 위치: [1200, ...], [600, ...], [0, ...]
Team2 타워 위치: [-1200, ...], [-600, ...], [0, ...]

커맨드 센터는 Mid Lane의 깊은 뒤쪽에 배치
```

---

### 🎮 AOSGameMode (게임 관리자)

**책임**: 맵 매니저 초기화 및 구조물 참조 관리

**새로운 함수**:

| 함수 | 설명 |
|------|------|
| `InitializeMapManager()` | 🟢 맵 매니저 찾기/생성 |
| `CacheTowerReferences()` | 🟢 타워 및 커맨드 센터 캐싱 |
| `InitializeStructures()` | 🟡 수정됨 |
| `GetCommandCenter()` | 팀별 커맨드 센터 반환 |
| `GetTowersByLane()` | 🟡 수정됨 - 라인별 타워 필터링 |

**구조물 저장소**:
```cpp
TMap<EAOSTeam, AAOSStructure*> CommandCenters;
// Key: Team1/Team2, Value: 커맨드 센터

TMap<EAOSLane, TArray<AAOSStructure*>> LaneTowers;
// Key: Top/Mid/Bottom, Value: 해당 라인의 모든 타워
```

---

### 🏰 AAOSStructure (구조물)

**책임**: 타워 및 커맨드 센터 자체

**핵심 기능**:

| 기능 | 설명 |
|------|------|
| **체력** | 타워 1000, 커맨드 센터 5000 |
| **공격** | 범위 내 적군 자동 공격 (2초마다) |
| **방어** | ReceiveDamage로 피해 처리 |
| **탐지** | DetectionRange 1500.0 내 적군 감지 |

**자동 공격 로직**:
```cpp
void Tick(float DeltaTime) {
    if (!IsDestroyed()) {
        UpdateAttackTarget();  // 목표 갱신

        if (CurrentAttackCooldown > 0) {
            CurrentAttackCooldown -= DeltaTime;
        } else if (CurrentTarget && CurrentTarget->IsAlive()) {
            FireAtTarget(CurrentTarget);
            CurrentAttackCooldown = 2.0f;
        }
    }
}
```

---

## 사용 방법

### 🎯 기본 설정

#### 1. 레벨에 MapManager 추가 (선택사항)

```
레벨 배치 → AAOSMapManager 드래그
```

**특징**:
- 만약 레벨에 없으면 AOSGameMode가 자동으로 생성
- 위치는 중요하지 않음 (기본값 사용)

#### 2. GameMode 설정 (필수)

```
World Settings → Game Mode → AOSGameMode
```

#### 3. Level Blueprint 연결 (필수)

```
Event BeginPlay → AOSGameMode.StartGame()
```

### 🔍 타워에 접근하기

**예시 1: 특정 라인의 타워 가져오기**
```cpp
TArray<AAOSStructure*> TopTowers =
    GameMode->GetTowersByLane(EAOSLane::Top, EAOSTeam::Team1);

// TopTowers[0], TopTowers[1], TopTowers[2] = 3개 타워
```

**예시 2: 커맨드 센터 가져오기**
```cpp
AAOSStructure* Team1Center =
    GameMode->GetCommandCenter(EAOSTeam::Team1);

if (Team1Center && Team1Center->IsDestroyed()) {
    // Team2 승리!
}
```

**예시 3: 타워의 체력 확인**
```cpp
for (AAOSStructure* Tower : Towers) {
    float Health = Tower->GetCurrentHealth();
    float MaxHealth = Tower->GetMaxHealth();
    float HealthPercent = Health / MaxHealth * 100.0f;

    UE_LOG(LogTemp, Warning, TEXT("Tower Health: %.1f%%"), HealthPercent);
}
```

---

## PIE 테스트

### 🧪 테스트 체크리스트

- [ ] 레벨 실행
- [ ] 콘솔에 타워 생성 로그 확인
  ```
  AOSMapManager created and structures spawned
  Team1 CommandCenter cached
  Team2 CommandCenter cached
  All structures initialized successfully
  ```

- [ ] 뷰포트에서 타워 시각 확인
  - Team1 타워 (빨간색 콜리전 스피어)
  - Team2 타워 (파란색 콜리전 스피어)
  - 커맨드 센터 (더 큰 스피어)

- [ ] 캐릭터가 타워 범위에 들어가면 자동 공격 확인
- [ ] 타워 체력 감소 확인
- [ ] 커맨드 센터 파괴 시 게임 종료 확인

### 📋 콘솔 명령어

```
stat Unit  // 성능 모니터링
log LogTemp Warning  // 경고 로그 확인
```

### 🐛 일반적인 문제

| 문제 | 원인 | 해결 |
|------|------|------|
| 타워가 보이지 않음 | 메시 미설정 | 메시 할당 필요 |
| 타워가 공격하지 않음 | 캐릭터 범위 밖 | 캐릭터를 타워 근처로 이동 |
| 게임 충돌 | MapManager 중복 | 레벨에서 MapManager 제거 |
| 콘솔 에러 | 레벨에 GameMode 미설정 | World Settings 확인 |

---

## 🔄 시스템 통합

### 게임 플로우 통합

```
BeginPlay
  ↓
AOSGameMode::BeginPlay()
  ├─ RegisterSpawnPoint()       ✅ 스폰 포인트 등록
  ├─ InitializeStructures()     ✅ 타워/커맨드 센터 생성
  └─ AOSGameState = Preparation
  ↓
StartGame() 호출
  ├─ AOSGameState = GameRunning
  └─ SpawnCharactersAtAllSpawnPoints() ✅ 캐릭터 자동 생성
  ↓
게임 진행 (Tick)
  ├─ 캐릭터 AI 순찰
  ├─ 타워 자동 공격
  └─ 게임 시간 카운트다운
  ↓
CheckVictoryConditions()
  ├─ CommandCenter.IsDestroyed()?
  └─ 승리 팀 결정 → EndGame()
```

### 캐릭터-타워 상호작용

```
AAOSCharacter가 타워 범위(1500) 진입
  ↓
AAOSStructure::FindNearestEnemy()
  ├─ 적군 팀 확인
  ├─ 거리 계산
  └─ CurrentTarget 설정
  ↓
AAOSStructure::FireAtTarget()
  └─ Target->ReceiveDamage(AttackDamage)
  ↓
AAOSCharacter::ReceiveDamage()
  ├─ CurrentHealth -= Damage
  └─ Health <= 0? → 캐릭터 제거
```

---

## 📊 통계

### 생성 구조물 개수

| 타입 | 개수 | 체력 | 설명 |
|------|------|------|------|
| 타워 | 18개 | 1000 | 3 라인 × 2팀 × 3개 |
| 커맨드 센터 | 2개 | 5000 | 1개 라인 × 2팀 |
| **합계** | **20개** | - | - |

### 공격 특성

| 속성 | 타워 | 커맨드 센터 |
|------|------|-----------|
| **공격력** | 20 | 20 |
| **공격 쿨타임** | 2초 | 2초 |
| **감지 범위** | 1500 | 1500 |
| **공격 범위** | 1000 | 1000 |

---

## 📝 마지막 메모

### 구현된 기능

✅ 타워 자동 생성 및 배치
✅ 커맨드 센터 생성
✅ 타워 자동 공격
✅ 체력 관리
✅ GameMode 통합

### 다음 단계

- [ ] 타워 메시 할당
- [ ] 타워 공격 애니메이션/이펙트
- [ ] 타워 파괴 애니메이션
- [ ] UI에서 타워 정보 표시 (체력 바 등)
- [ ] 타워 업그레이드 시스템 (선택사항)

### 기술적 주목사항

**장점**:
- 완전 자동 생성 (레벨 배치 불필요)
- 쉬운 커스터마이징 (SetupDefaultLaneInfo 수정)
- 명확한 책임 분리 (MapManager ↔ GameMode)

**고려사항**:
- 기본값이 하드코딩되어 있음 (에디터에서 수정 가능)
- 메시 할당 필수
- 네트워크 리플리케이션 미설정

---

**마지막 업데이트**: 2025-11-23
**상태**: ✅ 구현 완료 및 테스트 준비
**컴파일**: ✅ SUCCESS (에러 0개)
