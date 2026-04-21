# 게임 밸런스 가이드

**대상**: design-balance 에이전트 / 기획자
**작업 방식**: MCP 도구 → Blueprint 프로퍼티 조정

---

## 현재 수치 전체 요약

| 오브젝트 | 프로퍼티 | 기본값 | 단위 | 영향 범위 |
|---------|---------|-------|------|---------|
| **캐릭터** | MaxHealth | 100.0 | HP | 생존력 |
| | AttackDamage | 10.0 | HP | 대 구조물/캐릭터 딜 |
| | AttackRange | 500.0 | cm | 근접 공격 거리 |
| | AttackCooldown | 1.0 | 초 | DPS |
| | MovementSpeed | 600.0 | cm/s | 라인 진입 속도 |
| **타워** | MaxHealth | 1000.0 | HP | 방어 강도 |
| | AttackDamage | 20.0 | HP | 캐릭터 압박 |
| | AttackRange | 200.0 | cm | 타워 방어 범위 |
| | DetectionRange | 1500.0 | cm | 적 감지 거리 |
| | AttackCooldown | 2.0 | 초 | 발사 간격 |
| **커맨드센터** | MaxHealth | 5000.0 | HP | 최종 목표 내구성 |
| **AI 컨트롤러** | EnemyDetectionRange | 1500.0 | cm | 캐릭터 간 전투 개시 거리 |
| | AttackRange | 500.0 | cm | 캐릭터 간 공격 거리 |
| | ArrivalDistance | 100.0 | cm | 웨이포인트 도착 판정 |

---

## 파라미터별 조정 가이드

### 캐릭터 (BP_Character)

**MaxHealth (100)**
- 타워 1회 공격 = 20 HP → 기본 캐릭터가 타워 5발을 버팀
- 너무 낮으면: 캐릭터가 타워에 즉사, 라인 돌파 불가능
- 너무 높으면: 타워가 무의미해짐
- 권장 범위: 60~200

**AttackDamage (10)**
- 타워 HP 1000 / 캐릭터 딜 10 = 100회 공격 필요
- 공격 쿨다운 1초 기준 → 타워 파괴에 100초 소요 (전투 없을 때)
- 권장 범위: 5~30

**MovementSpeed (600)**
- UE5 기준 캐릭터 평균 이동속도 = 600 cm/s
- 너무 낮으면: 게임 속도 느려짐
- 너무 높으면: 타워 범위를 너무 빠르게 통과
- 권장 범위: 400~900

---

### 타워 (BP_Team1Tower, BP_Team2Tower)

**AttackDamage (20)**
- 캐릭터 HP 100 / 타워 딜 20 = 5발 → 2초 공격 쿨다운 기준 약 10초면 사망
- 권장 범위: 10~50

**DetectionRange (1500)**
- 타워가 적을 감지하는 거리
- 너무 크면: 타워 어그로 범위가 넓어져 접근 자체가 어려워짐
- 타워의 AttackRange(200)보다 항상 크게 유지할 것
- 권장 범위: 800~3000

---

### 게임 페이스 조절

**빠른 게임** 원할 때:
- AttackDamage 증가 (캐릭터/타워 모두)
- MaxHealth 감소
- MovementSpeed 증가

**느린 전략 게임** 원할 때:
- MaxHealth 증가
- AttackCooldown 증가
- DetectionRange 감소 (타워 어그로 범위 축소)

---

## MCP 작업 흐름

```
1. 현재 값 확인
   mcp__mcp-unreal__get_property (BP_Character, MaxHealth)

2. 값 변경
   mcp__mcp-unreal__set_property (BP_Character, MaxHealth, 120)

3. 레벨 저장 (필수!)
   mcp__mcp-unreal__level_ops → save_level

4. 뷰포트 확인
   mcp__mcp-unreal__capture_viewport
```

> **주의**: 저장하지 않으면 에디터 재시작 시 변경사항 소실

---

## 하드코딩 이슈 추적

C++ 코드에 직접 박혀 있어 Blueprint로 조정 불가한 값은 `CROSS_DOMAIN_REQUESTS.md`에 등록하여 프로그래머에게 요청하세요.

현재 이슈:
- 없음 (CR-001 해결됨: ReceiveDamage 하드코딩 → GetAttackDamage() 참조로 변경)

---

## 밸런스 변경 이력 기록

변경 후 `Guides/06_BalanceLog/YYYY-MM-DD_밸런스패치.md` 파일을 생성하여 기록하세요.

```markdown
# YYYY-MM-DD 밸런스 패치

## 변경 내용
- BP_Character.MaxHealth: 100 → 120 (사유: 타워 공격에 너무 빨리 사망)

## 테스트 결과
- PIE 10회 플레이 후 평균 게임 시간: 8분 → 12분
```

---

**최종 업데이트**: 2026-04-21
