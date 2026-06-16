# TDProject — 작업 타임라인

> **목적**: 발표·회고·이력 관리용. 의미 있는 작업(기능/리팩토링/트러블슈팅/환경/도구)을 시간 순으로 기록.
> **자동 갱신 규칙**: `CLAUDE.md` 의 "## 작업 타임라인 자동 갱신" 섹션 참고. 의미 단위로 묶어 작업 완료 시점에 항목 추가.
> **항목 형식**: `작업 내용` · `문제점/난관` · `해결 방법` · `결과/영향` (+ 관련 커밋 해시).

---

## 2026-04-27 ~ 04-30 — MCP 환경 표준화 + 멀티 에이전트 구조 + SpawnPoint 단일화

**작업 내용**
- 다른 컴퓨터에서 동일하게 작업 가능하도록 MCP(unreal-engine + unreal-rag) 셋업 표준화
- `.claude/agents/` 서브에이전트로 마이그레이션, 도메인별 모델 분기(Sonnet/Opus/Haiku)
- `FLaneInfo` 의 `Team*StartPosition` 제거 → SpawnPoint 를 라인 시작 위치의 단일 진실 공급원으로
- 12개 SpawnPoint 정규화(중복/잘못된 인덱스 정리)

**문제점**
- 엔진 설치 경로가 머신마다 달라 빌드 명령·MCP 설정 하드코딩이 불가능
- 라인 시작 좌표가 `FLaneInfo` 와 `AAOSSpawnPoint` 양쪽에 이중 저장 → MCP 자동화로 한쪽만 변경되면 desync

**해결 방법**
- `UE_ROOT` 환경변수 + `claude_desktop_config.example.json` 템플릿 + `Mcp_Tools/README.md` 셋업 가이드
- `AAOSMapManager::GetLaneStartPosition` 내부를 `GameMode->GetNearestSpawnPoint` 경유로 변경

**결과**
- 새 컴퓨터에서 약 5분 셋업, 라인 자동화 시 desync 0
- 커밋: `f61a6e4`, `d279ccb`, `3abf9e8`, `0ed299a`

---

## 2026-05-01 ~ 05-02 — ServerTravel race + NavMesh stale 수정

**작업 내용**
- 라운드 진행을 위한 `ServerTravel` 직후 안정성 확보
- 지형 4배 확장 작업 + 누락된 저장 복구

**문제점**
- 레벨 전환 직후 `MapManager` nullptr crash (초기화 race)
- 지형 변경 후에도 AI가 옛 영역에 갇혀 멈춤 — RecastNavMesh 가 `RuntimeGeneration=Static`(cooked)

**해결 방법**
- `MapManager` nullptr 가드 + race 재시도 로직
- 지형/Nav 영역 변경 시 **Build → Build Paths Only** 강제, `CLAUDE.md` 에 절대 규칙으로 기록

**결과**
- 라운드 전환 무중단, AI 정지 0
- 커밋: `2fe16d4`, `421e56b`, `9abad72`

---

## 2026-05-03 — GAS 도입 (Phase 0~3, 5)

**작업 내용**
- Gameplay Ability System 으로 데미지/속성/공격 일원화
- `UAOSAbilitySystemComponent` + `UAOSAttributeSet` + `UGA_Attack` + `UGE_Damage`
- DataTable 기반 속성 초기화 (Character/Tower/CommandCenter)
- Structure 도 `IAbilitySystemInterface` 구현 (Phase 5)

**문제점**
- 기존 float 멤버(Health/AttackDamage) + deprecated `ReceiveDamage` 흐름 + Structure 는 ASC 없음 → 데미지 경로 이원화
- UE 5.4+ 의 `GameplayEffectComponent` 시스템에서 cooldown GE 가 태그를 grant 못해 매 frame 활성화

**해결 방법**
- AttributeSet 의 `PostGameplayEffectExecute` 가 Owner 분기(Character/Structure)로 사망 처리
- Cooldown GE 에 `CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>` + `GEComponents.Add` 패턴

**결과**
- Damage 흐름 단일화, 공격자/피격자 모두 ASC 통과
- 커밋: `77b0d9b`

---

## 2026-05-07 — StateTree AI 마이그레이션 (Phase 6)

**작업 내용**
- `AOSAIController::UpdateAIBehavior` (if/else) 를 StateTree 로 교체
- Custom Tasks(`FindNearestEnemy`, `MoveToCurrentTarget`, `MoveToCurrentWaypoint`, `SendAttackEvent`, `ActivateAbilityByTag`) + Conditions

**문제점**
- 초기화 race 로 schema binding 실패 — `BeginPlay` 시점엔 `GetPawn()=null`
- `ContextActorClass` 를 AIController 로 잘못 지정해 binding 실패
- Running task 가 있는 state 는 자동 재선택 안 됨 → 적 만나도 안 싸움

**해결 방법**
- `bStartLogicAutomatically=false` + `OnPossess` 에서 수동 `StartLogic()`
- ContextActorClass = `AOSCharacter` (Pawn 클래스)
- Root 에 `On Tick` 트랜지션으로 우선순위 강제 재평가

**결과**
- 시각 편집 가능한 AI 행동 결정, 트러블슈팅 4종 문서화
- 커밋: `761b700`, `684c255`

---

## 2026-05-12 — 애니메이션 슬롯 + Death Montage Root Motion + Ragdoll (Phase 3.5)

**작업 내용**
- 캐릭터 애니 스캐폴딩: `UAOSAnimInstance`, 4개 몽타주 슬롯, AnimNotify
- 사망 모션: 루트모션으로 마지막 발걸음 → 랙돌 전환

**문제점**
- 사망 몽타주가 InPlace 로 재생됨 — RootMotionMode 기본값이 `NoRootMotionExtraction`
- 외부 AnimSequence 의 bone track 이 비어 있고 root motion 데이터가 curve 에만 있음
- `MOVE_NavWalking` 이 root motion delta 를 snap-to-navmesh 로 무효화
- StateTree 가 다음 틱에 `SendAttackEvent` 로 `AttackMontage` 를 재생해 `DeathMontage` 덮어씀
- 사망 진입 시 capsule collision 끄면 `FindFloor` 실패 → `MOVE_Falling` 으로 흐름 깨짐

**해결 방법**
- `RootMotionMode = RootMotionFromMontagesOnly` 생성자 설정
- `EncodeRootBoneModifier` (pelvis → root) baking — 자산 복제 후 적용
- `OnCharacterDeath` 호출 순서: ① `Brain->StopLogic` ② `MOVE_NavWalking → MOVE_Walking` ③ `Multicast_PlayDeathMontage` ④ `StartRagdoll` 타이머에서만 capsule NoCollision
- 사망 시퀀스 동안 capsule/Tick 절대 끄지 않음

**결과**
- 자연스러운 루트모션 사망 → 랙돌 정착
- 커밋: `42dbf14`

---

## 2026-05-15 — HitReact 충돌 규칙 + Montage-driven Damage + AttackSpeed 동적 적용

**작업 내용**
- 데미지 발생 시점을 AnimNotify 와 동기화 (`WaitGameplayEvent("AnimNotify.AttackHit")`)
- `AttackSpeed` 속성이 몽타주 재생속도(Rate) + 쿨다운(1/AttackSpeed) 양쪽에 반영
- HitReact ↔ Attack 충돌 규칙 두 가지

**문제점**
- 같은 DefaultSlot 에서 HitReact 가 Attack 을 강제 중단 → 공격이 끊김
- AI 가 HitReact 도중에도 공격을 시도해 부자연
- `AttackSpeed` 가 실제 게임플레이에 거의 영향 없었음

**해결 방법**
- Rule A: `Multicast_PlayHitReact` 진입 시 `Ability.Attack.Basic` 태그 보유면 HitReact 스킵
- Rule B: `UGE_HitReact_State` 가 `State.HitReact` 부여 → `GA_Attack::ActivationBlockedTags` 가 차단
- SetByCaller 로 HitReact GE 의 Duration 을 몽타주 길이로 동적 설정

**결과**
- 공격 끝까지 재생 + 데미지 타이밍 시각 일관, AttackSpeed 가 게임플레이에 직접 영향
- 커밋: `47137cc`

---

## 2026-05-26 — Alex 4스킬 (Phase 4) + Git LFS

**작업 내용**
- 캐릭터당 LoL 3스킬 + 1궁극기 — Alex = 가렌 Q(DecisiveStrike) / W(Courage) / E(Judgment) / R(DemacianJustice)
- `GA_Alex_Q/W/E/R` C++ 4종 + 쿨다운 GE 4종 + 효과 GE(`MoveSpeed_Boost`, `DamageShield`, `EnhancedAttack`)
- StateTree 에 `UseR/W/Q/E` state + 우선순위 (Design B)
- Git LFS — `.uasset/.umap` 추적 (forward-only)

**문제점**
- StateTree 에서 스킬이 발동 안 됨 — Root 의 On Tick 전환이 AttackEnemy 로만 강제 → 스킬은 평가도 안 됨
- `HasCooldownTag` 의 `bInvert` 기본값 false → 거꾸로(쿨다운 중에만 통과) 동작
- `FStateTreeTask_ActivateAbilityByTag` 가 `TriggerEventData` 를 전달 안 함 → R 이 Target null 로 데미지 0
- ParagonKwang 2.4GB 가 GitHub 무료 LFS 1GB 한계 초과

**해결 방법**
- Design B: running state(PushLane/AttackEnemy)만 `On Tick → Root` 재선택, 우선순위는 자식 순서, PushLane 맨 마지막
- 모든 스킬 EnterCondition 의 `HasCooldownTag.bInvert = true`
- R 에 AIController 타겟 fallback (`GetCurrentTargetCharacter` → `FindNearestEnemy`)
- LFS 전략 = "프로젝트 에셋만 추적 + forward-only, 대용량 외부 자산은 gitignore"

**결과**
- 4스킬 정상 발동 + 외부 자산 의존성 해소
- 커밋: `7bab478`, `9b9ab32`

---

## 2026-05-26 ~ 05-28 — 환경 트러블: BSOD 0x50 (cbfltfs4.sys / MarkAny ePageSafer)

**작업 내용**
- UE 에디터 + Claude Desktop 동시 실행 시 강제 재부팅 5회 진단/해결

**문제점**
- 외관상 RAM/PSU 의심 (UE 실행 시점에 터지므로) — 무관한 방향으로 시간 낭비 위험

**해결 방법**
- Event Viewer → BugCheck 1001 + WHEA 없음 + 디스크 SMART 정상 확인
- WinDbg(`cdb`) 설치 → `!analyze -v` → `IMAGE_NAME: cbfltfs4.sys` + `PROCESS_NAME: python.exe`(unreal-rag MCP) 식별
- 드라이버 추적: 2017년 EldoS CBFS Filter, 소유 앱 = MarkAny ePageSafer (이미 삭제됨, 드라이버만 잔재)
- `sc.exe config cbfltfs4 start= disabled` + 재부팅

**결과**
- BSOD 0회 + 가이드 문서화 (`Guides/04_Testing/TROUBLESHOOTING_BSOD_cbfltfs4.md`)
- 커밋: `322dd3b` (문서 포함)

---

## 2026-05-29 — unreal-rag 자동 stale 감지

**작업 내용**
- `ue_rag_mcp.py` 가 코드 변경 시 자동 재인덱싱

**문제점**
- 기존 로직이 `chroma_db` 존재 시 로드만 하고 재인덱싱 경로 없음 → 최초 인덱싱 이후 수정한 모든 코드가 검색 누락 → 디버깅 시간 손실

**해결 방법**
- 인덱싱 시 최신 소스 `mtime` 을 `chroma_db/index_meta.json` 에 기록
- 시작 시 현재 `Source/` 최신 mtime 과 비교 → 더 최신이면 전체 재인덱싱 (구버전 인덱스는 메타 없음 → stale 처리)

**결과**
- 코드 수정 → MCP 재시작만으로 인덱스 자동 갱신
- 커밋: `322dd3b`

---

## 2026-05-29 — 시전 root + 상하체 분리 Part A (Phase 4+ C++)

**작업 내용**
- 스킬별 "시전 중 이동 가능 여부" 플래그(`bAllowMovementDuringCast`) 도입
- `GE_Rooted` + `AOSCharacter::ApplyCastRoot` + `FStateTreeTask_ActivateAbilityByTag` 의 State.Rooted 대기 로직
- `bIsCasting` 태그 미러로 ABP 상하체 분리 트리거 준비

**문제점**
- 스킬 발동 시 슬라이드(StateTree task 즉시 Succeeded → AI 이동 재개 + DefaultSlot 전신 캐스트 애니가 캡슐 이동 위에 그대로 재생)
- 이동 불가 스킬도 AI 가 즉시 다른 상태로 빠짐 → 시각/논리 부조화

**해결 방법**
- 플래그 1개로 root 적용 여부 결정 (Q/W=true, E/R=false)
- StateTree task 가 `State.Rooted` 보유 중엔 RUNNING 유지(EnterState+Tick) → 이동 불가 스킬은 AI 가 끝까지 홀드
- ABP 신호용 `State.Casting` 태그 (4개 스킬 공통 `ActivationOwnedTags`)

**결과**
- Part A C++ 완료 + 커밋. Part B(ABP `Layered Blend Per Bone` + UpperBody 슬롯)는 진행 중.
- 커밋: `322dd3b`

---

## 2026-06-02 — 데이터 주도 스킬 마이그레이션 3단계 (cleanup) 완료

**작업 내용**
- 구버전 C++ 클래스 8개(파일 16개) 제거 — git rm:
  - `Source/TDProject/AOS/GAS/Abilities/GA_Alex_{Q,W,E,R}.h/cpp` (4쌍)
  - `Source/TDProject/AOS/GAS/Effects/GE_Cooldown_Alex_{Q,W,E,R}.h/cpp` (4쌍)
- CLAUDE.md "Phase 4" 섹션 상단에 historical note 추가 (스펙 참고용으로만 유지)
- CLAUDE.md 마이그레이션 상태 표를 1·2·3 단계 모두 ✅ 로 표기
- CLAUDE.md "Phase 4+ 시전 root" 섹션의 `GA_Alex_*.h` 참조를 `UGA_SkillBase` 로 갱신
- 외부 참조 점검: AOSAnimInstance.cpp/GE_SkillCooldown_Base.h 의 코멘트 2개가 옛 클래스 언급 → 현 패턴 기준 문구로 갱신 (코드 의존성 0)

**문제점 / 난관**
- 캐릭터당 8개씩 늘어나는 C++ 클래스 폭증을 막기 위해 데이터 주도로 전환했으나, 마이그레이션 동안 옛+새가 병행 운영 → 정리 누락 시 의도치 않은 이중 활성화 위험
- 단순 삭제 전에 잔존 참조(코멘트 포함) 모두 grep 으로 확인 필요

**해결 방법**
- 점진적 마이그레이션 후 PIE 검증 통과 시점에 일괄 삭제
- 코멘트는 historical 가치 있는 곳만 과거 시제로 정리, 나머지는 BP 패턴 기준으로 갱신

**결과**
- C++ 클래스 수: 8 (구) → 0 (현). 캐릭터 추가 시 BP 자산만 작성. 향후 N캐릭터·M스킬 확장 시 코드 0줄 추가.
- 마이그레이션 전체(1+2+3) 완료
- 관련 가이드: `Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md`

---

## 2026-06-02 — GA_Attack 인스턴스 영구 stuck race 디버그/픽스

**작업 내용**
- 데이터 주도 스킬 마이그레이션 후 PIE 검증 중 발견된 race 진단
- 3중 방어 픽스 적용:
  1. `GA_Attack::ActivationBlockedTags` 에 `State.Casting` 추가 — 스킬 시전 중 새 Attack 활성화 차단
  2. `UGA_SkillBase::ActivateAbility` 진입 시 `ASC->CancelAbilities({Ability.Attack.Basic})` — 살아있는 Attack 강제 종료
  3. `GA_Attack` 의 PlayMontageAndWait `bAllowInterruptAfterBlendOut: false → true` — Interrupted 콜백 누락 race 근본 차단

**문제점 / 난관**
- 첫 만남에서 일반 공격 1회 발동 후, Q 한 번 쓰면 이후 일반 공격 영영 안 나옴 — Q 가 잘 활성화되는데도 일반 공격만 dead
- 로그: `Can't activate ... Default__GA_Attack ... already a currently active instance` 가 매 틱 반복
- `showdebug abilitysystem`: Q 가 모두 끝났는데도 (`State.Casting`/`State.Rooted`/`Ability.Skill.Alex.Q` 다 해제) `Ability.Attack.Basic` 만 영구히 남음
- 원인: GA_Attack 의 `PlayMontageAndWait` 가 Q 몽타주에 의해 같은 슬롯/blend out 타이밍에 interrupt 될 때 `OnInterrupted` 콜백 누락 → `EndAbility` 미호출 → 인스턴스 영구 active (`InstancedPerActor` 라 재활성화 불가)

**해결 방법**
- 새 Attack 차단(State.Casting) 만으로는 부족 — **이미 stuck 된 인스턴스** 처리 못 함
- 스킬 시작 시 `CancelAbilities` 로 명시적 정리 + PlayMontageAndWait 자체 race 도 `bAllowInterruptAfterBlendOut=true` 로 차단
- 3중 방어가 서로 보완 — 어느 하나가 실패해도 나머지가 커버

**결과**
- Q ↔ 일반 공격 정상 사이클 복구 ("잘 공격하고 있어" 확인)
- 다음 검증 필요: W/E/R 도 동일하게 잘 동작하는지

---

## 2026-06-02 — 게임 방향성 확정 (오토배틀러 MOBA) + 비전 문서

**작업 내용**
- 게임 정체성을 명문화: **드래프트로 5캐릭 뽑아 라인 배치 → 라운드 사이 글로벌 골드로 아이템 구매 → AI 전투 관전 → 적응**하는 오토배틀러 MOBA (북극성: Mechabellum)
- 라운드 모델 확정: **맵 누적** (캐릭터는 라운드마다 리셋·재배치, 타워/CC HP는 누적)
- 디자인 기둥 4개 정립 (전략이 곧 플레이 / 관전이 읽혀야 함 / AI가 캐릭터 개성 연기 / AI로 만든·AI를 지휘하는 게임)
- 임계 경로 로드맵 Slice 0~4 (심장박동 → 가독성 → AI 깊이 → 드래프트 → 매크로)
- `Guides/01_GameOverview/GAME_VISION.md` 신규 — "무엇을 만드는가"의 단일 진실 공급원

**문제점 / 난관**
- 코드에 "누가 무엇을 하는 게임인가"가 드러나지 않았음 — 시스템(GAS/StateTree/데이터주도)은 두꺼운데 코어 재미 루프 정의가 약했음
- 플레이어 입력이 *상점+배치* 둘뿐 → 결정→결과 인과가 안 읽히면 전략적 쾌감이 안 생기는 구조적 리스크

**해결 방법**
- 방향성 점검 세션으로 코어 루프·플레이어 동사·라운드 모델을 명시적으로 정의
- 핵심 재정의: "관전하는 게임이라 **AI의 똑똑함보다 AI의 가독성**이 게임 그 자체" → 미니맵·결과요약을 장르 필수 인프라로 승격
- 이미 구현된 토대(AOSPlayerState.DeployCount*, 라운드 전환, 데이터주도 스킬)와 비전을 매핑해 진짜 빈칸(상점/골드/라인승패판정/AI깊이/미니맵)을 특정

**결과**
- 후속 모든 작업의 우선순위 판단 기준 확보 (GAME_VISION 이 1순위 문서)
- 다음 작업: Slice 0 (심장박동 — 골드 경제 + 최소 상점 + 아이템 1~2개)
- 열린 튜닝 포인트 기록: 2라인 재배치 룰 방향, 골드 스노볼 여부, 로스터 규모
- 관련 문서: `Guides/01_GameOverview/GAME_VISION.md`

---

## 2026-06-02 — Slice 0: 골드 경제 + 아이템 상점 메커니즘 (심장박동 검증 통과)

**작업 내용**
- 비전 로드맵 Slice 0 ("심장박동 증명") 구현 — *골드 벌고 → 아이템 사고 → 캐릭터 강해진다* 루프
- **골드 (`AOSGameState`)**: `Team1/Team2Gold` (Replicated + OnRep + 델리게이트), `ServerAddGold/ServerSetGold/GetGold`
- **적립 (`AOSGameMode`)**: 캐릭터 처치 시 상대 팀 +50, 구조물 파괴 시 +150, 라운드 시작 패시브 +100 (전부 EditAnywhere 튜닝). killer 추적 없이 "죽은 쪽 반대 팀" 으로 단순화
- **아이템 (`FAOSItemRow` DataTable + `AOSGameMode`)**: `(팀,라인)→구매 아이템 GE 목록` 서버 인벤토리. `SpawnCharactersForRound` 에서 스폰 직후 재적용 → **라운드 리셋돼도 누적**. `ServerBuyLaneItem`(준비/정산 단계 + 골드 검증), `BuyItem` 콘솔 exec(테스트), `Server_BuyLaneItem` RPC(위변조 방지=서버가 PlayerState 팀 강제)
- **BP 자산** (agent-design-balance): `BP_GE_Item_Sword`(AttackPower+25, Infinite), `BP_GE_Item_Vitality`(MaxHealth+200), `DT_Items`(Sword/Vitality 각 100골드), `TDProj_GM.ItemTable` 연결

**문제점 / 난관**
- 구매 시 "ItemTable 미설정" 로그 — 아이템이 안 사짐
- 원인 추적: World Settings 오버라이드는 TDProj_GM_C 로 정상인데도 실패 → `DefaultEngine.ini` 의 **`GlobalDefaultGameMode=/Script/TDProject.AOSGameMode` (C++ 스태틱 클래스)** 발견. BP `TDProj_GM` 의 모든 설정(ItemTable/Roster)이 빠진 raw C++ 인스턴스가 특정 경로에서 활성화됨
- 추가로 design-balance 가 Python `set_editor_property` 로 CDO 만 바꾸고 BP 재컴파일을 안 해 stale CDO 가능성

**해결 방법**
- `DefaultEngine.ini` → `GlobalDefaultGameMode=/Game/TDProj_GM.TDProj_GM_C` (BP 로 교체)
- TDProj_GM 강제 재컴파일 + 저장 (ItemTable 을 generated class CDO 에 확정)
- 사용자 직감("스태틱 클래스가 들어가 있다")이 정확했음 — 진단의 결정적 단서

**결과**
- PIE 검증 통과: 아이템 정상 구매 + 캐릭터 점점 강해짐 확인 → **게임의 핵심 루프 작동 증명**
- 다음(Slice 1 — 가독성): 미니맵 / 라운드 결과 요약 / 데미지 피드백. 비전 기둥 2 "관전이 읽혀야 한다"
- 관련 문서: `Guides/01_GameOverview/GAME_VISION.md` (로드맵)

---

## 2026-06-02 — DS 클라이언트 시각 버그 4종 수정 (HP바·구조물 파괴·로그)

**작업 내용**
- #1 로그 스팸: StateTree task EnterState 로그(MoveToCurrentWaypoint/Target, SendAttackEvent) Warning→Verbose (Design B 재선택으로 매 틱 도배)
- #2 캐릭터 HP바 팀 색상: `Team` 을 `ReplicatedUsing=OnRep_Team` 으로 + `RefreshHealthBarTeamColor`(Team1=Red/Team2=Blue) 를 OnRep + Tick lazy 적용 (DS 클라가 팀을 알아야 색칠 가능)
- #3 구조물 파괴 시 클라 메시 잔존 + #4 HP바 드레인 깜빡임 — **단일 근본 원인 해결**
- HP바 위젯 일반 개선: World-space 양면 렌더(`SetTwoSided`), Masked 블렌드, 드레인부 불투명 배경

**문제점 / 난관 (디버깅 여정)**
- #3·#4 가 **클라에서만** 발생 (호스트 정상) → 네트워킹 문제로 좁힘
- 멀티캐스트·Health델리게이트·복제 bool(OnRep) 등 여러 숨김 경로를 넣어도 클라 메시가 안 사라짐 → 진단 로그로 실측
- 로그 분석 결과 **`MapManager::SpawnStructures` 가 클라에서도 실행** (권한 가드 없음) → 클라가 구조물을 **로컬 중복 스폰**. 클라엔 구조물 2벌(로컬 유령 + 서버 복제본)이 존재:
  - #3 = 서버가 못 건드리는 로컬 유령 메시가 남음
  - #4 = HP바 2개(로컬 풀 + 복제본 드레인)가 겹쳐 깜빡임
- 클라 중복 제거 후엔 복제본에 메시가 없음(`SetupTowerMesh` 는 서버 전용 `Initialize` 에서만 호출) → Team1(레드)만 안 보임
- 그 원인: UE 초기 복제는 **CDO 기본값과 다른 프로퍼티만 전송** → Team1/Tower(둘 다 기본값)는 `OwnerTeam`/`StructureType` 이 전송조차 안 됨 → OnRep(REPNOTIFY_Always 로도) 미발화

**해결 방법**
- `MapManager::SpawnStructures` 를 `if (HasAuthority())` 가드 → 서버만 스폰, 클라는 복제 수신 (유령·중복 제거 = #3·#4 동시 해결)
- 클라 복제 구조물 메시: `BeginPlay` 에서 `if (!HasAuthority()) SetupMeshForCurrentType()` 직접 호출 (이 시점엔 복제 프로퍼티 적용됨 + 기본값은 생성자 기본과 동일 → 전 구조물 정확). OnRep_StructureType/OwnerTeam 은 보정·색상용
- `bDestroyedVisual` 복제 bool + late-join 가드(파괴된 구조물 재표시 방지)

**결과**
- 클라에서 양 팀 타워·CC 정상 표시 + 파괴 시 즉시 사라짐 + HP바 깜빡임 없음 (PIE 리슨서버 확인)
- 보너스: 클라가 스폰하던 유령 구조물 ~20개 제거 → 클라 성능·정확성 개선
- 교훈: "클라에서만" 증상 = 권한 가드 누락/초기 복제 미전송 의심. 증상별 땜질보다 NetMode 별 실행 경로 추적이 빠름

---

## 2026-06-03 — 관전 카메라: 방향 정렬 + 줌 리네이밍/확대 + 라운드시작 자기CC 포커스

**작업 내용**
- **방향(요청1)**: `BP_AOSPlayerController` CDO `CameraYaw` 30 → **270**
  - 스폰포인트 월드좌표 분석: Team1(Red)=(−X,+Y) 코너, Team2(Blue)=(+X,−Y) 코너, 맵은 월드축 정렬
  - 다운뷰 화면 매핑식으로 Yaw=270 이 "정사각형 축정렬 + Red 좌하단 + Blue 우상단"을 동시 만족함을 도출
- **시작 높이/범위**: `CameraHeight` 2000 → 6000 (시작값이 줌 범위 안으로 → 첫 휠 점프 제거)
- **확대 확장(요청2)**: 줌 인 한계 4000 → **800** (BP CDO 즉시 반영)
- **줌 프로퍼티 리네이밍(요청2)**: `MinZoomHeight`→`MaxZoomInHeight`, `MaxZoomHeight`→`MaxZoomOutHeight` (C++)
  - "Min/Max" 가 줌 인/아웃 중 뭔지 헷갈린다는 피드백 → 이름에 ZoomIn/ZoomOut 명시 + 주석("값이 작을수록 더 확대")
  - `DefaultEngine.ini [CoreRedirects] +PropertyRedirects` 2줄로 BP CDO override 자동 마이그레이션 (리빌드 후)
- **라운드 시작 시 자기 진영 CC 포커스(요청3)**: `AAOSPlayerController::FocusCameraOnOwnCommandCenter()` 신규 → `OnGameStateChanged(RoundRunning)` 에서 매 라운드 호출
  - 클라엔 GameMode 없음 → 복제된 `AAOSStructure`(CommandCenter + 내 OwnerTeam) 직접 탐색, 팀은 `PlayerState->GetTeam()`
  - 공유 방향(270/−70)·줌(Z) 유지, 지면 교차점이 CC 가 되도록 카메라 XY 만 재배치 (NewXY = CC − (−Z/Fwd.Z)·Fwd.xy)

**문제점**
- 카메라가 비스듬히 돌아가 정사각형이 다이아몬드처럼 보임 + 팀 코너가 원하는 화면 위치와 불일치
- 시작 높이(2000)가 줌 최소높이(4000)보다 낮아 첫 휠 입력에 튕기는 점프
- 줌 한계가 Min/MaxZoomHeight 라 줌 인/아웃 방향이 직관적이지 않음

**해결 방법**
- 실제 실행값 소스가 C++ CDO 가 아니라 **BP_AOSPlayerController CDO override** 임을 `inspect_cdo` 로 확인 → BP 값은 Python(`set_editor_property`+`save_asset`)으로 즉시 수정
- Yaw 는 스폰포인트 좌표 + 다운뷰 매핑식(screen_x=−Px·sinθ+Py·cosθ, screen_y=Px·cosθ+Py·sinθ)으로 270° 계산
- 프로퍼티 rename 은 CoreRedirects 로 기존 BP override 보존 (풀 리빌드 필요)

**결과**
- 정사각형이 화면축에 정렬 + Red 좌하단/Blue 우상단, 첫 휠 점프 제거, 800까지 더 확대 가능
- 카메라는 PlayerController `BeginPlay` 1회 스폰 → BP CDO 변경은 **다음 PIE 부터** 반영(현 세션 재시작 필요). rename 은 **풀 리빌드** 후 반영
- 줌 한계 이름이 명확해져 디자이너가 줌 인/아웃 구분 가능
- 라운드(RoundRunning) 시작마다 각 클라이언트가 자기 CC 중앙 뷰로 시작 (공유 방향·줌은 보존)
- 참고: pitch −70 유지 → 원근 때문에 완전 평면이 아닌 약한 사다리꼴(완전 top-down 원하면 pitch −90). 카메라 (0,0) 고정이라 약간 하단 프레이밍 — 중앙정렬은 후속 옵션

---

## 2026-06-03 — 상점 재설계: 유닛 귀속 아이템 + 팝업 UI (유닛 선택 → 아이템 페이지)

**작업 내용**
- 상점을 "준비창 상시 임베드(라인별)" → **"상점 버튼 → 팝업"** 으로, 아이템 귀속을 **라인별 → 유닛별**로 전환
- 백엔드(AOSGameMode): `ItemInventory[2][3]`(라인) → `UnitItemInventory[2]`(`TMap<UnitId,아이템목록>`, 유닛 귀속·라운드 누적). `ServerBuyLaneItem`→`ServerBuyItemForUnit(Team,UnitId,Row)`, `GetUnitItems`, `ApplyUnitItemsToCharacter`. `DeployPlan` 에 UnitId(=로스터 인덱스) 병렬 저장 → 스폰 시 유닛 아이템 재적용
- `FAOSItemRow.RecommendedClasses` 추가 (추천 정렬용)
- PlayerController: `Server_SetLaneDeployClasses` 에 UnitIds 추가, `Server_BuyItemForUnit` RPC, 콘솔 `BuyItem(UnitId)`
- CharacterSelect: 슬롯이 RosterIndex 저장 + `GetLaneUnitIds`, **중복 배치 방지**(같은 유닛 2슬롯 불가), 슬롯 이동 시 유닛 정체성 유지, "상점 열기" 버튼 + 상점을 전체화면 팝업 오버레이로 추가, 준비 타이머를 팝업 헤더로 포워딩
- AOSShopWidget 전면 재작성: View1 유닛(최대 5) 선택 → View2 아이템 페이지(`RecommendedClasses` 매칭 시 ★ 추천 먼저) + 뒤로가기/닫기, 헤더에 골드+남은시간 상시 표시

**문제점**
- "특정 캐릭터에 아이템"인데 캐릭터는 매 라운드 리스폰 → 안정적 유닛 정체성 필요. 기존 배치는 "클래스를 슬롯에"(중복 가능)라 유닛 ID 부재
- 빌드 실패: `'Slot' 선언이 클래스 멤버를 숨김` — UE가 변수 섀도잉을 에러로 승격(`UWidget::Slot` 가림)

**해결 방법**
- 유닛 = 로스터 항목(UnitId = 로스터 인덱스). 드래그가 이미 RosterIndex 를 들고 있어 슬롯에 저장 → 배치 RPC/스폰에 전달. 중복 배치 금지로 유닛 distinct 보장. 아이템은 (Team,UnitId)에 귀속되어 배치를 바꿔도 유닛을 따라감
- 클라는 GameMode 없음 → 카탈로그(DT_Items) 직접 로드 + 구매만 서버 RPC. 유닛 목록은 CharacterSelect 배치 슬롯에서 구성
- 섀도잉: 지역변수 `Slot`→`LaneSlot`

**결과**
- 준비창 "상점 열기" → 팝업 → 유닛 선택 → 추천 먼저 정렬 아이템 구매 → 뒤로/닫기, 남은시간 상시 표시. 산 아이템이 유닛에 누적돼 다음 라운드 강화 (빌드+PIE 확인 완료)
- 신규 UI 클래스: `UAOSShopWidget` / `UAOSShopUnitButton` / `UAOSShopItemButton`
- 후속: DT_Items 의 `RecommendedClasses` 데이터 입력(agent-design-balance), 쿡 빌드용 카탈로그 하드참조

---

## 2026-06-04 — 콘텐츠 확장: 캐릭터 5종화(캐미·가일) + 아이템 5종화 + 추천 데이터

**작업 내용**
- **추천 데이터**: DT_Items 각 행에 `RecommendedClasses` 입력 → 상점 ★ 추천 정렬 활성화
- **캐릭터 3→5**: `BP_Char_Cammy`(캐미)/`BP_Char_Guile`(가일) 추가 (BP_Char_Ken 복제 = 기본 공격형, Mannequin). `DT_CharacterAttributes` 행 추가(캐미: 러시다운 HP120/AS1.6/MS700, 가일: 존잉 HP180/Range600). `TDProj_GM` 로스터 등록
- **아이템 2→5**: 신속의 신발(이속+100)/재빠른 단검(공속+0.3)/오래된 포신(사거리+150) — `BP_GE_Item_Sword` 복제 후 Modifier 속성·크기 치환(Infinite + AddBase)
- 추천 매핑: 공격형(롱소드/단검)→켄·캐미, 내구(물약)→베가·가일, 이속(신발)→캐미, 사거리(포신)→가일·베가, 알렉스(브루저)→공격+체력 양쪽

**문제점**
- DataTable 행 / 구조체 배열을 Python 으로 편집하기 어려움 — `set_editor_property` 가 `EditDefaultsOnly` 구조체 인스턴스에서 "cannot be edited on instances" 로 막힘
- **BP CDO 편집 후 `save_asset` 기본값(`only_if_is_dirty=True`)이 저장을 스킵** → `TDProj_GM` 로스터가 디스크에 안 써짐(메모리만 5종 → 재시작 시 유실 위험)

**해결 방법**
- DataTable: `export_data_table_to_json_string` ↔ `fill_data_table_from_json_string` JSON 라운드트립 (기존 필드 보존하며 행 추가/수정)
- 구조체(`FCharacterRosterEntry`, `FGameplayModifierInfo`): `import_text` 직렬화 경로로 EditDefaultsOnly 우회
- 저장: BP CDO/구조체 편집 후 `save_asset(path, only_if_is_dirty=False)` 강제 저장 필수

**결과**
- 캐릭터 5종(알렉스/베가/켄/캐미/가일) + 아이템 5종(롱소드/물약/신발/단검/포신) + 캐릭터별 ★ 추천. 전부 에셋 작업이라 리빌드 불필요, PIE 확인 완료
- 신규 에셋: `BP_Char_Cammy/Guile`, `BP_GE_Item_Boots/Dagger/Cannon`. 수정: `DT_Items`, `DT_CharacterAttributes`, `TDProj_GM`
- 후속: 캐미/가일 전용 외형·팀색(아트), 전용 스킬(SKILL_AUTHORING_GUIDE), GAME_VISION 로스터 문서 갱신

---

## 2026-06-04 — MCP 자동화 브리지 0.6.0 → 0.1.4 교체 (네이티브 :3000 → WebSocket bridge)

**작업 내용**
- `McpAutomationBridge` 플러그인을 커스텀 0.6.0(네이티브 `:3000` HTTP transport)에서 공식 0.1.4(ChiR24/Unreal_mcp, WebSocket `:8091` bridge)로 교체
- 0.6.0 네이티브 레이어(`MCP/McpToolRegistry`·`McpNativeTransport`·`McpJsonRpc`·`Tools/McpTool_*`)와 상태바 위젯(`SMcpStatusBarWidget`) 제거(49개 삭제), 핸들러/Build.cs 0.1.4 원본 복원(17개)
- 연결 설정 전환: `.mcp.json`·`claude_desktop_config.json`(전역, git 미추적)·`example` — `type:url :3000/mcp` → `npx unreal-engine-mcp-server`(stdio, `UE_PROJECT_PATH` + `MCP_AUTOMATION_PORT=8091`)

**문제점**
- 다른 도구로 교체 중 토큰 소진으로 중단 → 파일 교체는 됐으나 연결 설정/문서가 구버전(`:3000`) 잔존
- 버전 혼동: 레포/npm 릴리스는 0.5.21인데 플러그인 모듈(uplugin)은 0.1.4 — 별개 체계
- 에디터 좌하단 "MCP 연결" 표시가 사라져 연결 끊김으로 오인

**해결 방법**
- 정합성 검증(0.6.0 심볼·삭제파일 참조·잘린 파일 0 확인) 후 연결 설정 4곳 + 문서(`CLAUDE.md`·`Mcp_Tools/README.md`)를 일괄 0.1.4 방식으로 갱신
- 좌하단 표시는 0.6.0 전용 위젯이라 0.1.4 미표시가 정상 — `inspect get_viewport_info` 호출로 실연결 확인(770×742 응답)

**결과**
- MCP 연결 정상(에디터 ↔ WebSocket `:8091` ↔ npx 중계). 플러그인 `Binaries` 없음 → 풀 리빌드는 사용자 진행 예정
- 커밋: `3882984` (플러그인 교체 + 설정 + 문서). 게임코드(`AOS*`)·발표자료는 분리 제외
- 관련 가이드: `Mcp_Tools/README.md`

---

## 2026-06-04 — Slice 1(가독성): 라운드 결과 요약 (라인 승패 판정 + 준비화면 패널)

**작업 내용**
- 비전의 '빈칸'이던 **라인 승패 판정** 구현. `AOSGameMode`: `LaneAlive[2][3]` 추적 — `InitLaneTracking`(스폰) → `RecordLaneDeath`(`OnCharacterDestroyed`) → `CheckLaneDecided`. 한 팀의 라인 생존이 0 되는 순간 확정(먼저 0=패, 상대=승, 그 시점 승자 잔존수=마진). 한 팀이 0명 배치한 라인은 즉시 상대 승. `EndRound` 에서 `PushRoundResultToGameState`
- **복제**: `AOSGameState` 신규 USTRUCT `FAOSRoundResult`(라인별 승자[3]·승자잔존[3]·라운드번호·bValid) + `LastRoundResult`(`ReplicatedUsing=OnRep_RoundResult`) + `OnRoundResultChanged` 델리게이트 + `ServerSetRoundResult`
- **UI**: `AOSCharacterSelectWidget` 준비화면 타이틀 아래 "지난 라운드 결과" 패널 — 로컬 팀 관점 라인별 승/패 + 생존 마진, 다수승=녹/1=노랑/0=빨강, 첫 라운드 숨김. `InitializeWithRoster` 에서 GameState 읽어 갱신

**문제점 / 설계 판단**
- 라운드는 "양 팀 캐릭터 전멸 시 종료"라 종료 시점엔 전원 사망 → 라인 승패는 "어느 팀이 그 라인에서 **먼저** 전멸했나"로 판정 (배치 라인 기준, 싸운 위치 무관 — 비전 정의 그대로)
- 표시 위치: 별도 화면 대신 **준비 화면 패널**(다음 라운드 재배치 직전 노출) — 비전의 "결과 보고 → 적응" 루프에 부합

**해결 방법**
- per-(팀,라인) 생존 카운트 + 선점(먼저 0) 확정 로직으로 "전멸 순서" 포착. GameState 복제 + 로컬 팀 관점 변환은 UI 에서 처리

**결과**
- 매 라운드 종료 후 다음 준비 화면에 라인별 승패+마진 표시 (관전 인과 가독성 = 디자인 기둥 2). 빌드+PIE 확인 완료
- `LaneWinner` 데이터는 비전의 **조건부 재배치(2라인↑ 패배 시만)** 토대로 재사용 예정
- 후속 Slice 1: 데미지 숫자, 미니맵

---

## 2026-06-04 — Slice 1 가독성: 플로팅 데미지 숫자

**작업 내용**
- 피격 시 머리 위로 떠오르며 페이드아웃하는 데미지 숫자 (캐릭터·타워·CC·스킬 데미지 전부 자동)
- 신규 `AOS/UI/AOSDamageNumberWidget` — C++ 빌드 위젯(BP 자산 불필요). 월드 한 점에서 상승 + 페이드, ~1.1s 후 자동 제거
- **단일 훅**: `AOSAttributeSet::PostGameplayEffectExecute` 데미지 차감 직후 victim 액터로 `Multicast_ShowDamageNumber(float)` 방송 → 모든 데미지 경로가 `GE_Damage` 를 지나므로 한 곳만 후킹하면 전부 커버
- `AOSCharacter`/`AOSStructure` 에 멀티캐스트 추가 (HitReact RPC 패턴, 단 **Unreliable**=비주얼이라 유실 허용). 피격 측 팀 색상 틴트(Team1=레드/Team2=블루), 큰 피해일수록 폰트 ↑

**문제점 / 난관**
- 순수 C++ `UUserWidget` 은 `TickFrequency=Auto` 에서 BP Tick/애니메이션이 없으면 `NativeTick` 이 호출되지 않음 → 떠오름/페이드 미동작 함정
- DS 에는 렌더 파이프라인/뷰포트 없음 → 위젯 생성 금지 필요

**해결 방법**
- 떠오름/페이드를 **월드 타이머(1/60s)** 로 구동 (프레임률 독립, Auto 틱 게이팅 회피). `NativeDestruct`·수명 만료 시 타이머 정리
- `SpawnDamageNumber` 가 `NM_DedicatedServer` 스킵 + 로컬 PC 가드 → 클라/리슨호스트에만 표시
- 화면 투영은 `UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition`(DPI 보정 뷰포트 로컬), UMG 트리(RootWidget) 빌드 후 `AddToViewport` 순서 준수

**결과**
- 전투 피해가 한눈에 보임 (관전 가독성 = 디자인 기둥 2). 풀 리빌드 + PIE 확인 완료
- 표시 데미지 = 방어막(W) 감산 후 **실제 깎인 HP**. 동시 타격多 시 타격당 위젯 1개 생성(추후 풀링 여지)
- 후속 Slice 1: 미니맵

---

## 2026-06-04 — Slice 1 가독성: 미니맵 (카메라 정렬 + 뷰박스 + 클릭 이동)

**작업 내용**
- 화면 우하단 고정 미니맵 (렌더 방식 ②: 정적 배경 슬롯 + 아이콘 오버레이, C++ 위젯이라 BP 자산 불필요)
- 신규 `AOS/UI/AOSMinimapWidget` — 구조물(타워/CC, 파괴 전까지) + 생존 캐릭터를 팀 색상 점으로, 월드 경계는 배치 구조물 위치에서 자동 산출(클라가 복제 액터로 직접). 갱신 20Hz 월드 타이머
- **방향 일치**: 카메라 yaw 로 회전 정렬 — 카메라가 쓰는 축(screen-right=(−sinθ,cosθ)/screen-up=(cosθ,sinθ))과 동일하게 매핑 → 화면 뷰(레드 좌하단/블루 우상단)와 방향 자동 일치. `ExtraYawDeg` 미세조정 노브
- **카메라 뷰 박스**: 뷰포트 4모서리를 지면(z=0) 역투영 → 미니맵 좌표 매핑 → 흰 사각형 테두리(가시 영역의 축정렬 바운딩 근사)
- **클릭/드래그/터치 이동**: 미니맵 좌표 → 월드 역변환 → `MoveCameraToGroundPoint` 로 카메라 재중심. 마우스 드래그 스크럽 + 모바일 터치 지원
- `AOSPlayerController`: 재중심 계산을 `MoveCameraToGroundPoint(GroundLocation)` 로 추출(FocusCameraOnOwnCommandCenter 와 공유) + `GetCameraYaw()` 추가. `OnGameStateChanged` 에서 RoundRunning 만 표시

**문제점 / 난관**
- 순수 C++ `UUserWidget` Auto 틱 게이팅 → 갱신은 월드 타이머로 구동
- 미니맵이 화면 전체를 막으면 게임 클릭(유닛 선택) 차단 → 루트/콘텐츠는 hit-test 통과, 프레임만 클릭 캐치
- 초기 버전은 고정 flip 으로 방향이 카메라 뷰와 불일치

**해결 방법**
- 카메라 yaw 회전으로 화면축과 동일 정렬(고정 flip 제거) → 어떤 yaw 에도 자동 일치
- `SelfHitTestInvisible`(루트/캔버스) + `HitTestInvisible`(콘텐츠) + `Visible`(프레임) 조합 → 미니맵 밖 클릭은 게임으로 통과
- 클릭 좌표 정합 위해 프레임 패딩 0 → 아이콘 캔버스 로컬 = MinimapSize

**결과**
- 라운드 전황을 한눈에 파악 + 미니맵으로 카메라 즉시 이동 (관전 가독성 = 디자인 기둥 2). 풀 리빌드 + PIE 확인 완료
- Slice 1(가독성) 3종(라운드 결과 요약 / 데미지 숫자 / 미니맵) 완료
- 배경 정적 이미지는 `BackgroundTexture` 슬롯으로 후속 아트 추가 가능

---

## 2026-06-04 — AI 스폰 직후 아군 오사(friendly fire) 수정 (StartLogic 타이밍 race)

**작업 내용**
- `AAOSAIController`: `StateTreeComponent->StartLogic()` 호출 위치를 **`OnPossess` → `StartDeployment()`** 로 이동
- 원인 확정용 임시 진단 로그(데미지 단일 훅에서 출처 vs victim 팀)는 확인 후 제거

**문제점**
- 게임 시작 직후 **자기 진영 근처에서 같은 팀 캐릭터끼리 기본공격**(데미지=공격자 AP). 두번째로 스폰되는 **Team2 에서만** 발생
- 원인: `OnPossess` 는 SpawnActor 중 auto-possess 로 `SetTeam` 보다 **먼저** 실행 → 여기서 StartLogic 하면 StateTree 가 `Team=기본값(Team1)` 으로 첫 평가 → 이미 Team2 로 설정된 동료를 적으로 오인·타겟 락
- (Team1 은 먼저 스폰돼 기본값==실제값이라 무사 — 이 **비대칭**이 진단 단서)

**해결 방법**
- 막 추가한 플로팅 데미지 숫자 덕에 "보이지 않던" 오사가 표면화 → `AOSAttributeSet::PostGameplayEffectExecute` 에서 `Data.EffectSpec.GetContext().GetSourceObject()` 팀 vs victim 팀 로그로 friendly-fire 100% 확정 (4건 모두 Team2↔Team2, 스폰 근처)
- 근본 수정: StartLogic 을 팀·라인·웨이포인트가 모두 확정된 `StartDeployment()` 로 이동 (possess 이후라 schema context actor binding 도 정상)

**결과**
- 스폰 직후 아군 오사 완전 제거, 중앙 교전은 정상 유지. 핫 리로드 + PIE 확인 완료
- 영향: `Source/TDProject/AOS/AOSAIController.cpp` (OnPossess/StartDeployment)
- 문서: `CLAUDE.md` Phase 6 핵심클래스 설명 + 트러블슈팅 **#5** 신규

---

## 2026-06-06 — AI 에셋 생성 파이프라인 (ComfyUI ControlNet) + 미니맵 SF 테두리

**작업 내용**
- **로컬 무료 생성 파이프라인** 구축: ComfyUI(`:8188`) + SDXL 계열 체크포인트(DreamShaper XL Lightning / Illustrious XL / Animagine XL) + scribble ControlNet. GTX 3080 에서 장당 수 초~십수 초.
- **스크립트** (`Mcp_Tools/Asset_Pipeline/`): `gen_icon.py`(text2img), `gen_img2img.py`, `gen_controlnet.py`(스케치→완성), `make_minimap_frame.py`(절차 프레임). 임포트는 기존 `import_ui_assets.py` / `manage_asset` MCP.
- **미니맵 테두리**: `UAOSMinimapWidget` 에 `FrameTexture`/`FrameImage` 오버레이(`/Game/AOS/UI/Assets/T_MinimapFrame` 자동 로드) + **content inset**(`WorldToLocal`/`LocalToWorldGround` 10%) + 절차생성 청록 베젤 텍스처(muted + 그라데이션).

**문제점**
- text2img/img2img 로는 "정확한 구도 + 솔리드 렌더" 동시 불가 (img2img: 구도 잠금↔채움 denoise 딜레마, 흰배경/외곽선 잔존).
- MCP `manage_asset import` 의 `save:true` 가 신규 텍스처를 디스크에 flush 안 함 → **에디터에서 수동 Save All 필요** (커밋 전 필수).

**해결 방법**
- **정석 워크플로 확정**: 거친 손스케치 → (정사각 패딩 + 색반전) → ControlNet(denoise 1.0, 선=구조 강제) → 완성. 모든 스킬/UI 에셋 재사용 → `SKILL_ICON_RECIPE.md`.
- 미니맵 프레임은 장식 변수 큰 AI 대신 **PIL 절차생성**(두께/색/그라데이션 정확 제어).
- inset 으로 아이콘/뷰박스를 프레임 안쪽으로 → 클릭 역변환도 동일 inset 유지(좌표 정합).

**결과**
- **생성 → 임포트 → C++ UI 적용 → 인게임** 전 과정을 실제 에디터에서 검증 (미니맵에 SF 청록 베젤 테두리 적용 완료).
- 영향: `Source/TDProject/AOS/UI/AOSMinimapWidget.h/.cpp`, `Content/AOS/UI/Assets/T_MinimapFrame`, `Mcp_Tools/Asset_Pipeline/*`
- Alex 스킬 아이콘(Q/W/E/R)도 동일 워크플로로 생성했으나 최종 픽/임포트는 보류(후속).
- 관련 가이드: `Mcp_Tools/Asset_Pipeline/README.md`, `SKILL_ICON_RECIPE.md`

---

## 2026-06-07 — 벤픽(Ban/Pick) 드래프트 단계 추가

**작업 내용**
- 매칭(Lobby)과 1라운드 사이에 MOBA 식 **밴/픽 드래프트 단계** 신설. 픽된 캐릭터만 이후 모든 라운드 준비에서 배치 가능
- `EAOSGameState::BanPick` 을 enum **끝에 append**(값 시프트 방지). Lobby 양팀 ready → `TransitionToRoundPreparation` 대신 `TransitionToBanPick`
- 서버 고정 **14스텝 시퀀스**(`FAOSDraftStep{Team, bBan}`): 밴4(T1·T2·T1·T2) + 픽10 스네이크(T1·T2·T2·T1·T1·T2·T2·T1·T1·T2) = 팀당 밴2·픽5. **전체 고유**(밴/픽 즉시 풀에서 제거)
- `AOSGameMode`: `TransitionToBanPick` / `ServerApplyDraftSelection`(턴·가용 검증→기록→step++→완료 시 RoundPreparation) / 턴 타이머(`DraftTurnDuration` 30s, 만료 시 랜덤 자동선택) / **배치 필터**(`ServerSetLaneDeployClassesForPlayer` 가 픽 안 된 UnitId 거부 = 서버 강제)
- `AOSGameState`: 6개 Replicated(Team1/2 Picked·Banned UnitIds, CurrentDraftStep, DraftTurnTimeRemaining — 모두 `OnRep_Draft`) + ServerSet/Record + getter 8종 + `FOnDraftChanged` 델리게이트
- `AOSPlayerController`: `OnGameStateChanged` BanPick 케이스, `Server_DraftSelect` RPC(서버가 PlayerState 팀 강제), 로스터 RPC 가 벤픽 위젯에도 주입
- 신규 **`UAOSBanPickWidget`**(순수 C++): 카드 그리드 + 밴/픽 슬롯 + 턴/타이머, 지오메트리 hit-test 클릭, `OnDraftChanged` 구독 갱신
- `AOSCharacterSelectWidget`: 준비화면 **픽 필터**(`GetPickedUnits` 에 없으면 카드 숨김, 픽 비면 전체 표시=하위호환)
- 로스터 **5→20종 확장**(`BP_Char_Unit6..20`, BP_Char_Ken 복제 기본형) — 벤픽이 의미를 가지려면 ≥14(밴4+픽10) 필요

**문제점 / 난관**
- enum 중간 삽입 시 기존 직렬화/BP 데이터 값 시프트 위험 → 끝에 append 로 회피
- **벤픽 화면이 안 뜸** — `ShowBanPick` 이 `InitializeWithRoster`(WidgetTree 구축)보다 `AddToViewport` 를 **먼저** 호출 → 빈 RootWidget 이 Slate 로 실현 (미니맵에서 겪은 위젯 실현 순서 함정 재발)

**해결 방법**
- `ShowBanPick` 의 호출 순서를 `InitializeWithRoster` → `AddToViewport` 로 교정 (cpp-only, Live Coding 가능)
- DS 규칙 준수: 드래프트 상태 변경은 `HasAuthority()` 서버, 위젯/입력은 `IsLocalPlayerController()`, 클라는 `OnRep_Draft` 리플리케이션으로만 수신

**결과 / 영향**
- 2-client DS PIE **기본 흐름 확인**: Lobby→BanPick(양 클라 표시)→14스텝 교대 드래프트(전체 고유)→RoundPreparation 픽 필터→라운드 진행. 세세한 수정은 후속 예정
- 영향: `AOSGameMode.h/.cpp`, `AOSGameState.h/.cpp`, `AOSPlayerController.h/.cpp`, `UI/AOSBanPickWidget.h/.cpp`(신규), `UI/AOSCharacterSelectWidget.cpp`, `Content/TDProj_GM`, `Content/Characters/BP_Char_Unit6..20`
- 신규 enum/USTRUCT(`FAOSDraftStep`)/UCLASS(`UAOSBanPickWidget`) → **풀 리빌드 필요**
- 후속: 플레이스홀더 15종 고유 스탯/스킬/초상화 차별화, 벤픽 UI 아트 스타일링
- 관련: `CLAUDE.md` "Ban/Pick Draft System (벤픽 드래프트)" 섹션

---

## 2026-06-07 — 벤픽 UI를 LoL 챔피언 선택 스타일로 리디자인 + 리소스 생성

**작업 내용**
- `UAOSBanPickWidget` 전면 재구성(순수 C++): 상단바(플레이어명+제목+타이머) + 좌우 팀 패널(밴 슬롯 + 픽 슬롯 5) + 중앙 챔피언 그리드 + 확정 버튼
- **2단계 선택**(LoL식): 카드 클릭=미리보기(골드 테두리) → **확정 버튼**에서만 서버 RPC 전송. 서버 드래프트 로직 무변경(클라 `PendingIndex` 상태만 추가)
- 요청 반영: 플레이어 이름 팀당 1개 상단 표시(캐릭터엔 이름만), 소환사주문·룬·스킨·역할탭 제외
- **리소스 생성**(`agent-asset-gen`, ComfyUI+PIL): 절차 다크 백드롭 `T_BanPick_Backdrop` + 5종 AI 초상화 `T_Portrait_{Alex,Vega,Ken,Cammy,Guile}` → 로스터 0~4 할당, 5~19 Portrait 비워 **인덱스별 컬러 타일**
- 겸사겸사 `GA_Attack` 의 deprecated `AbilityTags` → `GetAssetTags()/SetAssetTags()` 마이그레이션(GA_SkillBase 패턴)

**문제점 / 난관**
- `'Slot' 선언이 클래스 멤버(UWidget::Slot) 숨김` 에러 — 지역변수명 충돌(상점 때와 동일)
- 카드 클릭 시 **1초 뒤 미리보기 풀림** — 턴 타이머가 매 초 `OnDraftChanged` 브로드캐스트 → 위젯이 무조건 `PendingIndex` 초기화
- 초상화 **없는 카드가 얇게 붕괴**해 클릭 불가 — `SetDesiredSizeOverride` 가 컬러 브러시에 불안정
- 로스터 Portrait 스크립트 2연속 에러: (1) `load_object` 가 클래스 반환 → `get_editor_property` 실패(CDO 필요), (2) `load_object` 첫 인자(outer)에 클래스 전달
- MCP 가 `TArray<구조체>` 편집·`execute_python` 미지원 → 로스터 할당은 에디터 Python 수동 실행. 또 전 로스터 Portrait 가 Mannequin 기본 `T_UE_Logo_M`(빨간 "U")로 차 있었음

**해결 방법**
- 지역변수 `Slot`→`PickSlot` rename
- `OnDraftChanged`: **미리보기 유닛이 가용하지 않을 때만** `PendingIndex` 초기화(타이머 갱신엔 무영향)
- 그리드 카드를 **`USizeBox(72×72)`** 로 감싸 초상화 유무 무관 동일 크기/클릭영역
- 스크립트: `get_default_object(클래스)`로 CDO 획득 + `load_object(None, 경로)` + EditDefaultsOnly 막히면 부분 `import_text` 폴백. `update_roster_portraits.py` 를 에디터에서 1회 실행

**결과 / 영향**
- LoL 스타일 벤픽 화면 + 2단계 선택 + 초상화/백드롭/컬러타일 전부 동작(2-client DS PIE 확인)
- 위젯 레이아웃/사이징 수정은 cpp-only → **Live Coding 으로 반복**(초기 UPROPERTY/UFUNCTION 추가만 풀 리빌드)
- 영향: `UI/AOSBanPickWidget.h/.cpp`, `GA_Attack.cpp`, `Content/AOS/UI/Assets/T_BanPick_Backdrop`+`T_Portrait_*`(신규 6), `Content/TDProj_GM`(로스터 Portrait), `Mcp_Tools/Asset_Pipeline/{make_banpick_backdrop,update_roster_portraits}.py`(신규)
- 후속: 백드롭/초상화 아트 고도화, 플레이스홀더 15종 고유화
- 관련: `CLAUDE.md` "Ban/Pick Draft System" 섹션

---

## 2026-06-07 — 벤픽 UI 퀄리티 업 (프레임·팀패널·픽슬롯 채움·백드롭 깊이감)

**작업 내용**
- "너무 심플"하던 LoL 벤픽 화면을 프리미엄 톤으로 — 전부 **위젯 C++ 폴리시(새 멤버 없음 → Live Coding 반복)**
- **카드 프레임**: 장식 프레임 텍스처(9-slice 자동로드) / 없으면 금속 컬러 림 폴백 + 초상화 인셋, 상태별 틴트(기본 금속 / 미리보기 골드 / 픽 팀색 / 밴 적색)
- **팀 패널**: 좌우 픽 컬럼을 반투명 팀색 패널로 감싸 깊이감 + 전체 높이로 확장
- **픽 슬롯 재구성**: 작은 사각형 떠보임 → `Overlay`(초상화 슬롯 전체 채움 + 하단 이름 외곽선) + 폭 고정 `SizeBox` + 5칸 균등 분배(LoL 픽 슬롯)
- **구도**: 카드/슬롯 확대 + 그리드 `Spacer` 수직 중앙 정렬 → 하단 빈 공간 제거
- 상단 골드 구분선, 제목/타이머 외곽선, 타이머 5초 빨강 긴박감, 확정 버튼 팀색
- 백드롭은 깊이감 있는 다크 톤(asset-gen). **카드 9-slice 골드 프레임**(`T_BanPick_CardFrame`) 적용 — 처음 256px·25% 테두리는 얇게 렌더 시 코너가 뭉개져서, **64px·12.5% 테두리 PIL 절차생성**으로 교체(다운스케일 2:1 → 얇아도 또렷). 위젯 `Margin=0.125, ImageSize=32 → ~4px`. 두께는 텍스처 재생성 없이 `ImageSize` 숫자만으로 조정

**문제점 / 난관**
- 텍스처 없는 **컬러 브러시 위젯이 크기/폭 없이 붕괴**(카드·픽슬롯이 얇아짐) → `USizeBox` 로 크기 강제
- "심플"의 정체 = 평면 배경 + 프레임 부재 + 작은 요소 + 빈 하단 → 프레임·패널·구도로 해소
- 슬롯을 Fill 분배하니 빈 칸에 작은 사각형이 떠보임 → Overlay 전체 채움으로 전환

**해결 방법**
- **단계적 스크린샷 피드백 루프**(사용자 캡처 → 즉시 미세조정), 전부 cpp-only 라 Live Coding 으로 빠른 반복
- 레이아웃 안정화: `SizeBox`(크기 강제) + `Overlay`(채움) + `Spacer`(수직 중앙) 조합

**결과 / 영향**
- 평면 → 입체 프리미엄 드래프트 화면 (2-client DS PIE 단계별 확인)
- 영향: `UI/AOSBanPickWidget.cpp`(폴리시 전반), `Content/AOS/UI/Assets/T_BanPick_CardFrame`(신규 프레임), `Mcp_Tools/Asset_Pipeline/{make_banpick_backdrop_v2,make_banpick_cardframe}.py`
- 후속: 15 플레이스홀더 실제 초상화, 카드 호버/모션
**진행 스크린샷** (`images/2026-06-07_banpick_ui/`)

**① 초기 LoL 레이아웃** — 카드가 Mannequin 기본 텍스처(빨간 "U")
![초기 LoL 레이아웃](images/2026-06-07_banpick_ui/01_lol_layout.png)

**② 5종 AI 초상화 + 15 컬러 타일 적용**
![초상화 적용](images/2026-06-07_banpick_ui/02_portraits.png)

**③ 퀄리티 업 직전 (before)** — 평면 배경·프레임 없음 = "너무 심플"
![before](images/2026-06-07_banpick_ui/03_simple.png)

**④ 프레임·팀 패널·백드롭·구분선 적용**
![프레임/패널](images/2026-06-07_banpick_ui/04_polished.png)

**⑤ 픽 슬롯 채움 + 구도 정리 (after, 최종)**
![after](images/2026-06-07_banpick_ui/05_final.png)

---

## 2026-06-07 — 벤픽 UI LoL 완성도 Tier 1 (빈 슬롯 채움 + 차례 글로우 + 중앙 패널)

**작업 내용**
- LoL 챔피언 선택 원본과 비교해 "완성도" 격차를 메우는 1차 패스 (전부 위젯 C++, 새 멤버 없음 → Live Coding)
- **빈 픽 슬롯 채움**: 픽 전에도 `픽 1`~`픽 5` 라벨, 현재 차례 슬롯엔 "픽 중..."
- **현재 차례 글로우**: 활성 팀의 *다음* 픽/밴 슬롯에 골드 하이라이트(테두리+이미지) → 누가/어디를 고르는지 한눈에. 채워진 픽=팀색 진하게
- **중앙 챔피언 풀 패널**: 그리드를 얇은 골드 림 + 어두운 반투명 패널로 감싸 그룹핑 (LoL 중앙 프레임 느낌)

**문제점 / 난관**
- 빈 슬롯이 회색 사각형이라 "비어보임", 차례가 텍스트로만 표시돼 생동감 부족 (LoL은 슬롯에 정보 가득 + 차례 글로우)

**해결 방법**
- `RefreshSlots` 에서 활성 팀+밴/픽 단계의 *다음 슬롯 인덱스* 계산 → 그 슬롯만 골드 글로우 + "픽 중..." 라벨. 빈 슬롯은 슬롯번호 라벨로 채움
- 그리드를 `Border`(골드 림) + `Border`(다크 반투명) 2겹으로 감싸 중앙 패널화

**결과 / 영향**
- "누가·어디·무엇" 가독성 ↑ — 빈 공간 라벨로 채움, 차례 글로우로 생동감. 2-client DS PIE 로 밴→픽 흐름 단계별 확인
- 영향: `UI/AOSBanPickWidget.cpp` (RefreshSlots 활성슬롯 글로우, BuildUI 중앙 패널). cpp-only = Live Coding
- 후속(Tier 2 옵션): 카드 라운드코너+호버 글로우, 진영 글로우, 배경 비네팅, 글로우 펄스 애니
**진행 스크린샷**

**① 초기(밴 차례)** — 빈 픽 슬롯 `픽 1`~`픽 5` 라벨 + 활성 밴 슬롯 골드 글로우
![밴 초기](images/2026-06-07_banpick_ui/07_slots_labeled.png)

**② 팀1 픽 차례** — 활성 슬롯 골드 글로우 + "픽 중...", 밴된 챔피언은 그리드에서 어둡게
![픽 글로우](images/2026-06-07_banpick_ui/08_active_glow.png)

**③ 알렉스 픽 완료 + 팀2 차례** — 채워진 슬롯 초상화+이름, 다음 차례 슬롯 글로우
![픽 완료](images/2026-06-07_banpick_ui/09_pick_filled.png)

---

## 2026-06-07 — 벤픽 UI LoL 완성도 Tier 2 (진영 글로우 + 중앙 골드 비네팅)

**작업 내용**
- 좌우 팀 패널 **진영 외곽광** (팀1 레드 / 팀2 블루 5px 림) + 패널 바탕색 진하게 → 진영 구분 강화
- **중앙 골드 비네팅 글로우** (`T_BanPick_CenterGlow`, PIL 절차생성 256px 방사형) → 챔피언 풀 뒤를 따뜻하게 비춤
- 그리드 패널 어두운 바탕을 거의 투명(α 0.6→0.18)으로 풀어 글로우가 비치게

**문제점 / 난관**
- 1차(얇은 림 + 약한 글로우)는 너무 미묘 → "뭐가 바뀐지 모르겠다" 피드백 2회 (효과 안 읽힘)
- MCP `manage_asset import` 가 신규 텍스처 .uasset 을 디스크에 즉시 flush 안 함 (재임포트 후 반영)

**해결 방법**
- **과감하게 강화**: 글로우 peak α 0.42→0.62 + 따뜻한 골드, 그리드 가림막 거의 제거, 글로우 영역 확대(1120×800), 팀 패널 채도↑
- 텍스처 재임포트로 디스크 flush 확인 후 커밋

**결과 / 영향**
- 중앙이 골드로 환하게 + 좌우 진영색 뚜렷 → 분위기 확연. cpp-only(글로우 텍스처 자동로드) = Live Coding
- 영향: `UI/AOSBanPickWidget.cpp`, `Content/AOS/UI/Assets/T_BanPick_CenterGlow`(신규), `Mcp_Tools/Asset_Pipeline/make_banpick_centerglow.py`(신규)
- 후속: 호버 글로우 + 펄스 애니(타이머 멤버 → 풀 리빌드), 15 플레이스홀더 실제 초상화

**진행 스크린샷** — 진영 글로우(좌 레드 / 우 블루 패널) + 중앙 패널
![Tier2 진영 글로우](images/2026-06-07_banpick_ui/11.png)

---

## 2026-06-07 — 벤픽 UI 화려함: 드라마틱 AI 배경 (ComfyUI 인라인)

**작업 내용**
- "화려하게" 요청 → 처음엔 **PIL 기하 장식**(god rays / 골드 아치 / 헤더 배너) 시도했으나 템플릿 느낌·크기 안 맞아 **전부 제거**
- 핵심 전환: **AI 배경 한 장**이 답 — ComfyUI(DreamShaper XL Lightning)로 **웅장한 고딕 홀 + 빛기둥 + 횃불** 배경 생성, PIL 후처리(어둡게+비네팅)로 UI 가독성 확보 → `T_BanPick_Backdrop` 교체
- `gen_banpick_backdrop.py` 신규 (ComfyUI HTTP API 인라인 호출 → 16:9 생성 → PIL 후처리)

**문제점 / 난관**
- `agent-asset-gen` 서브에이전트가 **사용량 한도로 반복 취소** (프레임 때도, 이번 장식 4종 때도)
- PIL 기하 장식은 "화려"보다 "템플릿" — 아치가 그리드보다 커서 빈 박스, 배너 크기 안 맞음

**해결 방법**
- **ComfyUI를 인라인 파이썬으로 직접 구동**(`gen_banpick_backdrop.py`) → 에이전트 사용량 제한 우회. 체크포인트 자동감지 + Lightning/일반 SDXL 자동 파라미터
- 어색한 PIL 장식은 과감히 제거(코드 add→remove 넷 제로), 화려함은 배경에 집중

**결과 / 영향**
- 평면 다크 배경 → **드라마틱 고딕 홀** 한 장으로 화면 전체가 화려해짐 (깔끔 + 웅장)
- 영향: `Content/AOS/UI/Assets/T_BanPick_Backdrop`(교체), `Mcp_Tools/Asset_Pipeline/gen_banpick_backdrop.py`(신규)
- 교훈: **"화려함" = 기하 PIL 장식 N개 < AI 배경 1장**. 서브에이전트 사용량 한도엔 **인라인 ComfyUI**가 대안
- 미사용 장식 시도분(T_BanPick_Rays/Arc/Banner + make_banpick_* 스크립트)은 커밋 제외(추후 정리)

**진행 스크린샷**
![배경 적용](images/2026-06-07_banpick_ui/12.png)
![화려함 최종](images/2026-06-07_banpick_ui/13.png)

---

## 2026-06-10 — 벤픽 UI: UMG(WBP) 하이브리드 마이그레이션 (출시 스펙 Pass 1)

**작업 내용**
- 벤픽 위젯을 **순수 C++ Slate → UMG(WBP) 하이브리드**로 전환 (`WBP_MainMenu`/`WBP_Settlement`/`AOSLobbyWidget` 패턴 답습)
- 정적 프레임(Backdrop/Title/Timer/Status/플레이어명/Confirm)을 `meta=(BindWidgetOptional)` 로, `RebuildWidget()` 가 WBP 없을 때만 C++ 폴백 프레임(`BuildFallbackFrame`) 생성
- 동적 자식(카드 20 / 픽슬롯 5+5 / 밴슬롯 2+2)은 BindWidget 불가 → C++ 가 바인딩/폴백 컨테이너(`CardGrid`/`Team1·2BanRow`/`Team1·2PickColumn`)에 채움(`PopulateBanRow`/`PopulatePickColumn` + 카드 루프, `InitializeWithRoster` 에서 멱등)
- `ConfirmButton.OnClicked` 바인딩을 `InitializeWithRoster` 로 일원화(`IsAlreadyBound` 가드) → WBP/폴백 단일 경로

**문제점 / 난관**
- 출시 스펙(LoL·이터널리턴) 대비 격차의 근본 원인 = **폴리시 양이 아니라 저작 파이프라인**: 벤픽만 순수 C++ Slate 라 모든 비주얼 수정이 코드+리빌드, UMG 디자이너/그라디언트/머티리얼/애니메이션 전부 불가
- 벤픽은 카드/슬롯이 **동적 개수**라 단순 BindWidget 전환 불가 (MainMenu/Settlement 와 다른 점)

**해결 방법**
- **정적 프레임은 바인딩 / 동적 자식은 바인딩된 컨테이너에 C++ 가 채움**의 2분할 설계
- PlayerController 는 이미 `ShowBanPick` → `LoadClass(WBP_BanPick_C)` → C++ 폴백 배선됨 → **PC 변경 0**, WBP 미생성 시 폴백이 기존과 동일(회귀 0)
- WBP 저작 이름 계약을 헤더 주석 + CLAUDE.md 표로 명문화

**결과 / 영향**
- 이제 `WBP_BanPick`(Parent=`UAOSBanPickWidget`)을 만들면 디자이너에서 레이아웃·아트·UMG 애니를 **코드 리빌드 없이** 저작 가능 → 출시 스펙 천장 확보
- 영향: `UI/AOSBanPickWidget.h/.cpp` (헤더 BindWidget 리플렉션 변경 → **풀 리빌드 필요**), `CLAUDE.md`, 본 TIMELINE
- 범위(사용자 확정): *마이그레이션만 먼저*. 다음 Pass = 사용자 **손그림 와이어프레임 + LoL/ER 레퍼런스** 기준으로 WBP 위에 아트·모션·오디오
- 관련 계획: `C:\Users\wjrm7\.claude\plans\breezy-wishing-noodle.md`

---

## 2026-06-11 — 벤픽 3D 캐릭터 프리뷰 (SceneCapture→RT→UMG, 클라 전용)

**작업 내용**
- 사용자 레퍼런스(LoL/이터널리턴 식 챔피언 선택)의 **좌상단·우하단 큰 캐릭터 공간을 2D 이미지가 아닌 실시간 3D 렌더**로 구현
- 신규 `AAOSCharacterPreviewStage`(`UI/AOSCharacterPreviewStage.h/.cpp`): 화면 밖 스폰 액터 = SkeletalMesh + `SceneCaptureComponent2D`(`PRM_UseShowOnlyList` 격리) + 포인트라이트 2개. `SetPreviewCharacter`가 클래스 **CDO 메시/AnimClass**만 추출해 적용 → 런타임 RT 에 캡처
- 위젯: `MyPreviewImage`(우하단=내 팀)/`EnemyPreviewImage`(좌상단=상대) BindWidgetOptional + `UpdatePreviewSelections`(내 미리보기/최신픽, 상대 최신픽) → 카드 클릭 즉시 3D 스왑. RT 는 **`FSlateBrush::SetResourceObject`로 직접 표시**(머티리얼/RT 에셋 불필요)
- PC: `ShowBanPick`에서 스테이지 2개 스폰+`InitRenderTarget`+`SetPreviewStages`, `HideBanPick`에서 파괴 (클라+비DS 가드)

**문제점 / 난관**
- UMG엔 3D 뷰포트 위젯이 없음 → SceneCapture→RenderTarget 우회 필요
- 전체 `AAOSCharacter` 액터를 프리뷰로 스폰하면 ASC/AI 등 게임플레이가 딸려와 무겁고 위험
- AnimBP가 캐릭터 없는 메시에서 크래시할 위험

**해결 방법**
- 액터 대신 **CDO의 `GetMesh()`에서 SkeletalMesh+AnimClass만** 떼어 standalone 메시에 적용 (가볍고 안전)
- `UAOSAnimInstance::NativeUpdateAnimation`이 OwningCharacter null 시 조기반환 확인 → AnimBP 그대로 붙여도 **크래시 없이 idle 포즈**, AnimGraph는 Speed=0 idle 평가
- `AlwaysTickPoseAndRefreshBones`로 오프스크린에서도 포즈 갱신, 메시 보일 때만 `bCaptureEveryFrame`
- 라이팅/프레이밍 `EditAnywhere` → 빌드 없이 PIE 중 튜닝

**결과 / 영향**
- 카드 클릭/픽 시 좌상단·우하단에 캐릭터 3D 모델 렌더. 머티리얼·RT 에셋 0개(순수 C+++런타임 RT), DS 안전(클라 전용)
- 영향: `UI/AOSCharacterPreviewStage.h/.cpp`(신규), `UI/AOSBanPickWidget.h/.cpp`, `AOSPlayerController.h/.cpp`, `CLAUDE.md` → **풀 리빌드 필요**
- 범위: 3D 시스템 우선(사용자 확정). 레퍼런스 레이아웃 전면 재현(픽 슬롯 재배치·LOCK IN 등)은 WBP 디자이너에서 후속
- 후속: 라이팅/카메라 프레이밍 튜닝, WBP에 프리뷰 Image 배치, (옵션) 턴테이블 회전·투명 배경

---

## 2026-06-11 — 벤픽 레퍼런스 레이아웃 골격 (코너 배치 + 가로 픽 행 + LOCK IN)

**작업 내용**
- 사용자 레퍼런스(모바일레전드/LoL 식 챔피언 선택) 배치를 C++ 폴백에 반영 (골격 — 정교화는 WBP에서)
- 풀-하이트 좌우 패널 → **코너 앵커드 오버레이**로 전환: 상단중앙=제목/타이머/CHAMPION SELECT, 중앙=그리드+**LOCK IN**, **팀1(레드) 상단-우 블록**(이름+가로 픽행+밴), **팀2(블루) 하단-좌 블록**, 3D 프리뷰는 좌상단(상대)/우하단(내 팀)
- 픽 슬롯 세로 컬럼 → **가로 행(tall 카드 5칸)**: `Team1/2PickColumn`(VerticalBox) → `Team1/2PickRow`(HorizontalBox), `PopulatePickColumn` → `PopulatePickRow`

**해결 방법 / 설계**
- BindWidget 계약도 픽 컨테이너를 HorizontalBox(`Team1/2PickRow`)로 갱신 — WBP 미저작이라 변경 비용 0
- `RefreshSlots`는 슬롯 배열(Borders/Images/Names) 기반이라 컨테이너 방향 바뀌어도 로직 불변

**결과 / 영향**
- 레퍼런스에 가까운 배치(코너 팀 블록 + 중앙 LOCK IN). cpp-only(레이아웃) → 빌드 필요
- 영향: `UI/AOSBanPickWidget.h/.cpp`, `CLAUDE.md`(계약 표)
- 범위: **골격**. 픽 카드 디테일(SELECTED HERO/Level/Role 라벨), 정확한 픽셀·아트는 WBP 디자이너에서 후속
- 미해결(다음): 프리뷰 배경 하늘 비침 제거(캡처 ShowFlags Atmosphere/Fog off), 턴 적용 버그

---

## 2026-06-11 — 벤픽 레퍼런스 디자인: 라이트 테마 + 카드/밴/그리드 콘텐츠 (C++ 폴백)

**작업 내용** (디자인 갭 체크 후 사용자 확정: 밝은 테마 + C++ 폴백 우선)
- **라이트 테마 전환**: 다크 고딕 배경 → 밝은 무채색(`kBgLight`), 모든 텍스트 어두운색, 그리드 패널 라이트+얇은 테두리, LOCK IN 블루 버튼. 파일 상단 라이트 팔레트 상수(`kPanelLight/kCardEmpty/kBorderLight/kTextDark/kTextGray/kLockInBlue/kBanRed`)
- **픽 카드(레퍼런스)**: 초상화 영역 + "SELECTED HERO" + 이름(빈=CHOOSE HERO/픽=챔피언명) + 슬롯탭(R1~R5/B1~B5 팀색 바). 108×168 세로 카드
- **밴 슬롯**: 라이트 박스 + 빈칸 빨간 ✕(이미지 투명→X 비침), 밴되면 초상화 회색조
- **그리드**: 카드 아래 챔피언명 라벨 + 폭 고정(600)으로 ~8열 래핑 + 라이트 셀
- **제목**: "CHARACTER SELECT" + "SEASON 9 DRAFT"
- `RefreshCards/RefreshSlots/RefreshStatus` 라이트 리컬러 (타이머/상태 어두운색, 활성=골드/블루, CHOOSE HERO 갱신)

**문제점 / 난관**
- "완전히 같은 디자인"의 최대 분기 = 배경 테마(다크 vs 레퍼런스 밝은) → 사용자에게 확인 후 밝은 테마 확정
- 레퍼런스 "Level 1 / Role"은 로스터에 대응 데이터 없음 → 생략(클러터 회피), "SELECTED HERO/CHOOSE HERO/슬롯탭"만 의미 구현

**해결 방법 / 메모**
- 헤더 변경 없는 cpp 본문 + 파일 스코프 상수 → **Live Coding 호환**(Ctrl+Alt+F11 + PIE 재시작)
- 다크 배경/글로우/카드프레임 텍스처 로드 제거(라이트 테마). `TryLoadTexture`+경로 상수는 향후 라이트 백드롭용 보존

**결과 / 영향**
- 레퍼런스의 밝은 에디터풍 + 카드/밴/그리드 콘텐츠 반영. 영향: `UI/AOSBanPickWidget.cpp`(cpp-only)
- 후속: 픽셀 정밀·폰트(Open Sans)·아트는 WBP 디자이너에서. (옵션) Level/Role 더미 라벨 추가

---

## 2026-06-13 — 벤픽: 그리드 세로 스크롤 + 스케치 기반 장식 (대각 경계선/LOCK IN 라인/플러리시)

**작업 내용**
- **그리드 세로 스크롤**: `SizeBox(MaxDesiredHeight=410≈5행) → ScrollBox → CardGrid` — 5행 초과 시 스크롤바(가로 없음). 현 20종(3행)은 무변화
- **스크롤 히트테스트 가드**: 뷰포트 밖으로 스크롤된 카드의 cached geometry 오클릭 방지 — `NativeOnMouseButtonDown` 에 CardGrid 부모(뷰포트) geometry 선검사
- **스케치 기반 장식 4종** (사용자 손스케치 → "반듯하고 깨끗하게"):
  - 대각 팀 경계선 (좌=블루팀 코너 경계/우=레드팀 코너 경계, 팀색 ±8° — PIL 테이퍼 라인)
  - LOCK IN 양쪽 레드 강조선 (가로 테이퍼 라인)
  - **제목 양옆을 감싸는 필리그리 날개** (단일 날개 생성 → 좌 원본/우 미러) + 타이머 아래 그리드 폭 가로 라인 (레퍼런스 레드)
- 색 규칙(사용자 지정): 팀을 나누는 선=팀색 / 팀 무관 선·장식=레퍼런스 레드

**문제점 / 난관**
- 직선·강조선은 AI 생성보다 해석적 PIL 이 "반듯" — 반면 플러리시(곡선 장식)는 ComfyUI 가 한 방에 고품질 대칭 장식 생성
- 생성된 플러리시의 모서리에 잘린 원 장식 클러터 + 중앙 텍스트 겹침 위험 → **두 밴드(위/아래)로 크롭 + 엣지 알파 페이드**로 해결, 제목 위/타이머 아래로 분리 배치 (텍스트 겹침 0)
- **1차 PIE 피드백 "장식이 잘려 보임"**: 생성 이미지 배경이 순흑이 아니라 휘도→알파 변환 시 노이즈가 남아 **희미한 분홍 사각 박스**로 렌더 + 크롭 창이 장식을 가로질러 실제 잘림 → **알파 레벨(BLACK=48 임계값) + 콘텐츠 bbox 자동 크롭(+여백)** 으로 해결
- **피드백 반복 (3회)**: "스크롤 장식보다 날개처럼" → 독수리 엠블럼 → "너무 새 같다, 레이스+날개 혼합" → 필리그리 날개쌍 → "제목과 따로 노는 데다 우측 잘림, 텍스트를 감싸는 형태로" → **단일 날개 생성**(`T_BanPick_WingL_A_gen` 중앙 날개쌍에서 왼쪽 날개만 추출) → 위젯에서 [날개]CHARACTER SELECT[미러 날개] HBox 로 제목을 감쌈. 디바이더는 장식 대신 **그리드 폭(614) 가로 테이퍼 라인 1줄**로 단순화(사용자 요청)
- **생성물에 섞인 조각 제거**: 창 크롭으로도 모서리 프레임 장식 조각이 남음 → 크롭 스크립트에 **최대 연결 성분(BFS) 필터** 추가 — 날개(최대 덩어리)만 유지, 떠다니는 조각 전부 제거
- MCP `manage_asset import save:true` 가 .uasset 디스크 flush 안 함(기존 함정 재현) → **`control_editor save_all`** 로 강제 저장

**해결 방법 / 메모**
- 적재적소: 라인=PIL(`make_banpick_linetaper.py`, V+H 2종) / 곡선 장식=ComfyUI 인라인(`gen_banpick_flourish.py`, 기하 폴백 내장) / 크롭=`crop_banpick_flourish.py`(시드 의존 — 재생성 시 크롭 창 재조정)
- 전 장식 HitTestInvisible + 텍스처 미존재 시 솔리드/skip 폴백 — 클릭·회귀 0
- 헤더 불변(cpp-only) → **Live Coding 호환**

**결과 / 영향**
- 로스터 확장 대비 스크롤 + 화면에 구조감(팀 경계)·포인트(레드 장식) 추가
- 영향: `UI/AOSBanPickWidget.cpp`, 신규 스크립트 3종, `Content/AOS/UI/Assets/T_BanPick_{LineTaper,LineTaperH,FlourishTop,FlourishBottom}`, `CLAUDE.md`

**진행 스크린샷** — 제목 날개 장식 반복 (피드백 3회)

**① 독수리 엠블럼** — "너무 새 같다" → 폐기
![독수리 크레스트](images/2026-06-13_banpick_decor/1.png)

**② 레이스 필리그리 날개쌍** — 제목과 따로 + 우측 잘림 → 폐기
![레이스 날개쌍](images/2026-06-13_banpick_decor/2.png)

**③ 최종** — 단일 날개 좌/우 미러로 **제목을 감싸는** 형태 + 타이머 아래 그리드 폭 가로 라인
![감싸는 날개](images/2026-06-13_banpick_decor/3.png)

---

## 2026-06-13 — 벤픽: 창 리사이즈 반응형 (디자인 캔버스 + ScaleBox ScaleToFit)

**작업 내용**
- 절대 픽셀 코너 레이아웃이라 창 크기/비율 변경 시 코너 블록이 가운데로 몰려 **겹치는** 문제
- 루트를 `OuterOverlay[배경 + UScaleBox(ScaleToFit) → SizeBox(1920×1080 디자인 캔버스) → 콘텐츠]` 로 감쌈 — 콘텐츠는 1920×1080 절대 배치, ScaleBox 가 비율 유지 균일 스케일(안쪽 UI 포함)

**문제점 / 난관 — "꽉 채우기" 3-라운드**
- trade-off: 비율 보존(여백) ∥ 왜곡(stretch) ∥ 잘림(crop) ∥ 코너고정(작은 창 겹침), 공짜 없음
- ① `ScaleToFit` → 비율 보존+여백 → "꽉 채우고 싶다"
- ② `Stretch=Fill` → **"내부 UI 크기 안 변함"**: ⚠️ ScaleBox `Fill` 은 **고정크기(SizeBox) 자식을 스케일 안 하고 슬롯만 늘림** → 안쪽이 원본 1920×1080 그대로 클립
- ③ 수동 `SetRenderScale((W/1920,H/1080))`(NativeTick) → **"한쪽 과도 잘림"**: 뷰포트/DPI 좌표 계산이 까다로워 스케일이 어긋남
- **결론: `ScaleToFit` 으로 확정**(안쪽까지 정상 스케일, 잘림/왜곡 0). **실제 플레이는 16:9 → 여백 0**, 여백은 비-16:9 PIE 창 드래그 시에만(출시 빌드 무관)

**해결 방법 / 메모**
- 클릭 히트테스트는 스케일된 cached geometry 로 계산돼 영향 없음. 헤더 불변(cpp+include) → Live Coding 호환
- 비율 무시 꽉 채움이 꼭 필요하면 DPI 글로벌 스케일 + 코너 앵커 반응형이 대안(단 극단 비율서 겹침) — 미채택

**결과 / 영향**
- 창 크기 변해도 벤픽 UI 가 비율 유지하며 통째 스케일(겹침 0). 영향: `UI/AOSBanPickWidget.cpp`, `CLAUDE.md`

**진행 스크린샷** — 비-16:9 창에서의 동작 (실제 16:9 플레이에선 여백 0)

**좁은 창** — Fill/수동스케일 시도 시 한쪽 과도 잘림 (폐기)
![좁은 창 잘림](images/2026-06-13_banpick_decor/4.png)

**큰/오프-비율 창** — ScaleToFit 비율 유지(여백). 16:9 로 맞추면 여백 0
![오프비율 여백](images/2026-06-13_banpick_decor/5.png)

---

## 진행 중 (작업 완료 시 위 형식으로 이동)

### Part B — ABP 상하체 분리 배선
- UpperBody 슬롯 생성 + 4개 스킬 몽타주 재배치
- AnimGraph: `Save Cached Pose 'UpperFull'` + 2× `Use cached pose` 로 fan-out, `Layered blend per bone(spine_01)` + `Blend Poses by bool(bIsMoving)`
- 진행 상황: 캐시 포즈 fix 적용 중 (`Slot 'UpperBody'` 출력의 직접 fan-out 불가 → cached pose 경유 필요)
- 문제점/난관: AnimGraph 포즈 핀이 "출력→입력 1개"만 가능한 제약을 처음 만나 우회 패턴 발견
- 관련 가이드: `Guides/03_Implementation/UPPER_LOWER_BODY_SPLIT.md`

### 스킬 데이터 주도 리팩토링 — `UGA_SkillBase` (1·2단계 완료, 3단계 대기)

**작업 내용**
- 캐릭터 개별 스킬 = C++ 1개 base + BP child 자산으로 생산 구조 전환
- **1단계 ✅**: `UGA_SkillBase`(데이터 주도 부모 GA) + `UGE_SkillCooldown_Base`(쿨다운 GE 부모) C++ 작성
  - UPROPERTY: SkillIdentityTag, bAllowMovementDuringCast, CooldownDuration, SelfAppliedEffects[], TargetAppliedEffects[], DamageGameplayEffectClass, TargetType(Self/SingleEnemy/AoE_Sphere), AoERadius, BaseDamage, MissingHpDamageScale, PeriodicTickCount/Interval
  - 통합 ActivateAbility: 쿨다운→Commit→Self GE→Target 분기→Cast Root→Montage→EndAbility
  - BlueprintNativeEvent `CalculateTargetDamage` (R 의 처형식 등 BP override 가능)
  - `PostInitProperties/PostLoad` 가 SkillIdentityTag 를 AssetTags(`GetAssetTags()`/`SetAssetTags()` 신 API) + ActivationOwnedTags 에 자동 추가
- **2단계 ✅** (agent-design-balance + agent-design-docs 병렬):
  - **BP 자산 8개**:
    - `/Game/AOS/GAS/Effects/Cooldowns/Alex/BP_GE_Cooldown_Alex_{Q,W,E,R}` (parent `UGE_SkillCooldown_Base`, GrantedTag `Cooldown.Skill.Alex.*`)
    - `/Game/AOS/GAS/Abilities/Alex/BP_GA_Alex_{Q,W,E,R}` (parent `UGA_SkillBase`, UPROPERTY 풀세팅)
  - **BP_Char_Alex.StartupAbilities** 4 C++ → 4 BP 교체 (`GA_Attack` C++ 는 유지)
  - **신규 가이드**: `Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md` (~330줄, 새 스킬 5~10분 작성 절차)
  - 기존 C++ 4종(`GA_Alex_*` + `GE_Cooldown_Alex_*`) 은 그대로 보존 — BP 만 부여되므로 충돌 없음
- **3단계 🔜**: 기존 C++ 4종 cleanup 제거 (PIE 검증 통과 후)

**문제점 / 난관**
- 캐릭터당 4 C++ + 4 쿨다운 GE → 캐릭터 N개로 늘면 코드 폭증, 디자이너가 새 스킬 추가하려면 C++ 빌드 필요
- 빌드 시 `TObjectPtr<const T>::IsValid` 멤버 없음 에러 (FGameplayEventData::Target 패턴 미숙지) + UE 5.7 의 `AbilityTags` deprecation 경고
- `UTargetTagsGameplayEffectComponent::SetAndApplyTargetTagChanges()` Python 노출 안 됨, 속성 이름 `InheritableGrantedTagsContainer` (가이드의 `InheritableOwnedTagsContainer` 와 다름)

**해결 방법**
- C++ 1 base + BP child 패턴 (UE 표준). 통합 흐름이 Self-buff(Q/W) + 단일 처형(R) + AoE 주기(E) 모두 커버, 특이 로직만 BP `CalculateTargetDamage` override
- `Target.Get()` 후 null 체크로 IsValid 우회, `GetAssetTags()/SetAssetTags()` 신 API 로 마이그레이션
- 컴포넌트 추가 + GrantedTag 설정은 `unreal.EditorAssetLibrary.rename_asset` 등 Python 직접 호출로 우회

**결과**
- 캐릭터·스킬 확장 시 BP 만으로 작성 가능 → 디자이너 이터레이션 시간 대폭 단축
- C++ 클래스 수 = 1 (base) + 1 (cooldown base) — 기존 4+4=8 대비
- 3단계 cleanup 후 기존 C++ 8개 모두 제거 예정 → 코드 베이스 ↓
- 관련 가이드: `Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md`

## 2026-06-16 — 라운드 준비 창: 벤픽 디자인 비주얼 리스킨

**작업 내용**:
- `AOSCharacterSelectWidget::BuildUI()` 전면 재작성 — 벤픽 창(`UAOSBanPickWidget`)과 동일한 라이트 테마 비주얼 적용. 배치 흐름(타이틀→타이머→3레인 슬롯→카드 그리드→준비 버튼)과 드래그앤드롭은 유지, **비주얼만** 리스킨 (사용자 선택: 레이아웃 재구성이 아닌 비주얼 리스킨)
- 반응형 1920×1080 `UScaleBox(ScaleToFit)` 디자인 캔버스 + 솔리드 라이트 그레이 배경
- 벤픽 장식 텍스처 재사용: 대각 팀 경계선(좌 Team2 블루/우 Team1 레드), 타이틀 양옆 날개(우측 미러), 타이머 아래 + 준비 버튼 양옆 테이퍼 라인
- 레인 슬롯/카드/준비 버튼(LOCK IN 스타일 파란 버튼) 라이트 리스킨, 타이머/팀상태/RoundResult 색도 라이트 배경 가독 색으로
- cpp-only(헤더 불변) → Live Coding 호환

**문제점**:
- 1차 빌드 시 UE 유니티(Jumbo) 빌드가 `AOSCharacterSelectWidget.cpp` + `AOSBanPickWidget.cpp` 를 한 TU 로 합치면서, 복제한 익명-네임스페이스 심볼(`kBgLight`/`MakeFont`/`PlaceholderColor`/색·경로 상수)이 벤픽 것과 재정의 충돌 (C2374/C2084)
- 초기 배경이 `T_BanPick_Backdrop`(성당 이미지)로 떠서 벤픽(솔리드 라이트 그레이)과 불일치

**해결 방법**:
- 충돌 심볼 전부 `CS` 접두사로 고유화 (벤픽 파일 무수정). `TryLoadTexture`/`TeamColor` 는 벤픽에선 static 멤버라 free 함수와 비충돌 → 그대로 유지
- 배경을 벤픽 C++ 폴백과 동일하게 솔리드 `CSBgLight` 로 변경 (텍스처 백드롭 미사용)

**결과**:
- 라운드 준비 창이 벤픽(CHARACTER SELECT)과 동일한 디자인 언어로 통일 — Lobby→BanPick→RoundPreparation 흐름 시각적 일관성 확보
- 변경: `Source/TDProject/AOS/UI/AOSCharacterSelectWidget.cpp` (cpp 1파일) + 문서(CLAUDE.md)
- 관련 커밋: 본 작업 커밋 (라운드 준비 벤픽 리스킨)
