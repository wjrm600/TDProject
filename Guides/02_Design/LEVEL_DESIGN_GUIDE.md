# 레벨 디자인 가이드

**대상**: design-level 에이전트 / 기획자
**작업 방식**: MCP 도구 → 레벨 액터 배치 및 MapManager 설정

---

## 3레인 구조 원칙

TDProject는 MOBA 스타일 3레인 구조입니다:

```
Team1 진영           중립 지역           Team2 진영
[CC1]─[T1]─[T1]─[T1]  Top Lane  [T2]─[T2]─[T2]─[CC2]
[CC1]─[T1]─[T1]─[T1]  Mid Lane  [T2]─[T2]─[T2]─[CC2]
[CC1]─[T1]─[T1]─[T1]  Bot Lane  [T2]─[T2]─[T2]─[CC2]

T = Tower, CC = Command Center
```

- 각 팀: 타워 9개(3레인 × 3개) + 커맨드센터 1개
- 스폰 포인트: 12개 (2팀 × 3레인 × 2개)

---

## MapManager 설정 (AOSMapManager)

`LanesInfo` 배열에 레인 정보를 설정합니다.

### LanesInfo 구성 (레인당 1개)

```
Lane: Top / Mid / Bottom
Team1TowerPositions: [FVector, FVector, FVector]  ← 3개
Team2TowerPositions: [FVector, FVector, FVector]  ← 3개
Team1CommandCenterPosition: FVector
Team2CommandCenterPosition: FVector
```

> **주의**: `LanesInfo`는 에디터 설정값, `AllTowers`는 런타임 인스턴스.
> 런타임 로직에서는 항상 `AllTowers` 사용.

### 권장 맵 크기

- 전체 맵: 40,000 × 40,000 cm (400m × 400m)
- 레인 길이: 약 25,000~30,000 cm
- 타워 간격: 5,000~8,000 cm
- 진영 간 거리: 맵 전체 길이

### 기준 좌표 예시 (Mid Lane 기준)

```
Team1 진영 (양수 X):
  Tower 1: (20000, 0, 100)   ← 커맨드센터와 가까운 아군 타워
  Tower 2: (12000, 0, 100)
  Tower 3:  (5000, 0, 100)   ← 적 진영과 가까운 아군 타워
  Command:  (25000, 0, 100)

Team2 진영 (음수 X):
  Tower 1: (-20000, 0, 100)
  Tower 2: (-12000, 0, 100)
  Tower 3:  (-5000, 0, 100)
  Command:  (-25000, 0, 100)
```

Top/Bot 레인은 Y축으로 ±15,000 cm 오프셋 적용.

---

## 스폰 포인트 설정

12개의 `AOSSpawnPoint` 액터를 레벨에 배치하고 각각 설정합니다.

### 설정 항목

각 스폰 포인트 Details 패널에서:
```
Team:          Team1 / Team2
Lane:          Top / Mid / Bottom
Spawn Index:   0 또는 1 (같은 팀/레인 내 순서)
bSpawnEnabled: true (기본값, false로 디버깅 시 비활성화)
```

### 배치 기준 좌표

| 스폰 포인트 | 팀 | 레인 | Index | 권장 위치 |
|-----------|---|------|-------|---------|
| SP_T1_Top_0 | Team1 | Top | 0 | (18000, 15000, 100) |
| SP_T1_Top_1 | Team1 | Top | 1 | (16000, 13000, 100) |
| SP_T1_Mid_0 | Team1 | Mid | 0 | (18000, 0, 100) |
| SP_T1_Mid_1 | Team1 | Mid | 1 | (16000, 0, 100) |
| SP_T1_Bot_0 | Team1 | Bottom | 0 | (18000, -15000, 100) |
| SP_T1_Bot_1 | Team1 | Bottom | 1 | (16000, -13000, 100) |
| SP_T2_Top_0 | Team2 | Top | 0 | (-18000, -15000, 100) |
| SP_T2_Top_1 | Team2 | Top | 1 | (-16000, -13000, 100) |
| SP_T2_Mid_0 | Team2 | Mid | 0 | (-18000, 0, 100) |
| SP_T2_Mid_1 | Team2 | Mid | 1 | (-16000, 0, 100) |
| SP_T2_Bot_0 | Team2 | Bottom | 0 | (-18000, 15000, 100) |
| SP_T2_Bot_1 | Team2 | Bottom | 1 | (-16000, 13000, 100) |

> **팁**: 스폰 포인트 간 최소 간격 200 units 이상 유지 (캐릭터 겹침 방지)

---

## 새 레벨 생성 절차

1. `File → New Level → Blank`
2. `File → Save As → Content/AOS/Lvl_NewMap`
3. `Window → World Settings → Game Mode Override → AOSGameMode`
4. 기본 바닥(Floor) 추가
5. `AOSMapManager` 액터 배치 (1개만)
6. `LanesInfo` 3개 항목 설정 (Top/Mid/Bottom)
7. 타워 위치 입력
8. 스폰 포인트 12개 배치 및 설정
9. Ctrl+S 저장

---

## 레이아웃 검증

배치 완료 후 다음을 확인합니다:

- [ ] MapManager 1개만 배치됨
- [ ] LanesInfo 3레인 모두 설정됨 (Top/Mid/Bottom)
- [ ] 타워 총 18개 (팀당 9개)
- [ ] 커맨드센터 2개
- [ ] 스폰 포인트 12개 (팀당 6개, 레인당 2개)
- [ ] bSpawnEnabled = true (전부)
- [ ] Game Mode = AOSGameMode

### MCP 도구로 확인

```
mcp__mcp-unreal__get_level_actors → AOSSpawnPoint 12개 확인
mcp__mcp-unreal__capture_viewport → 시각 확인
```

---

## 디버그 가시화

PIE 실행 시 MapManager가 자동으로 디버그 박스를 그립니다:
- **파란 박스**: Team1 타워
- **빨간 박스**: Team2 타워
- **노란 박스**: Team1 커맨드센터 (1.5x 크기)
- **주황 박스**: Team2 커맨드센터 (1.5x 크기)

---

**최종 업데이트**: 2026-04-21
