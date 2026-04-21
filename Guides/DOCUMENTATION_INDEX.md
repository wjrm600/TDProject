# AOS 프로젝트 문서 색인

**위치**: `Guides/` 폴더
**엔진**: Unreal Engine 5.7 / Visual Studio 2026
**최종 업데이트**: 2026-04-21

---

## 폴더 구조

```
Guides/
├── 01_GameOverview/    게임 전체 구조 이해
├── 02_Design/         기획자 에이전트 전용 (밸런스, 레벨, Blueprint)
├── 03_Implementation/ C++ 구현 상세 (AI, 타워, 캐릭터 시스템)
├── 04_Testing/        테스트 및 설정 가이드
├── 05_ProgressLog/    작업 이력
└── 06_BalanceLog/     밸런스 변경 이력 (design-balance 에이전트 기록용)
```

---

## 01_GameOverview

### [AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md)
- 전체 게임 시스템 아키텍처, 클래스 구조, 게임 플로우
- **추천 대상**: 프로젝트 처음 접하는 모든 에이전트

### [SPAWN_SYSTEM_DIAGRAM.md](./01_GameOverview/SPAWN_SYSTEM_DIAGRAM.md)
- 맵 배치 다이어그램, 스폰 프로세스 플로우
- **추천 대상**: 시각적 이해 필요 시

---

## 02_Design — 기획자 에이전트 전용

### [GAME_BALANCE_GUIDE.md](./02_Design/GAME_BALANCE_GUIDE.md)
- **내용**: 모든 게임 수치 파라미터 + 조정 가이드
- **추천 대상**: `design-balance` 에이전트 — 반드시 먼저 읽을 것

### [LEVEL_DESIGN_GUIDE.md](./02_Design/LEVEL_DESIGN_GUIDE.md)
- **내용**: 3레인 레이아웃 원칙, MapManager 설정, 스폰 포인트 배치
- **추천 대상**: `design-level` 에이전트 — 반드시 먼저 읽을 것

### [BLUEPRINT_PROPERTIES_REFERENCE.md](./02_Design/BLUEPRINT_PROPERTIES_REFERENCE.md)
- **내용**: Blueprint에서 수정 가능한 모든 프로퍼티 목록 + C++ 재컴파일 필요 항목 구분
- **추천 대상**: `design-balance`, `design-level` 에이전트

---

## 03_Implementation

### [WAYPOINT_QUEUE_SYSTEM.md](./03_Implementation/WAYPOINT_QUEUE_SYSTEM.md)
- **내용**: AI 이동의 핵심 웨이포인트 큐 아키텍처 상세
- **추천 대상**: `prog-ai` 에이전트 — 핵심 문서

### [AI_MOVEMENT_SYSTEM_COMPLETE.md](./03_Implementation/AI_MOVEMENT_SYSTEM_COMPLETE.md)
- **내용**: AI 이동 시스템 전체 구현 (아키텍처, 실행 흐름, 설정값)
- **추천 대상**: `prog-ai` 에이전트

### [AI_MOVEMENT_IMPLEMENTATION.md](./03_Implementation/AI_MOVEMENT_IMPLEMENTATION.md)
- **내용**: AI 이동 기본 구현 (Tick 기반, 감지/공격 시스템)
- **추천 대상**: `prog-ai` 에이전트 (보조 참고)

### [AI_CONTROLLER_NOTES.md](./03_Implementation/AI_CONTROLLER_NOTES.md)
- **내용**: AIController 초기화 타이밍 이슈 3종 (BeginPlay, AutoPossess, 런타임 스폰)
- **추천 대상**: `prog-ai`, `prog-character` 에이전트

### [CHARACTER_SYSTEM.md](./03_Implementation/CHARACTER_SYSTEM.md)
- **내용**: 자동 스폰 시스템 + 위치 설정 버그 수정
- **추천 대상**: `prog-character` 에이전트

### [TOWER_SYSTEM_GUIDE.md](./03_Implementation/TOWER_SYSTEM_GUIDE.md)
- **내용**: 타워 및 커맨드센터 시스템 구현 상세
- **추천 대상**: `prog-object` 에이전트

### [TOWER_CRASH_FIX.md](./03_Implementation/TOWER_CRASH_FIX.md)
- **내용**: 타워 크래시 수정 (LoadObject vs ConstructorHelpers)
- **추천 대상**: `prog-object` 에이전트 (트러블슈팅)

---

## 04_Testing

### [QUICK_START.md](./04_Testing/QUICK_START.md)
- **내용**: 3분 타워 테스트 + 30분 전체 PIE 테스트 체크리스트
- **추천 대상**: 빠른 실행 확인 필요 시

### [PIE_TEST_GUIDE.md](./04_Testing/PIE_TEST_GUIDE.md)
- **내용**: PIE 테스트 단계별 상세 설명
- **추천 대상**: 상세 테스트 가이드 필요 시

### [SPAWN_POINT_SETUP.md](./04_Testing/SPAWN_POINT_SETUP.md)
- **내용**: 스폰 포인트 12개 설정 완전 가이드 (Team/Lane/Index/bSpawnEnabled)
- **추천 대상**: `design-level` 에이전트, 레벨 세팅 시

### [TOWER_TEST_SETUP.md](./04_Testing/TOWER_TEST_SETUP.md)
- **내용**: 타워 테스트 상세 설정 가이드

### [TOWER_SPAWNING_DEBUG.md](./04_Testing/TOWER_SPAWNING_DEBUG.md)
- **내용**: 타워 스폰 디버깅 가이드

### [NEW_MACHINE_SETUP.md](./04_Testing/NEW_MACHINE_SETUP.md)
- **내용**: 새 컴퓨터에서 프로젝트 환경 구성 (MCP 포함)

---

## 05_ProgressLog

### [PROJECT_PROGRESS_LOG.md](./05_ProgressLog/PROJECT_PROGRESS_LOG.md)
- 초기 요구사항부터 Phase 4까지 전체 진행 이력

### 날짜별 로그
- [2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md](./05_ProgressLog/2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md) — HP 바 UI, 구조물 공격, 사망 처리
- [2026-02-17_WAYPOINT_BUG_FIX.md](./05_ProgressLog/2026-02-17_WAYPOINT_BUG_FIX.md) — bSpawnEnabled, ArrivalDistance
- [2026-01-05_MAP_STRUCTURE_IMPROVEMENTS.md](./05_ProgressLog/2026-01-05_MAP_STRUCTURE_IMPROVEMENTS.md) — MapManager, UPROPERTY 크래시
- [2026-01-01_RTS_CAMERA_AND_LANE_FIX.md](./05_ProgressLog/2026-01-01_RTS_CAMERA_AND_LANE_FIX.md) — RTS 카메라
- [AUTO_SPAWN_CHANGES.md](./05_ProgressLog/AUTO_SPAWN_CHANGES.md) — 자동 스폰 설계

---

## 06_BalanceLog

밸런스 수치 변경 이력. `design-balance` 에이전트가 작성.

- [README.md](./06_BalanceLog/README.md) — 기록 형식 안내

---

## 상황별 빠른 접근

**"지금 바로 게임을 실행하고 싶다"**
→ [04_Testing/QUICK_START.md](./04_Testing/QUICK_START.md) (3분)

**"밸런스를 조정하고 싶다"**
→ [02_Design/GAME_BALANCE_GUIDE.md](./02_Design/GAME_BALANCE_GUIDE.md)

**"레벨/맵을 새로 만들고 싶다"**
→ [02_Design/LEVEL_DESIGN_GUIDE.md](./02_Design/LEVEL_DESIGN_GUIDE.md)

**"AI 이동 시스템을 수정하고 싶다"**
→ [03_Implementation/WAYPOINT_QUEUE_SYSTEM.md](./03_Implementation/WAYPOINT_QUEUE_SYSTEM.md) → [AI_MOVEMENT_SYSTEM_COMPLETE.md](./03_Implementation/AI_MOVEMENT_SYSTEM_COMPLETE.md)

**"타워 시스템을 수정하고 싶다"**
→ [03_Implementation/TOWER_SYSTEM_GUIDE.md](./03_Implementation/TOWER_SYSTEM_GUIDE.md)

**"스폰 포인트를 설정하고 싶다"**
→ [04_Testing/SPAWN_POINT_SETUP.md](./04_Testing/SPAWN_POINT_SETUP.md)

**"프로젝트 전체 흐름을 이해하고 싶다"**
→ [01_GameOverview/AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md)

---

## 문서 통계

| 카테고리 | 문서 수 | 주요 대상 |
|---------|--------|---------|
| 01_GameOverview | 2개 | 전체 |
| 02_Design | 3개 | 기획자 에이전트 |
| 03_Implementation | 7개 | 프로그래머 에이전트 |
| 04_Testing | 6개 | 전체 |
| 05_ProgressLog | 6개 | 참고용 |
| 06_BalanceLog | 1개 (README) | design-balance |
| **총계** | **25개** | |
