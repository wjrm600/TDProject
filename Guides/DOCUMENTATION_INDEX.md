# AOS 프로젝트 문서 색인

**위치**: `Guides/` 폴더 (카테고리별로 정리됨)
**엔진**: Unreal Engine 5.7 / Visual Studio 2026

프로젝트 진행 과정에서 생성된 모든 문서의 색인입니다.

---

## 폴더 구조

```
Guides/
├── 01_GameOverview/        (게임 개요)
├── 02_ProgressLog/         (작업 진행 사항)
├── 03_ClassReview/         (클래스 리뷰)
├── 04_Implementation/      (핵심 시스템 구현 문서)
└── 04_UsageGuide/          (사용 가이드)
```

---

## 01_GameOverview (게임 개요)

게임의 전체 구조와 시각적 이해를 돕는 문서들입니다.

### [AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md)
- **내용**: 전체 게임 시스템 아키텍처
- **범위**: 고수준 설계 (클래스 구조, 게임 플로우, 상태 다이어그램)
- **추천 대상**: 시스템 설계 이해 필요

### [SPAWN_SYSTEM_DIAGRAM.md](./01_GameOverview/SPAWN_SYSTEM_DIAGRAM.md)
- **내용**: 시각적 다이어그램 및 예시
- **범위**: 맵 배치 다이어그램, 스폰 프로세스 플로우, 배치 예시
- **추천 대상**: 시각적 학습 선호

---

## 02_ProgressLog (작업 진행 사항)

프로젝트의 진행 과정과 변경 이력을 정리한 문서들입니다.

### [PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md)
- **내용**: 초기 요구사항부터 Phase 4까지의 진행 과정
- **분량**: 긴 문서 (매우 상세함)
- **추천 대상**: 프로젝트 전체 흐름 이해 필요

### [AUTO_SPAWN_CHANGES.md](./02_ProgressLog/AUTO_SPAWN_CHANGES.md)
- **내용**: 자동 스폰 시스템 변경사항 계획서 (Approach A)
- **범위**: 초기 설계 의도 이해

### [2026-01-01_RTS_CAMERA_AND_LANE_FIX.md](./02_ProgressLog/2026-01-01_RTS_CAMERA_AND_LANE_FIX.md)
- **내용**: RTS 카메라 시스템 완성 및 라인 이동 버그 수정
- **범위**: 카메라 4방향 이동, 줌, DefaultPawnClass 분리, 라인 분산 수정

### [2026-01-05_MAP_STRUCTURE_IMPROVEMENTS.md](./02_ProgressLog/2026-01-05_MAP_STRUCTURE_IMPROVEMENTS.md)
- **내용**: 맵 구조 개선, 웨이포인트 큐 구현, 메모리/충돌 수정
- **범위**: MapManager 리팩터링, 충돌 비활성화, UPROPERTY 크래시 수정

### [2026-02-17_WAYPOINT_BUG_FIX.md](./02_ProgressLog/2026-02-17_WAYPOINT_BUG_FIX.md)
- **내용**: 웨이포인트 시스템 버그 수정 및 디버깅
- **범위**: bSpawnEnabled 추가, 무한루프 수정, ArrivalDistance 조정, FLaneInfo 초기화

### [2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md](./02_ProgressLog/2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md)
- **내용**: 사망/파괴 처리, 구조물 공격, HP 바 UI 구현
- **범위**: OnCharacterDeath, OnStructureDestroyed, AttackStructure, AOSHealthBarWidget, 구조물 스탯 조정

---

## 03_ClassReview (클래스 리뷰)

각 Phase별 기술 구현과 클래스 상세 설명입니다.

### [AI_MOVEMENT_IMPLEMENTATION.md](./03_ClassReview/AI_MOVEMENT_IMPLEMENTATION.md)
- **내용**: AI 움직임 기본 구현 가이드
- **범위**: Tick 기반 AI, 감지/공격 시스템

### [AUTO_SPAWN_IMPLEMENTATION.md](./03_ClassReview/AUTO_SPAWN_IMPLEMENTATION.md)
- **내용**: 자동 캐릭터 생성 시스템 구현 (Phase 3)

### [POSITION_FIX.md](./03_ClassReview/POSITION_FIX.md)
- **내용**: 캐릭터 위치 설정 중복 문제 해결 (Phase 4)

### [TOWER_SYSTEM_GUIDE.md](./03_ClassReview/TOWER_SYSTEM_GUIDE.md)
- **내용**: 타워 및 커맨드 센터 시스템 구현

### [TOWER_CRASH_FIX.md](./03_ClassReview/TOWER_CRASH_FIX.md)
- **내용**: 타워 크래시 수정 (LoadObject vs ConstructorHelpers)

### [CODE_CHANGES_REFERENCE.md](./03_ClassReview/CODE_CHANGES_REFERENCE.md)
- **내용**: 초기 코드 변경사항 참고서 (Phase 3 기준, 이후 웨이포인트 큐로 대체됨)

### [AICONTROLLER_BEGINPLAY_TIMING.md](./03_ClassReview/AICONTROLLER_BEGINPLAY_TIMING.md)
- **내용**: AI 컨트롤러 BeginPlay 타이밍 이슈

### [AICONTROLLER_POSSESSION_FIX.md](./03_ClassReview/AICONTROLLER_POSSESSION_FIX.md)
- **내용**: AutoPossessAI 문제 해결

### [RUNTIME_SPAWN_AICONTROLLER_FIX.md](./03_ClassReview/RUNTIME_SPAWN_AICONTROLLER_FIX.md)
- **내용**: 런타임 스폰 시 AIController 할당 문제 해결

---

## 04_Implementation (핵심 시스템 구현 문서)

핵심 시스템의 상세 구현 문서입니다.

### [WAYPOINT_QUEUE_SYSTEM.md](./04_Implementation/WAYPOINT_QUEUE_SYSTEM.md)
- **내용**: 웨이포인트 큐 시스템 상세 구현
- **범위**: 순차적 라인 푸시 로직, 코드 스니펫, 동작 예시
- **추천 대상**: AI 이동 시스템의 핵심 아키텍처 이해 필요

### [AI_MOVEMENT_SYSTEM_COMPLETE.md](./04_Implementation/AI_MOVEMENT_SYSTEM_COMPLETE.md)
- **내용**: AI 움직임 시스템 전체 구현 완료 문서
- **범위**: 아키텍처, 실행 흐름, 의사결정, 설정값

---

## 04_UsageGuide (사용 가이드)

게임 실행, 테스트, 설정 방법을 정리한 실용 가이드들입니다.

### [QUICK_TOWER_TEST.md](./04_UsageGuide/QUICK_TOWER_TEST.md)
- **내용**: 타워 빠른 테스트 (3분)
- **추천 대상**: 지금 바로 타워를 보고 싶을 때

### [PIE_QUICK_CHECKLIST.md](./04_UsageGuide/PIE_QUICK_CHECKLIST.md)
- **내용**: 30분 내 PIE 테스트 체크리스트

### [README_PIE_START.md](./04_UsageGuide/README_PIE_START.md)
- **내용**: PIE 테스트 시작 가이드 (종합)

### [PIE_TEST_GUIDE.md](./04_UsageGuide/PIE_TEST_GUIDE.md)
- **내용**: PIE 테스트 단계별 상세 설명

### [UPDATED_SPAWN_SETUP.md](./04_UsageGuide/UPDATED_SPAWN_SETUP.md)
- **내용**: 스폰 포인트 상세 설정 가이드 (Team/Lane/Index/bSpawnEnabled)

### [SPAWN_POINT_SETUP.md](./04_UsageGuide/SPAWN_POINT_SETUP.md)
- **내용**: 스폰 포인트 기본 설정

### [TOWER_TEST_SETUP.md](./04_UsageGuide/TOWER_TEST_SETUP.md)
- **내용**: 타워 테스트 상세 설정 가이드

### [TOWER_SPAWNING_DEBUG.md](./04_UsageGuide/TOWER_SPAWNING_DEBUG.md)
- **내용**: 타워 스포닝 디버깅 가이드

---

## 빠른 접근 가이드

### 상황별 추천 문서

**"지금 바로 게임을 실행하고 싶다"**
1. [04_UsageGuide/QUICK_TOWER_TEST.md](./04_UsageGuide/QUICK_TOWER_TEST.md) (3분)
2. [04_UsageGuide/PIE_QUICK_CHECKLIST.md](./04_UsageGuide/PIE_QUICK_CHECKLIST.md) (5분)

**"프로젝트 전체를 이해하고 싶다"**
1. [01_GameOverview/AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md) (아키텍처)
2. [04_Implementation/WAYPOINT_QUEUE_SYSTEM.md](./04_Implementation/WAYPOINT_QUEUE_SYSTEM.md) (핵심 AI 시스템)
3. [02_ProgressLog/PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md) (전체 이력)

**"AI 이동 시스템을 이해하고 싶다"**
1. [04_Implementation/WAYPOINT_QUEUE_SYSTEM.md](./04_Implementation/WAYPOINT_QUEUE_SYSTEM.md) (핵심)
2. [04_Implementation/AI_MOVEMENT_SYSTEM_COMPLETE.md](./04_Implementation/AI_MOVEMENT_SYSTEM_COMPLETE.md) (상세)

**"최근 변경사항을 확인하고 싶다"**
1. [02_ProgressLog/2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md](./02_ProgressLog/2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md) (최신)
2. [02_ProgressLog/2026-02-17_WAYPOINT_BUG_FIX.md](./02_ProgressLog/2026-02-17_WAYPOINT_BUG_FIX.md)

---

## 문서 통계

| 카테고리 | 문서 수 | 주요 목적 |
|---------|--------|---------|
| 01_GameOverview | 2개 | 게임 전체 구조 이해 |
| 02_ProgressLog | 6개 | 작업 진행 과정 추적 |
| 03_ClassReview | 9개 | 기술 구현 상세 검토 |
| 04_Implementation | 2개 | 핵심 시스템 구현 문서 |
| 04_UsageGuide | 8개 | 실제 사용 방법 가이드 |
| **총계** | **27개** | |

---

## 핵심 문서 3개

시간이 부족하다면 이 3개만 읽으세요:

1. **[04_Implementation/WAYPOINT_QUEUE_SYSTEM.md](./04_Implementation/WAYPOINT_QUEUE_SYSTEM.md)** - AI 이동의 핵심 로직
2. **[04_UsageGuide/QUICK_TOWER_TEST.md](./04_UsageGuide/QUICK_TOWER_TEST.md)** - 게임 즉시 실행
3. **[02_ProgressLog/2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md](./02_ProgressLog/2026-02-18_COMBAT_AND_UI_IMPROVEMENTS.md)** - 최신 수정사항

---

**최종 업데이트**: 2026-02-18
**총 문서 수**: 27개
