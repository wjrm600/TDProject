# 🏰 타워 테스트 설정 가이드

**Status**: 준비 단계
**Date**: 2025-11-23

---

## 📋 개요

타워 시스템을 테스트하기 위한 완전한 설정 가이드입니다.

현재까지 완료된 작업:
- ✅ 타워 메시 및 색상 설정 (AOSStructure)
- ✅ MapManager 타워 배치 로직 (AOSMapManager)
- ✅ 게임모드 타워 통합 (AOSGameMode)

다음 단계: 에디터에서 맵 설정

---

## 🎮 에디터에서의 설정 (필수)

### 단계 1: 새 AOS 테스트 맵 생성

1. **에디터 열기**
   - `C:\UnrealProject\TDProject\Binaries\Win64\TDProject.exe` 실행
   - 또는 Visual Studio에서 "TDProject Editor" 선택

2. **새 맵 만들기**
   ```
   File → New Level → Blank
   ```

3. **맵 저장**
   ```
   File → Save As
   위치: Content/AOS/
   이름: Lvl_AOS_Test
   ```

---

### 단계 2: GameMode 설정

1. **World Settings 열기**
   ```
   Window → World Settings (또는 Ctrl+W)
   ```

2. **GameMode 설정**
   - Game Mode: `AOSGameMode` 검색 및 선택
   - Default Pawn Class: `(선택하지 않음 - 스폰 포인트에서 처리)`

3. **저장**
   ```
   Ctrl + S
   ```

---

### 단계 3: MapManager 배치

1. **Place Actors 패널에서 검색**
   ```
   Place → 검색 "AOSMapManager"
   ```

2. **레벨에 배치**
   - MapManager를 레벨에 드래그
   - 위치는 상관없음 (0, 0, 0)으로 설정해도 좋음)

3. **MapManager 확인**
   - Details 패널에서 "Launch Towers" 버튼 클릭
   - (또는 게임 시작 시 자동으로 타워 생성)

---

### 단계 4: 기본 조명 설정 (선택)

1. **Sky Sphere 또는 Directional Light 추가**
   ```
   Place → Light → Directional Light
   ```

2. **저장**
   ```
   Ctrl + S
   ```

---

## 🧪 PIE 테스트 (에디터에서 플레이)

1. **PIE 플레이**
   ```
   Editor Toolbar → Play (또는 Alt + P)
   ```

2. **확인 항목**
   - ✅ 타워들이 맵에 생성되었는가?
   - ✅ 타워의 색상이 팀별로 다른가? (빨강/파랑)
   - ✅ 콘솔에 타워 생성 로그가 있는가?
   - ✅ 타워가 정확한 위치에 배치되었는가?

3. **콘솔 확인**
   ```
   Ctrl + ` (backtick) → 콘솔 열기
   "tower", "spawn", "structure" 등으로 필터링
   ```

---

## 📊 예상 결과

### 맵에 배치되어야 할 타워
- **Top Lane**: Team1 3개 + Team2 3개 = 6개
- **Mid Lane**: Team1 3개 + Team2 3개 = 6개 (+ 커맨드 센터 2개)
- **Bottom Lane**: Team1 3개 + Team2 3개 = 6개

**총 타워**: 18개
**총 커맨드 센터**: 2개

### 색상
- **Team1 (빨강)**: 타워, 커맨드 센터
- **Team2 (파랑)**: 타워, 커맨드 센터

---

## 🔍 문제 해결

### 문제: 타워가 보이지 않음

**원인 1**: MapManager가 레벨에 배치되지 않음
- 해결: Lvl_AOS_Test에서 AOSMapManager를 Place하고 저장

**원인 2**: GameMode가 설정되지 않음
- 해결: World Settings에서 GameMode를 AOSGameMode로 설정

**원인 3**: 메시가 로드되지 않음
- 해결: 콘솔에서 로그 확인 (`SetupTowerMesh()`, `SetupCommandCenterMesh()`)

### 문제: 타워가 색상이 적용되지 않음

**원인**: 기본 머티리얼이 색상 파라미터를 지원하지 않음
- 해결: 에디터에서 개별 타워를 선택해서 머티리얼 색상 수동 조정

---

## 📝 다음 단계

1. **타워 자동 공격 테스트**
   - 캐릭터 스포닝 시스템과 통합
   - 타워 → 캐릭터 자동 공격 확인

2. **UI 추가**
   - 체력 바 표시
   - 팀 정보 표시

3. **게임 로직 완성**
   - 승리 조건: 커맨드 센터 파괴
   - 라운드 타이머

---

## 💾 저장된 설정

현재 생성된 파일:
- ✅ `AOSStructure.cpp/h` - 메시 설정 완료
- ✅ `AOSMapManager.cpp/h` - 타워 배치 로직 완료
- ✅ `AOSGameMode.cpp/h` - 통합 로직 완료
- ⏳ `Lvl_AOS_Test.umap` - (에디터에서 생성 필요)

---

**마지막 업데이트**: 2025-11-23
