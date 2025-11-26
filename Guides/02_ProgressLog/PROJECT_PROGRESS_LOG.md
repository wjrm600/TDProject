# 📋 AOS 게임 프로젝트 진행 일지

**프로젝트명**: Tower Defense AOS Game (1v1 자동 전략 게임)
**엔진**: Unreal Engine 5.6
**시작일**: 2025-11-16
**현재일**: 2025-11-17

---

## 📚 목차

1. [초기 요구사항](#1-초기-요구사항)
2. [Phase 1: 핵심 시스템 아키텍처](#2-phase-1-핵심-시스템-아키텍처)
3. [Phase 2: 스폰 포인트 시스템](#3-phase-2-스폰-포인트-시스템)
4. [Phase 3: 자동 캐릭터 생성 시스템](#4-phase-3-자동-캐릭터-생성-시스템)
5. [Phase 4: 위치 설정 문제 해결](#5-phase-4-위치-설정-문제-해결)
6. [주요 학습사항 및 기술 결정](#주요-학습사항-및-기술-결정)

---

## 1. 초기 요구사항

### 📌 사용자 요청 (Message 1-2)

**주요 내용**:
> "1대1 자동 AOS 게임을 만들꺼야"

**게임 기본 사양**:
- **플레이어 수**: 1v1 (팀별 1명씩)
- **캐릭터**: 플레이어당 4마리 (자동 AI 제어)
- **맵 구조**: 3개 라인 (Top, Mid, Bottom)
- **게임 시간**: 10분 (600초) 라운드
- **승리 조건**: 상대 팀의 Command Center 파괴
- **방어**: 각 라인마다 3개의 타워

### 📊 게임 플로우

```
Preparation Phase (준비 단계)
  ↓
Player가 4마리 캐릭터를 3개 라인에 배치
  ↓
GameRunning Phase (게임 진행 단계)
  ├─ 10분 동안 자동 전투
  ├─ 캐릭터들이 라인 순찰
  ├─ 타워 및 적군과 자동 전투
  └─ Command Center 파괴 감지
  ↓
GameEnded Phase (게임 종료)
  └─ 승자 결정
```

---

## 2. Phase 1: 핵심 시스템 아키텍처

**진행 기간**: Message 3-5
**목표**: 7개 핵심 C++ 클래스 생성 및 컴파일 성공

### 🟢 생성된 클래스

#### 1️⃣ AOSGameMode (게임 관리자)
**파일**: `Source/TDProject/AOS/AOSGameMode.h/cpp`

**핵심 기능**:
- 게임 상태 관리 (Preparation → GameRunning → GameEnded)
- 라운드 타이머 관리 (600초)
- 스폰 포인트 등록 및 관리
- 캐릭터 배치 관리
- 구조물(타워, Command Center) 초기화
- 승리 조건 확인

**주요 함수**:
```cpp
void StartGame()                      // 게임 시작
void EndGame(EAOSTeam WinningTeam)   // 게임 종료
void RegisterSpawnPoint()             // 스폰 포인트 등록
AAOSSpawnPoint* GetNearestSpawnPoint() // 사용 가능한 스폰 포인트 찾기
void SpawnCharacter()                 // 캐릭터 생성 배치
void DeployCharacters()               // 플레이어 배치 정보 저장
```

**주요 프로퍼티**:
```cpp
EAOSGameState AOSGameState           // 현재 게임 상태
float RemainingGameTime              // 남은 시간
TArray<AAOSCharacter*> Team1Characters // 팀1 캐릭터들
TArray<AAOSCharacter*> Team2Characters // 팀2 캐릭터들
TArray<AAOSSpawnPoint*> AllSpawnPoints // 모든 스폰 포인트
TMap<EAOSTeam, TArray<AAOSSpawnPoint*>> TeamSpawnPoints // 팀별 스폰 포인트
```

---

#### 2️⃣ AOSCharacter (플레이어 캐릭터)
**파일**: `Source/TDProject/AOS/AOSCharacter.h/cpp`

**핵심 기능**:
- 플레이어가 관리하는 4마리 캐릭터의 기본 클래스
- 체력 시스템
- 공격 시스템 (데미지, 공격 범위, 쿨타임)
- 이동 속도 관리
- AI 컨트롤러 연결

**주요 함수**:
```cpp
void SetTeam(EAOSTeam NewTeam)      // 팀 설정
void SetLane(EAOSLane NewLane)      // 라인 설정
void DeployToLane()                 // AI 시작 (라인 배치)
void ReceiveDamage(float DamageAmount) // 피해 받기
bool IsAlive() const                // 생존 여부 확인
FVector GetLaneStartPosition() const // 라인 시작 위치
FVector GetLaneEndPosition() const   // 라인 끝 위치
```

**주요 프로퍼티**:
```cpp
EAOSTeam Team                       // 팀 (Team1/Team2)
EAOSLane AssignedLane              // 배치된 라인
float MaxHealth = 100.0f            // 최대 체력
float CurrentHealth                 // 현재 체력
float AttackDamage = 10.0f          // 공격 데미지
float AttackRange = 500.0f          // 공격 범위
float MovementSpeed = 600.0f        // 이동 속도
AAOSAIController* AOSAIController  // AI 컨트롤러
```

---

#### 3️⃣ AOSAIController (자동 제어)
**파일**: `Source/TDProject/AOS/AOSAIController.h/cpp`

**핵심 기능**:
- 캐릭터 자동 제어 (플레이어 개입 없음)
- 라인 순찰 AI
- 적군 감지 및 공격
- 상태 관리 (Idle, Patrolling, Attacking, Retreating)

**주요 함수**:
```cpp
void StartPatrolLane(EAOSLane Lane)  // 라인 순찰 시작
void FindNearestEnemy()              // 근처 적군 찾기
void AttackEnemy()                   // 적군 공격
void UpdateAIState(float DeltaTime)  // AI 상태 업데이트
```

**주요 상태**:
```cpp
enum EAOSAIState {
    Idle,           // 대기
    Patrolling,     // 순찰 중
    Attacking,      // 전투 중
    Retreating      // 후퇴 중
};
```

---

#### 4️⃣ AOSStructure (타워 및 커맨드 센터)
**파일**: `Source/TDProject/AOS/AOSStructure.h/cpp`

**핵심 기능**:
- 타워 (각 라인에 3개씩 배치)
- Command Center (팀 본진, 파괴 시 게임 종료)
- 자동 공격 (범위 내 적군 자동 공격)

**주요 타입**:
```cpp
enum EStructureType {
    Tower,           // 타워 (1000 HP, 20 데미지)
    CommandCenter    // 커맨드 센터 (5000 HP)
};
```

**주요 함수**:
```cpp
void TakeDamage(float DamageAmount)    // 피해 받기
void AttackInRange()                   // 범위 공격
bool IsDestroyed() const               // 파괴 여부
```

---

#### 5️⃣ AOSPlayerController (플레이어 관리)
**파일**: `Source/TDProject/AOS/AOSPlayerController.h/cpp`

**핵심 기능**:
- 플레이어별 게임 정보 관리
- 캐릭터 배치 정보 수집
- 게임 시작 신호

**주요 함수**:
```cpp
void SetCharacterDeployment(const TArray<EAOSLane>& LaneAssignments)
void DeployCharactersToLanes()      // 배치 실행
void StartGameFromPreparation()     // 게임 시작
void SetPlayerTeam(EAOSTeam Team)  // 팀 설정
```

---

#### 6️⃣ AOSMapManager (맵 관리)
**파일**: `Source/TDProject/AOS/AOSMapManager.h/cpp`

**핵심 기능**:
- 맵 레이아웃 정의 (3개 라인)
- 각 라인의 시작/끝 위치
- 타워 배치 위치

**주요 함수**:
```cpp
void InitializeMap()                 // 맵 초기화
void SpawnStructures()              // 구조물 생성
FLaneInfo GetLaneInfo(EAOSLane Lane) // 라인 정보 조회
```

---

#### 7️⃣ AOSSpawnPoint (스폰 위치)
**파일**: `Source/TDProject/AOS/AOSSpawnPoint.h/cpp`

**핵심 기능**:
- 캐릭터 스폰 위치 지정
- 팀/라인 정보 저장
- 스폰 포인트 점유 상태 관리

**주요 함수**:
```cpp
void Initialize(EAOSTeam Team, EAOSLane Lane, int32 Index)
void SetOccupiedCharacter(AAOSCharacter* Character)
void ReleaseCharacter()
AAOSCharacter* GetOccupiedCharacter() const
bool IsOccupied() const
```

---

### 🔴 발생한 에러 및 해결

#### Error 1: TMap with TArray reflection 미지원
**문제**: `TMap<EAOSTeam, TArray<AAOSStructure*>>` 선언 시 에러
```
The type 'TArray<AAOSStructure*>' can not be used as a value in a TMap
```

**해결**: UPROPERTY 매크로 제거, 순수 C++ 멤버로 변경
```cpp
// 변경 전 (❌)
UPROPERTY(BlueprintReadOnly)
TMap<EAOSTeam, TArray<AAOSStructure*>> CommandCenters;

// 변경 후 (✅)
TMap<EAOSTeam, TArray<AAOSStructure*>> CommandCenters; // UPROPERTY 제거
```

---

#### Error 2: 변수명 충돌 (Variable Shadowing)
**문제**: `GameState`, `Character`, `SpawnLocation` 등이 부모 클래스 멤버와 충돌

**해결**: 변수명 변경
```cpp
// GameState → AOSGameState
// Character → Char
// SpawnLocation → SpawnPos
```

---

#### Error 3: GetActorsByClass() 함수 없음
**문제**: UGameplayStatics에 GetActorsByClass() 함수가 없음 (UE 5.6)

**해결**: TActorIterator 사용으로 변경
```cpp
// 변경 전 (❌)
TArray<AAOSCharacter*> Characters;
UGameplayStatics::GetActorsByClass(GetWorld(), AAOSCharacter::StaticClass(), (TArray<AActor*>&)Characters);

// 변경 후 (✅)
for (TActorIterator<AAOSCharacter> CharItr(GetWorld()); CharItr; ++CharItr) {
    // 처리
}
```

---

### ✅ Phase 1 완료

**컴파일 결과**: ✅ SUCCESS
- 에러: 0개
- 경고: 0개 (Visual Studio 버전 경고 제외)
- 생성된 클래스: 7개
- 생성된 문서: 4개

---

## 3. Phase 2: 스폰 포인트 시스템

**진행 기간**: Message 6-8
**목표**: Lane 속성 추가 및 스폰 시스템 개선

### 📝 문제 인식

**사용자 피드백** (Message 6):
> "SpawnPoint에 Team 설정은 보이는데 Lane 설정은 안보여"

### 🟡 해결 과정

#### Step 1: Lane 속성 추가
**파일**: `AOSSpawnPoint.h`

```cpp
// 🟡 MODIFIED - Lane 속성 추가
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Spawn")
EAOSLane Lane = EAOSLane::Mid;
```

**이유**:
- 스폰 포인트가 어느 라인인지 에디터에서 설정 필요
- `GetNearestSpawnPoint()` 함수가 팀+라인으로 검색

---

#### Step 2: GetNearestSpawnPoint() 개선
**파일**: `AOSGameMode.cpp`

```cpp
// 변경 전: Team만 고려
AAOSSpawnPoint* GetNearestSpawnPoint(EAOSTeam Team, EAOSLane Lane) {
    // 팀의 모든 스폰 포인트 검색
}

// 변경 후: Team + Lane 모두 고려 (✅)
AAOSSpawnPoint* GetNearestSpawnPoint(EAOSTeam Team, EAOSLane Lane) {
    for (AAOSSpawnPoint* SpawnPoint : TeamSpawnPoints[Team]) {
        if (SpawnPoint->GetLane() == Lane && !SpawnPoint->IsOccupied()) {
            return SpawnPoint; // 해당 라인의 첫 번째 사용 가능 포인트 반환
        }
    }
    return nullptr;
}
```

**테스트 시나리오**:
```
배치: [Top, Top, Mid, Bottom]

1번 캐릭터 (라인: Top)
  → GetNearestSpawnPoint(Team1, Top)
  → SP_Team1_Top_0 반환 ✓

2번 캐릭터 (라인: Top)
  → GetNearestSpawnPoint(Team1, Top)
  → SP_Team1_Top_1 반환 (0은 이미 점유) ✓

3번 캐릭터 (라인: Mid)
  → GetNearestSpawnPoint(Team1, Mid)
  → SP_Team1_Mid_0 반환 ✓

4번 캐릭터 (라인: Bottom)
  → GetNearestSpawnPoint(Team1, Bottom)
  → SP_Team1_Bottom_0 반환 ✓
```

---

#### Step 3: 레벨 설정 가이드 작성
**생성 문서**:
- `UPDATED_SPAWN_SETUP.md` - Lane 설정 방법
- `SPAWN_SYSTEM_DIAGRAM.md` - 시각적 다이어그램

### ✅ Phase 2 완료

**개선사항**:
- Lane 속성 에디터에 노출 ✓
- GetNearestSpawnPoint()가 Team+Lane으로 정확히 검색 ✓
- 레벨 설정 가이드 제공 ✓

---

## 4. Phase 3: 자동 캐릭터 생성 시스템

**진행 기간**: Message 9-19
**목표**: 스폰 포인트에서 자동으로 캐릭터 생성

### 📌 핵심 결정사항

**사용자 요청** (Message 9):
> "캐릭터는 내가 직접 레벨에 배치하는 방식으로 안할꺼야... 게임모드에 있는 디폴트 폰 클래스를 Spawn Point에 세팅하는 방식으로 하고 싶어"

**2가지 접근 방식 제시**:

#### Approach A: 자동 생성 (선택됨) ✅
```
모든 스폰 포인트에서 게임 시작 시 자동으로 캐릭터 생성
장점: 간단함, 확장성 좋음
단점: 모든 캐릭터가 생성됨 (선택 불가)
```

#### Approach B: 선택적 생성
```
플레이어 배치 선택에 따라 해당 라인만 캐릭터 생성
장점: 유연함
단점: 복잡함
```

**사용자 선택**: "방식 A로 진행해줘"

---

### 🟢 구현 내용

#### Step 1: AOSSpawnPoint에 생성 함수 추가

**파일**: `AOSSpawnPoint.h/cpp`

```cpp
// 🟢 NEW - 스폰 포인트에서 캐릭터 생성
UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
class AAOSCharacter* SpawnCharacterAtPoint(TSubclassOf<class AAOSCharacter> CharacterClass);

// 🟢 NEW - 생성된 캐릭터 초기화
UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
void InitializeCharacter(class AAOSCharacter* Character);
```

**SpawnCharacterAtPoint() 구현**:
```cpp
AAOSCharacter* AAOSSpawnPoint::SpawnCharacterAtPoint(TSubclassOf<AAOSCharacter> CharacterClass)
{
    if (!CharacterClass || !GetWorld())
        return nullptr;

    // 스폰 포인트 위치에 캐릭터 생성
    AAOSCharacter* NewCharacter = GetWorld()->SpawnActor<AAOSCharacter>(
        CharacterClass,
        GetActorLocation(),
        FRotator::ZeroRotator
    );

    if (NewCharacter) {
        InitializeCharacter(NewCharacter);
    }

    return NewCharacter;
}
```

**InitializeCharacter() 구현**:
```cpp
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
    if (!Character)
        return;

    // 1. 캐릭터 팀/라인 설정
    Character->SetTeam(Team);
    Character->SetLane(Lane);

    // 2. 스폰 포인트에 등록
    SetOccupiedCharacter(Character);

    // 3. AI 시작
    Character->DeployToLane();

    UE_LOG(LogTemp, Warning, TEXT("Character spawned at SpawnPoint - Team: %d, Lane: %d"),
           static_cast<int32>(Team), static_cast<int32>(Lane));
}
```

---

#### Step 2: AOSGameMode에 일괄 생성 함수 추가

**파일**: `AOSGameMode.h/cpp`

```cpp
// 🟡 MODIFIED - StartGame() 수정
void AAOSGameMode::StartGame()
{
    if (AOSGameState == EAOSGameState::Preparation)
    {
        AOSGameState = EAOSGameState::GameRunning;
        RemainingGameTime = GameDuration;

        // 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
        SpawnCharactersAtAllSpawnPoints();
    }
}

// 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
void AAOSGameMode::SpawnCharactersAtAllSpawnPoints()
{
    if (!DefaultPawnClass)
    {
        UE_LOG(LogTemp, Error, TEXT("DefaultPawnClass not set!"));
        return;
    }

    // DefaultPawnClass가 AAOSCharacter 파생 클래스인지 확인
    if (!DefaultPawnClass->IsChildOf(AAOSCharacter::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("DefaultPawnClass is not a valid AAOSCharacter class!"));
        return;
    }

    // 모든 스폰 포인트에서 캐릭터 생성
    for (AAOSSpawnPoint* SpawnPoint : AllSpawnPoints)
    {
        if (SpawnPoint)
        {
            AAOSCharacter* NewCharacter = SpawnPoint->SpawnCharacterAtPoint(
                TSubclassOf<AAOSCharacter>(DefaultPawnClass)
            );

            if (NewCharacter)
            {
                // 팀별 리스트에 추가
                if (NewCharacter->GetTeam() == EAOSTeam::Team1)
                    Team1Characters.Add(NewCharacter);
                else
                    Team2Characters.Add(NewCharacter);
            }
        }
    }
}
```

---

#### Step 3: 기존 함수 제거

**파일**: `AOSGameMode.cpp`
```cpp
// 🔴 REMOVED: SpawnCharacter() 함수는 더 이상 사용되지 않습니다.
// SpawnCharactersAtAllSpawnPoints()로 대체되었습니다.
```

**파일**: `AOSPlayerController.cpp`
```cpp
// 🔴 REMOVED: SpawnPlayerCharacters() 함수는 더 이상 사용되지 않습니다.
// AOSGameMode의 SpawnCharactersAtAllSpawnPoints()로 대체되었습니다.
```

---

### 🔴 발생한 에러 및 해결

#### Error: DefaultPawnClass 타입 불일치
**문제**: `DefaultPawnClass`는 `TSubclassOf<APawn>`, 필요한 것은 `TSubclassOf<AAOSCharacter>`

**해결**:
```cpp
// 변경 전 (❌)
AAOSCharacter* NewCharacter = SpawnPoint->SpawnCharacterAtPoint(DefaultPawnClass);

// 변경 후 (✅)
if (!DefaultPawnClass->IsChildOf(AAOSCharacter::StaticClass())) {
    return; // 유효성 검사
}
TSubclassOf<AAOSCharacter> CharacterClass = DefaultPawnClass;
AAOSCharacter* NewCharacter = SpawnPoint->SpawnCharacterAtPoint(CharacterClass);
```

---

### ✅ Phase 3 완료

**컴파일 결과**: ✅ SUCCESS
- 에러: 0개
- 새로운 함수: 3개 추가
- 기존 함수: 2개 제거
- 생성된 문서: 2개

**생성 문서**:
- `AUTO_SPAWN_IMPLEMENTATION.md` - 구현 상세 설명
- `CODE_CHANGES_REFERENCE.md` - 변경사항 참고서

---

## 5. Phase 4: 위치 설정 문제 해결

**진행 기간**: Message 20-24
**목표**: 게임 실행 시 캐릭터가 올바른 위치에 배치되도록 수정

### 🐛 문제 분석

**사용자 보고**:
> "게임을 실행하면 캐릭터들이 이상한 위치에 설정되어 있어"

**근본 원인**: 위치가 4곳에서 중복으로 설정됨

```
생성 흐름:
1. SpawnActor (Line 58)
   └─ 스폰 포인트 위치로 설정

2. InitializeCharacter() (Line 81)
   └─ SetActorLocation() 호출 (중복!) ❌

3. SetOccupiedCharacter() (Line 38)
   └─ SetActorLocation() 호출 (중복!) ❌

4. DeployToLane() (Line 61)
   └─ GetLaneStartPosition()으로 재설정 (중복!) ❌

최종 결과: GetLaneStartPosition()에서 반환한 하드코딩 위치로 배치됨
```

---

### 🔧 해결 방법

#### Fix 1: SetOccupiedCharacter() 단순화

**파일**: `AOSSpawnPoint.cpp` (Line 31-40)

```cpp
// 변경 전 (❌)
void AAOSSpawnPoint::SetOccupiedCharacter(AAOSCharacter* Character)
{
    OccupiedCharacter = Character;

    if (Character)
    {
        Character->SetActorLocation(GetActorLocation()); // 중복!
    }
}

// 변경 후 (✅)
void AAOSSpawnPoint::SetOccupiedCharacter(AAOSCharacter* Character)
{
    OccupiedCharacter = Character;

    // 🟡 MODIFIED - 위치 설정 제거 (InitializeCharacter에서 처리)
    // 스폰 포인트 등록만 수행
}
```

---

#### Fix 2: InitializeCharacter() 정리

**파일**: `AOSSpawnPoint.cpp` (Line 70-91)

```cpp
// 변경 전 (❌)
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
    // ...
    Character->SetActorLocation(GetActorLocation()); // 중복!
    SetOccupiedCharacter(Character);
    Character->DeployToLane(); // 여기서 또 위치 변경
}

// 변경 후 (✅)
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
    if (!Character)
        return;

    // 1. 캐릭터 팀/라인 설정
    Character->SetTeam(Team);
    Character->SetLane(Lane);

    // 2. 스폰 포인트에 등록 (위치는 이미 SpawnActor에서 설정됨)
    SetOccupiedCharacter(Character);

    // 3. AI 시작 (이제 위치 변경 안 함)
    Character->DeployToLane();

    // 디버그 로그 개선
    UE_LOG(LogTemp, Warning, TEXT("Character spawned at SpawnPoint - Team: %d, Lane: %d, Position: (%.1f, %.1f, %.1f)"),
           static_cast<int32>(Team), static_cast<int32>(Lane),
           Character->GetActorLocation().X, Character->GetActorLocation().Y, Character->GetActorLocation().Z);
}
```

---

#### Fix 3: DeployToLane() 수정

**파일**: `AOSCharacter.cpp` (Line 57-72)

```cpp
// 변경 전 (❌)
void AAOSCharacter::DeployToLane()
{
    // 라인의 시작 위치로 캐릭터 배치
    FVector StartPos = GetLaneStartPosition();
    SetActorLocation(StartPos); // 스폰 포인트 위치 덮어씀!

    if (AOSAIController)
    {
        AOSAIController->StartPatrolLane(AssignedLane);
    }
}

// 변경 후 (✅)
void AAOSCharacter::DeployToLane()
{
    // 🟡 MODIFIED - 캐릭터는 이미 스폰 포인트 위치에 있으므로 추가 위치 변경하지 않음
    // 스폰 포인트가 정확한 배치 위치를 제공하므로 GetLaneStartPosition() 사용 제거

    // AI 컨트롤러에 라인 정보 전달하여 순찰 시작
    if (AOSAIController)
    {
        AOSAIController->StartPatrolLane(AssignedLane);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DeployToLane: AOSAIController not set for %s"), *GetName());
    }
}
```

---

### 📊 변경 전후 비교

**변경 전 (문제)**:
```
SpawnActor 위치 설정
  ↓
InitializeCharacter 위치 재설정 ❌
  ↓
SetOccupiedCharacter 위치 재설정 ❌
  ↓
DeployToLane 위치 재설정 ❌
  ↓
최종 위치 = GetLaneStartPosition() (예상과 다름!)
```

**변경 후 (올바름)**:
```
SpawnActor 위치 설정 ✅
  ↓
InitializeCharacter (팀/라인 설정만)
  ↓
SetOccupiedCharacter (등록만)
  ↓
DeployToLane (AI만 시작)
  ↓
최종 위치 = 스폰 포인트 위치 ✅
```

---

### ✅ Phase 4 완료

**컴파일 결과**: ✅ SUCCESS
- 에러: 0개
- 경고: 0개
- 수정된 파일: 3개
- 생성된 문서: 1개

**생성 문서**:
- `POSITION_FIX.md` - 위치 설정 문제 상세 분석 및 해결

---

## 주요 학습사항 및 기술 결정

### 📚 Unreal Engine 5.6 특수성

1. **Reflection System 제한**
   - `TMap<Enum, TArray<Actor*>>` 같은 복잡한 컨테이너는 UPROPERTY 불가
   - 해결: UPROPERTY 제거하고 순수 C++ 멤버 사용

2. **API 변경**
   - `UGameplayStatics::GetActorsByClass()` 제거됨
   - 해결: `TActorIterator<T>` 사용

3. **Live Coding**
   - 에디터 실행 중 CLI 빌드 불가능
   - 해결: 에디터 종료 후 빌드 필요

---

### 🎨 아키텍처 결정사항

#### 1. 게임 모드 기반 관리
```
GameMode가 중앙에서 모든 것을 관리
├─ 스폰 포인트 등록/관리
├─ 캐릭터 생성
├─ 게임 상태 관리
├─ 승리 조건 확인
└─ 타이머 관리
```

**장점**: 중앙화된 제어, 쉬운 디버깅
**단점**: GameMode 변경 시 영향 범위 넓음

---

#### 2. 스폰 포인트 패턴
```
AAOSSpawnPoint
├─ Team (팀 정보)
├─ Lane (라인 정보)
├─ SpawnIndex (순서)
└─ 캐릭터 생성 함수들
```

**장점**: 응집도 높음, 재사용 가능
**단점**: SpawnPoint 클래스 크기 증가

---

#### 3. 자동 생성 패턴 (Approach A)
```
게임 시작 → 모든 스폰 포인트에서 자동 생성
```

**선택 이유**:
- 구현 간단
- 확장성 좋음
- 일관성 유지

**대안 (Approach B)**: 선택적 생성 (미사용)

---

### 🔑 핵심 기술 결정

| 항목 | 결정 | 이유 |
|------|------|------|
| **캐릭터 배치** | 자동 생성 | 간편함, 스폰 포인트 기반 |
| **위치 설정** | 스폰 포인트 위치만 | 하드코딩 방지, 유연함 |
| **AI 제어** | GameMode → AIController | 중앙 관리 |
| **상태 관리** | Enum 기반 (Preparation/GameRunning/GameEnded) | 명확한 상태 전이 |
| **컨테이너** | Array + Map (Reflection 고려) | Unreal 시스템 호환 |

---

## 📊 프로젝트 통계

### 생성된 파일

| 카테고리 | 파일 수 | 설명 |
|---------|--------|------|
| **C++ 클래스** | 7개 | 핵심 게임 시스템 |
| **설명서** | 10개 | 개발 가이드 및 문서 |
| **로그** | 1개 | 이 문서 |

---

### 코드 변경 통계

| 항목 | 개수 |
|------|------|
| **새로 추가된 함수** | 8개 |
| **수정된 함수** | 5개 |
| **제거된 함수** | 2개 |
| **추가된 라인** | ~150줄 |
| **제거된 라인** | ~70줄 |
| **파일 수정** | 5개 |

---

### 컴파일 기록

| Phase | 날짜 | 상태 | 에러 |
|-------|------|------|------|
| Phase 1 | 2025-11-16 | ✅ SUCCESS | 0개 |
| Phase 2 | 2025-11-16 | ✅ SUCCESS | 0개 |
| Phase 3 (1차) | 2025-11-17 | ❌ FAILED | 1개 |
| Phase 3 (2차) | 2025-11-17 | ✅ SUCCESS | 0개 |
| Phase 4 | 2025-11-17 | ✅ SUCCESS | 0개 |

---

## 🎯 다음 단계 (TODO)

### 즉시 실행 (우선순위 높음)

- [ ] BP_AOSCharacter 블루프린트 생성
- [ ] 레벨에 12개 스폰 포인트 배치
  - [ ] Team1 Top Lane: 2개
  - [ ] Team1 Mid Lane: 2개
  - [ ] Team1 Bottom Lane: 2개
  - [ ] Team2 Top Lane: 2개
  - [ ] Team2 Mid Lane: 2개
  - [ ] Team2 Bottom Lane: 2개
- [ ] AOSGameMode의 DefaultPawnClass 설정 (BP_AOSCharacter)
- [ ] Level Blueprint 연결 (Event BeginPlay → StartGame)
- [ ] PIE 테스트 실행

### 단기 목표 (1주일 이내)

- [ ] 타워 생성 및 배치
- [ ] 커맨드 센터 생성
- [ ] AI 순찰 로직 완성 테스트
- [ ] 캐릭터 자동 공격 테스트

### 중기 목표 (2주일 이내)

- [ ] 카메라 시스템 구현
- [ ] UI (타이머, 팀 정보) 추가
- [ ] 네트워크 리플리케이션 설정

### 장기 목표 (1개월 이내)

- [ ] 완전한 게임 루프 테스트
- [ ] 밸런싱 및 튜닝
- [ ] 멀티플레이 테스트

---

## 📝 마지막 메모

### 주요 성과

✅ 완전한 게임 아키텍처 설계
✅ 7개 핵심 클래스 구현
✅ 자동 캐릭터 생성 시스템 완성
✅ 위치 설정 문제 해결
✅ 포괄적인 문서화

### 남은 작업

- Level Designer의 작업이 필요함 (스폰 포인트, 타워, 맵 배치)
- 게임 밸런싱 필요
- 멀티플레이 테스트

### 기술적 교훈

1. **Reflection System 이해**: Unreal의 UPROPERTY/UFUNCTION 제약 이해의 중요성
2. **아키텍처 설계**: 초기 설계가 이후 변경에 미치는 영향
3. **위치 설정 중복**: 여러 곳에서 같은 작업을 반복하면 버그 발생
4. **문서화**: 변경사항을 명확히 기록해야 나중에 추적 가능

---

**프로젝트 상태**: 🟢 DEVELOPMENT (개발 진행 중)
**다음 체크포인트**: PIE 테스트
**예상 완료**: 2025-11-20

---

*이 문서는 자동으로 생성되었으며 최종 업데이트 날짜: 2025-11-17*
