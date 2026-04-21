# Blueprint 프로퍼티 레퍼런스

**대상**: design-balance, design-level 에이전트
**목적**: C++ 재컴파일 없이 에디터에서 수정 가능한 프로퍼티 전체 목록

---

## Blueprint에서 수정 가능한 프로퍼티

아래 프로퍼티는 모두 `UPROPERTY(EditAnywhere)` 또는 `UPROPERTY(EditDefaultsOnly)`로 선언되어 있습니다.

---

### AOSCharacter (BP_Character)

| 프로퍼티 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| MaxHealth | float | 100.0 | 최대 체력 |
| AttackDamage | float | 10.0 | 공격력 |
| AttackRange | float | 500.0 | 공격 사거리 (cm) |
| AttackCooldown | float | 1.0 | 공격 쿨다운 (초) |
| MovementSpeed | float | 600.0 | 이동 속도 (cm/s) |
| Team | EAOSTeam | - | 팀 (Team1/Team2) |
| AssignedLane | EAOSLane | - | 배치 레인 |
| HealthBarWidgetClass | TSubclassOf | WBP_HealthBar | HP 바 위젯 |

---

### AOSStructure / AOSTower (BP_Team1Tower, BP_Team2Tower)

| 프로퍼티 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| MaxHealth | float | 1000.0 | 최대 체력 |
| AttackDamage | float | 20.0 | 공격력 |
| AttackRange | float | 200.0 | 공격 사거리 (cm) |
| AttackCooldown | float | 2.0 | 공격 쿨다운 (초) |
| DetectionRange | float | 1500.0 | 적 감지 거리 (cm) |
| Team | EAOSTeam | - | 팀 (Team1/Team2) |
| StructureType | EAOSStructureType | Tower | Tower / CommandCenter |
| HealthBarWidgetClass | TSubclassOf | WBP_HealthBar | HP 바 위젯 |

---

### AOSStructure / AOSCommandCenter (BP_Team1CC, BP_Team2CC)

| 프로퍼티 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| MaxHealth | float | 5000.0 | 최대 체력 |
| Team | EAOSTeam | - | 팀 |

---

### AOSAIController (BP_AOSAIController)

| 프로퍼티 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| EnemyDetectionRange | float | 1500.0 | 적 캐릭터 감지 범위 (cm) |
| AttackRange | float | 500.0 | 공격 시작 거리 (cm) |
| ArrivalDistance | float | 100.0 | 웨이포인트 도착 판정 거리 (cm) |
| AttackCooldownDuration | float | 1.0 | 공격 쿨다운 (초) |

---

### AOSPlayerController (BP_AOSPlayerController)

| 프로퍼티 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| CameraHeight | float | 12000.0 | 카메라 높이 (cm) |
| CameraPitch | float | -70.0 | 카메라 피치 각도 |
| CameraYaw | float | 0.0 | 카메라 요 각도 |
| CameraMoveSpeed | float | 8000.0 | 카메라 이동 속도 (cm/s) |
| ZoomSpeed | float | 2000.0 | 줌 속도 |
| MinZoomHeight | float | 4000.0 | 최소 줌 높이 |
| MaxZoomHeight | float | 20000.0 | 최대 줌 높이 |
| MapBoundaryX | float | 40000.0 | 맵 X 경계 |
| MapBoundaryY | float | 40000.0 | 맵 Y 경계 |

---

### AOSSpawnPoint (배치 액터)

| 프로퍼티 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| Team | EAOSTeam | Team1 | 팀 |
| Lane | EAOSLane | Top | 레인 |
| SpawnIndex | int32 | 0 | 같은 팀/레인 내 순서 |
| bSpawnEnabled | bool | true | 이 포인트에서 캐릭터 스폰 여부 |

---

### AOSGameMode

| 프로퍼티 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| GameDuration | float | 600.0 | 최대 게임 시간 (초) |

---

## C++ 재컴파일이 필요한 변경

아래 항목은 코드에 하드코딩되어 있어 Blueprint에서 변경 불가:
- 웨이포인트 큐 빌드 로직 (`AOSAIController::BuildWaypointQueue()`)
- 타워 순서 결정 방식 (스폰 포인트와 가까운 순서)
- 충돌 설정 (CollisionComponent, DetectionRange 오버랩 타입)

변경이 필요하면 `CROSS_DOMAIN_REQUESTS.md`에 prog-object 또는 prog-ai에게 요청하세요.

---

## HP 바 위젯 설정

Blueprint에서 HP 바를 표시하려면:

1. `Content/AOS/UI/` 에서 `WBP_HealthBar` Blueprint 생성
2. 부모 클래스: `AOSHealthBarWidget`
3. ProgressBar 추가 → 이름: **정확히** `HealthProgressBar`
4. 각 Blueprint (BP_Character, BP_Tower 등)의 `HealthBarWidgetClass` 슬롯에 `WBP_HealthBar` 지정

> ProgressBar 이름이 `HealthProgressBar`가 아니면 `BindWidget` 오류 발생

---

**최종 업데이트**: 2026-04-21
