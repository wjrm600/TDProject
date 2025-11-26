# 🚀 타워 빠른 테스트 (3분)

**난이도**: ⭐ (아주 쉬움)
**시간**: 3분

---

## 📋 현재 상태

### ✅ 완료됨
- AOSStructure에 메시 설정 완료
- 타워 색상 (Team1=빨강, Team2=파랑) 자동 설정
- MapManager 타워 배치 로직 완료
- AOSGameMode 통합 완료

### ⏳ 필요한 것
1. 에디터에서 새 맵 만들기
2. MapManager 배치
3. 게임 실행

---

## 🎮 3분 안에 끝내기

### 1️⃣ 에디터 실행
```
C:\UnrealProject\TDProject\Binaries\Win64\TDProject.exe
```

> 또는 Visual Studio에서 TDProject Editor 선택

### 2️⃣ 새 맵 만들기 (30초)

**방법 A: 완전히 새로 만들기**
```
File → New Level → Blank
File → Save As
위치: Content/AOS/
이름: Lvl_AOS_Test
```

**방법 B: 빠른 방법 (추천)**
```
Content Browser → ThirdPerson → Lvl_ThirdPerson 우클릭
Duplicate → 이름을 "Lvl_AOS_Test"로 변경
Content/AOS/로 이동
```

### 3️⃣ GameMode 설정 (1분)

```
Window → World Settings
또는 Ctrl+W
```

**World Settings에서**:
- **Game Mode**: "AOSGameMode" 검색 후 선택
- **Default Pawn Class**: (비워둠)

**저장**: Ctrl+S

### 4️⃣ MapManager 배치 (1분)

**Place Actors 패널에서**:
```
검색: AOSMapManager
→ 레벨에 클릭해서 배치
```

또는:

```
Window → Place Actors
"AOSMapManager" 검색
레벨에 드래그
```

### 5️⃣ 게임 실행 (30초)

```
Editor Toolbar → Play (또는 Alt+P)
```

---

## 👀 확인할 항목

게임이 시작되면 콘솔에 다음과 같은 로그가 보여야 합니다:

```
AOSMapManager created and structures spawned
Character spawned at SpawnPoint - Team: X, Lane: Y
Tower and structure references cached
```

### 화면에 보일 것:

- 🔴 **빨간색 큐브들** (Team1 타워/커맨드 센터)
- 🔵 **파란색 큐브들** (Team2 타워/커맨드 센터)
- 💡 **배치 패턴**:
  - Top 라인: 6개 타워 (좌상향-우하향 대각선)
  - Mid 라인: 6개 타워 + 2개 커맨드 센터 (중앙)
  - Bottom 라인: 6개 타워 (좌하향-우상향 대각선)

### 실패 시 확인:

❌ 아무것도 안 보임
→ Ctrl+` (백틱)으로 콘솔 열기
→ "error", "warning" 검색
→ 로그 메시지 확인

❌ 타워는 보이는데 색상이 회색
→ 정상 동작 (머티리얼 색상 설정은 선택사항)
→ 위치와 개수만 맞는지 확인

---

## 📝 다음 단계 (선택)

게임이 정상 작동한다면:

1. **캐릭터 스포닝 테스트**
   - 스폰 포인트 12개 배치
   - 캐릭터 블루프린트 생성
   - Auto-spawn 확인

2. **타워 공격 테스트**
   - 타워가 적팀 캐릭터를 감지하는가?
   - 자동 공격이 작동하는가?

3. **UI 추가**
   - 타워 체력 바
   - 팀 정보 표시

---

## 💡 팁

- **빠른 재설정**: Alt+P로 에디터에서 바로 플레이
- **레벨 다시 로드**: 에디터 도구바의 "Reload" 또는 Ctrl+R
- **콘솔**: Ctrl+` (게임 중 실시간 로그 확인)
- **에디터 닫기**: Alt+F4 (변경사항 저장 여부 물어봄)

---

## ❓ FAQ

**Q: MapManager를 여러 번 배치하면?**
A: 첫 번째 MapManager만 작동. 나머지는 무시됨.

**Q: 게임 중 타워를 선택할 수 있나?**
A: 에디터에서만 선택 가능. 게임 중에는 피직스 충돌만 있음.

**Q: 캐릭터가 타워를 통과하나?**
A: 타워의 충돌 박스로 인해 통과 불가.

---

**완료 시간**: ~3분
**성공 지표**: 화면에 빨강/파랑 큐브들이 보이면 성공! ✅
