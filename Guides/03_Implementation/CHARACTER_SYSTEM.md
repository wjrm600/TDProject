# 캐릭터 시스템 구현

**통합 문서**: AUTO_SPAWN_IMPLEMENTATION + POSITION_FIX

---

## 자동 스폰 시스템 (Phase 3)

게임 시작 시 모든 스폰 포인트에서 캐릭터가 자동 생성됩니다.

### 관련 클래스

| 클래스 | 역할 |
|-------|------|
| `AOSSpawnPoint` | 스폰 위치 + 팀/레인 정보 |
| `AOSGameMode::SpawnCharactersAtAllSpawnPoints()` | 모든 스폰 포인트 순회 |
| `AOSSpawnPoint::SpawnCharacterAtPoint()` | 개별 캐릭터 생성 |
| `AOSSpawnPoint::InitializeCharacter()` | 팀/레인 설정 + AI 시작 |

### 스폰 플로우

```
GameMode::SpawnCharactersAtAllSpawnPoints()
  ↓
SpawnPoint::SpawnCharacterAtPoint()
  └─ GetWorld()->SpawnActor<AAOSCharacter>(위치: 스폰 포인트 위치)
       ↓
InitializeCharacter()
  ├─ Character->SetTeam(Team)
  ├─ Character->SetLane(Lane)
  ├─ SetOccupiedCharacter(Character)  ← 등록만, 위치 변경 없음
  └─ SetTimerForNextTick → Character->DeployToLane()
       ↓ (다음 프레임)
AOSAIController::StartDeployment()
  └─ BuildWaypointQueue() → AI 이동 시작
```

---

## 위치 설정 버그 수정 (Phase 4)

### 문제

스폰 위치가 4곳에서 중복 설정되어 캐릭터가 잘못된 위치에 배치되었습니다:
1. `SpawnActor()` ← 올바른 위치
2. `InitializeCharacter()` → SetActorLocation() (중복)
3. `SetOccupiedCharacter()` → SetActorLocation() (중복)
4. `DeployToLane()` → GetLaneStartPosition() (하드코딩 위치로 덮어씀)

### 해결

**단일 책임 원칙** 적용:

| 함수 | 책임 | 위치 설정 |
|-----|------|---------|
| `SpawnActor()` | 액터 생성 + 초기 위치 | **여기서만** |
| `InitializeCharacter()` | 팀/레인 설정 | ❌ 제거 |
| `SetOccupiedCharacter()` | 스폰 포인트 등록 | ❌ 제거 |
| `DeployToLane()` | AI 시작 | ❌ 제거 |

### 수정 후 코드 (AOSSpawnPoint.cpp)

```cpp
void AAOSSpawnPoint::SetOccupiedCharacter(AAOSCharacter* Character)
{
    OccupiedCharacter = Character;
    // 위치 설정 제거 - SpawnActor에서 처리
}

void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
    if (!Character) return;

    Character->SetTeam(Team);
    Character->SetLane(Lane);
    SetOccupiedCharacter(Character);

    // BeginPlay 이후 실행 보장 (런타임 스폰 타이밍 이슈)
    GetWorld()->GetTimerManager().SetTimerForNextTick([Character]()
    {
        if (Character && Character->IsValidLowLevel())
        {
            Character->DeployToLane();
        }
    });
}
```

### 수정 후 코드 (AOSCharacter.cpp)

```cpp
void AAOSCharacter::DeployToLane()
{
    // 위치 변경 제거 - 스폰 포인트 위치를 그대로 유지
    if (AOSAIController)
    {
        AOSAIController->StartDeployment(AssignedLane);
    }
}
```

---

## 레벨 설정 (기획자용)

1. `AOSGameMode` 배치 + DefaultPawnClass = `BP_AOSCharacter`
2. `AOSSpawnPoint` 12개 배치
3. 각 스폰 포인트: Team / Lane / SpawnIndex / bSpawnEnabled 설정
4. PIE 실행 → 캐릭터 자동 생성 확인

---

**마지막 업데이트**: 2025-11-17 (AUTO_SPAWN), 2025-11-17 (POSITION_FIX)
