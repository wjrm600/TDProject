# 2026-01-05 맵 구조 개선 및 버그 수정

## 작업 개요
이전 세션에서 구현한 RTS 카메라와 라인 이동 시스템을 기반으로, MapManager 구조 개선과 여러 버그 수정을 진행했습니다.

---

## 1. MapManager 구조 개선

### 1.1 EndPosition 제거
**문제점**: FLaneInfo에 StartPosition과 EndPosition이 모두 있었지만, EndPosition은 결국 상대팀 Command Center 위치와 동일하여 중복된 데이터였습니다.

**해결**:
- FLaneInfo 구조체에서 Team1EndPosition, Team2EndPosition 필드 제거
- GetLaneEndPosition() 함수를 간소화하여 상대팀 Command Center 위치를 직접 반환

**변경된 코드** ([AOSMapManager.h](Source/TDProject/AOS/AOSMapManager.h)):
```cpp
USTRUCT(BlueprintType)
struct FLaneInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAOSLane LaneType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Team1StartPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Team2StartPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FVector> Team1TowerPositions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FVector> Team2TowerPositions;

    // EndPosition 필드 제거됨
};
```

**커밋**: "Refactor: Remove redundant EndPosition from lane system"

---

### 1.2 Command Center를 라인별에서 팀별로 변경

**문제점**: 각 라인(Top, Mid, Bottom)마다 Command Center 위치가 설정되어 있어 팀당 3개의 Command Center 위치가 존재했습니다. 하지만 실제로는 팀당 1개의 Command Center만 필요합니다.

**해결**:
- FLaneInfo에서 Command Center 위치 필드 제거
- AAOSMapManager에 팀별 Command Center 위치 추가 (Team1CommandCenterPosition, Team2CommandCenterPosition)
- 모든 관련 함수 업데이트 (GetLaneEndPosition, SpawnStructures, 시각화 등)

**변경된 코드** ([AOSMapManager.h](Source/TDProject/AOS/AOSMapManager.h)):
```cpp
class AOS_API AAOSMapManager : public AActor
{
    // ...

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Map")
    TArray<FLaneInfo> LanesInfo;

    // 팀별 Command Center 위치 (팀당 1개)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Map")
    FVector Team1CommandCenterPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Map")
    FVector Team2CommandCenterPosition;

    UPROPERTY(BlueprintReadOnly, Category = "AOS|Map")
    TMap<EAOSTeam, AAOSStructure*> CommandCenters;
};
```

**커밋**: "Command Center 구조 개선: 라인별에서 팀별로 변경"

---

## 2. 에디터 시각화 개선

### 2.1 실시간 업데이트 구현

**문제점**: LanesInfo 값을 에디터에서 변경해도 디버그 시각화가 자동으로 갱신되지 않았습니다.

**해결**:
- PostEditChangeProperty() 함수 구현
- LanesInfo, Command Center 위치, 시각화 관련 속성이 변경되면 자동으로 UpdateEditorVisualization() 호출

**구현 코드** ([AOSMapManager.cpp](Source/TDProject/AOS/AOSMapManager.cpp)):
```cpp
void AAOSMapManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr)
        ? PropertyChangedEvent.Property->GetFName()
        : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, LanesInfo) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, Team1CommandCenterPosition) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, Team2CommandCenterPosition) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, bShowEditorVisualization) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAOSMapManager, EditorVisualizationThickness))
    {
        UpdateEditorVisualization();
    }
}
```

### 2.2 DrawDebugBox/Sphere 렌더링 문제 해결

**문제점**: 라인은 잘 그려지지만 타워와 Command Center를 표시하는 큐브/구체가 에디터 뷰포트에 표시되지 않았습니다.

**원인**: DrawDebugBox와 DrawDebugSphere는 에디터 뷰포트에서 제대로 렌더링되지 않습니다.

**해결**:
- DrawDebugLine만을 사용한 와이어프레임 구현
- 타워: X 마커 (3개의 교차선)
- Command Center: 와이어프레임 박스 (12개의 선)

**구현 코드** ([AOSMapManager.cpp](Source/TDProject/AOS/AOSMapManager.cpp)):
```cpp
void AAOSMapManager::UpdateEditorVisualization()
{
    if (!bShowEditorVisualization || !GetWorld())
        return;

    FlushPersistentDebugLines(GetWorld());

    for (const FLaneInfo& LaneInfo : LanesInfo)
    {
        // 타워를 X 마커로 표시
        for (const FVector& TowerPos : LaneInfo.Team1TowerPositions)
        {
            DrawDebugLine(GetWorld(),
                TowerPos + FVector(-100, -100, 0),
                TowerPos + FVector(100, 100, 0),
                Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
            DrawDebugLine(GetWorld(),
                TowerPos + FVector(-100, 100, 0),
                TowerPos + FVector(100, -100, 0),
                Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
            DrawDebugLine(GetWorld(),
                TowerPos + FVector(0, 0, -100),
                TowerPos + FVector(0, 0, 100),
                Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
        }

        // Command Center를 와이어프레임 박스로 표시 (12개 선)
        float Size = 200.0f;
        FVector CC = Team1CommandCenterPosition;
        // Bottom face
        DrawDebugLine(GetWorld(), CC + FVector(-Size, -Size, -Size), CC + FVector(Size, -Size, -Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
        DrawDebugLine(GetWorld(), CC + FVector(Size, -Size, -Size), CC + FVector(Size, Size, -Size), Team1Color, true, -1.0f, 0, EditorVisualizationThickness * 2.0f);
        // ... (총 12개 선으로 박스 구성)
    }
}
```

---

## 3. 메모리 크래시 수정

### 3.1 AllTowers 배열 UPROPERTY 추가

**문제점**: 게임 종료 시 MapManager 소멸자에서 유효하지 않은 메모리에 접근하여 크래시가 발생했습니다.

**원인**: AllTowers 배열이 UPROPERTY로 선언되지 않아 언리얼의 가비지 컬렉터가 관리하지 못했습니다. 타워가 먼저 소멸되면 AllTowers에 댕글링 포인터가 남게 됩니다.

**해결**:
- AllTowers 선언에 UPROPERTY() 추가

**수정 코드** ([AOSMapManager.h](Source/TDProject/AOS/AOSMapManager.h)):
```cpp
// 라인별 타워들 (런타임용)
UPROPERTY()  // 가비지 컬렉션을 위해 추가
TArray<AAOSStructure*> AllTowers;
```

**커밋**: "AllTowers 배열에 UPROPERTY 추가하여 메모리 크래시 수정"

---

## 4. 충돌(Collision) 문제 수정

### 4.1 타워 및 Command Center 충돌 비활성화

**문제점**: 타워와 Command Center에 큐브 메시가 설치되면서 캐릭터 이동을 물리적으로 막고 있었습니다.

**해결**:
- CollisionComponent와 MeshComponent의 충돌을 NoCollision으로 설정
- DetectionRange는 적 감지를 위해 QueryOnly 유지

**수정 코드** ([AOSStructure.cpp](Source/TDProject/AOS/AOSStructure.cpp)):
```cpp
AAOSStructure::AAOSStructure()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    RootComponent = CollisionComponent;
    CollisionComponent->SetSphereRadius(100.0f);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 물리 충돌 제거

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 메시 충돌 제거

    DetectionRange = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionRange"));
    DetectionRange->SetupAttachment(RootComponent);
    DetectionRange->SetSphereRadius(1500.0f);
    DetectionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);  // 적 감지만 유지
}
```

**커밋**: "타워 및 커맨드 센터 콜리전 비활성화"

---

## 5. 로그 정리

### 5.1 AI 이동 디버그 로그 주석 처리

**변경 사항**:
- 너무 빈번하게 출력되는 AI 이동 관련 로그 주석 처리
- 스폰 로그는 유지하되 가독성 개선

**수정 코드** ([AOSAIController.cpp](Source/TDProject/AOS/AOSAIController.cpp)):
```cpp
FVector AAOSAIController::GetNextTargetLocation()
{
    // ... 코드 ...
    TArray<AAOSStructure*> TowersInLane = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);

    // 주석 처리
    // UE_LOG(LogTemp, Warning, TEXT("[AI] GetNextTargetLocation - NextTowerIndex: %d, TotalTowers: %d, Lane: %d"),
    //     NextTowerIndex, TowersInLane.Num(), static_cast<int32>(DeployedLane));

    if (TowersInLane.Num() > 0 && NextTowerIndex < TowersInLane.Num())
    {
        AAOSStructure* TargetTower = TowersInLane[NextTowerIndex];
        if (TargetTower && !TargetTower->IsDestroyed())
        {
            // 주석 처리
            // UE_LOG(LogTemp, Warning, TEXT("[AI] Moving to Tower %d at (%.1f, %.1f, %.1f)"),
            //     NextTowerIndex, TargetTower->GetActorLocation().X, ...);
            return TargetTower->GetActorLocation();
        }
    }
    // ...
}
```

### 5.2 스폰 로그 가독성 개선

**수정 코드** ([AOSSpawnPoint.cpp](Source/TDProject/AOS/AOSSpawnPoint.cpp)):
```cpp
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
    Character->SetTeam(Team);
    Character->SetLane(Lane);

    FString LaneName;
    switch (Lane)
    {
    case EAOSLane::Top: LaneName = TEXT("TOP"); break;
    case EAOSLane::Mid: LaneName = TEXT("MID"); break;
    case EAOSLane::Bottom: LaneName = TEXT("BOTTOM"); break;
    default: LaneName = TEXT("UNKNOWN"); break;
    }

    UE_LOG(LogTemp, Warning, TEXT("===== CHARACTER SPAWN ====="));
    UE_LOG(LogTemp, Warning, TEXT("Team: %s, Lane: %s, Position: (%.1f, %.1f, %.1f)"),
        Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
        *LaneName,
        Character->GetActorLocation().X, Character->GetActorLocation().Y,
        Character->GetActorLocation().Z);

    SetOccupiedCharacter(Character);
    Character->DeployToLane();
}
```

**커밋**: "AI 이동 디버그 로그 주석 처리 및 스폰 로그 개선"

---

## 6. 현재 진행 중인 이슈

### 6.1 캐릭터가 라인을 따라가지 않고 중앙으로만 이동

**증상**:
- 스폰 로그는 정상적으로 팀과 라인을 표시
- 하지만 캐릭터들이 자기 라인의 타워를 따라가지 않고 모두 중앙으로 이동

**로그 예시**:
```
LogTemp: Warning: ===== CHARACTER SPAWN =====
LogTemp: Warning: Team: Team2, Lane: TOP, Position: (1600.0, -1668.0, 400.0)
LogTemp: Warning: [AI Controller] Cached lane info - Team: 1, Lane: 0
LogTemp: Warning: [AI Controller] Start: (1400.0, -1600.0, 300.0), End: (-1600.0, 1600.0, 300.0)
LogTemp: Warning: [AI Controller] Deployment started on lane: 0
```

**현재 AI 이동 플로우**:
1. **스폰 단계** (AOSSpawnPoint): 캐릭터에 팀과 라인 할당
2. **AI 초기화** (AOSAIController::OnPossess): AI 컨트롤러 초기화
3. **라인 정보 캐싱** (CacheLaneInfo): MapManager에서 라인 시작/끝 위치와 타워 정보 가져오기
4. **배치** (DeployToLane): 다음 목표 위치 설정 (GetNextTargetLocation)
5. **매 프레임** (UpdateAIBehavior): 적 확인 → 이동 또는 공격

**예상되는 이동 경로**:
```
스폰 지점 → 타워1 → 타워2 → 타워3 → 적 Command Center
```

**가설**:
- GetNextTargetLocation()이 타워를 건너뛰고 바로 Command Center(End Position)로 반환하고 있을 가능성
- 가능한 원인:
  1. 타워가 제대로 스폰되지 않음
  2. GetTowersInLane()이 빈 배열 반환
  3. 타워 위치가 모두 원점이거나 잘못됨

**다음 조사 필요 사항**:
- SpawnStructures()에서 타워 스폰 성공 여부 로그 추가
- GetNextTargetLocation()에서 TowersInLane.Num() 확인
- 실제 반환되는 타워 위치 확인

---

## 수정된 파일 목록

1. [AOSMapManager.h](Source/TDProject/AOS/AOSMapManager.h) - 구조체 및 클래스 정의 변경
2. [AOSMapManager.cpp](Source/TDProject/AOS/AOSMapManager.cpp) - 구조 개선, 시각화 개선, 실제 스폰된 타워 위치 디버그 표시
3. [AOSStructure.cpp](Source/TDProject/AOS/AOSStructure.cpp) - 충돌 비활성화
4. [AOSAIController.h](Source/TDProject/AOS/AOSAIController.h) - 웨이포인트 큐 시스템 변수 추가, 소멸자 추가
5. [AOSAIController.cpp](Source/TDProject/AOS/AOSAIController.cpp) - 웨이포인트 큐 로직 구현, 메모리 정리
6. [AOSSpawnPoint.cpp](Source/TDProject/AOS/AOSSpawnPoint.cpp) - 로그 개선

---

## Git 커밋 내역

```bash
f4a5f88 웨이포인트 큐 시스템 구현 및 디버그 시각화 개선
cfedd83 AI 이동 디버그 로그 주석 처리 및 스폰 로그 개선
a23286d 타워 및 커맨드 센터 콜리전 비활성화
56a85ba AllTowers 배열에 UPROPERTY 추가하여 메모리 크래시 수정
df04a30 Command Center 구조 개선: 라인별에서 팀별로 변경
4f90caf Refactor: Remove redundant EndPosition from lane system
```

---

## 7. 웨이포인트 큐 시스템 구현 (라인 푸시 로직)

### 7.1 문제 분석

**증상**:
- AI 캐릭터들이 아군 타워를 무시하고 바로 적진으로 직행
- 맵 중앙을 가로질러 적 타워로 바로 이동

**원인**:
- [AOSAIController.cpp:94](Source/TDProject/AOS/AOSAIController.cpp#L94)에서 적 타워만 목표로 설정
- 아군 타워를 경유하지 않는 로직

**기존 코드**:
```cpp
// 문제: 적 타워만 가져옴
TArray<AAOSStructure*> TowersInLane = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);
```

### 7.2 해결 방법: 웨이포인트 큐

**개념**:
- 방문해야 할 모든 구조물을 순서대로 저장하는 큐
- 순차적으로 이동하며, 파괴된 구조물은 자동으로 건너뜀

**이동 순서**:
```
스폰 → 아군 타워 3 → 아군 타워 2 → 아군 타워 1 → 적 타워 1 → 적 타워 2 → 적 타워 3 → 적 커맨드 센터
```

### 7.3 구현 내용

#### 추가된 변수 ([AOSAIController.h:97-103](Source/TDProject/AOS/AOSAIController.h#L97-L103))

```cpp
// 웨이포인트 큐 (순차적으로 방문할 구조물들)
UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
TArray<AAOSStructure*> WaypointQueue;

// 현재 웨이포인트 인덱스
UPROPERTY(BlueprintReadOnly, Category = "AOS|AI|Debug")
int32 CurrentWaypointIndex = 0;
```

#### BuildWaypointQueue() 함수 ([AOSAIController.cpp:246-334](Source/TDProject/AOS/AOSAIController.cpp#L246-L334))

```cpp
void AAOSAIController::BuildWaypointQueue()
{
    WaypointQueue.Empty();
    CurrentWaypointIndex = 0;

    // MapManager 찾기
    AAOSMapManager* MapManager = nullptr;
    for (TActorIterator<AAOSMapManager> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
        MapManager = *ActorItr;
        break;
    }

    EAOSTeam MyTeam = ControlledCharacter->GetTeam();
    EAOSTeam EnemyTeam = (MyTeam == EAOSTeam::Team1) ? EAOSTeam::Team2 : EAOSTeam::Team1;

    // 1단계: 아군 타워들을 스폰 지점에서 가까운 순서로 추가
    TArray<AAOSStructure*> FriendlyTowers = MapManager->GetTowersInLane(DeployedLane, MyTeam);
    FriendlyTowers.Sort([this](const AAOSStructure& A, const AAOSStructure& B) {
        float DistA = FVector::Dist(LaneStartPosition, A.GetActorLocation());
        float DistB = FVector::Dist(LaneStartPosition, B.GetActorLocation());
        return DistA > DistB;
    });
    for (int32 i = FriendlyTowers.Num() - 1; i >= 0; i--)
    {
        if (FriendlyTowers[i] && !FriendlyTowers[i]->IsDestroyed())
            WaypointQueue.Add(FriendlyTowers[i]);
    }

    // 2단계: 적 타워들을 스폰 지점에서 가까운 순서로 추가
    TArray<AAOSStructure*> EnemyTowers = MapManager->GetTowersInLane(DeployedLane, EnemyTeam);
    EnemyTowers.Sort([this](const AAOSStructure& A, const AAOSStructure& B) {
        float DistA = FVector::Dist(LaneStartPosition, A.GetActorLocation());
        float DistB = FVector::Dist(LaneStartPosition, B.GetActorLocation());
        return DistA < DistB;
    });
    for (AAOSStructure* Tower : EnemyTowers)
    {
        if (Tower && !Tower->IsDestroyed())
            WaypointQueue.Add(Tower);
    }

    // 3단계: 적 커맨드 센터 추가
    AAOSStructure* EnemyCommandCenter = MapManager->GetCommandCenter(EnemyTeam);
    if (EnemyCommandCenter)
        WaypointQueue.Add(EnemyCommandCenter);
}
```

#### GetNextTargetLocation() 수정 ([AOSAIController.cpp:68-112](Source/TDProject/AOS/AOSAIController.cpp#L68-L112))

```cpp
FVector AAOSAIController::GetNextTargetLocation()
{
    if (!ControlledCharacter)
        return FVector::ZeroVector;

    // 웨이포인트 큐가 비어있으면 기본 위치 반환
    if (WaypointQueue.Num() == 0)
        return LaneEndPosition;

    // 현재 웨이포인트가 유효한지 확인
    while (CurrentWaypointIndex < WaypointQueue.Num())
    {
        AAOSStructure* CurrentWaypoint = WaypointQueue[CurrentWaypointIndex];

        // 웨이포인트가 유효하고 파괴되지 않았으면 해당 위치로 이동
        if (CurrentWaypoint && !CurrentWaypoint->IsDestroyed())
        {
            // 디버그 로그 포함
            return CurrentWaypoint->GetActorLocation();
        }

        // 파괴되었으면 다음으로 건너뛰기
        CurrentWaypointIndex++;
    }

    // 모든 웨이포인트 완료
    bAllTowersDestroyed = true;
    return LaneEndPosition;
}
```

#### MoveTowardsTarget() 수정 ([AOSAIController.cpp:270-283](Source/TDProject/AOS/AOSAIController.cpp#L270-L283))

```cpp
// 도착 판정
if (Distance <= ArrivalDistance)
{
    ControlledCharacter->GetCharacterMovement()->Velocity = FVector::ZeroVector;

    // 다음 웨이포인트로 이동
    CurrentWaypointIndex++;
    CurrentMoveTarget = GetNextTargetLocation();
    return;
}

// 목표 방향으로 이동
ControlledCharacter->AddMovementInput(Direction, 1.0f);
```

#### 소멸자 추가 ([AOSAIController.cpp:13-21](Source/TDProject/AOS/AOSAIController.cpp#L13-L21))

에디터 종료 시 메모리 에러 방지를 위한 명시적 정리:

```cpp
AAOSAIController::~AAOSAIController()
{
    // 웨이포인트 큐 정리
    WaypointQueue.Empty();

    // 참조 정리
    ControlledCharacter = nullptr;
    CurrentTarget = nullptr;
}
```

### 7.4 제거된 코드

**NextTowerIndex 변수 제거**:
- 웨이포인트 큐 시스템으로 대체됨
- `CurrentWaypointIndex`가 동일한 역할 수행

---

## 8. 디버그 시각화 개선 (런타임)

### 8.1 문제

**증상**:
- 런타임 디버그 박스 위치가 실제 스폰된 타워/커맨드 센터 위치와 일치하지 않음

**원인**:
- DrawDebugTowerPositions()가 LanesInfo 설정 데이터를 기반으로 박스를 그림
- 실제 스폰된 타워의 위치와 설정값이 다를 수 있음

### 8.2 해결

**수정 내용** ([AOSMapManager.cpp:497-533](Source/TDProject/AOS/AOSMapManager.cpp#L497-L533)):
- `LanesInfo` 대신 `AllTowers` 배열을 순회
- 각 타워의 실제 위치(`Tower->GetActorLocation()`)에 디버그 박스 표시

```cpp
void AAOSMapManager::DrawDebugTowerPositions()
{
    if (!GetWorld())
        return;

    // 실제 스폰된 타워들의 위치에 디버그 박스 표시
    for (AAOSStructure* Tower : AllTowers)
    {
        if (!Tower)
            continue;

        FVector TowerPos = Tower->GetActorLocation();
        EAOSTeam Team = Tower->GetOwnerTeam();
        EAOSLane Lane = Tower->GetLane();

        FColor BoxColor = (Team == EAOSTeam::Team1) ? FColor::Blue : FColor::Red;
        FString TeamName = (Team == EAOSTeam::Team1) ? TEXT("Team1") : TEXT("Team2");

        DrawDebugBox(GetWorld(), TowerPos,
            FVector(DebugBoxSize, DebugBoxSize, DebugBoxSize),
            BoxColor, true, -1.0f, 0, 10.0f);
    }

    // Command Center 박스 (노란색/주황색, 크기 1.5배)
    DrawDebugBox(GetWorld(), Team1CommandCenterPosition,
        FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
        FColor::Yellow, true, -1.0f, 0, 10.0f);

    DrawDebugBox(GetWorld(), Team2CommandCenterPosition,
        FVector(DebugBoxSize * 1.5f, DebugBoxSize * 1.5f, DebugBoxSize * 1.5f),
        FColor::Orange, true, -1.0f, 0, 10.0f);
}
```

**디버그 박스 색상**:
- Team1 타워: 파란색
- Team2 타워: 빨간색
- Team1 커맨드 센터: 노란색 (크기 1.5배)
- Team2 커맨드 센터: 주황색 (크기 1.5배)

---

## 다음 작업 예정

1. **테스트 및 검증**: 웨이포인트 큐 시스템이 모든 라인에서 정상 작동하는지 확인
2. **밸런스 조정**: ArrivalDistance, EnemyDetectionRange, AttackRange 등 파라미터 조정
3. **캐릭터 분산**: 필요시 네비게이션 메시를 활용한 자연스러운 경로 이동 구현
4. **보류**: 캐릭터 하이라이트 기능 (캐릭터 정보 UI와 함께 구현 예정)

---

## 기술적 학습 내용

### 언리얼 에디터 시각화
- `PostEditChangeProperty()`: 에디터에서 속성 변경 감지
- `FlushPersistentDebugLines()`: 기존 디버그 라인 제거
- `DrawDebugBox/Sphere`는 에디터 뷰포트에서 제대로 렌더링되지 않음 (에디터용)
- `DrawDebugLine`을 사용한 와이어프레임 구현이 더 안정적 (에디터용)
- 런타임 디버그 시각화는 `DrawDebugBox`가 정상 작동함

### 메모리 관리
- `UPROPERTY()`는 단순히 블루프린트 노출이 아닌 가비지 컬렉션 관리에도 필수
- UObject* 포인터 배열은 반드시 UPROPERTY로 선언해야 안전
- AI 컨트롤러 소멸 시 명시적으로 배열 정리(`Empty()`)와 포인터 null 처리 필요

### 충돌 시스템
- `ECollisionEnabled::NoCollision`: 완전히 충돌 끔
- `ECollisionEnabled::QueryOnly`: 오버랩/트레이스만 가능 (물리 충돌 없음)
- `ECollisionEnabled::PhysicsOnly`: 물리 시뮬레이션만
- `ECollisionEnabled::QueryAndPhysics`: 모든 충돌 활성화

### 웨이포인트 큐 시스템
- 순차적 경로 이동을 위한 구조물 배열 관리
- 람다 함수를 사용한 거리 기반 정렬
- 파괴된 구조물 자동 건너뛰기 로직

---

## 개발 환경

- **언리얼 엔진**: 5.7
- **Visual Studio**: 2026
- **빌드 시스템**: UnrealBuildTool (Unreal Build Accelerator 사용)
