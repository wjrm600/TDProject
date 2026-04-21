# AI 움직임 구현 가이드

**버전**: 1.0
**날짜**: 2025-11-23
**상태**: 완성 (기본 AI 구현 완료)

---

## 📋 개요

이 문서는 AOS 게임의 AI 캐릭터 움직임 구현을 설명합니다.

**구현 방식**: 기본 Tick 기반 AI (State Tree로 향후 확장 가능)

**AI 행동 패턴**:
```
시작
  ↓
라인 배포 (StartDeployment)
  ↓
적군 감지 및 우선순위 결정
  ├─ 근처에 적 캐릭터 발견 → 공격 모드
  ├─ 적 타워 발견 → 타워 방향으로 이동
  └─ 아무것도 없음 → 다음 목표로 이동
  ↓
목표 도착까지 반복
```

---

## 🎯 핵심 시스템

### 1. AAOSAIController (AI 컨트롤러)

**파일**: `Source/TDProject/AOS/AOSAIController.h/cpp`

**주요 기능**:
- 캐릭터 제어 및 AI 의사결정
- 라인 배포 및 목표 관리
- 적군 감지 및 추적
- 공격 실행

**주요 메서드**:

#### StartDeployment(EAOSLane Lane)
```cpp
// 배포 시작 - 캐릭터가 라인에 배치될 때 호출
AIController->StartDeployment(EAOSLane::Mid);
```
- 배포 라인 설정
- 라인 정보 캐싱 (시작/끝 위치)
- 첫 번째 목표 계산

#### GetNextTargetLocation()
```cpp
// 다음 이동 목표 위치 반환 (웨이포인트 큐 기반)
FVector Target = AIController->GetNextTargetLocation();
```
**순서** (웨이포인트 큐 시스템):
1. WaypointQueue[CurrentWaypointIndex]가 유효하면 해당 위치 반환
2. 파괴된 웨이포인트는 자동으로 건너뜀 (CurrentWaypointIndex++)
3. 모든 웨이포인트 완료 시 → bAllTowersDestroyed = true, LaneEndPosition 반환

#### FindNearestEnemy()
```cpp
// 감지 범위(1500.0f) 내 가장 가까운 적 캐릭터 찾기
AAOSCharacter* Enemy = AIController->FindNearestEnemy();
```

#### FindNearestEnemyTower()
```cpp
// 감지 범위 내 가장 가까운 적 타워 찾기
AAOSStructure* Tower = AIController->FindNearestEnemyTower();
```

---

### 2. AI 행동 패턴 구현

#### UpdateAIBehavior()
매 Tick마다 실행되며 다음의 우선순위로 동작:

```cpp
void UpdateAIBehavior(float DeltaTime)
{
    // 1. 적 캐릭터 찾기 (최우선)
    if (NearestEnemy) {
        공격 모드로 전환
        AttackTarget()
    }

    // 2. 적 타워 찾기
    else if (NearestTower) {
        타워 방향으로 목표 설정
        MoveTowardsTarget()
    }

    // 3. 계획된 목표로 이동
    else {
        다음 목표 계산
        MoveTowardsTarget()
    }
}
```

#### MoveTowardsTarget()
목표 위치로 이동합니다:

```
1. 거리 계산
2. 도착 판정 (거리 <= 100.0f)
   ├─ 도착 시: 다음 목표 설정
   └─ 미도착: 방향으로 AddMovementInput
3. 목표 방향 계산 (GetSafeNormal)
4. 캐릭터 회전
```

#### AttackTarget()
감지된 적을 공격합니다:

```
1. 거리 계산
2. 공격 범위 확인 (AttackRange = 500.0f)
   ├─ 범위 밖: 추격 (방향 이동)
   └─ 범위 내:
      ├─ 움직임 멈춤
      ├─ 적 방향 회전
      └─ 공격 실행 (쿨타임 체크)
```

---

## 🔧 주요 설정값

### 감지 및 공격

| 설정 | 기본값 | 설명 |
|------|-------|------|
| EnemyDetectionRange | 1500.0f | 적군 감지 범위 |
| AttackRange | 500.0f | 공격 가능 범위 |
| AttackCooldownDuration | 1.0f | 공격 쿨타임 (초) |
| ArrivalDistance | 100.0f | 웨이포인트 도착 판정 거리 (인접 타워 간 거리보다 작아야 함) |

### 이동

| 설정 | 기본값 | 설명 |
|------|-------|------|
| MovementSpeed | 600.0f | 캐릭터 기본 이동 속도 |

---

## 📊 프로퍼티

### Public Properties

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
float EnemyDetectionRange = 1500.0f;  // 적군 감지 범위

UPROPERTY(EditAnywhere, BlueprintReadWrite)
float AttackRange = 500.0f;  // 공격 범위
```

### Debug Properties

```cpp
UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
FVector CurrentMoveTarget;  // 현재 이동 목표

UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
TObjectPtr<AAOSCharacter> CurrentTarget;  // 현재 공격 목표

UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
TArray<AAOSStructure*> WaypointQueue;  // 웨이포인트 큐

UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
int32 CurrentWaypointIndex;  // 현재 웨이포인트 인덱스

UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
bool bAllTowersDestroyed;  // 모든 웨이포인트 완료 여부
```

---

## 💡 작동 플로우 상세설명

### 1단계: 배포 (BeginPlay에서 자동 호출)

```
AAOSCharacter::BeginPlay()
  ↓
AOSAIController->StartDeployment(AssignedLane)
  ├─ DeployedLane = Lane 설정
  ├─ CacheLaneInfo() - 라인 정보 캐싱
  └─ CurrentMoveTarget = 첫 번째 목표 (보통 첫 번째 타워)
```

### 2단계: AI 틱 (매 프레임)

```
AAOSAIController::Tick(DeltaTime)
  ↓
UpdateAIBehavior(DeltaTime)
  ├─ 1. FindNearestEnemy() 호출
  │   └─ 발견 시 AttackTarget()
  │
  ├─ 2. FindNearestEnemyTower() 호출
  │   └─ 발견 시 목표를 타워로 설정
  │
  └─ 3. MoveTowardsTarget() 호출
      ├─ 거리 계산
      ├─ 도착 여부 확인
      └─ 미도착 시 이동 입력
```

### 3단계: 웨이포인트 큐 로직

**GetNextTargetLocation()**에서:

```cpp
// 웨이포인트 큐를 순회하며 유효한 웨이포인트 반환
while (CurrentWaypointIndex < WaypointQueue.Num())
{
    AAOSStructure* CurrentWaypoint = WaypointQueue[CurrentWaypointIndex];
    if (CurrentWaypoint && !CurrentWaypoint->IsDestroyed())
        return CurrentWaypoint->GetActorLocation();
    CurrentWaypointIndex++;  // 파괴된 웨이포인트 건너뛰기
}

// 모든 웨이포인트 완료
bAllTowersDestroyed = true;
return LaneEndPosition;
```

**웨이포인트 도착 시 (MoveTowardsTarget에서)**:

```cpp
if (Distance <= ArrivalDistance) {
    if (bAllTowersDestroyed) return;  // 무한루프 방지
    CurrentWaypointIndex++;
    CurrentMoveTarget = GetNextTargetLocation();
}
```

---

## 🎮 테스트 방법

### 1. 캐릭터 배포 확인

1. PIE 시작
2. 콘솔에서 다음 로그 확인:
   ```
   [AI Controller] AI initialized for character in lane: 0
   [AI Controller] Deployment started on lane: 0
   [AI Controller] Cached lane info - Start: (...), End: (...)
   ```

### 2. 타워 이동 확인

1. 캐릭터가 첫 번째 타워를 향해 이동하는지 확인
2. 도착 시 다음 로그:
   ```
   [AI] Arrived at target: (...)
   ```

### 3. 적군 감지 확인

1. 다른 팀 캐릭터를 감지 범위 내로 가져오기
2. 캐릭터가 추격을 시작하고 다음 로그 확인:
   ```
   [AI] Attacking enemy! Distance: xxx
   ```

---

## 🚀 향후 개선사항

### State Tree 통합 (예정)
- 현재 기본 Tick 기반 AI
- State Tree로 마이그레이션 가능

### 고급 기능 (선택사항)
- 경로 탐색 (Pathfinding)
- 팀 협력 AI
- 전술적 회피 (Kiting)
- 스킬 시스템 통합

---

## 📝 코드 참고

### 관련 파일
- [AOSAIController.h](../../Source/TDProject/AOS/AOSAIController.h)
- [AOSAIController.cpp](../../Source/TDProject/AOS/AOSAIController.cpp)
- [AOSCharacter.h](../../Source/TDProject/AOS/AOSCharacter.h)
- [AOSCharacter.cpp](../../Source/TDProject/AOS/AOSCharacter.cpp)

### 디버그 출력

콘솔에서 AI 움직임 모니터링:
```
[AI Controller] - 초기화 관련 로그
[AI] - 움직임 및 공격 관련 로그
```

---

**최종 업데이트**: 2026-02-17
**담당**: AI 시스템
**상태**: 완성 (웨이포인트 큐 시스템으로 업그레이드됨)
