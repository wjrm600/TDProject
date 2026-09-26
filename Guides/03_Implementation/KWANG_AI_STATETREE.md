# Kwang 전용 AI StateTree (`ST_KwangAI`) — 설계·구현·테스트 기록

> **상태**: 🧪 테스트용 (2026-09-27 작성, 같은 날 **v2** 로 요구사항 확정). 공용 `ST_AOSCharacterAI` 는 건드리지 않았다 — Kwang(로스터 0)만 이 트리를 쓴다.
> **진행 (v2)**: ✅ 에셋 변경(사거리·R 쿨·트리 구조) · ✅ 풀 리빌드 · ✅ 후퇴 쿨 조건·후퇴 파라미터·`BasicAttackCooldownOverride=5` (컴파일 메시지 0, 디스크 확인) · ⏳ PIE 테스트
> **되돌리기**: `BP_Char_Kwang` → Class Defaults → **AOS|AI → AI State Tree Override** 를 비우면 즉시 공용 트리로 복귀.

---

## 1. 요구사항 — 원문 → 사용자 확정(v2) → 구현

| # | 원문 (v1) | 사용자 확정 (v2) | 구현 |
|---|-----------|------------------|------|
| 1 | **선호 1** — 일반 공격 후 다음 일반 공격 전까지 옆 걸음질 | 확인할 수 있게 **공격 쿨다운 5초** | Kwang 기본공격 쿨다운 5초 고정(모션 속도는 그대로) → 공격 모션 ≈1.3초 뒤 **≈3.7초 틈**. 틈 동안 적을 바라본 채 **150씩 좌우 반복** 옆걸음 |
| 2 | Q·W 는 일반 공격 3회 후. Q = 200 내 1명 또는 W 쿨 / W = 200 내 2명+ 또는 Q 쿨 | **W↔E 정정** + "3회 채우고 Q/E 를 쓴 뒤 다시 3회 채워야 Q/E" | 기본공격 ≥3 + 200 안 적 ≥1 → **Q**(1명 또는 E 쿨) / **E 회전**(2명+ 또는 Q 쿨). **Q·E 발동 시 카운트 0** |
| 3 | E 는 버프 — 400 안에 적이 있으면 언제든 | **W 가 버프** | 400 안 적 ≥1 + W 준비 → **W**. 카운트 무관·리셋 없음 |
| 4 | R — 상대 체력 30% 이하 | **R 쿨 45초** | 현재 타겟 HP < 30% + R 준비 → R. `BP_GA_Kwang_R.CooldownDuration 1 → 45` |
| 5 | **선호 2** — 체력 40% 떨어지면 Q·W 후 타워 쪽으로 100씩 | "전체 체력의 40%까지 떨어지면" · "한 번 후퇴하면 라인 푸시 재개, 후퇴 로직 쿨 60초, 100 이동이 아니라 **가장 가까운 아군 타워 200 안까지** 후퇴" | HP ≤ 40% + 후퇴 쿨 아님 + 타워 200 밖 → (적 있으면) 긴급 **Q → E** → 타워 200 안까지 **한 번에** 후퇴 → **도착 순간부터 60초 후퇴 쿨** → 라인 푸시 재개 |
| 6 | **선호 3** — 500 안 적 2명 이상이면 최대 체력 최저 적 먼저 | (변경 없음) | 500 안 적 ≥2 → MaxHP 최저(동률이면 가까운 쪽) 타겟 |
| — | (사거리) | **Kwang 사거리 300 → 200** | `DT_CharacterAttributes` Kwang 행 AttackRange 200 (Kwang 만 쓰는 행) |

### v2 에서 제가 정한 기본값 (다르면 알려주면 즉시 변경)
| 항목 | 기본값 | 근거 | 바꾸려면 |
|------|--------|------|----------|
| 선호 2 의 긴급 스킬 | **Q → E** | W↔E 정정을 규칙 2 와 똑같이 적용 (원문의 "Q, W" 쌍이 규칙 2 와 같은 쌍) | `EmergencyE` 의 두 태그를 W 로 (보호막 후 도주) |
| 7번 "100 이동 후 재교전" vs 8번 "타워까지 후퇴" | **8번만 구현** | 8번의 "100 이동이 아니라 타워까지"가 7번을 대체하는 것으로 해석 | — |
| 60초 쿨 시작 시점 | **후퇴 완료(타워 200 도달) 순간** | 먼 거리 후퇴면 시작 기준일 때 쿨이 도중에 거의 다 지나감 | `CooldownKey` 기록 위치 변경(코드) |
| 옆걸음 형태 | 틈 동안 **좌우 반복** | 3.7초 틈에 한 걸음만 가면 3초 넘게 서 있음 | — |
| "40%까지" | HP **< 40%** (float 비교라 ≤ 와 실질 동일) | — | — |

---

## 2. 트리 구조 (형제 순서 = 우선순위)

```
Root [TrySelectChildrenInOrder]
├─ UseR               규칙4  HasNearbyEnemy + TargetHealthBelowPct(0.3) + R 쿨X → Activate R
├─ LowHP_Retreat ▣    선호2  HP<40% + BehaviorCooldownReady("Retreat",60)* + 아군 타워 200 밖
│   ├─ EmergencyQ           HasNearbyEnemy + Q 쿨X → Q (+카운트 리셋)
│   ├─ EmergencyE           HasNearbyEnemy + E 쿨X → E (+카운트 리셋)
│   └─ RetreatToTower       Retreat To Friendly Tower(StopRadius 200, Key "Retreat")*
├─ SkillQE ▣          규칙2  BasicAttackCount ≥3 + 200 안 적 ≥1
│   ├─ Q_SingleTarget       200 안 적 = 1명 + Q 쿨X → Q (+리셋)
│   ├─ E_MultiTarget        200 안 적 ≥2명 + E 쿨X → E (+리셋)
│   ├─ Q_EOnCooldown        E 쿨 중 + Q 쿨X → Q (+리셋)
│   └─ E_QOnCooldown        Q 쿨 중 + E 쿨X → E (+리셋)
├─ UseW               규칙3  400 안 적 ≥1 + W 쿨X → W (버프, 리셋 없음)
├─ FocusLowestMaxHP   선호3  500 안 적 ≥2 → Select Lowest Max-Health Enemy(500)   [OnTick→Root]
│   ├─ Focus_Strafe   선호1  공격 쿨다운 중 + 공격 모션 끝남 → Strafe Around Target(150)
│   └─ Focus_Engage          MoveToCurrentTarget + SendAttackEvent
├─ AttackEnemy               HasNearbyEnemy → FindNearestEnemy                     [OnTick→Root]
│   ├─ Nearest_Strafe 선호1  공격 쿨다운 중 + 공격 모션 끝남 → Strafe Around Target(150)
│   └─ Nearest_Engage        MoveToCurrentTarget + SendAttackEvent
├─ StrafeStructure    선호1  HasCurrentWaypointStructure + 공격 쿨다운 중 + 공격 모션 끝남
│                            → Strafe Around Target(150, 구조물)                   [OnTick→Root]
├─ AttackStructure           (공용 트리 그대로)
└─ PushLane                  (공용 트리 그대로)
```
▣ = Group (태스크 없음, 자식이 하나도 선택 안 되면 그룹 자체가 스킵되고 다음 형제로)
\* = v2 리빌드 후 배치/설정 (✅ 완료)

"공격 쿨다운 중" = `Owner Has Tag(Cooldown.Attack.Basic)`, "공격 모션 끝남" = `Owner Has Tag(Ability.Attack.Basic, 반전)` → **"일반 공격 후 ~ 다음 일반 공격 전"** 을 그대로 옮긴 조건.

**우선순위 결정 근거**
- **R 이 후퇴보다 위** — 빈사여도 처형 각이면 킬을 먼저 (R 은 시전 중 root).
- **후퇴가 Q/E·W 보다 위** — 저체력이면 일반 스킬 규칙보다 생존이 먼저. 후퇴 그룹 안에서 Q → E 를 먼저 쓴다(원문 "Q, W 사용 후" → W↔E 정정 적용).
- **후퇴는 끝까지 커밋** — 후퇴 태스크는 도중에 재평가하지 않는다. 그래야 후퇴 중에 Q 쿨(5초)이 돌아와도 다시 멈춰서 Q 를 쓰지 않는다. 대가: 후퇴 중엔 R 도 끼어들지 못한다.
- **Q/E 가 W 보다 위** — Q/E 는 3회 조건이 까다로워 기회가 드물고, W 는 조건이 느슨해 한 틱 양보해도 곧 다시 온다.
- **옆걸음(Strafe)이 Engage 보다 위** — 같은 부모 안에서 쿨다운 틈이면 공격 대기 대신 옆걸음.

**Design B 와의 관계**: 루트 재선택 패턴 그대로. RUNNING 유지 state(FocusLowestMaxHP / AttackEnemy / StrafeStructure / AttackStructure / PushLane)만 `OnTick → Root`. 스킬·후퇴는 완료형이라 전이 없음 — 완료 시 엔진이 루트로 복귀한다 (`StateTreeExecutionContext.cpp` "Could not trigger completion transition, jump back to root state." 확인).

---

## 3. 구현

### 3-1. 연결 방식 — 캐릭터별 트리 지정
- `AAOSCharacter::AIStateTreeOverride` (`UStateTree*`, EditDefaultsOnly). **비우면 공용 트리.**
- `AAOSAIController::StartDeployment` 가 `StartLogic()` 직전에 `StateTreeComponent->SetStateTree(Override)`.
  (`SetStateTree` 는 실행 중엔 거부되므로 반드시 시작 전에 교체.)
- 다른 캐릭터에 개성을 줄 때도 같은 칸 하나만 채우면 된다 — AI 컨트롤러 BP 를 복제할 필요 없음.

### 3-2. 캐릭터별 기본공격 쿨다운 고정 (v2 신규)
- `AAOSCharacter::BasicAttackCooldownOverride` (초, 0 = 기본 규칙 `1/AttackSpeed`). Kwang = **5**.
- `GA_Attack` 이 쿨다운 GE 에 넣는 Duration 만 바꾸고, **몽타주 재생 속도는 계속 AttackSpeed** — 모션은 그대로, 간격만 늘어난다.
  (`AttackSpeed` 를 0.2 로 낮추면 쿨다운은 5초가 되지만 공격 모션이 5배 느려져서 쓸 수 없다.)

### 3-3. 전투 메모리 (`AAOSAIController`)
| 멤버 | 의미 | 갱신 |
|------|------|------|
| `BasicAttackCount` | 마지막 리셋 이후 기본공격 횟수 | ASC `Ability.Attack.Basic` 태그가 **붙을 때** +1 (`RegisterGameplayTagEvent`, OnPossess 에서 구독) |
| `LastBasicAttackEndTime` | 직전 기본공격이 **끝난** 시각 | 같은 태그가 **떨어질 때** |
| `StrafeSign` / `StrafeGoal` | 옆걸음 방향 / 진행 중인 한 걸음 | 도착(30 이내) 또는 막힘(1.5초)마다 방향 반전 후 다음 걸음 |
| `BehaviorLastUsedTime` | 행동별 마지막 사용 시각 (v2) | `MarkBehaviorUsed(Key)` — 후퇴 완료 시 `"Retreat"` |

태그 이벤트로 집계하므로 공격을 어떤 태스크가 발동하든 한 곳에서 센다 (기존 `SendAttackEvent` 무수정).

### 3-4. 신규 StateTree 노드 (`AI/AOSStateTreeBehaviorNodes.h/.cpp`)
모든 파라미터가 **InstanceData** 에 있다 → unreal-statetree MCP 로 값 변경 가능.

| 노드 | 종류 | 파라미터 (기본값) | 역할 | ST_KwangAI 사용 |
|------|------|-------------------|------|:---:|
| Owner Has Tag | 조건 | Tag, bInvert(false) | 태그 유/무. 스킬 준비 = 쿨다운 태그 + bInvert=true | ✅ |
| Enemy Count In Radius | 조건 | Radius(500), MinCount(1), MaxCount(-1=무제한) | 반경(중심 간 거리) 안 적 수 범위 | ✅ |
| Basic Attack Count At Least | 조건 | Count(3) | Q/E 게이트 | ✅ |
| Friendly Tower Distance | 조건 | Radius(400), bWithin(false) | 가장 가까운 아군 타워(없으면 CC)까지 거리 | ✅ |
| Behavior Cooldown Ready (v2) | 조건 | BehaviorKey("Retreat"), CooldownSeconds(60) | 행동별 쿨다운 | ✅* |
| In Post-Attack Window | 조건 | WindowSeconds(0.5) | 공격 종료 후 N초 창 — 쿨다운 틈이 0 인 캐릭터용 | ❌ (v1 에서 사용) |
| Select Lowest Max-Health Enemy | 태스크 | Radius(500) | 최대 체력 최저 적 → CurrentTarget | ✅ |
| Strafe Around Target | 태스크 | StepDistance(150), bTargetCurrentEnemy(true) | 옆걸음 좌우 반복 (ExitState 에서 회전 모드 원복) | ✅ |
| Retreat To Friendly Tower (v2 재설계) | 태스크 | StopRadius(200), CooldownKey("Retreat") | 타워 반경 안까지 한 번에 후퇴 → 도착 시 쿨다운 기록 | ✅* |
| Activate Ability (Reset Attack Count) | 태스크 | AbilityTag, bResetBasicAttackCount(true) | 공용 ActivateAbilityByTag + 성공 시 카운트 0 | ✅ |

**왜 공용 노드를 재사용하지 않고 새로 만들었나** — 공용 조건의 `bInvert`, `SendAttackEvent.bTargetCurrentEnemy` 는 **노드 본체** 속성이라 MCP(InstanceData 전용)로 설정할 수 없다. 그래서 ① 트리는 공용 트리를 **복제**해서 기존 노드의 반전 값을 보존하고(UseR/UseW 의 쿨다운 조건, AttackStructure), ② 새로 필요한 판정은 전부 InstanceData 파라미터 노드로 만들었다.

### 3-5. 옆걸음 동작 원리
- 시작 시 캐릭터 무브먼트를 `bOrientRotationToMovement=false`, `bUseControllerDesiredRotation=true` 로 바꾸고 AI 포커스를 타겟에 고정 → 몸은 타겟을 보고 다리는 옆으로 → `BS_Kwang_Locomotion` 의 `Jog_Left/Right` 가 재생된다.
- 목표점 = 타겟 중심 원호 위에서 150 만큼 회전한 점 (반경은 현재 거리, 최대 사거리×0.9 = **180**) → **사거리를 벗어나지 않고** 옆으로 비킨다. 도착하면 반대 방향으로 150 → 두 점 사이를 오간다.
- 틈이 끝나면(쿨다운 해제) Engage 로 넘어가며 ExitState 에서 회전 모드 복원 + 포커스 해제. `StopMovement` 는 부르지 않는다 (사망 시 StopLogic → ExitState 경유 — root motion 보호 규칙).
- 서버에서만 바꾸는 값이라 클라는 복제된 회전을 그대로 받는다 (DS 정합).

### 3-6. 후퇴 동작 원리 (v2)
- 그룹 진입 조건 3개: HP < 40% · `"Retreat"` 쿨 아님(60초) · 가장 가까운 아군 타워 200 **밖**.
- 긴급 Q → E (주변에 적이 있을 때만, 각 스킬 준비된 것만) → `RetreatToTower` 가 **타워 200 안에 들 때까지** 이동 (가던 타워가 부서지면 다음 타워로 갈아탐).
- 도착 순간 `"Retreat"` 사용 기록 → 그룹 조건이 60초 동안 거짓 → 평소처럼 라인 푸시/교전.
- 60초 뒤에도 HP 가 40% 미만이면(회복 수단 없음) 타워 200 밖으로 나가는 순간 다시 후퇴 → **"60초 푸시 ↔ 후퇴" 순환**.
- 타워 중심이 내비 밖이라 200 안까지 못 가면, 경로 끝에 도착한 시점을 완료로 처리 (무한 대기 방지).

---

## 4. 실측 기록 (v1 에서 발견 → v2 에서 사용자 결정)

| # | 발견 | v2 결정 |
|---|------|---------|
| A | `AM_Kwang_Attack` 섹션 ≈1.3초 > 공격 쿨다운 1.0초(=1/공속) → **공격 사이 틈 0**. v1 은 AI 가 공격 후 0.5초 창을 일부러 열었음(DPS −28%) | 공격 쿨다운 **5초** 고정 → 자연스러운 틈 ≈3.7초. 쿨다운 틈 그대로를 옆걸음 조건으로 사용 |
| B | Q/W 반경 200 < 사거리 300 (AI 는 사거리 진입 즉시 정지 → 교전 거리 ≈280~300) → Q/W 거의 안 나옴 | Kwang **사거리 200** → 교전 거리 ≤200 이라 1명 이상이 항상 반경 안 |
| C | 원문 "E = 버프" 인데 실제 E = 반경 250 회전 AoE, W = 보호막 | 사용자 정정: **W 버프 / E 회전**. 규칙 2 → Q/E, 규칙 3 → W |
| D | R 쿨다운 1초 (Alex 90초와 달리 테스트값) | **45초** |
| E | 회복 수단이 없어 저체력 후 타워 대기 = 라인 푸시 영구 중단 | 후퇴 1회 후 라인 푸시 재개 + **후퇴 쿨 60초** |

### 현재 Kwang 스킬 (Alex 백본 태그 공유: `Ability.Skill.Alex.*`)
| 슬롯 | 실제 효과 | 시전 중 이동 | 쿨다운 | AI 사용처 |
|------|-----------|:---:|:---:|------|
| Q | 이동속도 증가 + 다음 공격 강화 | ❌ root | 5s | 규칙 2 (1명) · 긴급 |
| W | 받는 데미지 감소(보호막) | ✅ | 15s | 규칙 3 (400 안 적) |
| E | 반경 250 회전 AoE (6틱, 틱당 50) | ❌ root | 10s | 규칙 2 (2명+) · 긴급 |
| R | 단일 처형 (20 + 잃은 HP×0.3) | ❌ root | **45s** | 규칙 4 |

---

## 5. 테스트 방법

1. ~~풀 리빌드 (v2)~~ ✅
2. ~~후퇴 노드 배치/설정 + `BP_Char_Kwang.BasicAttackCooldownOverride = 5` + 컴파일~~ ✅
3. PIE — Listen Server 2-Client (또는 Play As Dedicated Server). 벤픽에서 **Kwang 을 픽**.
4. 확인 로그 (Output Log 필터 `ST/Behavior` / `전용 StateTree`):
   - `[AI Controller] BP_Char_Kwang_C_… → 전용 StateTree 'ST_KwangAI' 사용` — 연결
   - `[ST/Behavior] … 스킬 Ability.Skill.Alex.Q 발동 — 기본공격 카운트 3 → 0` — 규칙 2
   - `[ST/Behavior] … 집중 타겟 → … (MaxHP …)` — 선호 3
   - `[ST/Behavior] … 후퇴 시작 → …` / `후퇴 완료(반경 도달) → … 'Retreat' 쿨다운 시작` — 선호 2
5. 디버그: StateTree 디버거(에디터 Tools → Debug → StateTree) 또는 Rewind Debugger 로 활성 state 확인. `showdebug ai`.

### 관찰 체크리스트
| # | 확인 | 기대 | 결과 |
|---|------|------|------|
| 1 | 1:1 교전 공격 사이 | 공격(≈1.3초) → **약 3.7초 동안 좌우 옆걸음** (몸은 적을 봄) → 다음 공격 | ☐ |
| 1b | 타워 공격 사이 | 타워 둘레로 좌우 옆걸음 | ☐ |
| 1c | 옆걸음 끝난 뒤 | 라인 푸시할 때 이동 방향을 정상으로 봄 (회전 모드 원복) | ☐ |
| 2 | Q/E 발동 | 기본공격 3회 전엔 안 나감. 200 안 1명=Q, 2명+=E. 쓰고 나면 다시 3회 | ☐ |
| 3 | W | 400 안 적 있으면 쿨마다 (공격 횟수 무관) | ☐ |
| 4 | R | 타겟 HP 30% 미만에서, 45초에 1번 | ☐ |
| 5 | 저체력 후퇴 | HP<40% → Q → E → 가장 가까운 아군 타워 200 안까지 후퇴 → 바로 라인 푸시 재개 | ✅ (2·3차 PIE) |
| 5b | 후퇴 쿨 | 후퇴 완료 후 60초 동안은 HP 가 낮아도 후퇴 안 함 | ✅ 재진입 없음 (1차 PIE 에선 반복 1회 — §8) |
| 6 | 다수 교전 타겟 | 500 안 2명+ 이면 MaxHP 최저 적만 때림 | ☐ |
| 7 | 회귀 | 다른 13 캐릭터 AI 는 기존과 동일 (공격 쿨·사거리·트리 모두 Kwang 전용 변경) | ☐ |

---

## 6. 튜닝 표 (에디터 또는 MCP `statetree_set_node_properties`)

| 조정하고 싶은 것 | 위치 | 속성 | 현재 |
|------------------|------|------|------|
| 공격 간격(= 옆걸음 시간) | `BP_Char_Kwang` Class Defaults | AOS\|GAS · Basic Attack Cooldown Override (0=원래 규칙) | 5 |
| 사거리 | `DT_CharacterAttributes` Kwang 행 | AttackRange | 200 |
| R 쿨다운 | `BP_GA_Kwang_R` | Cooldown Duration | 45 |
| 옆걸음 거리 | Focus_Strafe / Nearest_Strafe / StrafeStructure | Strafe Around Target · StepDistance | 150 |
| Q/E 필요 공격 수 | SkillQE | Basic Attack Count At Least · Count | 3 |
| Q/E 대상 반경 | SkillQE / Q_SingleTarget / E_MultiTarget | Enemy Count In Radius · Radius | 200 |
| W 트리거 반경 | UseW | Enemy Count In Radius · Radius | 400 |
| R 처형 기준 | UseR | Target Health Below % · Threshold | 0.3 |
| 후퇴 체력 기준 | LowHP_Retreat | Health Below % · Threshold | 0.4 |
| 후퇴 쿨다운 | LowHP_Retreat | Behavior Cooldown Ready · CooldownSeconds | 60 |
| 타워 도착 반경 | LowHP_Retreat **와** RetreatToTower (둘 다 맞출 것) | Friendly Tower Distance · Radius / Retreat … · StopRadius | 200 |
| 긴급 스킬 | EmergencyQ / EmergencyE | Owner Has Tag · Tag + Activate … · AbilityTag | Q, E |
| 집중 타겟 반경/인원 | FocusLowestMaxHP | Enemy Count In Radius · Radius/MinCount, Select … · Radius | 500 / 2 |

⚠️ 형제 순서 = 우선순위. 조건만 바꾸고 순서를 안 보면 의도와 다르게 동작한다.

---

## 7. 변경 이력

| 버전 | 내용 |
|------|------|
| v1 | 원문대로 구현. 옆걸음 = 공격 후 0.5초 창(1걸음), Q/W 게이트, E 버프(400), 후퇴 = 100씩 → 타워 400 안 대기 |
| v2 | 사용자 확정 반영: 공격 쿨 5초 · 사거리 200 · W↔E 정정(Q/E 게이트, W 버프) · R 쿨 45초 · 후퇴 = 타워 200 안까지 한 번에 + 60초 쿨 + 라인 푸시 재개 · 옆걸음 = 쿨다운 틈 내내 좌우 반복 |

## 8. 테스트 결과 기록

| 날짜 | 빌드 | 관찰 | 조치 |
|------|------|------|------|
| 2026-09-27 | v2 풀 빌드 | ❌ 후퇴 완료 직후 `후퇴 시작` 재발 → 타워 200 경계에서 왔다갔다 (60초 쿨이 막지 못함) | 코드·MCP 값·조건 연산자·엔진 경로 점검 모두 정상 → 진단 로그 3곳 추가 |
| 2026-09-27 | + Live Coding (진단) | ✅ 진입 1회 → 거리 5463 에서 타워로 → 쿨 기록 t=67.13 (키·컨트롤러 일치) → 199 도착 → 라인 푸시, 재진입 없음 | 조건 쪽 진단 로그 제거(조건은 단락 평가 없이 매번 전부 평가 → 쿨 종료 후 로그 폭주 위험) + 방어 수정: 완료 시 `RetreatTarget` 유지 |
| 2026-09-27 | + Live Coding (방어 수정) | ✅ 정상 | 진단 로그 전부 제거. 1차 반복의 원인은 **미확정** — 재발하면 `후퇴 시작`/`후퇴 완료` 로그가 연달아 찍히는지 볼 것 |
