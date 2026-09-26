# AOS 프로젝트 문서 색인

**위치**: `Guides/` 폴더
**엔진**: Unreal Engine 5.7 / Visual Studio 2026
**최종 업데이트**: 2026-07-23 (UI 다크·프리미엄 리디자인 — ART_DIRECTION + UI_Specs 신설)

---

## 폴더 구조

```
Guides/
├── 01_GameOverview/    게임 전체 구조 이해
├── 02_Design/         기획자 에이전트 전용 (아트 디렉션, UI 스펙, 밸런스, 레벨, Blueprint)
│   └── UI_Specs/      위젯별 스펙 (DS 인지형, /ui-spec 형식)
├── 03_Implementation/ C++ 구현 상세 (AI, 타워, 캐릭터, UI 텍스처)
├── 04_Testing/        테스트 및 설정 가이드
├── 05_ProgressLog/    작업 이력
└── 06_BalanceLog/     밸런스 변경 이력 (design-balance 에이전트 기록용)
```

---

## 01_GameOverview

### [GAME_VISION.md](./01_GameOverview/GAME_VISION.md) ⭐ "무엇을 만드는가"
- **내용**: 게임 정체성(오토배틀러 MOBA), 코어 루프, 라운드 모델(맵 누적), 디자인 기둥, 임계 경로 로드맵(Slice 0~4)
- **추천 대상**: 신규 합류자·에이전트 1순위. 기능/우선순위 판단 전 반드시 읽을 것

### [AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md)
- 전체 게임 시스템 아키텍처, 클래스 구조, 게임 플로우 ("어떻게")
- **추천 대상**: 프로젝트 처음 접하는 모든 에이전트

### [SPAWN_SYSTEM_DIAGRAM.md](./01_GameOverview/SPAWN_SYSTEM_DIAGRAM.md)
- 맵 배치 다이어그램, 스폰 프로세스 플로우
- **추천 대상**: 시각적 이해 필요 시

---

## 02_Design — 기획자 에이전트 전용

### [ART_DIRECTION.md](./02_Design/ART_DIRECTION.md) ⭐ UI 디자인 시스템
- **내용**: 다크·프리미엄 v2 UI 아트 디렉션 — 비주얼 톤("전술 지휘 콘솔"), 팔레트 토큰 표(hex·역할), 타입/간격/깊이/라운드 스케일, 컴포넌트 스펙(패널/버튼/칩/카드/코너 브래킷), 액센트 역할 분리(Gold=경제·Info=시간·RedCTA=진행·팩션·상태), Do/Don't. **토큰 SSOT 는 `Source/.../AOSUIStyle.h`** — 이 문서는 그 해설/의도
- **추천 대상**: `design-*`·`art-*`·`prog-ui` 에이전트, 모든 위젯 저작자

### UI_Specs/ — 위젯 스펙 (DS 인지형, `/ui-spec` 형식)
- [UI_Specs/CharacterSelect.md](./02_Design/UI_Specs/CharacterSelect.md) — 라운드 준비/배치 위젯(`WBP_CharacterSelect`/`UAOSCharacterSelectWidget`): 데이터 명세·BindWidget 계약·실현 순서·수용 기준 + 현재 코드 drift
- [UI_Specs/Shop.md](./02_Design/UI_Specs/Shop.md) — 상점 팝업(`WBP_Shop`/`UAOSShopWidget`): 2뷰 흐름·구매 서버 권위·토큰 솔리드 크롬(장식 텍스처 제거)·BindWidget 계약 + drift
- **추천 대상**: `prog-ui`(구현), `design-*`(설계), 위젯 마이그레이션 작업자

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

### [UPPER_LOWER_BODY_SPLIT.md](./03_Implementation/UPPER_LOWER_BODY_SPLIT.md)
- **내용**: 스킬 상하체 분리 (Layered Blend Per Bone) + 시전 중 이동 플래그(`bAllowMovementDuringCast`) — Part A(C++ 완료) + Part B(ABP 수동 배선 가이드)
- **추천 대상**: `prog-anim`, `art-anim` 에이전트, ABP 작업 시

### [SKILL_AUTHORING_GUIDE.md](./03_Implementation/SKILL_AUTHORING_GUIDE.md)
- **내용**: `UGA_SkillBase` 데이터 주도식 패턴으로 **새 스킬 1개를 5~10분 안에 추가**하는 절차 — BP 자산 2개 + 태그 2개 + 몽타주 매핑. 패턴별 UPROPERTY 치트시트(Self/SingleEnemy/AoE_Sphere/Periodic), 특이 데미지식 BP override, 흔한 함정 8가지
- **추천 대상**: `design-balance` 에이전트, 새 캐릭터 스킬 작업자, `prog-character` 에이전트

### [KWANG_AI_STATETREE.md](./03_Implementation/KWANG_AI_STATETREE.md)
- **내용**: 🧪 **캐릭터별 AI 개성 첫 사례** — Kwang 전용 `ST_KwangAI`(선호 행동 3종 + 스킬 규칙). 요구사항 원문→해석 표, 트리 구조·우선순위 근거, `AIStateTreeOverride` 연결 방식, InstanceData 파라미터 노드 9종, 실측 발견(공격 몽타주>쿨다운 → 틈 0 등), 테스트 체크리스트·튜닝 표·결과 기록란
- **추천 대상**: `prog-ai` 에이전트, 캐릭터별 AI 작업자, 플레이테스트 시

### [ANIMATION_REQUEST_TEMPLATE.md](./03_Implementation/ANIMATION_REQUEST_TEMPLATE.md)
- **내용**: 사용자가 원하는 모션(스킬 Q/W/E·Idle·Move·Death)을 **자유 서술로 적어 채워 넣는 기획 템플릿**. 하이브리드 애니 파이프라인(Blender 베이크 → UE 재임포트 → 몽타주)의 입력값(타이밍 비트·임팩트 순간·도약 여부)을 뽑아냄. R 슬램 완료작이 채워진 예시
- **추천 대상**: 사용자(모션 발주), `art-anim`·`prog-anim` 에이전트

### [UI_TEXTURE_KIT.md](./03_Implementation/UI_TEXTURE_KIT.md)
- **내용**: AI 생성 UI 텍스처 파이프라인 — **v2 개정: AI=콘텐츠(초상화/아이콘) 전용, 크롬(패널/버튼/프레임)=토큰 솔리드**. 파일럿 상점 장식 텍스처(`T_UI_Shop_*`) 제거 방침, 배선 계약(`KitBrush`=콘텐츠 / `SolidBrush`=크롬), 크롬→솔리드 마이그레이션 백로그
- **추천 대상**: `art-vfx`·`prog-ui` 에이전트, UI 리디자인 트랙. 방향 상세는 `02_Design/ART_DIRECTION.md`

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

### [TROUBLESHOOTING_BSOD_cbfltfs4.md](./04_Testing/TROUBLESHOOTING_BSOD_cbfltfs4.md)
- **내용**: UE 에디터 + Claude Desktop 동시 실행 시 강제 재부팅(BSOD 0x50) 진단/해결 — 고아 DRM 드라이버 `cbfltfs4.sys`(MarkAny ePageSafer) 원인
- **추천 대상**: 데브머신 강제 재부팅/BSOD 발생 시

---

## 05_ProgressLog

### [TIMELINE.md](./05_ProgressLog/TIMELINE.md) ⭐ 발표/회고 자료
- **내용**: 시간 순 작업 이력 (작업 내용 · 문제점 · 해결 · 결과). 자동 갱신 규칙은 CLAUDE.md "작업 타임라인 자동 갱신" 섹션
- **추천 대상**: 발표/회고 자료 작성 시 1순위, 모든 에이전트 — 의미 작업 완료 시 갱신 필수

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

**"이 게임이 뭘 만드는 건지 / 다음 우선순위가 뭔지 알고 싶다"**
→ [01_GameOverview/GAME_VISION.md](./01_GameOverview/GAME_VISION.md) ⭐

**"프로젝트 전체 흐름을 이해하고 싶다"**
→ [01_GameOverview/AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md)

**"새 스킬을 추가하고 싶다"**
→ [03_Implementation/SKILL_AUTHORING_GUIDE.md](./03_Implementation/SKILL_AUTHORING_GUIDE.md) (BP 자산만으로 5~10분)

**"UI 를 디자인하거나 위젯을 만들고 싶다"**
→ [02_Design/ART_DIRECTION.md](./02_Design/ART_DIRECTION.md) (디자인 시스템) → [02_Design/UI_Specs/](./02_Design/UI_Specs/) (위젯 스펙) → [03_Implementation/UI_TEXTURE_KIT.md](./03_Implementation/UI_TEXTURE_KIT.md) (콘텐츠 텍스처)

---

## 문서 통계

| 카테고리 | 문서 수 | 주요 대상 |
|---------|--------|---------|
| 01_GameOverview | 3개 | 전체 (GAME_VISION 1순위) |
| 02_Design | 4개 + UI_Specs 2개 | 기획자 에이전트 (ART_DIRECTION ⭐) |
| 03_Implementation | 11개 | 프로그래머 에이전트 |
| 04_Testing | 7개 | 전체 |
| 05_ProgressLog | 7개 | 발표/회고용 (TIMELINE 1순위) |
| 06_BalanceLog | 1개 (README) | design-balance |
| **총계** | **35개** | |
