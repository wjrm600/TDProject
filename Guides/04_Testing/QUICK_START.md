# 빠른 시작 가이드

**통합 문서**: QUICK_TOWER_TEST (3분) + PIE_QUICK_CHECKLIST (30분)

---

## 3분 빠른 테스트 (타워만 확인)

MapManager만 배치해서 타워 생성을 확인합니다.

### 절차

1. **새 레벨 생성**
   - `File → New Level → Blank`
   - `Content/AOS/Lvl_AOS_Test`로 저장

2. **게임 모드 설정**
   - `Window → World Settings`
   - `Game Mode Override → AOSGameMode`

3. **MapManager 배치**
   - Place Actors에서 `AOSMapManager` 검색
   - 레벨에 드래그 (1개만)

4. **실행** `Alt+P`

### 성공 기준

화면에 빨간(Team1) + 파란(Team2) 큐브들이 나타나면 성공.

콘솔 확인:
```
AOSMapManager created and structures spawned
```

---

## 30분 전체 PIE 테스트 (캐릭터 + AI 포함)

### 1단계: 레벨 설정 (5분)

- [ ] 새 레벨 생성 → `AOS_TestLevel` 저장
- [ ] World Settings → Game Mode: `AOSGameMode`
- [ ] 바닥/조명 추가
- [ ] `AOSMapManager` 배치

### 2단계: 스폰 포인트 배치 (10분)

12개 `AOSSpawnPoint` 배치 후 각각 설정:

**Team1 (Red) - 맵 양수 X쪽**

| 이름 | 위치 | Team | Lane | Index |
|-----|------|------|------|-------|
| SP_T1_Top_0 | (1500, 1500, 100) | Team1 | Top | 0 |
| SP_T1_Top_1 | (1300, 1300, 100) | Team1 | Top | 1 |
| SP_T1_Mid_0 | (1500, 0, 100) | Team1 | Mid | 0 |
| SP_T1_Mid_1 | (1300, 0, 100) | Team1 | Mid | 1 |
| SP_T1_Bot_0 | (1500, -1500, 100) | Team1 | Bottom | 0 |
| SP_T1_Bot_1 | (1300, -1300, 100) | Team1 | Bottom | 1 |

**Team2 (Blue) - 맵 음수 X쪽**

| 이름 | 위치 | Team | Lane | Index |
|-----|------|------|------|-------|
| SP_T2_Top_0 | (-1500, -1500, 100) | Team2 | Top | 0 |
| SP_T2_Top_1 | (-1300, -1300, 100) | Team2 | Top | 1 |
| SP_T2_Mid_0 | (-1500, 0, 100) | Team2 | Mid | 0 |
| SP_T2_Mid_1 | (-1300, 0, 100) | Team2 | Mid | 1 |
| SP_T2_Bot_0 | (-1500, 1500, 100) | Team2 | Bottom | 0 |
| SP_T2_Bot_1 | (-1300, 1300, 100) | Team2 | Bottom | 1 |

### 3단계: 캐릭터 블루프린트 (5분)

- [ ] Content Browser → Create → Blueprint Class
- [ ] Parent Class: `AAOSCharacter` → 이름: `BP_AOSCharacter`
- [ ] Compile & Save

### 4단계: GameMode에 BP 연결 (2분)

- [ ] BP_AOSGameMode (또는 World Settings) → `Default Pawn Class = BP_AOSCharacter`

### 5단계: PIE 실행 확인

- [ ] `Alt+P` → 캐릭터들이 스폰 포인트에서 생성됨
- [ ] 캐릭터들이 레인을 따라 이동 시작
- [ ] 타워를 만나면 공격
- [ ] 커맨드센터 파괴 시 게임 종료

---

## 문제 해결

| 증상 | 확인 사항 |
|-----|---------|
| 타워가 안 보임 | MapManager 배치 확인, LanesInfo 설정 확인 |
| 캐릭터가 안 생김 | DefaultPawnClass 설정, bSpawnEnabled=true 확인 |
| AI가 안 움직임 | AIControllerClass 생성자 설정, AutoPossessAI 확인 |
| 캐릭터가 겹침 | 스폰 포인트 간격 200 units 이상 확인 |

---

**팁**: 디버깅 시 스폰 포인트 1개만 `bSpawnEnabled=true`로 두면 로그 추적이 쉬워집니다.
