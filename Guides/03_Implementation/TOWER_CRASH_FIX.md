# 🔧 타워 크래시 수정 (LoadObject 변경)

**Date**: 2025-11-23
**Issue**: ConstructorHelpers::FObjectFinder 크래시
**Status**: ✅ 고정됨

---

## 문제

타워 메시 로딩 시 크래시 발생:
```
ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(...)
→ Initialize()에서 호출 (생성자가 아님)
→ 크래시!
```

### 원인
- `ConstructorHelpers::FObjectFinder`는 **생성자 전용**
- `Initialize()` 함수는 런타임에 호출 (생성자 이후)
- 런타임 에셋 로딩이 필요함

---

## 해결책

**Before** (크래시):
```cpp
static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
    TEXT("StaticMesh'/Engine/BasicShapes/Cube'")
);
```

**After** (안전):
```cpp
UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
    nullptr,
    TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")
);

if (CubeMesh)
{
    MeshComponent->SetStaticMesh(CubeMesh);
    // ... 메시 설정
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Failed to load cube mesh"));
}
```

---

## 변경 사항

### 파일: `AOSStructure.cpp`

**🟡 SetupTowerMesh()** 수정
- ❌ ConstructorHelpers::FObjectFinder 제거
- ✅ LoadObject<UStaticMesh>() 사용
- ✅ 에러 로깅 추가
- ✅ 디버그 로깅 추가 (색상 정보)

**🟡 SetupCommandCenterMesh()** 수정
- 동일한 변경 적용
- 더 크고 밝은 색상 유지

---

## 테스트 및 확인

### 빌드
```
✅ 성공 (26.74초)
```

### 다음 단계

1. **에디터 실행** (이미 시작됨)
2. **새 맵 만들기**
   ```
   File → New Level → Blank
   File → Save As → Content/AOS/Lvl_AOS_Test
   ```
3. **GameMode 설정**
   ```
   Window → World Settings
   Game Mode: AOSGameMode
   ```
4. **MapManager 배치**
   ```
   Window → Place Actors
   "AOSMapManager" 검색 → 드래그
   ```
5. **Play** (Alt+P)

### 콘솔 로그 확인
게임 실행 후 콘솔에서 확인:
```
Tower mesh setup: Team=0, Color=(1.0,0.0,0.0)
Tower mesh setup: Team=1, Color=(0.0,0.0,1.0)
Command Center mesh setup: Team=0, Color=(1.0,0.5,0.5)
Command Center mesh setup: Team=1, Color=(0.5,0.5,1.0)
```

---

## 기술 노트

### LoadObject vs ConstructorHelpers

| 항목 | ConstructorHelpers | LoadObject |
|------|------------------|-----------|
| **사용 위치** | 생성자만 | 어디서나 |
| **시간** | 컴파일 타임 | 런타임 |
| **에러 처리** | 어려움 | 쉬움 (nullptr 체크) |
| **성능** | 빠름 | 약간 느림 |
| **안정성** | 한 번만 로드 | 반복 로드 가능 |

### 메시 경로 형식

**엔진 기본 메시**:
```cpp
StaticMesh'/Engine/BasicShapes/Cube.Cube'
```

**프로젝트 콘텐츠**:
```cpp
StaticMesh'/Game/Path/To/Mesh.Mesh'
```

---

## 다음 개선사항

1. **캐싱**: 여러 번 로드되지 않도록 메시를 캐시
2. **커스텀 메시**: 프로젝트 폴더에 타워 메시 추가
3. **머티리얼**: 기본 머티리얼 대신 커스텀 머티리얼 사용
4. **에디터 설정**: 블루프린트에서 메시 커스터마이징

---

**마지막 업데이트**: 2025-11-23
**빌드 상태**: ✅ 성공
