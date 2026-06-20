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
enum class EAOSGameState : uint8 { MainMenu, Lobby, RoundPreparation, RoundRunning, Settlement, BanPick }; // BanPick 은 끝에 append (값 시프트 방지)

// 구조물 타입 (AOSStructure.h에 정의, Object 소유)
enum class EStructureType : uint8 { CommandCenter, Tower };
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

## AOSAnimInstance → AOSCharacter / AOSAIController

소유: **prog-anim** | 소비: **prog-character, prog-ai, art-anim**

```cpp
// AOSAnimInstance.h — prog-character에서 캐릭터 BP에 AnimClass로 할당
// art-anim에서 ABP 부모 클래스로 사용

// ABP 스테이트 머신 조건용 (NativeUpdateAnimation에서 갱신)
float Speed;                // 이동 속도
FVector Velocity;           // 이동 벡터
bool bIsAttacking;          // 공격 중 여부
bool bIsDead;               // 사망 여부
bool bIsHit;                // 피격 중 여부

// prog-ai/prog-character에서 호출
void PlayAttackMontage();
void PlayDeathMontage();
void PlayHitReactMontage();
```

## AOSAnimNotify → AOSCharacter

소유: **prog-anim** | 소비: **art-anim (배치), prog-character (로직 수신)**

```cpp
// AOSAnimNotify_AttackHit — 공격 데미지 적용 시점
// AOSAnimNotify_DeathEnd — 사망 애니메이션 완료 → Destroy
// AOSAnimNotify_HitEnd — 히트 리액션 종료 → 정상 복귀
```

---

## Blueprint 프로퍼티 계약

소유: **Designer (design-balance)** | 소비: **Programmer (C++ 기본값 제공)**

프로그래머가 C++ 기본값을 변경하면, Blueprint에서 오버라이드하지 않은 인스턴스에만 영향.
Blueprint 레벨 값은 design-balance가 관리하며, 프로그래머는 C++ 기본값만 담당.

```
C++ Default (Programmer) → Blueprint Override (Designer) → Instance Override (Level Designer)
```

### 주요 프로퍼티 목록

| Blueprint | 프로퍼티 | C++ 기본값 | Designer 관할 |
|-----------|---------|-----------|--------------|
| BP_Character | MaxHealth | 100.0f | Yes |
| BP_Character | AttackDamage | 10.0f | Yes |
| BP_Character | MovementSpeed | 600.0f | Yes |
| BP_Team1Tower | MaxHealth | 1000.0f | Yes |
| BP_Team1Tower | AttackDamage | 20.0f | Yes |
| BP_AOSAIController | EnemyDetectionRange | 1500.0f | Yes |
| BP_AOSPlayerController | CameraHeight | 12000.0f | Yes |
| BP_ThirdPersonGameMode | GameDuration | 600.0f | Yes |

---

## MCP 에셋 계약

동시 MCP 수정을 방지하기 위한 에셋 소유권은 `ASSET_OWNERSHIP.md` 참조.

---

## 변경 요청 로그

인터페이스 변경이 필요할 때 아래에 추가하세요:

| 날짜 | 요청 에이전트 | 대상 인터페이스 | 변경 내용 | 상태 |
|------|-------------|----------------|-----------|------|
| — | — | — | — | — |
