# 🔄 자동 캐릭터 생성 방식 변경사항

## 📋 변경 요약

**목표**: 스폰 포인트에서 자동으로 캐릭터 생성 및 배치

### 기존 방식 ❌
```
1. 레벨에 직접 캐릭터 배치
2. 게임 시작
3. AOSGameMode가 캐릭터를 스폰 포인트로 이동
```

### 새로운 방식 ✅
```
1. 스폰 포인트만 레벨에 배치 (Team/Lane/Index 설정)
2. 게임 시작
3. AOSGameMode가 자동으로 각 스폰 포인트에서 캐릭터 생성
4. 캐릭터가 스폰 포인트 위치에 배치되고 AI 시작
```

---

## 🔧 수정할 파일

| 파일 | 변경사항 |
|------|---------|
| **AOSSpawnPoint.h/cpp** | 🟢 캐릭터 생성 함수 추가 |
| **AOSGameMode.h/cpp** | 🟡 스폰 포인트에서 자동 생성 로직 추가 |
| **AOSPlayerController.cpp** | 🔴 SpawnPlayerCharacters() 함수 제거 |

---

## 📝 상세 변경사항

### 1️⃣ AOSSpawnPoint.h

🟢 **NEW**: 캐릭터 생성 함수 추가

```cpp
public:
    // 🟢 NEW - 이 스폰 포인트에서 캐릭터 생성
    UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
    class AAOSCharacter* SpawnCharacterAtPoint(TSubclassOf<class AAOSCharacter> CharacterClass);

    // 🟢 NEW - 생성된 캐릭터 초기화
    UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
    void InitializeCharacter(AAOSCharacter* Character);
```

### 2️⃣ AOSSpawnPoint.cpp

🟢 **NEW**: 캐릭터 생성 및 초기화 구현

```cpp
🟢 AAOSCharacter* AAOSSpawnPoint::SpawnCharacterAtPoint(TSubclassOf<AAOSCharacter> CharacterClass)
🟢 {
🟢     if (!CharacterClass || !GetWorld())
🟢     {
🟢         return nullptr;
🟢     }
🟢
🟢     // 스폰 포인트 위치에 캐릭터 생성
🟢     AAOSCharacter* NewCharacter = GetWorld()->SpawnActor<AAOSCharacter>(
🟢         CharacterClass,
🟢         GetActorLocation(),
🟢         FRotator::ZeroRotator
🟢     );
🟢
🟢     if (NewCharacter)
🟢     {
🟢         InitializeCharacter(NewCharacter);
🟢     }
🟢
🟢     return NewCharacter;
🟢 }
🟢
🟢 void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
🟢 {
🟢     if (!Character)
🟢     {
🟢         return;
🟢     }
🟢
🟢     // 캐릭터 설정
🟢     Character->SetTeam(Team);
🟢     Character->SetLane(Lane);
🟢     Character->SetActorLocation(GetActorLocation());
🟢
🟢     // 스폰 포인트에 등록
🟢     SetOccupiedCharacter(Character);
🟢
🟢     // AI 시작
🟢     Character->DeployToLane();
🟢
🟢     UE_LOG(LogTemp, Warning, TEXT("Character spawned at SpawnPoint - Team: %d, Lane: %d"),
🟢            static_cast<int32>(Team), static_cast<int32>(Lane));
🟢 }
```

### 3️⃣ AOSGameMode.h

🟡 **MODIFIED**: SpawnCharactersAtAllSpawnPoints() 함수 추가

```cpp
public:
    // 🟡 NEW - 모든 스폰 포인트에서 캐릭터 생성
    UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
    void SpawnCharactersAtAllSpawnPoints();

    🔴 // REMOVED: void SpawnCharacter(AAOSCharacter* Character, EAOSTeam Team, EAOSLane Lane);
    // ↑ 더 이상 필요 없음 (자동 생성으로 변경)
```

### 4️⃣ AOSGameMode.cpp

🟢 **NEW**: 게임 시작 시 캐릭터 자동 생성

```cpp
void AAOSGameMode::StartGame()
{
    if (AOSGameState == EAOSGameState::Preparation)
    {
        AOSGameState = EAOSGameState::GameRunning;
        RemainingGameTime = GameDuration;

        🟢 // NEW - 모든 스폰 포인트에서 캐릭터 생성
        🟢 SpawnCharactersAtAllSpawnPoints();
    }
}

🟢 void AAOSGameMode::SpawnCharactersAtAllSpawnPoints()
🟢 {
🟢     if (!DefaultPawnClass)
🟢     {
🟢         UE_LOG(LogTemp, Error, TEXT("DefaultPawnClass not set!"));
🟢         return;
🟢     }
🟢
🟢     for (AAOSSpawnPoint* SpawnPoint : AllSpawnPoints)
🟢     {
🟢         if (SpawnPoint)
🟢         {
🟢             // 스폰 포인트에서 캐릭터 생성
🟢             AAOSCharacter* NewCharacter = SpawnPoint->SpawnCharacterAtPoint(DefaultPawnClass);
🟢
🟢             if (NewCharacter)
🟢             {
🟢                 // 팀별 리스트에 추가
🟢                 if (NewCharacter->GetTeam() == EAOSTeam::Team1)
🟢                 {
🟢                     Team1Characters.Add(NewCharacter);
🟢                 }
🟢                 else
🟢                 {
🟢                     Team2Characters.Add(NewCharacter);
🟢                 }
🟢             }
🟢         }
🟢     }
🟢 }
```

### 5️⃣ AOSPlayerController.cpp

🔴 **REMOVED**: SpawnPlayerCharacters() 함수 제거

```cpp
🔴 // REMOVED - 더 이상 필요 없음 (자동 생성으로 변경)
🔴 void AAOSPlayerController::SpawnPlayerCharacters()
🔴 {
🔴     // ... 전체 함수 제거
🔴 }
```

---

## 🎯 레벨 구성 방법 (간단!)

### 이제 해야 할 것:
1. ✅ **스폰 포인트만 배치** (Team/Lane/Index 설정)
2. ✅ **게임 모드를 AOSGameMode로 설정**
3. ✅ **Level Blueprint에서 StartGame() 호출**

### 더 이상 할 필요가 없는 것:
4. ❌ 캐릭터를 레벨에 직접 배치
5. ❌ 캐릭터의 Team/Lane 설정

---

## 🚀 게임 실행 플로우

```
BeginPlay (게임 시작)
  ↓
AOSGameMode::BeginPlay()
  ├─ 모든 AAOSSpawnPoint 등록 ✓
  └─ AOSGameState = Preparation
  ↓
Event BeginPlay (Level Blueprint)
  ├─ StartGame() 호출
  ↓
AOSGameMode::StartGame()
  ├─ AOSGameState = GameRunning
  ├─ SpawnCharactersAtAllSpawnPoints() 호출 🟢 NEW
  │  ├─ 각 SpawnPoint에서 캐릭터 생성
  │  ├─ 캐릭터 Team/Lane 자동 설정
  │  ├─ 캐릭터를 TeamCharacters에 등록
  │  └─ AI 시작
  └─ RemainingGameTime = 600초
  ↓
게임 진행 (600초 동안)
  ├─ 캐릭터들이 라인 순찰
  └─ AI 전투 진행
```

---

## ✨ 이 방식의 장점

| 장점 | 설명 |
|------|------|
| **간단함** | 스폰 포인트만 배치하면 됨 |
| **확장성** | 스폰 포인트 추가만으로 캐릭터 자동 추가 |
| **일관성** | 모든 캐릭터가 동일한 방식으로 생성됨 |
| **자동화** | 수동 작업 최소화 |

---

## 📊 변경 전후 비교

| 항목 | 이전 | 현재 |
|------|------|------|
| **레벨에 배치** | 캐릭터 + 스폰 포인트 | 스폰 포인트만 |
| **캐릭터 개수** | 고정 (수동 배치) | 동적 (스폰 포인트만큼) |
| **초기화** | 수동 설정 | 자동 설정 |
| **추가/제거** | 레벨 수정 필요 | 스폰 포인트만 추가/제거 |

---

이제 수정된 코드를 제공하겠습니다! 준비되셨나요?

[✅ 예, 코드를 보여주세요]
