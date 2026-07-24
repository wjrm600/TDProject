# UI 스펙 — CharacterSelect (라운드 준비 / 배치)

**위젯**: `WBP_CharacterSelect` · **Parent C++**: `UAOSCharacterSelectWidget`
**진입/이탈**: `EAOSGameState::RoundPreparation` 진입 시 로컬 PC 가 표시, `RoundRunning` 진입 시 숨김.
**스타일 계약**: 다크·프리미엄 v2, 색/폰트/메트릭은 전부 `AOSUIStyle` 토큰. 상세 → [`Guides/02_Design/ART_DIRECTION.md`](../ART_DIRECTION.md).

> **DS(Dedicated Server) 인지형**: 상태의 진실은 서버(GameMode). 클라는 오직 `GetGameState<AAOSGameState>()` 경유로 읽고, 생성/카메라는 `IsLocalPlayerController()` 가드 안에서만. `GetAuthGameMode()` 는 클라에서 null.

---

## A. 목적 & 진입/이탈

관전형 오토배틀러에서 플레이어가 **실제로 조작하는 핵심 화면**. 매 라운드 준비 단계에 뜨며, 픽된 로스터를 3레인×2슬롯에 드래그 배치하고, 상점을 열어 유닛 아이템을 사고, "라운드 준비"로 확정한다.

| 시점 | 동작 | 권한/가드 |
|------|------|-----------|
| 서버가 `EAOSGameState::RoundPreparation` 전이 | GameState `OnRep_CurrentGameState` → 로컬 PC 가 `ShowCharacterSelect` | 표시는 `IsLocalPlayerController()` 안 |
| 픽 필터 | 벤픽에서 **픽된 캐릭터만** 로스터로 주입(`InitializeWithRoster`) | 로스터 자체는 서버가 결정 |
| 서버가 `RoundRunning` 전이 | 위젯 Hide(전투는 관전) | — |

- 첫 라운드가 아니면 상단에 지난 라운드 결과(`RoundResultText`) 요약. 첫 라운드엔 숨김.

---

## B. 레이아웃 존

3존 구성 (헤더 / 콘텐츠 / 액션바):

```
┌─ 헤더 ────────────────────────────────────────────────┐
│ TitleText(Display)   RoundResultText   [타이머칩 Info] [골드칩 Gold] │
├─ 콘텐츠 ──────────────────────────────────────────────┤
│  ┌ 레인 존(좌) ──────────┐   ┌ 로스터 존(우) ─────────┐ │
│  │ TopLaneBox  [슬롯][슬롯] │   │ CardGrid (WrapBox)      │ │
│  │ MidLaneBox  [슬롯][슬롯] │   │  [카드][카드][카드]…    │ │
│  │ BotLaneBox  [슬롯][슬롯] │   │  (보유 로스터 / 벤치)   │ │
│  └───────────────────────┘   └─────────────────────────┘ │
├─ 액션바 ──────────────────────────────────────────────┤
│ DeployCountText(N/5)  TeamReadyStatusText   [상점 Gold-ghost] [라운드 준비 RedCTA] │
└───────────────────────────────────────────────────────┘
```

- 헤더 칩: 타이머 = Info(파랑), 골드 = Gold. 코너 브래킷은 레인 존/액티브 슬롯에.
- 상점은 이 위젯 위에 **자식 오버레이**(`WBP_Shop`)로 열림 — 새 화면 전환 아님.

---

## C. 상태 & 변형

| 상태 | 조건 | 표시 |
|------|------|------|
| **loading** | 로스터 복제 대기 / `InitializeWithRoster` 이전 | 카드 그리드 스켈레톤, 액션 비활성 |
| **empty** | 픽 결과가 비었거나(예외) 배치 가능 유닛 0 | "배치할 유닛 없음" 안내, 라운드 준비만 활성 |
| **populated** | 로스터 주입 완료 | 카드/슬롯 정상, 배치·상점·준비 활성 |

- 팀 변형: 슬롯/카드 림 = `TeamAccent(bTeam1)`, 준비 상태 = 팩션색.
- 배치 완료 카운트(`DeployCountText`) = `Success`(N/5 도달 시 강조).
- 초상화 없는 유닛 = `PlaceholderColor(RosterIndex)`.

---

## D. ⭐ 데이터 명세 (DS 인지)

| 데이터 요소 | 출처 시스템 | GameState 복제? | OnRep_ 필요? | 클라 접근 | 갱신 빈도 |
|-------------|------------|:---------------:|:------------:|-----------|-----------|
| 라운드 번호(`TitleText`) | GameMode → GameState `CurrentRound` | ✅ | ✅ | `GetGameState()->GetCurrentRound()` / `SetRoundNumber` | 라운드 전이 시 |
| 남은 준비 시간(`TimerText`) | 서버 준비 타이머 | 서버 권위(잔여초 push) | — | PC Tick → `UpdatePreparationTimer(sec)` | 매 프레임(로컬 보간) |
| 팀 골드(`TeamGoldText`) | GameState `Team1/2Gold` | ✅ | ✅ `OnRep`/델리게이트 | `GetGameState()` 경유 로컬팀 골드 | 골드 변동 시 |
| 지난 라운드 결과(`RoundResultText`) | GameState 직전 라운드 결과 | ✅ | ✅ | `UpdateRoundResult()`(GameState 조회) | 라운드 전이 시 |
| 로스터(픽 결과)(`CardGrid`) | GameMode 벤픽 결과 → PC 주입 | 서버 결정 | — | `InitializeWithRoster(Roster)` | 준비 진입 1회 |
| 양 팀 준비 상태(`TeamReadyStatusText`) | GameState `TeamReady` | ✅ | ✅ `OnRep_TeamReady` | PC 콜백 → `UpdateTeamReadyStatus(b1,b2)` | 준비 확정 시 |
| 배치 카운트(`DeployCountText`) | 로컬 슬롯 배정 상태(위젯 로컬) | — | — | `GetTotalAssignedCount()` | 드롭/클리어 시 |

- **규칙**: 클라는 `GetGameState<AAOSGameState>()` 경유만. `GetAuthGameMode()` 금지.
- **초기 복제 함정**: CDO 기본값과 같은 프로퍼티는 초기 전송 안 됨 → 늦게 접속하거나 상태 전이 직후에도 라운드/골드/준비상태가 채워지는지 검증(수용 기준 참고).
- 데이터 소유는 GameState/GameMode. 위젯은 표시·로컬 배치 상태만 소유.

---

## E. 상호작용 맵 (클라 → 서버)

| 요소 | 액션 | 즉시 피드백 | 서버 RPC 진입점 |
|------|------|-------------|-----------------|
| 로스터 카드 → 레인 슬롯 | 드래그앤드롭 | 슬롯 채움 + 카운트 갱신 | (확정 시점에 일괄) |
| 슬롯 Clear 버튼 | 클릭 | 슬롯 비움 + 카운트 감소 | — |
| `ShopButton` | 클릭 | `WBP_Shop` 오버레이 열림 | (구매는 Shop 스펙 참고) |
| `StartRoundButton` | 클릭 | 버튼 라벨 "준비 완료 ✓" + 비활성(재클릭 방지) | 레인 배정 → `ServerSetLaneDeployClassesForPlayer`(서버 강제 필터: 픽된 유닛만) |

- 클라는 요청만. 배치 유효성/픽 필터/골드 차감은 **서버 권위**.
- `bLocalPressedReady` 로 로컬 재클릭 차단, 새 라운드 진입 시 `ResetReadyState()` 로 복원.

---

## F. 스타일 계약

- 색/폰트/메트릭 = **전부 `AOSUIStyle` 토큰**. 위젯 .cpp/WBP 에 인라인 색 리터럴 금지.
- 크롬(패널/버튼/슬롯/구획선/코너 브래킷) = **토큰 솔리드**(`SolidBrush`/`SolidButtonStyle`). AI 장식 텍스처 금지.
- 액센트 역할: 타이머=`Info`, 골드=`Gold`, 라운드 준비 버튼=`RedCTA`, 상점 열기=`Gold` ghost, 팀 림=`TeamAccent`, 배치완료=`Success`.
- 수치(타이머/골드/카운트)=monospace tabular-nums. 텍스트는 텍스처에 굽지 않음(전부 TextBlock).
- 콘텐츠 이미지(초상화)만 `KitBrush` 허용, 없으면 `PlaceholderColor` 폴백 → 텍스처 부재에도 PIE 정상.

---

## G. BindWidget 계약 & 실현 순서

**정적(WBP 이관, `meta = (BindWidgetOptional)` — C++ 폴백 유지):**

| 프로퍼티 | 타입 | 역할 | 액센트 |
|----------|------|------|--------|
| `TitleText` | `UTextBlock` | 라운드 대제목 | — |
| `TimerText` | `UTextBlock` | 남은 준비 시간 | Info |
| `RoundResultText` | `UTextBlock` | 지난 라운드 결과(첫 라운드 숨김) | — |
| `TeamGoldText` | `UTextBlock` | 로컬 팀 골드 | Gold |
| `DeployCountText` | `UTextBlock` | 총 배치 N/5 | Success |
| `CardGrid` | `UWrapBox` | 보유 로스터/벤치 카드 컨테이너 | — |
| `TopLaneBox` | `UPanelWidget`(HorizontalBox 등) | Top 레인 슬롯 2개 컨테이너 | 팩션 |
| `MidLaneBox` | `UPanelWidget` | Mid 레인 슬롯 2개 컨테이너 | 팩션 |
| `BotLaneBox` | `UPanelWidget` | Bottom 레인 슬롯 2개 컨테이너 | 팩션 |
| `ShopButton` | `UButton` | 상점 열기 | Gold ghost |
| `StartRoundButton` | `UButton` | 라운드 준비 확정 | RedCTA |
| `TeamReadyStatusText` | `UTextBlock` | 양 팀 준비 상태 | 팩션/Success |

**동적(C++ 유지 — WBP 이관 안 함):**
- 로스터 카드: `InitializeWithRoster()` → `UAOSCharacterCardWidget` 생성해 `CardGrid` 에 채움.
- 레인 슬롯: 각 `*LaneBox` 에 `UAOSLaneSlotWidget` 2개씩 생성.
- 드래그앤드롭: 카드(소스) ↔ 슬롯(드롭 타겟), `UAOSCharacterDragDropOperation`.

**실현 순서** (빈 위젯 방지 — 미니맵/벤픽 교훈):
1. `CreateWidget<UAOSCharacterSelectWidget>` (`IsLocalPlayerController()` 가드 안)
2. `InitializeWithRoster(Roster)` — 카드/슬롯 트리 구축(BindWidget 컨테이너에 채움)
3. **그 다음** `AddToViewport(ZOrder)`
4. Show/Hide 는 GameState 전이 반응(`RoundPreparation`↔`RoundRunning`)

---

## H. 수용 기준 (테스트 가능)

- [ ] **DS 2-Client 검증**: Play As Dedicated Server 또는 Listen Server 2-Client 에서 서버·양 클라 모두 준비 화면 정상 표시(단일 프로세스 아님 — `Guides/04_Testing/PIE_TEST_GUIDE.md`).
- [ ] **초기 복제 타이밍**: 라운드 전이 직후·늦은 접속에도 라운드 번호/골드/준비상태/로스터가 채워짐(CDO 기본값 함정 확인).
- [ ] **드래그 배치 → 서버 반영**: 카드를 레인 슬롯에 드롭 → "라운드 준비" → `ServerSetLaneDeployClassesForPlayer` 로 서버 강제, RoundRunning 에서 해당 유닛만 스폰.
- [ ] **empty 상태**: 픽 결과 없음/배치 유닛 0 상태에서 안내 표시 + 크래시 없음.
- [ ] **텍스처 부재 폴백**: 초상화 텍스처 없어도 `PlaceholderColor` 로 정상 렌더.
- [ ] **스타일 규율**: 인라인 색 없음, 액센트 역할(Info/Gold/RedCTA/팩션) 준수, 크롬 솔리드.

---

## ⚠️ 현재 코드와의 Drift (구현 트랙 참고)

`UAOSCharacterSelectWidget`(현재 헤더)는 서브트리를 **전량 프로그래밍 방식 `BuildUI()`** 로 생성하며, 멤버가 `meta=(BindWidget)` 이 아니라 **plain `UPROPERTY()`** 다. 이 스펙의 목표 계약과 차이:
- `TotalCountText`(현재) → **`DeployCountText`**(목표 이름).
- **미존재 → 신규**: `TeamGoldText`, `TopLaneBox`/`MidLaneBox`/`BotLaneBox`(현재는 레인 슬롯을 `LaneSlotWidgets` TArray 로 프로그래밍 생성, 레인별 컨테이너 없음).
- 나머지(`TitleText`/`TimerText`/`RoundResultText`/`CardGrid`/`ShopButton`/`StartRoundButton`/`TeamReadyStatusText`)는 이름 일치.

C++ 마이그레이션 트랙이 위 이름으로 `BindWidgetOptional` 하이브리드로 전환(WBP 계층 편집 + C++ 폴백)한다. 이 스펙은 그 목표 계약을 규정한다.
