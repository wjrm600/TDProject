# ART_DIRECTION — UI 아트 디렉션 & 디자인 시스템 (v2 다크·프리미엄)

**대상**: `design-*`(기획), `art-*`(아트), `prog-ui`(구현) 에이전트 + 위젯 저작자
**상태**: 방향 확정(사용자 승인, 2026-07-23). 다크 단일 테마로 커밋.
**SSOT(Single Source of Truth)**: 색/폰트/메트릭 토큰의 **코드 진실은 [`Source/TDProject/AOS/UI/AOSUIStyle.h`](../../Source/TDProject/AOS/UI/AOSUIStyle.h)**. 이 문서는 그 토큰의 **해설·의도·사용 규칙**이다. 값이 어긋나면 `AOSUIStyle.h` 가 우선이며, 이 문서를 코드에 맞춰 갱신한다.

> 이 프로젝트의 UI 는 **관전형 오토배틀러 MOBA(PC 단일 플랫폼)**. 플레이어 상호작용은 **벤픽 드래프트 + 라운드 사이 준비/상점/배치**뿐이고 전투는 관전한다. 따라서 UI 의 무게중심은 "긴장된 실시간 HUD"가 아니라 **정보 밀도 높은 준비 콘솔**이다.

---

## 1. 비주얼 톤 / 무드 / 참조

### 컨셉: "전술 지휘 콘솔 (Tactical Command Console)"
방향성은 무드보드가 아니라 **게임 고유 세계**에서 도출한다 — 3레인, 2팩션, 골드 이코노미, 벤픽 드래프트. 플레이어는 사령관이고, UI 는 라운드를 준비하는 지휘 콘솔이다.

- **무드**: 어둡고 차분한 지반 위에 정보가 명료하게 떠오르는 프리미엄 콘솔. 고요하지만 밀도 높은. 장식은 최소, 위계는 강하게.
- **참조**: Mechabellum(준비/배치 콘솔의 정보 밀도), League of Legends 클라이언트(다크 패널 계층 + 골드 이코노미 강조 + 드래프트 UI).
- **북극성**: Mechabellum (게임 비전 문서 `Guides/01_GameOverview/GAME_VISION.md` 와 일치).

### AI 아트 경계 (중요 원칙)
> **AI 생성 아트는 "콘텐츠"에만, "크롬(chrome)"에는 절대 쓰지 않는다.**

- **콘텐츠 = AI OK**: 캐릭터 초상화, 아이템 아이콘 — 고유하고 예측 불가한 시각 자산.
- **크롬 = 토큰 솔리드**: 패널·프레임·버튼·구획선·코너 브래킷 — 전부 `AOSUIStyle` 토큰 색의 솔리드 드로잉. AI 장식 프레임 텍스처(예: 파일럿 `T_UI_Shop_*`)는 **제거 대상**.
- 이유: 크롬을 텍스처로 구우면 (1) 9-slice/라운드가 해상도마다 깨지고, (2) 색 토큰 규율이 무너져 일관성이 사라지고, (3) 다크 리테마 시 전량 재생성이 필요하다. 솔리드 크롬은 재컴파일만으로 리테마된다.

---

## 2. 팔레트 (토큰 표)

색은 **sRGB decimal(hex/255)** 관례 — `AOSUIStyle.h` 의 `FLinearColor` 저작 방식과 동일. 아래 hex 는 사람이 읽기 위한 표기이며, 코드 값(decimal)이 진실이다.

### 지반 / 패널 계층 (진한 청색 편향 차콜, 3단 위계)
| 토큰 | hex | 역할 |
|------|-----|------|
| `BgDeep` | `#04070C` | 최심층 — dim/오버레이 베이스(모달 뒤 암전) |
| `BgBase` | `#080C13` | 화면 베이스(뷰포트 바탕) |
| `PanelBase` | `#0E1420` | 패널 1단 — 큰 컨테이너/영역 바탕 |
| `PanelRaised` | `#141C2B` | 패널 2단 — 카드/버튼/슬롯 |
| `PanelHi` | `#1C2536` | 패널 3단 — hover/선택 상태 |
| `Border` | `#212D43` | 구획선/얇은 테두리 |
| `BorderHi` | `#374A6A` | 강조 테두리 / **코너 브래킷** |

### 텍스트
| 토큰 | hex | 역할 |
|------|-----|------|
| `TextPrimary` | `#E7EDF9` | 본문 / 헤딩 |
| `TextMuted` | `#8A96AE` | 보조 라벨 / 캡션 |
| `TextFaint` | `#525E77` | 최약 — placeholder / 비활성 |

### 액센트 (★ 역할 분리 — 이 규칙이 v2 의 핵심)
| 토큰 | hex | 역할(전용) | 보조 변형 |
|------|-----|-----------|-----------|
| `Gold` | `#FFC94D` | **경제 / 상점 / 골드 / 구매 CTA** — 유일한 볼드 액센트 | `GoldDeep` `#B08328`(테두리/그림자), `GoldSoft`(α0.12 배경) |
| `Info` | `#4FA6F2` | **시간 / 타이머**(파랑) | `InfoDeep` `#1E5A9C`(칩 테두리), `InfoSoft`(α0.12 배경) |
| `RedCTA` | `#E24A3C` | **진행 / 실행**(라운드 준비 버튼) | `RedCTADeep` `#7A2119`(그라디언트 저단) |

### 상태
| 토큰 | hex | 역할 |
|------|-----|------|
| `Success` | `#4ECB84` | 유효 / 구매가능 / 배치완료 |
| `Danger` | `#E05B54` | 밴 / 불가 / 제거 |

### 팩션 (`TeamAccent(bTeam1)` / `TeamDeep(bTeam1)`)
| 팀 | 림/라인(Accent) | 진한 채움(Deep) |
|----|-----------------|-----------------|
| Team1 | 코랄 `#F2704F` | `#B0503C` |
| Team2 | 애저 `#4C92EC` | `#2F6FB8` |

### 액센트 역할 분리 규칙 (Do / Don't)
- ✅ 골드/가격/구매/상점 = **Gold 전용**. Gold 는 화면당 소수의 볼드 포인트로만.
- ✅ 남은 시간/카운트다운/타이머 칩 = **Info(파랑) 전용**.
- ✅ "라운드 준비 / 시작 / 실행" 등 게임을 앞으로 미는 주 CTA = **RedCTA(빨강) 전용**.
- ✅ 팀 소속 표시(라인/슬롯 림, 준비 상태) = 팩션 코랄/애저.
- ❌ 타이머를 골드로, 구매 버튼을 파랑으로 쓰는 **역할 교차 금지** — 색이 곧 의미다.
- ❌ 한 화면에서 Gold/RedCTA 를 둘 다 볼드 대면적으로 쓰지 말 것(주의 경쟁). RedCTA=주 진행, Gold=경제 포인트로 분리.

---

## 3. 타입 스케일

폰트 패밀리 = **Segoe UI**(대문자 라벨은 자간 트래킹 적용). 수치(골드/타이머/카운트)는 **monospace tabular-nums**(자릿수 흔들림 방지). 코드에서는 현재 `AOSUIStyle::Heading(Size)`(Bold) / `Body(Size)`(Regular) 로 Roboto 기본 폰트를 반환 — WBP 이관 시 Segoe UI + tabular 수치로 상향한다.

| 역할 | 토큰 | pt | 용도 |
|------|------|----|----|
| Display | `TypeDisplay` | 26 | 화면 대제목(벤픽/준비 헤더) |
| H1 | `TypeH1` | 22 | 섹션 제목 |
| H2 | `TypeH2` | 17 | 카드/패널 제목 |
| Body | `TypeBody` | 14 | 본문/라벨 |
| Caption | `TypeCaption` | 11 | 캡션/메타/힌트 |

- **대문자 라벨**(버튼/탭/섹션 헤더)은 자간 트래킹으로 콘솔 톤을 강화.
- **수치 강조**는 크기가 아니라 **색(Gold/Info)** 과 monospace 로 — 타입 스케일을 과하게 늘리지 않는다.

---

## 4. 간격 · 깊이 · 라운드 스케일

### 간격 스케일 (`Space1`~`Space6`, px)
`4 / 8 / 12 / 16 / 24 / 32`. 위젯 패딩·갭은 매직넘버 대신 이 토큰을 참조.
- 밀집 요소(칩 내부, 아이콘-라벨) = Space1/Space2
- 카드 내부 패딩 = Space3/Space4
- 패널 간 갭 / 섹션 분리 = Space5/Space6

### 깊이(Depth) — 3단 패널 계층
지반에서 위로 올라올수록 밝아진다: `BgBase < PanelBase < PanelRaised < PanelHi`.
- 그림자 대신 **명도 계단 + 1px `Border`** 로 깊이를 표현(다크에서 드롭섀도는 잘 안 보임).
- hover/선택 = 한 계단 위로(`PanelRaised → PanelHi`), 보간 속도 `HoverLerpSpeed=12/s`.

### 라운드 (메트릭)
| 토큰 | px | 용도 |
|------|----|----|
| `CardRadius` | 9 | 카드/패널 코너 |
| `SlotRadius` | 8 | 슬롯/버튼 코너 |
| `DividerThickness` | 3 | 구획선 두께 |

---

## 5. 컴포넌트 스펙

### 패널 (3단 계층 + 코너 브래킷)
- 바탕 = 계층에 맞는 `Panel*` 솔리드, 테두리 = `Border`(1px), 코너 = `CardRadius`.
- **코너 브래킷**: 패널 네 모서리에 `BorderHi` 색의 짧은 ㄱ자 선(장식이 아니라 콘솔 프레이밍). 텍스처가 아니라 토큰 솔리드 라인으로 그린다.
- 강조 패널(현재 액티브 영역)은 테두리를 `BorderHi` 로 승격.

### 버튼
- **주 CTA(라운드 준비/실행)** = `RedCTA` 솔리드(hover 밝게/pressed 어둡게, `SolidButtonStyle` 3상태), 대문자 라벨.
- **경제 CTA(상점 열기/구매)** = Gold. 열기 버튼은 **Gold ghost**(투명 배경 + `Gold` 테두리/텍스트), 구매 확정 버튼은 Gold 솔리드.
- **보조/중립 버튼** = `PanelRaised` 솔리드 + `TextPrimary`.
- **비활성** = `PanelBase` + `TextFaint`, 상호작용 차단.
- 패딩 규약: Normal `(14,7)`, Pressed `(14,9,14,5)` — 눌림 시 1px 내려앉는 착시(`SolidButtonStyle` 기본값).

### 칩 (타이머 / 골드 / 상태)
작은 pill 형. 배경 = 액센트 Soft(α0.12), 테두리 = 액센트 Deep, 텍스트/수치 = 액센트 본색.
- 타이머 칩 = `InfoSoft` / `InfoDeep` / `Info`(monospace `M:SS`).
- 골드 칩 = `GoldSoft` / `GoldDeep` / `Gold`(monospace).
- 상태 칩(준비완료 등) = `Success`, 밴/불가 = `Danger`.

### 카드 (로스터 유닛 / 아이템)
- 바탕 = `PanelRaised`, `CardRadius`, 테두리 `Border`.
- **콘텐츠 슬롯**(초상화/아이콘) = AI 아트가 들어가는 유일한 자리. 초상화 없으면 `PlaceholderColor(Index)` 로 인덱스 분산된 저명도 타일.
- 팀 소속 카드/슬롯 = 상단 또는 좌측 림에 팩션색(`TeamAccent`).
- hover = `PanelHi` 로 상승 + 테두리 `BorderHi`.

### 코너 브래킷 (프레이밍 모티프)
콘솔 톤의 시그니처. 패널/프리뷰 스테이지/액티브 슬롯의 모서리에 `BorderHi` 짧은 ㄱ자. 과용 금지 — 위계상 "지금 주목할 영역"에만.

---

## 6. Do / Don't 요약

**Do**
- 크롬(패널/버튼/프레임/구획선/코너 브래킷)은 **`AOSUIStyle` 토큰 솔리드**로만 그린다.
- 색으로 의미를 전달한다 — Gold=경제, Info=시간, RedCTA=진행, 팩션=소속, Success/Danger=상태.
- 수치는 monospace tabular-nums + 액센트색.
- 깊이는 명도 계단 + 1px 테두리로.
- 인라인 색 리터럴 대신 토큰만 참조(위젯 .cpp/WBP 에 팔레트 재산포 금지).

**Don't**
- ❌ **AI 장식 프레임/패널 텍스처 금지** — 크롬은 토큰 솔리드. (파일럿 `T_UI_Shop_*` 는 제거 대상.)
- ❌ 액센트 역할 교차(타이머=골드, 구매=파랑 등) 금지.
- ❌ 화면당 볼드 대면적 액센트 남발 금지(Gold/RedCTA 경쟁 회피).
- ❌ 텍스트를 텍스처에 굽지 말 것 — 전부 TextBlock(선명도).
- ❌ 라이트 테마 잔재/화이트 카드 유지 금지 — 다크 단일 테마.

---

## 7. 코드 연동 & 마이그레이션

- **토큰 진실 = `AOSUIStyle.h`.** 신규 색/폰트/메트릭 상수는 전부 거기 한 곳에 추가하고, 위젯 코드는 참조만 한다.
- **하이브리드 마이그레이션**: 옛 심볼(`CardWhite`/`PanelSoft`/`Accent`/`BanRed`/`PreviewRing`…)은 이름을 유지하되 값만 다크로 갱신됨 → 다운스트림은 재컴파일만으로 즉시 다크. 위젯을 손댈 때 신규 시맨틱 이름(`PanelRaised`/`Info`/`Danger`/`Gold`…)으로 점진 교체.
- **크롬 텍스처 → 솔리드**: `KitBrush`/`KitButtonStyle`(텍스처 경로)를 `SolidBrush`/`SolidButtonStyle`(토큰 색)로 대체. `KitBrush` 는 이제 **콘텐츠 이미지(초상화/아이콘) 전용**으로만 남긴다.
- 위젯별 상세 스타일 계약 → `Guides/02_Design/UI_Specs/*.md`, 텍스처 파이프라인 → `Guides/03_Implementation/UI_TEXTURE_KIT.md`.

---

## 관련 문서
- [`Source/TDProject/AOS/UI/AOSUIStyle.h`](../../Source/TDProject/AOS/UI/AOSUIStyle.h) — 토큰 SSOT
- [`Guides/02_Design/UI_Specs/CharacterSelect.md`](./UI_Specs/CharacterSelect.md) — 라운드 준비/배치 위젯 스펙
- [`Guides/02_Design/UI_Specs/Shop.md`](./UI_Specs/Shop.md) — 상점 위젯 스펙
- [`Guides/03_Implementation/UI_TEXTURE_KIT.md`](../03_Implementation/UI_TEXTURE_KIT.md) — 텍스처 파이프라인(콘텐츠 전용 개정)
- [`Guides/01_GameOverview/GAME_VISION.md`](../01_GameOverview/GAME_VISION.md) — 게임 비전(북극성=Mechabellum)
