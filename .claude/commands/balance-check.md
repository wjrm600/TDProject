---
description: "TDProject 오토배틀러 밸런스 분석 — DataTable(캐릭터/타워/CC 스탯)·골드 산식·아이템을 읽어 OP 캐릭터·붕괴 전략(degenerate)·경제 불균형을 탐지하고 수치 조정안 제시. 밸런스가 이상하다, 특정 유닛이 너무 세다/약하다, OP 찾아줘, 골드/경제 점검, 아이템 밸런스, 로스터 점검, balance check/report 요청 시 사용."
argument-hint: "[combat|economy|roster|items|DataTable경로]"
---

# 밸런스 체크

당신은 TDProject(오토배틀러 MOBA)의 **밸런스 분석기**입니다.
DataTable · Blueprint 파라미터 · 골드 산식을 읽어 **아웃라이어 · 붕괴 전략(degenerate) · 경제 불균형**을 탐지하고, 우선순위가 매겨진 수치 조정안을 제시합니다.

> ⚠️ 이 스킬의 기본은 **읽기 전용 분석**입니다. 데이터 변경은 사용자 확인 후 `agent-design-balance` 로 위임하거나 MCP 라운드트립으로만 수행합니다.
> ⚠️ 우리 데이터는 `.uasset` 안에 있어 평문이 아닙니다 → **MCP `export_data_table_to_json_string` 로 뽑아서 분석**합니다 (일반 파일 Read 로는 안 열림).

## 사용자 인자

$ARGUMENTS

(도메인 이름 또는 DataTable 경로. 비어 있으면 어떤 도메인을 볼지 질문.)

---

## Phase 1: 밸런스 도메인 식별

`$ARGUMENTS` 로 도메인을 판별합니다:

- **Combat (전투)** → 캐릭터 DPS(AttackPower×AttackSpeed), 사거리, TTK(타워 HP 1000 / CC 5000 / 캐릭터 Health)
- **Economy (경제)** → 골드 faucet(처치 +50 / 구조물 +150 / 라운드 패시브 +100) vs sink(DT_Items 가격)
- **Roster/Draft (로스터·벤픽)** → 20종 로스터 스탯 분포, 픽 가치 편차, 밴 대상 쏠림
- **Items (아이템)** → DT_Items 효과(Infinite GE) · 유닛 귀속 · 가격 대비 효용
- **경로가 주어지면** → 그 DataTable 을 직접 export 해서 내용으로 도메인 추론

인자가 없으면 `AskUserQuestion` 으로 어떤 도메인을 볼지 먼저 질문합니다.

---

## Phase 2: 데이터 소스 읽기 (MCP 라운드트립)

도메인별 관련 DataTable 을 `export_data_table_to_json_string` 로 export 합니다 (모두 `/Game/AOS/GAS/Data/` = `Content/AOS/GAS/Data/`):

| 도메인 | DataTable 경로 | Row 타입 |
|--------|----------------|----------|
| Combat / Roster | `/Game/AOS/GAS/Data/DT_CharacterAttributes` | `FAOSAttributeInitRow` |
| Combat | `/Game/AOS/GAS/Data/DT_TowerAttributes`, `.../DT_CommandCenterAttributes` | `FAOSAttributeInitRow` |
| Economy / Items | `/Game/AOS/GAS/Data/DT_Items` | `FAOSItemRow` |

> 관련 자동화 테스트 `Source/TDProject/AOS/Tests/AOSGoldEconomyTest.cpp` 가 골드 산식을 검증하므로, Economy 도메인 분석 시 이 테스트의 기대값과 교차 검증하면 좋습니다.

경제 상수(골드 산식)는 `AOSGameMode` 의 EditAnywhere 프로퍼티(GoldPerKill=50 / GoldPerStructure=150 / GoldPerRound=100)를 `inspect` / `manage_blueprint` 로 확인합니다.
읽은 DataTable / 프로퍼티를 **모두 기록** → 리포트의 "분석한 데이터 소스" 섹션에 표기.

---

## Phase 3: 설계 기준선(baseline) 확보

우리 프로젝트의 "GDD" 역할을 하는 기준값 문서를 읽습니다:

- `CLAUDE.md` GAS 섹션 — 기본값 **Health=100 / AP=10 / Range=500 / AS=1.0 / MS=600**, **Tower HP 1000 / CC 5000**, 골드 **50/150/100**
- `Guides/01_GameOverview/PROJECT_REFERENCE.md` "GAS" / "Economy & Shop" 섹션 — 상세 타겟·산식
- `Guides/01_GameOverview/GAME_VISION.md` — 의도된 게임 페이스(라인 푸시 속도)

이 값들이 "정상"의 baseline 입니다. DataTable 실측이 여기서 크게 벗어나면 아웃라이어 후보로 잡습니다.

---

## Phase 4: 도메인별 분석

**Combat (전투):**
- 캐릭터별 **유효 DPS = AttackPower × AttackSpeed** 계산 후 상호 비교
- **TTK(Time-To-Kill)**: 타워(HP 1000) · CC(HP 5000) · 상대 캐릭터(Health) 각각에 대해
- 모든 지표에서 우월한(strictly better) 캐릭터 탐지 = OP 후보
- 원거리(히트스캔) 유닛의 `AttackRange`(예: Sparrow=`Ranged` 행 900)가 근접(500) 대비 격차를 정당화하는지 — **사거리 대비 DPS 과다** 여부
- ⚠️ 스킬 데미지는 BP GE(`BP_GA_*` / `BP_GE_*`) 안이라 DataTable 로 안 잡힘 → **수동 확인 항목**으로 리포트에 반드시 명시

**Economy (경제):**
- faucet(처치 50 / 구조물 150 / 라운드 100) 총 유입률을 라운드별로 투영
- sink(DT_Items 가격)와 대조 — 골드 생성 대비 소비처가 스케일하는지
- 아무도 안 사는(strictly useless) 아이템, 무한 골드 루프 여부
- **유닛 귀속 · 라운드 누적** 규칙상 특정 아이템 스택이 붕괴를 유발하는지

**Roster/Draft (로스터·벤픽):**
- 20종(실캐릭터 0~13 + 플레이스홀더 15) 스탯 분포 — 픽률 쏠림 / 사(死)픽 후보
- 벤픽 14스텝(밴4+픽10) 상 특정 유닛이 항상 밴/픽 되는 편차
- 플레이스홀더(Unit6~20, char1 재활용)는 **밸런스 대상에서 제외** 표기(더미)

**Items (아이템):**
- 각 아이템 효과(Infinite GE 크기) / 가격 / 유닛 귀속
- 가격 대비 효용 곡선, 특정 유닛에만 과한(비대칭) 효과 여부

---

## Phase 5: 리포트 출력

```
## 밸런스 체크: [도메인]

### 분석한 데이터 소스
- [export 한 DataTable / 확인한 BP 프로퍼티 목록]

### 헬스 요약: [건강 / 주의 / 심각]

### 아웃라이어
| 항목 | 기대 범위 | 실측 | 문제 |
|------|-----------|------|------|

### 붕괴 전략 (Degenerate)
- [전략 + 왜 문제인지]

### 로스터/경제 곡선 분석
[분포 표 또는 서술]

### 권장 조정
| 우선순위 | 문제 | 제안 수정 | 영향 |
|---------|------|-----------|------|

### 수동 확인 필요 (DataTable 밖)
- [스킬 GE 데미지 등 코드/BP 값 — 이 스킬 범위 밖]
```

---

## Phase 6: 수정 & 검증 사이클

리포트 후 `AskUserQuestion` 으로:
- 프롬프트: "밸런스 체크 완료. 다음 작업은?"
- 옵션:
  - `[A] 최우선 이슈 지금 수정 — agent-design-balance 로 위임`
  - `[B] 리포트 저장 (Guides/05_ProgressLog/balance/)`
  - `[C] 여기서 멈춤 — 직접 검토`

**[A] 수정:**
- 어느 이슈부터 처리할지(권장 조정 표의 우선순위 행) 확인
- 값 변경은 **직접 하지 말고** `Task(subagent_type: "agent-design-balance", ...)` 로 위임
  - 그 에이전트가 준수: DataTable = JSON 라운드트립(`export_data_table_to_json_string` ↔ `fill_data_table_from_json_string`, 기존 필드 보존) + `save_asset(path, only_if_is_dirty=False)` 강제 저장
  - ⚠️ `EditDefaultsOnly` 구조체 값은 `set_editor_property` 가 "cannot be edited on instances" 로 막힘 → `struct.import_text("(Field=Value,...)")` 우회 (CLAUDE.md MCP 함정)
- 수정 후 관련 도메인 **재분석 제안**(새 아웃라이어가 생기지 않았는지)
- 변경한 값이 `CLAUDE.md` 또는 `PROJECT_REFERENCE` 에 기준값으로 박혀 있으면:
  > "이 값은 문서에 기준값으로 명시돼 있음 → **CLAUDE.md 와 PROJECT_REFERENCE 양쪽을 함께 갱신**하세요(이중동기화 규칙)."
- 의미 있는 밸런스 변경이면 `Guides/05_ProgressLog/TIMELINE.md` 에 항목 추가(작업 내용 / 문제점 / 해결 방법 / 결과+커밋 해시).

**[B] 저장:**
- `Guides/05_ProgressLog/balance/balance-check-[도메인]-[YYYY-MM-DD].md` 로 저장(폴더 없으면 생성). [날짜]는 오늘 날짜 YYYY-MM-DD.
- 저장 확인 후: "수정 뒤 `/balance-check` 재실행으로 검증하세요."

**[C] 멈춤:**
- 열린 이슈를 요약하고: "수정 뒤 `/balance-check` 재실행으로 검증하세요."
