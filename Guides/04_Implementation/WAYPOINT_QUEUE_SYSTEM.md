# 웨이포인트 큐 시스템 - 순차적 라인 푸시 구현

## 개요
MOBA 스타일의 라인 푸시 로직을 구현하기 위한 웨이포인트 큐 시스템입니다. AI 캐릭터가 아군 타워 → 적 타워 → 적 커맨드 센터 순서로 이동하도록 보장합니다.

---

## 문제 상황

### 기존 로직의 문제점
이전 구현에서는 AI 캐릭터가 **적 타워만** 목표로 설정하여, 아군 타워를 무시하고 바로 적진으로 직행했습니다.

**기존 코드**:
```cpp
// 문제: 적 타워만 가져옴
TArray<AAOSStructure*> TowersInLane = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);
```

### 예상 동작
```
스폰 → 아군 타워1 → 아군 타워2 → 아군 타워3 → 적 타워1 → 적 타워2 → 적 타워3 → 적 커맨드 센터
```

### 실제 동작 (문제)
```
스폰 → 적 타워1 (직행) → 적 타워2 → 적 타워3 → 적 커맨드 센터
```

---

## 해결 방법: 웨이포인트 큐 시스템

### 핵심 아이디어
1. **웨이포인트 큐**: 캐릭터가 방문해야 할 모든 구조물을 순서대로 저장
2. **순차 이동**: 현재 웨이포인트에 도착하면 다음 웨이포인트로 자동 이동
3. **동적 업데이트**: 웨이포인트가 파괴되면 자동으로 다음으로 건너뜀

---

## 구현 내용

### 1. 헤더 파일 수정 ([AOSAIController.h](Source/TDProject/AOS/AOSAIController.h))

#### 추가된 변수

```cpp
// 웨이포인트 큐 (순차적으로 방문할 구조물들)
UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
TArray<AAOSStructure*> WaypointQueue;

// 현재 웨이포인트 인덱스
UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
int32 CurrentWaypointIndex = 0;
```

#### 추가된 함수

```cpp
void BuildWaypointQueue();  // 웨이포인트 큐 구축
```

#### 제거된 변수

```cpp
// 더 이상 사용하지 않음
// int32 NextTowerIndex = 0;
```

---

### 2. 웨이포인트 큐 구축 함수 ([AOSAIController.cpp](Source/TDProject/AOS/AOSAIController.cpp))

```cpp
void AAOSAIController::BuildWaypointQueue()
{
    WaypointQueue.Empty();
    CurrentWaypointIndex = 0;

    if (!ControlledCharacter)
        return;

    // MapManager 찾기
    AAOSMapManager* MapManager = nullptr;
    for (TActorIterator<AAOSMapManager> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
        MapManager = *ActorItr;
        break;
    }

    if (!MapManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[AI Controller] MapManager not found!"));
        return;
    }

    EAOSTeam MyTeam = ControlledCharacter->GetTeam();
    EAOSTeam EnemyTeam = (MyTeam == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;

    // 1단계: 아군 타워들을 순서대로 추가 (스폰 지점에서 가까운 순)
    TArray<AAOSStructure*> FriendlyTowers = MapManager->GetTowersInLane(DeployedLane, MyTeam);

    // 스폰 지점에서 가까운 순서로 정렬
    FriendlyTowers.Sort([this](const AAOSStructure& A, const AAOSStructure& B)
    {
        float DistA = FVector::Dist(LaneStartPosition, A.GetActorLocation());
        float DistB = FVector::Dist(LaneStartPosition, B.GetActorLocation());
        return DistA > DistB; // 먼 것부터 (역순으로 추가하기 위해)
    });

    // 역순으로 추가 (가장 가까운 타워가 먼저)
    for (int32 i = FriendlyTowers.Num() - 1; i >= 0; i--)
    {
        if (FriendlyTowers[i] && !FriendlyTowers[i]->IsDestroyed())
        {
            WaypointQueue.Add(FriendlyTowers[i]);
        }
    }

    // 2단계: 적 타워들을 순서대로 추가 (내 진영에서 가까운 순)
    TArray<AAOSStructure*> EnemyTowers = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);

    // 스폰 지점에서 가까운 순서로 정렬
    EnemyTowers.Sort([this](const AAOSStructure& A, const AAOSStructure& B)
    {
        float DistA = FVector::Dist(LaneStartPosition, A.GetActorLocation());
        float DistB = FVector::Dist(LaneStartPosition, B.GetActorLocation());
        return DistA < DistB; // 가까운 것부터
    });

    for (AAOSStructure* Tower : EnemyTowers)
    {
        if (Tower && !Tower->IsDestroyed())
        {
            WaypointQueue.Add(Tower);
        }
    }

    // 3단계: 마지막으로 적 커맨드 센터 추가
    AAOSStructure* EnemyCommandCenter = MapManager->GetCommandCenter(EnemyTeam);
    if (EnemyCommandCenter)
    {
        WaypointQueue.Add(EnemyCommandCenter);
    }

    // 디버그 로그 출력
    UE_LOG(LogTemp, Warning, TEXT("[AI] Waypoint Queue Built: %d waypoints"), WaypointQueue.Num());
    for (int32 i = 0; i < WaypointQueue.Num(); i++)
    {
        if (WaypointQueue[i])
        {
            FString StructureType = WaypointQueue[i]->GetStructureType() == EStructureType::Tower
                ? TEXT("Tower") : TEXT("CommandCenter");
            FString TeamName = WaypointQueue[i]->GetOwnerTeam() == EAOSTeam::Team1
                ? TEXT("Team1") : TEXT("Team2");

            UE_LOG(LogTemp, Warning, TEXT("  [%d] %s (%s) at (%.0f, %.0f, %.0f)"),
                i, *StructureType, *TeamName,
                WaypointQueue[i]->GetActorLocation().X,
                WaypointQueue[i]->GetActorLocation().Y,
                WaypointQueue[i]->GetActorLocation().Z);
        }
    }
}
```

#### 작동 원리
1. **아군 타워 정렬**: 스폰 지점에서 가까운 순서로 추가
2. **적 타워 정렬**: 스폰 지점에서 가까운 순서로 추가 (교전 라인 기준)
3. **커맨드 센터**: 최종 목표로 추가

---

### 3. 목표 위치 계산 수정 ([AOSAIController.cpp](Source/TDProject/AOS/AOSAIController.cpp))

```cpp
FVector AAOSAIController::GetNextTargetLocation()
{
    if (!ControlledCharacter)
        return FVector::ZeroVector;

    // 웨이포인트 큐가 비어있으면 기본 위치 반환
    if (WaypointQueue.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] Waypoint queue is empty!"));
        return LaneEndPosition;
    }

    // 현재 웨이포인트가 유효한지 확인
    while (CurrentWaypointIndex < WaypointQueue.Num())
    {
        AAOSStructure* CurrentWaypoint = WaypointQueue[CurrentWaypointIndex];

        // 웨이포인트가 유효하고 파괴되지 않았으면 해당 위치로 이동
        if (CurrentWaypoint && !CurrentWaypoint->IsDestroyed())
        {
            FString StructureType = CurrentWaypoint->GetStructureType() == EStructureType::Tower
                ? TEXT("Tower") : TEXT("CommandCenter");
            FString TeamName = CurrentWaypoint->GetOwnerTeam() == EAOSTeam::Team1
                ? TEXT("Team1") : TEXT("Team2");

            UE_LOG(LogTemp, Warning, TEXT("[AI] Target: Waypoint[%d/%d] - %s (%s)"),
                CurrentWaypointIndex, WaypointQueue.Num() - 1,
                *StructureType, *TeamName);

            return CurrentWaypoint->GetActorLocation();
        }

        // 현재 웨이포인트가 파괴되었으면 다음으로 넘어감
        UE_LOG(LogTemp, Warning, TEXT("[AI] Waypoint[%d] destroyed, skipping"),
            CurrentWaypointIndex);
        CurrentWaypointIndex++;
    }

    // 모든 웨이포인트를 통과했으면 완료
    UE_LOG(LogTemp, Warning, TEXT("[AI] All waypoints completed!"));
    bAllTowersDestroyed = true;
    return LaneEndPosition;
}
```

#### 핵심 변경점
- ~~적 타워만 가져오기~~ → **웨이포인트 큐에서 순차 조회**
- 파괴된 구조물 자동 건너뛰기
- 진행 상황 로그 출력

---

### 4. 이동 완료 처리 수정 ([AOSAIController.cpp](Source/TDProject/AOS/AOSAIController.cpp))

```cpp
// 도착 판정
if (Distance <= ArrivalDistance)
{
    ControlledCharacter->GetCharacterMovement()->Velocity = FVector::ZeroVector;

    // 모든 웨이포인트 완료 시 더 이상 진행하지 않음 (무한루프 방지)
    if (bAllTowersDestroyed)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AI] Arrived at waypoint! Distance: %.1f, CurrentWaypointIndex: %d"),
        Distance, CurrentWaypointIndex);

    // 다음 웨이포인트로 이동
    CurrentWaypointIndex++;
    UE_LOG(LogTemp, Warning, TEXT("[AI] Moving to next waypoint. New index: %d/%d"),
        CurrentWaypointIndex, WaypointQueue.Num() - 1);

    CurrentMoveTarget = GetNextTargetLocation();
    return;
}
```

#### 핵심 변경점
- ~~NextTowerIndex++~~ → **CurrentWaypointIndex++**
- **bAllTowersDestroyed 체크** 추가 (2026-02-17): 모든 웨이포인트 완료 후 무한 인덱스 증가 방지
- 웨이포인트 진행 상황을 명확하게 로그 출력

#### ArrivalDistance 주의사항
- 현재 값: **100.0f** (2026-02-17 수정: 200→100)
- 인접 웨이포인트 간 거리보다 작아야 함 (너무 크면 한 프레임에 여러 웨이포인트를 건너뜀)

---

## 사용 방법

### 배포 시작
```cpp
// StartDeployment() 호출 시 자동으로 웨이포인트 큐 구축
void AAOSAIController::StartDeployment(EAOSLane Lane)
{
    DeployedLane = Lane;
    CacheLaneInfo();

    // 웨이포인트 큐 구축 ⭐
    BuildWaypointQueue();

    // 첫 목표 설정
    CurrentMoveTarget = GetNextTargetLocation();
}
```

### 디버그 정보 확인

게임 실행 시 다음과 같은 로그가 출력됩니다:

```
[AI Controller] Waypoint Queue Built: 7 waypoints
  [0] Tower (Team1) at (1400, 1400, 0)    ← 아군 타워 3
  [1] Tower (Team1) at (700, 700, 0)      ← 아군 타워 2
  [2] Tower (Team1) at (-350, -350, 0)   ← 아군 타워 1
  [3] Tower (Team2) at (-700, -700, 0)   ← 적 타워 1
  [4] Tower (Team2) at (-1400, -1400, 0) ← 적 타워 2
  [5] Tower (Team2) at (-2100, -2100, 0) ← 적 타워 3
  [6] CommandCenter (Team2) at (2200, 0, 0) ← 적 커맨드 센터

[AI] Target: Waypoint[0/6] - Tower (Team1)
[AI] Arrived at waypoint! Distance: 150.0, Index: 0
[AI] Moving to next waypoint. New index: 1/6
[AI] Target: Waypoint[1/6] - Tower (Team1)
...
```

---

## 장점

### 1. **명확한 이동 경로**
- 아군 타워 → 적 타워 → 커맨드 센터 순서 보장
- MOBA 게임의 라인 푸시 메커니즘 정확히 재현

### 2. **유연한 대응**
- 타워가 이미 파괴되어 있으면 자동으로 건너뜀
- 동적으로 변하는 전장 상황에 대응

### 3. **디버깅 용이**
- 웨이포인트 큐 전체를 로그로 확인 가능
- 현재 진행 상황을 명확하게 추적

### 4. **확장 가능**
- 새로운 구조물 타입 추가 시 BuildWaypointQueue()만 수정
- 특수 조건(예: 특정 타워 우선 공격)도 쉽게 구현 가능

---

## 예제: 웨이포인트 큐 구조

### Team1 캐릭터 (Top Lane)
```
스폰 위치: (2000, 2000, 0)

웨이포인트 큐:
[0] Team1 Tower at (1400, 1400, 0)   ← 가장 가까운 아군 타워
[1] Team1 Tower at (700, 700, 0)
[2] Team1 Tower at (-350, -350, 0)   ← 중앙선 근처
[3] Team2 Tower at (-700, -700, 0)   ← 중앙선 넘어서 첫 적 타워
[4] Team2 Tower at (-1400, -1400, 0)
[5] Team2 Tower at (-2100, -2100, 0) ← 적 본진 근처
[6] Team2 CommandCenter at (2200, 0, 0) ← 최종 목표
```

### Team2 캐릭터 (Top Lane)
```
스폰 위치: (-2000, -2000, 0)

웨이포인트 큐:
[0] Team2 Tower at (-1400, -1400, 0) ← 가장 가까운 아군 타워
[1] Team2 Tower at (-700, -700, 0)
[2] Team2 Tower at (350, 350, 0)     ← 중앙선 근처
[3] Team1 Tower at (700, 700, 0)     ← 중앙선 넘어서 첫 적 타워
[4] Team1 Tower at (1400, 1400, 0)
[5] Team1 Tower at (2100, 2100, 0)   ← 적 본진 근처
[6] Team1 CommandCenter at (-2200, 0, 0) ← 최종 목표
```

---

## 코드 스니펫: 블루프린트에서 웨이포인트 확인

블루프린트에서 웨이포인트 큐를 확인하고 싶다면:

```cpp
// 블루프린트 호출 가능 함수 추가 (AOSAIController.h)
UFUNCTION(BlueprintCallable, Category = "AOS|AI")
TArray<AAOSStructure*> GetWaypointQueue() const { return WaypointQueue; }

UFUNCTION(BlueprintCallable, Category = "AOS|AI")
int32 GetCurrentWaypointIndex() const { return CurrentWaypointIndex; }

UFUNCTION(BlueprintCallable, Category = "AOS|AI")
AAOSStructure* GetCurrentWaypoint() const
{
    if (CurrentWaypointIndex >= 0 && CurrentWaypointIndex < WaypointQueue.Num())
        return WaypointQueue[CurrentWaypointIndex];
    return nullptr;
}
```

---

## 참고 사항

### 타워가 파괴될 때
웨이포인트가 파괴되면 `GetNextTargetLocation()`에서 자동으로 다음 웨이포인트로 건너뜁니다:

```cpp
// 파괴된 웨이포인트 건너뛰기
if (CurrentWaypoint && !CurrentWaypoint->IsDestroyed())
{
    return CurrentWaypoint->GetActorLocation();
}
CurrentWaypointIndex++; // 다음으로 이동
```

### 적 조우 시 동작
적을 발견하면 웨이포인트 이동을 일시 중단하고 전투에 돌입합니다:

```cpp
void AAOSAIController::UpdateAIBehavior(float DeltaTime)
{
    // 가장 가까운 적군 찾기 (우선순위: 캐릭터 > 타워)
    AAOSCharacter* NearestEnemy = FindNearestEnemy();
    if (NearestEnemy)
    {
        CurrentTarget = NearestEnemy;
        AttackTarget(DeltaTime);  // 웨이포인트 이동 중단, 전투
        return;
    }

    // 적이 없으면 웨이포인트 경로 계속 진행
    CurrentMoveTarget = GetNextTargetLocation();
    MoveTowardsTarget(DeltaTime);
}
```

---

## 관련 파일

- [AOSAIController.h](Source/TDProject/AOS/AOSAIController.h) - 헤더 파일
- [AOSAIController.cpp](Source/TDProject/AOS/AOSAIController.cpp) - 구현 파일
- [AOSMapManager.cpp](Source/TDProject/AOS/AOSMapManager.cpp) - 타워 위치 관리

---

## 향후 개선 방향

1. **웨이포인트 갱신**: 타워가 새로 건설되거나 파괴될 때 동적으로 큐 갱신
2. **우선순위 시스템**: 특정 타워에 더 높은 우선순위 부여
3. **경로 최적화**: A* 알고리즘을 사용한 더 효율적인 경로 계산
4. **팀 전술**: 여러 캐릭터가 협력하여 특정 타워 집중 공격
5. **네비게이션 메시 활용**: 캐릭터들이 장애물을 피하면서 자연스럽게 분산되도록 구현
