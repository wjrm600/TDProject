# 에이전트 간 인터페이스 계약

이 문서는 에이전트 경계를 넘어 호출되는 안정적인 public API를 정의합니다.
이 시그니처를 변경하려면 소유 에이전트와 소비 에이전트가 반드시 조율해야 합니다.

---

## AOSMapManager → AOSAIController

소유: **Object** | 소비: **AI**

```cpp
// AOSMapManager.h - AOSAIController::CacheLaneInfo()에서 호출
FVector GetLaneStartPosition(EAOSLane Lane, EAOSTeam Team) const;
FVector GetLaneEndPosition(EAOSLane Lane, EAOSTeam Team) const;

// AOSMapManager.h - AOSAIController::BuildWaypointQueue()에서 호출
AAOSStructure* GetCommandCenter(EAOSTeam Team) const;
TArray<AAOSStructure*> GetTowersInLane(EAOSLane Lane, EAOSTeam Team) const;

// AOSMapManager.h - 일반 참조
FLaneInfo GetLaneInfo(EAOSLane Lane) const;
```

## AOSStructure → AOSAIController

소유: **Object** | 소비: **AI**

```cpp
// AOSStructure.h - AOSAIController::BuildWaypointQueue(), UpdateAIBehavior()에서 호출
bool IsDestroyed() const;
EAOSTeam GetOwnerTeam() const;
EAOSLane GetLane() const;
EStructureType GetStructureType() const;

// AOSStructure.h - AOSAIController::AttackStructure()에서 호출
void ReceiveDamage(float DamageAmount);
float GetCurrentHealth() const;
float GetMaxHealth() const;
```

## AOSCharacter → AOSAIController

소유: **Character** | 소비: **AI**

```cpp
// AOSCharacter.h - AOSAIController::OnPossess(), UpdateAIBehavior()에서 호출
EAOSTeam GetTeam() const;
EAOSLane GetLane() const;
bool IsAlive() const;

// AOSCharacter.h - AOSAIController::AttackTarget()에서 호출
void ReceiveDamage(float DamageAmount);
float GetCurrentHealth() const;
```

## AOSCharacter → AOSStructure

소유: **Character** | 소비: **Object**

```cpp
// AOSCharacter.h - AOSStructure::FireAtTarget(), FindNearestEnemy()에서 호출
EAOSTeam GetTeam() const;
bool IsAlive() const;
void ReceiveDamage(float DamageAmount);
```

## AOSGameMode.h Enum 정의 (공유 계약)

소유: **Character** | 소비: **모든 담당**

```cpp
// 이 enum 값을 변경하면 모든 AOS 파일에 영향
enum class EAOSTeam : uint8 { Team1, Team2 };
enum class EAOSLane : uint8 { Top, Mid, Bottom };
enum class EAOSGameState : uint8 { Preparation, GameRunning, GameEnded };

// 구조물 타입 (AOSStructure.h에 정의, Object 소유)
enum class EStructureType : uint8 { CommandCenter, Tower };
```

## AOSGameMode → AOSCharacter

소유: **Character** | 소비: **UI**

```cpp
// AOSGameMode.h - AOSCharacter::OnCharacterDeath()에서 호출
void OnCharacterDestroyed(AAOSCharacter* DestroyedCharacter);
```

---

## 변경 요청 로그

인터페이스 변경이 필요할 때 아래에 추가하세요:

| 날짜 | 요청 에이전트 | 대상 인터페이스 | 변경 내용 | 상태 |
|------|-------------|----------------|-----------|------|
| — | — | — | — | — |
