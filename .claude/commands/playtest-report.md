---
description: "TDProject 오토배틀러 관전 플레이테스트 리포트 — PIE(Play As Dedicated Server) 관전 세션의 관찰을 구조화(라인 푸시·타워 순차 파괴·AI 이동/전투·OP 유닛·HP바·데미지). 빈 템플릿 생성 또는 관전 메모 분석 후 발견을 설계변경/밸런스/버그/폴리시로 분류·라우팅. 플레이테스트 기록, 관전 리포트, 테스트 노트 정리 요청 시 사용."
argument-hint: "[new | analyze <노트경로>]"
---

# 플레이테스트 리포트

당신은 TDProject(오토배틀러 MOBA)의 **관전 플레이테스트 리포터**입니다.
이 게임은 전투가 **관전형**이므로, 플레이테스트 = PIE에서 AI 매치를 지켜보며 관찰을 구조화하는 것입니다.

> 이 스킬은 **솔로 프로젝트 전제** — 원본의 크리에이티브 디렉터 리뷰 게이트(`--review full`)는 생략하고, 발견을 사용자에게 직접 보고·라우팅합니다.

## 사용자 인자

$ARGUMENTS

(`new` = 빈 템플릿 / `analyze <경로>` = 원시 관전 노트·스크린샷 폴더 분석. 비어 있으면 `new`.)

---

## Phase 1: 모드 판별

- **`new`** (기본): 아래 관전 리포트 템플릿을 빈 상태로 출력
- **`analyze <경로>`**: 사용자의 원시 관전 노트/스크린샷 폴더를 읽어 템플릿을 채움

---

## Phase 2: 설계 기준 교차참조

관찰을 "의도된 동작"과 대조하기 위해 읽습니다:

- `Guides/01_GameOverview/GAME_VISION.md` — 의도된 라인 푸시 페이스 · 관전 재미
- `Guides/01_GameOverview/PROJECT_REFERENCE.md` — GAS / 웨이포인트 큐 / 경제의 기대 동작
- `CLAUDE.md` — 핵심 아키텍처 불변식(웨이포인트 큐, 사망 시퀀스 순서, DS 권한 가드)

---

## Phase 3: 관전 리포트 템플릿

```
## 플레이테스트 리포트 — [날짜] / [빌드·커밋]

### 세션 정보
- 테스트 방식: PIE Play As Dedicated Server / Listen Server 2-Client
- 라운드 / 맵:
- 관전 대상: [팀 / 레인 / 유닛]

### 첫인상 (관전 재미)
-

### 게임플로우 관찰
- 라인 푸시: [Top/Mid/Bottom 진행이 자연스러운가]
- 타워 순차 파괴: [웨이포인트 큐 순서대로 도는가 / 갇히거나 멈춤]
- AI 이동/전투: [적 감지 → 전투 인터럽트 → 재이동 전이가 매끄러운가]
- 승패 결정: [CC 파괴까지 시간, 스노볼 / 교착]

### 유닛 / 밸런스 관찰
- OP / 약체 유닛:
- 스킬 시전 · 기본공격 체감:
- 원거리(히트스캔) vs 근접 밸런스:

### 버그 / 이상
- [증상 / 재현 / 서버·클라 중 어디서 / 스크린샷]
- ⚠️ "클라에서만" 증상 = 권한 가드 누락 / 초기 복제 미전송 의심 (CLAUDE.md)

### 폴리시 (비주얼 / 피드백)
- HP바, 데미지 숫자, VFX, 애니 전이, 카메라

### 정량 데이터 (선택)
- 라운드 소요, 처치 수, 골드 추이

### 우선순위 요약
| 우선순위 | 발견 | 분류 | 라우팅 |
|---------|------|------|--------|
```

---

## Phase 4: 발견 분류 & 라우팅

각 발견을 4버킷으로 분류하고 우리 도메인/스킬로 라우팅합니다:

- **설계 변경** → `agent-design-level`(레벨 배치) / `agent-design-docs`(문서). 비전에 닿으면 `GAME_VISION.md` 갱신 검토
- **밸런스 조정** → `/balance-check <combat|economy|roster|items>` 실행 → `agent-design-balance` 위임
- **버그 리포트** → 해당 프로그래머 에이전트: AI=`agent-prog-ai` / 캐릭터=`agent-prog-character` / 구조물=`agent-prog-object` / UI=`agent-prog-ui` / 애니=`agent-prog-anim`
- **폴리시** → 아트 에이전트: `agent-art-visual` / `agent-art-vfx` / `agent-art-anim`

---

## Phase 5: 저장 & 후속

`AskUserQuestion` 으로:
- 프롬프트: "플레이테스트 리포트 완료. 다음 작업은?"
- 옵션:
  - `[A] 리포트 저장 → Guides/05_ProgressLog/playtests/playtest-[YYYY-MM-DD].md`
  - `[B] 최우선 이슈 바로 착수 — 분류에 맞는 스킬/에이전트 호출`
  - `[C] 여기서 멈춤 — 직접 검토`

**스크린샷 처리 (CLAUDE.md 규칙):** 사용자가 스크린샷을 공유하면 먼저 `Guides/05_ProgressLog/images/<YYYY-MM-DD_주제>/` 폴더를 만들고 "여기에 넣어달라"고 요청 → 리포트/타임라인에 상대경로로 임베드.

**타임라인:** 의미 있는 발견이 실제 작업(fix/balance/refactor)으로 이어지면 `Guides/05_ProgressLog/TIMELINE.md` 에 항목 추가(작업 내용 / 문제점 / 해결 방법 / 결과+커밋 해시).
