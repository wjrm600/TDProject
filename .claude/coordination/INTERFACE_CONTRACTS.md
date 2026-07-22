# 에이전트 간 인터페이스 계약

이 문서는 에이전트 경계를 넘어 호출되는 안정적인 public API를 정의합니다.
이 시그니처를 변경하려면 소유 에이전트와 소비 에이전트가 반드시 조율해야 합니다.

> ⚠️ **진실은 항상 `Source/` 코드.** 이 문서가 코드와 어긋나면 문서 쪽 drift — 발견 즉시 갱신할 것.
> (2026-07-22 전면 동기화: Phase 6 StateTree + GAS 반영. 구 `UpdateAIBehavior`/`AttackTarget`/`AttackStructure`/`MoveTowardsTarget` 은 제거된 API.)

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
// AOSStructure.h - AOSAIController::BuildWaypointQueue(), GetCurrentWaypointStructure()에서 호출
bool IsDestroyed() const;
EAOSTeam GetOwnerTeam() const;
EAOSLane GetLane() const;
EStructureType GetStructureType() const;

// AOSStructure.h - HP 조회 (UI/디버그) — 진실은 ASC/AttributeSet, 이 함수들은 wrapper
float GetCurrentHealth() const;
float GetMaxHealth() const;

// ⚠️ ReceiveDamage(float) 는 deprecated 래퍼(내부에서 GE_Damage 적용) — 신규 코드 직접 호출 금지.
//    AI→구조물 데미지는 GA_Attack(GAS) 흐름으로 적용된다. (아래 "GAS 단일 데미지 흐름" 참고)
```

## AOSCharacter → AOSAIController

소유: **Character** | 소비: **AI**

```cpp
// AOSCharacter.h - AOSAIController::OnPossess(), FindNearestEnemy(), StateTree task 에서 호출
EAOSTeam GetTeam() const;
EAOSLane GetLane() const;
bool IsAlive() const;              // AttributeSet Health > 0
float GetCurrentHealth() const;    // AttributeSet wrapper

// GAS — 공격/스킬 실행·속성 조회는 ASC 경유 (IAbilitySystemInterface)
UAbilitySystemComponent* GetAbilitySystemComponent() const;

// ⚠️ ReceiveDamage(float) 는 deprecated 래퍼(내부 GE_Damage) — 신규 코드 직접 호출 금지.
```

## AOSCharacter → AOSStructure

소유: **Character** | 소비: **Object**

```cpp
// AOSCharacter.h - AOSStructure::FireAtTarget(), FindNearestEnemy()에서 호출
EAOSTeam GetTeam() const;
bool IsAlive() const;
void ReceiveDamage(float DamageAmount);  // 타워 자동공격은 아직 이 래퍼 경유 (내부는 GE_Damage 적용)
```

## AOSGameMode.h Enum 정의 (공유 계약)

소유: **Character** | 소비: **모든 담당**

```cpp
// 이 enum 값을 변경하면 모든 AOS 파일에 영향
enum class EAOSTeam : uint8 { Team1, Team2 };
enum class EAOSLane : uint8 { Top, Mid, Bottom };
enum class EAOSGameState : uint8 { MainMenu, Lobby, RoundPreparation, RoundRunning, Settlement, BanPick }; // BanPick 은 끝에 append (값 시프트 방지)

// 구조물 타입 (AOSStructure.h에 정의, Object 소유)
enum class EStructureType : uint8 { CommandCenter, Tower };
```

## GAS 단일 데미지 흐름 (Phase 0~6 — 전 도메인 공유 계약)

소유: **Character (GAS 코어 = `Source/TDProject/AOS/GAS/`)** | 소비: **모든 담당**

```cpp
// 모든 데미지는 GE_Damage → UAOSAttributeSet::PostGameplayEffectExecute 한 곳을 통과.
//   Health 차감 + Owner 분기(OnCharacterDeath / OnStructureDestroyed). HP바·데미지숫자 훅도 여기.
// 캐릭터·구조물 모두 ASC(UAOSAbilitySystemComponent) + UAOSAttributeSet 소유 (PlayerState 미사용).
// 속성: Health / MaxHealth / AttackPower / AttackRange / AttackSpeed / MoveSpeed / Damage(메타)
// 초기값: DT_CharacterAttributes(캐릭터별 행)·DT_TowerAttributes·DT_CommandCenterAttributes
//         (row = FAOSAttributeInitRow, GAS/Data/AOSAttributeInitData.h) → float 멤버 fallback
// 기본 공격 = UGA_Attack (태그 Ability.Attack.Basic, 쿨다운 단일 진실 = Cooldown.Attack.Basic)
// 스킬 = UGA_SkillBase(C++ abstract) + BP_GA_<Char>_<Slot> 자산 (C++ 빌드 불필요)
```

## AOSAIController StateTree 헬퍼 (Phase 6)

소유: **AI** | 소비: **StateTree task/condition (`AI/AOSStateTreeTasks·Conditions`), ST_AOSCharacterAI 자산**

```cpp
// AOSAIController.h — StateTree task 가 호출하는 public 헬퍼
AAOSCharacter* GetCurrentTargetCharacter() const;
void SetCurrentTarget(AAOSCharacter* InTarget);
AAOSStructure* GetCurrentWaypointStructure() const;
bool IsCurrentTargetInAttackRange() const;   // GetEffectiveAttackRange() 기반
bool HasArrivedAtCurrentWaypoint() const;
void RequestMoveToCurrentTarget();
void RequestMoveToCurrentWaypoint();
void AdvanceToNextWaypoint();
float GetEffectiveAttackRange() const;       // AttributeSet AttackRange 우선 → 멤버 fallback
```

## AOSGameMode 라운드 시스템 API (Phase 2 신규)

소유: **Character** | 소비: **UI, AI**

```cpp
// AOSGameMode.h — 라운드 배치 관련 (prog-ui에서 호출)
void SetLaneDeployCount(EAOSTeam Team, EAOSLane Lane, int32 Count); // 라인별 배치 수 설정 (0~2)
int32 GetLaneDeployCount(EAOSTeam Team, EAOSLane Lane) const;
int32 GetTotalDeployCount(EAOSTeam Team) const;
static const int32 MaxCharactersPerLane = 2;

// AOSGameMode.h — 라운드 진행 (prog-ui에서 호출)
void StartRound();          // 배치된 캐릭터 스폰 + RoundRunning 전이
int32 GetCurrentRound() const;

// AOSGameMode.h — 라운드 종료 (내부 + prog-ai 참조)
// OnCharacterDestroyed()에서 양팀 전원사망 감지 → EndRound() 자동 호출
// EndRound() → 커맨드센터 파괴 확인 → Settlement 또는 다음 RoundPreparation

// AOSGameMode.h — 라운드 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundEnded, int32, RoundNumber);
FOnRoundEnded OnRoundEnded;
```

## AOSGameState 리플리케이션 API (Phase 3A 신규)

소유: **Character** | 소비: **UI (클라이언트 상태 구독)**

```cpp
// AOSGameState.h — 서버만 리플리케이션된 게임 상태를 변경할 수 있음
UPROPERTY(ReplicatedUsing=OnRep_CurrentState) EAOSGameState CurrentState;
UPROPERTY(ReplicatedUsing=OnRep_CurrentRound) int32 CurrentRound;
UPROPERTY(ReplicatedUsing=OnRep_TeamReady) bool bTeam1Ready;
UPROPERTY(ReplicatedUsing=OnRep_TeamReady) bool bTeam2Ready;

// 서버 전용 Setter (AOSGameMode에서 호출)
void ServerSetCurrentState(EAOSGameState NewState);
void ServerSetCurrentRound(int32 NewRound);
void ServerSetTeamReady(EAOSTeam Team, bool bReady);

// 클라이언트 UI 바인딩용 델리게이트 (OnRep_* 호출 시 브로드캐스트)
FOnGameStateChangedClient OnGameStateChangedClient;   // EAOSGameState NewState
FOnRoundNumberChanged    OnRoundNumberChanged;        // int32 NewRound
FOnTeamReadyChanged      OnTeamReadyChanged;          // (no param)

// 헬퍼 (클라이언트에서 조회 가능)
EAOSGameState GetCurrentState() const;
int32 GetCurrentRound() const;
bool IsTeamReady(EAOSTeam Team) const;
bool AreBothTeamsReady() const;
```

## AOSPlayerState 리플리케이션 API (Phase 3A 신규)

소유: **Character** | 소비: **UI (플레이어 팀/준비 상태), GameMode**

```cpp
// AOSPlayerState.h — 서버만 리플리케이션 프로퍼티 변경 가능
UPROPERTY(ReplicatedUsing=OnRep_Team)       EAOSTeam Team;          // 팀 (서버가 PostLogin에서 할당)
UPROPERTY(ReplicatedUsing=OnRep_Ready)      bool bIsReady;          // 준비 완료 여부
UPROPERTY(ReplicatedUsing=OnRep_DeployPlan) int32 DeployCountTop;   // 라인별 배치 수 (0~2)
UPROPERTY(ReplicatedUsing=OnRep_DeployPlan) int32 DeployCountMid;
UPROPERTY(ReplicatedUsing=OnRep_DeployPlan) int32 DeployCountBottom;

// 서버 전용 Setter
void ServerSetTeam(EAOSTeam NewTeam);
void ServerSetReady(bool bReady);
void ServerSetDeployCount(EAOSLane Lane, int32 Count); // 자동 0~2 Clamp

// 클라이언트 UI 바인딩용 델리게이트
FOnPlayerTeamChanged  OnPlayerTeamChanged;
FOnPlayerReadyChanged OnPlayerReadyChanged;
FOnDeployPlanChanged  OnDeployPlanChanged;

// Getter (어디서든 호출 가능)
EAOSTeam GetTeam() const;
bool IsReady() const;
int32 GetDeployCount(EAOSLane Lane) const;
int32 GetTotalDeployCount() const;
```

## AOSPlayerController Server RPC (Phase 3A 신규)

소유: **UI** | 소비: **Character (GameMode 라우팅)**

```cpp
// AOSPlayerController.h — 클라이언트에서 호출하면 서버에서 실행됨
// 라인당 0~2 검증 포함 (WithValidation)
UFUNCTION(Server, Reliable, WithValidation)
void Server_SetLaneDeployCount(EAOSLane Lane, int32 Count);

// 준비 상태 토글. 양쪽 준비 시 RoundPreparation → RoundRunning 자동 전이
UFUNCTION(Server, Reliable)
void Server_SetReady(bool bReady);

// 라운드 시작 요청 (서버가 AreAllPlayersReady() 확인)
UFUNCTION(Server, Reliable)
void Server_RequestStartRound();
```

## AOSGameMode 서버 권한 API (Phase 3A 신규)

소유: **Character** | 소비: **AOSPlayerController (RPC 라우팅)**

```cpp
// AOSGameMode.h — 모두 HasAuthority() 체크 내장
virtual void PostLogin(APlayerController* NewPlayer) override;  // 접속 순서 팀 할당
virtual void Logout(AController* Exiting) override;              // 게임 중 퇴장 → 상대 승리 처리

void ServerSetPlayerReady(AAOSPlayerState* PlayerState, bool bReady);
void ServerSetLaneDeployCountForPlayer(AAOSPlayerState* PlayerState, EAOSLane Lane, int32 Count);
bool AreAllPlayersReady();  // GetNumPlayers()가 non-const라 non-const

// StartRound(), EndRound(), EndGame(), SetLaneDeployCount() 모두 HasAuthority() 선행 체크
// SetGameState() 내부에서 AAOSGameState::ServerSetCurrentState() 호출로 리플리케이션 동기화
```

## AOSGameMode → AOSCharacter

소유: **Character** | 소비: **UI**

```cpp
// AOSGameMode.h - AOSCharacter::OnCharacterDeath()에서 호출
void OnCharacterDestroyed(AAOSCharacter* DestroyedCharacter);
```

---

## AOSAnimInstance / 몽타주 (prog-anim ↔ prog-character ↔ art-anim)

소유: **prog-anim** | 소비: **prog-character, art-anim**

```cpp
// AOSAnimInstance.h — ABP 부모 클래스 (생성자 RootMotionMode = RootMotionFromMontagesOnly).
// GAS 태그/무브먼트 미러 변수 (ABP 스테이트 머신 조건용, NativeUpdateAnimation 갱신):
float Speed; float Direction;
bool bIsMoving; bool bIsFalling;
bool bIsAttacking;    // Ability.Attack.Basic 태그 미러
bool bIsCasting;      // State.Casting
bool bIsHitReacting;  // State.HitReact
bool bIsDead;

// 몽타주 재생 주체는 AAOSCharacter (prog-character 소유):
//   슬롯: AttackMontage / HitReactMontage / DeathMontage / SkillMontages(map)
//         — nullptr 면 조용히 skip (자산 없어도 PIE 동작)
//   Multicast_PlayHitReact / Multicast_PlayDeathMontage (서버→전클라)
//   HitReact↔Attack/Skill 충돌 규칙 (A)(B) = CLAUDE.md "애니메이션" 섹션 준수
```

## AnimNotify

소유: **prog-anim** | 소비: **art-anim (몽타주 배치), prog-character (로직 수신)**

```cpp
// Anim/AOSAnimNotify_AttackHit — 공격 타격 시점 (서버에서 데미지 이벤트 게이트)
// ⚠️ DeathEnd/HitEnd Notify 는 존재하지 않음 — 사망 정리는 SetLifeSpan 타이머,
//    피격 종료는 GE_HitReact_State 의 Duration(SetByCaller=몽타주 길이) 기반.
```

---

## Blueprint 프로퍼티 계약

소유: **Designer (design-balance)** | 소비: **Programmer (C++ 기본값 제공)**

프로그래머가 C++ 기본값을 변경하면, Blueprint에서 오버라이드하지 않은 인스턴스에만 영향.
Blueprint 레벨 값은 design-balance가 관리하며, 프로그래머는 C++ 기본값만 담당.

```
C++ Default (Programmer) → Blueprint Override (Designer) → Instance Override (Level Designer)
```

### 주요 파라미터 목록

⚠️ **캐릭터·구조물 전투 속성의 단일 진실은 DataTable** (GAS 초기화) — BP float 멤버는 fallback.
기본공격 쿨다운 = `1 / AttackSpeed` (별도 AttackCooldown 속성 없음 — 캐릭터 기준).

| 대상 | 파라미터 | 위치 (단일 진실) | Designer 관할 |
|------|---------|-----------------|--------------|
| 캐릭터 전투 속성 | Health/MaxHealth/AttackPower/AttackRange/AttackSpeed/MoveSpeed | `DT_CharacterAttributes` 캐릭터별 행 (row=FAOSAttributeInitRow) | Yes |
| 타워 HP | MaxHealth 1000 | `DT_TowerAttributes` | Yes |
| 커맨드센터 HP | MaxHealth 5000 | `DT_CommandCenterAttributes` | Yes |
| 타워 자동공격 | AttackDamage / AttackRange 600 / AttackCooldown 2.0 | AOSStructure BP 멤버 (레거시 직접 경로) | Yes |
| AI 감지 | EnemyDetectionRange 1500 | BP_AOSAIController | Yes |
| 경제 | GoldPerCharacterKill 50 / GoldPerStructureKill 150 / GoldPerRoundIncome 100 | AOSGameMode (EditAnywhere) | Yes |
| 아이템 | 가격/효과 | `DT_Items`(FAOSItemRow) + BP_GE_Item_* | Yes |
| 카메라 | CameraHeight 12000 등 | BP_AOSPlayerController | Yes |
| 게임 시간 | GameDuration 600 | AOSGameMode | Yes |

---

## MCP 에셋 계약

동시 MCP 수정을 방지하기 위한 에셋 소유권은 `ASSET_OWNERSHIP.md` 참조.

---

## 변경 요청 로그

인터페이스 변경이 필요할 때 아래에 추가하세요:

| 날짜 | 요청 에이전트 | 대상 인터페이스 | 변경 내용 | 상태 |
|------|-------------|----------------|-----------|------|
| — | — | — | — | — |
