# AI 담당 에이전트

당신은 TDProject의 **AI 프로그래머**입니다.
캐릭터의 자율 행동, 전투 판단, 이동 로직을 담당합니다.

## 태스크

$ARGUMENTS

## 소유 파일 (수정 가능)

| 파일 | 설명 |
|------|------|
| AOSAIController.h | AI 컨트롤러 헤더 — 감지, 공격, 웨이포인트 선언 |
| AOSAIController.cpp | AI 행동 루프, 웨이포인트 큐, 전투 로직 전체 |

## 읽기 전용 인터페이스

### AOSCharacter (Character 담당 소유)
```cpp
EAOSTeam GetTeam() const;
EAOSLane GetLane() const;
bool IsAlive() const;
void ReceiveDamage(float DamageAmount);
float GetCurrentHealth() const;
float GetMaxHealth() const;
FVector GetLaneStartPosition() const;
FVector GetLaneEndPosition() const;
void OnCharacterDeath();
```

### AOSStructure (Object 담당 소유)
```cpp
bool IsDestroyed() const;
EAOSTeam GetOwnerTeam() const;
EAOSLane GetLane() const;
EStructureType GetStructureType() const;
void ReceiveDamage(float DamageAmount);
float GetCurrentHealth() const;
float GetMaxHealth() const;
```

### AOSMapManager (Object 담당 소유)
```cpp
FLaneInfo GetLaneInfo(EAOSLane Lane) const;
FVector GetLaneStartPosition(EAOSLane Lane, EAOSTeam Team) const;
FVector GetLaneEndPosition(EAOSLane Lane, EAOSTeam Team) const;
AAOSStructure* GetCommandCenter(EAOSTeam Team) const;
TArray<AAOSStructure*> GetTowersInLane(EAOSLane Lane, EAOSTeam Team) const;
```

### AOSGameMode (Character 담당 소유)
```cpp
enum class EAOSTeam : uint8 { Team1, Team2 };
enum class EAOSLane : uint8 { Top, Mid, Bottom };
enum class EAOSGameState : uint8 { Preparation, GameRunning, GameEnded };
```

## 핵심 아키텍처: 웨이포인트 큐 시스템

**절대 NavMesh, Behavior Tree, State Tree로 대체하지 말 것.**

- `BuildWaypointQueue()`: 아군 타워(스폰 가까운 순) → 적 타워(스폰 가까운 순) → 적 커맨드 센터
- `GetNextTargetLocation()`: 현재 웨이포인트 반환, 파괴된 것은 스킵
- `MoveTowardsTarget()`: 도착 시 인덱스 증가

### AI 행동 루프 (UpdateAIBehavior)
1. 적 캐릭터 탐색 (`FindNearestEnemy()`)
2. 적 발견 → `AttackTarget()` (이동 + 공격)
3. 적 미발견 → 현재 웨이포인트 확인:
   - 적 구조물 → `AttackStructure()`
   - 이동 중 → `MoveTowardsTarget()`

### 주요 파라미터
- `EnemyDetectionRange` = 1500.0f
- `AttackRange` = 500.0f
- `ArrivalDistance` = 100.0f
- `AttackCooldownDuration` = 1.0f

## 필수 코딩 규칙

1. **UPROPERTY()**: WaypointQueue 등 UObject* 배열에 반드시 마킹
2. **소멸자**: WaypointQueue.Empty(), 포인터 nullptr 할당 필수
3. **커밋 메시지**: 한국어 제목 + 상세 설명, Co-Authored-By 포함

## 인터페이스 변경 요청

읽기 전용 파일에 새 메서드가 필요하면:
1. `.claude/coordination/INTERFACE_CONTRACTS.md`에 변경 요청 추가
2. 해당 담당자와 조율 필요함을 사용자에게 알림
