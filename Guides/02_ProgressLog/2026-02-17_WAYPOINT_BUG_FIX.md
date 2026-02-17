# 2026-02-17 웨이포인트 시스템 버그 수정 및 디버깅

## 작업 개요

PIE 테스트를 통해 웨이포인트 큐 시스템의 버그를 발견하고 수정하였다.

---

## 1. 스폰 활성화/비활성화 기능 추가

### 문제
- 캐릭터 8개가 동시에 스폰되어 Output Log에서 디버그 로그를 확인하기 어려움

### 해결
- `AOSSpawnPoint`에 `bSpawnEnabled` 프로퍼티 추가
- 에디터 Details 패널에서 체크박스로 개별 스폰포인트 활성화/비활성화 가능

### 수정 파일
- `AOSSpawnPoint.h` - `bSpawnEnabled` UPROPERTY 추가
- `AOSSpawnPoint.cpp` - `SpawnCharacterAtPoint()` 시작 부분에 체크 로직 추가

### 코드 변경

```cpp
// AOSSpawnPoint.h
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Spawn")
bool bSpawnEnabled = true;

// AOSSpawnPoint.cpp - SpawnCharacterAtPoint() 시작 부분
if (!bSpawnEnabled)
{
    UE_LOG(LogTemp, Warning, TEXT("[SpawnPoint] Spawn disabled - Team: %s, Lane: %d, Index: %d"),
        Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"),
        static_cast<int32>(Lane), SpawnIndex);
    return nullptr;
}
```

---

## 2. 웨이포인트 완료 후 무한 루프 버그 수정

### 문제
- 캐릭터가 마지막 웨이포인트(적 커맨드 센터)에 도착한 후, `CurrentWaypointIndex`가 무한히 증가
- 매 프레임마다 "All waypoints completed!" 로그가 반복 출력
- `CurrentWaypointIndex`가 7, 8, 9... 130 이상까지 증가

### 원인
- `MoveTowardsTarget()`에서 도착 판정 시 `CurrentWaypointIndex++`를 계속 실행
- 모든 웨이포인트 완료(`bAllTowersDestroyed = true`) 후에도 도착 판정 → 인덱스 증가 반복

### 해결
- 도착 판정 시작 부분에 `bAllTowersDestroyed` 체크 추가
- 이미 완료 상태이면 더 이상 인덱스를 증가시키지 않음

### 수정 파일
- `AOSAIController.cpp` - `MoveTowardsTarget()` 도착 판정 부분

### 코드 변경

```cpp
// 도착 판정 시작 부분에 추가
if (Distance <= ArrivalDistance)
{
    ControlledCharacter->GetCharacterMovement()->Velocity = FVector::ZeroVector;

    // 모든 웨이포인트 완료 시 더 이상 진행하지 않음
    if (bAllTowersDestroyed)
    {
        return;
    }
    // ... 기존 로직
}
```

---

## 3. ArrivalDistance 조정

### 변경
- `ArrivalDistance`를 `200.0f` → `100.0f`로 축소

### 이유
- 기존 200 범위가 너무 넓어서, 인접한 웨이포인트를 한 프레임에 여러 개 건너뛰는 현상 발생
- Top Lane 테스트에서 웨이포인트 3~6이 한 프레임에 모두 도착 처리됨
- 100으로 줄여서 정확한 도착 판정 수행

### 수정 파일
- `AOSAIController.h` - `ArrivalDistance` 값 변경

---

## 4. FLaneInfo 구조체 기본값 초기화

### 문제
- UE 5.7에서 구조체 멤버 초기화 관련 에러 발생
- `LogClass: Error: EnumProperty FLaneInfo::LaneType is not initialized properly`
- `LogClass: Error: StructProperty FLaneInfo::Team1StartPosition is not initialized properly`

### 해결
- `FLaneInfo` 구조체의 멤버 변수에 기본값 추가

### 수정 파일
- `AOSMapManager.h` - `FLaneInfo` 구조체

### 코드 변경

```cpp
// 기존
EAOSLane LaneType;
FVector Team1StartPosition;
FVector Team2StartPosition;

// 수정
EAOSLane LaneType = EAOSLane::Mid;
FVector Team1StartPosition = FVector::ZeroVector;
FVector Team2StartPosition = FVector::ZeroVector;
```

---

## 5. UHT 캐시 문제 해결

### 문제
- 코드 변경 후 Visual Studio에서 빌드 실패 (에러 코드 6)
- `Unhandled exception: ArgumentException: Must specify valid information for parsing in the string`

### 원인
- UnrealHeaderTool(UHT) 캐시가 이전 상태를 유지하고 있어 파싱 에러 발생

### 해결
- `Intermediate/Build` 폴더 삭제 후 재빌드

```bash
rm -rf "c:/UnrealProject/TDProject/Intermediate/Build"
```

---

## PIE 테스트 결과

### 정상 동작 확인 사항
- 타워 18개 전부 정상 스폰 (3 레인 × 2 팀 × 3 타워)
- 커맨드 센터 2개 정상 스폰
- 웨이포인트 큐 정상 빌드 (7개 웨이포인트: 아군 타워 3 + 적 타워 3 + 적 커맨드 센터 1)
- 캐릭터가 웨이포인트를 순서대로 방문하며 이동
- 스폰 비활성화 기능 정상 작동

### Top Lane 테스트 로그 (정상 이동 확인)

```
Waypoint Queue Built: 7 waypoints
  [0] Tower (Team1) at (-1600, 1300, 300)
  [1] Tower (Team1) at (-1500, 500, 0)
  [2] Tower (Team1) at (-1300, -300, 0)
  [3] Tower (Team2) at (-300, -1300, 0)
  [4] Tower (Team2) at (500, -1500, 0)
  [5] Tower (Team2) at (1300, -1600, 300)
  [6] CommandCenter (Team2) at (1600, -1600, 300)

Arrived at waypoint! Distance: 188.7, CurrentWaypointIndex: 0 → Moving to 1
Arrived at waypoint! Distance: 190.3, CurrentWaypointIndex: 1 → Moving to 2
Arrived at waypoint! Distance: 193.5, CurrentWaypointIndex: 2 → Moving to 3
...
```

---

## 수정 파일 요약

| 파일 | 변경 내용 |
|------|-----------|
| `AOSSpawnPoint.h` | `bSpawnEnabled` 프로퍼티 추가 |
| `AOSSpawnPoint.cpp` | 스폰 비활성화 체크 로직 추가 |
| `AOSAIController.h` | `ArrivalDistance` 200 → 100 |
| `AOSAIController.cpp` | 웨이포인트 완료 후 무한 루프 수정 |
| `AOSMapManager.h` | `FLaneInfo` 구조체 기본값 초기화 |

---

## 다음 작업 예정

1. **전체 레인 테스트** - 스폰포인트 8개 전부 활성화하여 3개 레인 동시 동작 확인
2. **캐릭터 메시 할당** - `SetupMesh()` 에 실제 메시 연결
3. **사망/파괴 처리** - 캐릭터 사망, 구조물 파괴 이펙트 구현
4. **AI 파라미터 밸런싱** - `EnemyDetectionRange`, `AttackRange` 조정
