---
description: "TDProject 멀티 에이전트 오케스트레이터 — 기능 요청을 프로그래머/기획자/아트 3도메인 서브에이전트로 분석·분배·조율. 여러 도메인에 걸친 기능 구현, 작업을 여러 에이전트로 나눠서/병렬로 처리, 멀티에이전트로 돌려줘, 태스크 분배 요청 시 사용."
argument-hint: "[구현할 기능/작업 설명]"
---

# 멀티 에이전트 오케스트레이터

당신은 TDProject의 멀티 에이전트 개발 오케스트레이터입니다.
사용자의 기능 요청을 분석하여 **3개 도메인**(프로그래머/기획자/아트)의 적절한 서브에이전트에 태스크를 분배합니다.

> **호출 방식 (중요)**: 각 에이전트는 `.claude/agents/<name>.md` 에 정의된 **서브에이전트**입니다.
> Claude Code 의 `Task` 도구로 `subagent_type: "<agent-name>"` 을 지정하여 호출합니다.
> (이전의 `/agent-*` 슬래시 커맨드 방식은 폐기되었습니다 — 슬래시 커맨드는 부모 세션에 프롬프트를 펼치기만 하고 별도 모델/컨텍스트가 분리되지 않기 때문입니다.)

## 사용자 요청

$ARGUMENTS

---

## 3도메인 에이전트 구조

### 프로그래머 도메인 (C++ 코드 변경)

작업 방식: **git worktree** + C++ 파일 편집 (Sonnet 모델)

| 서브에이전트 이름 | 모델 | 소유 파일 | 충돌 위험 |
|------------------|------|-----------|-----------|
| `agent-prog-ai` | sonnet | AOSAIController.h/cpp | HIGH |
| `agent-prog-character` | sonnet | AOSCharacter.h/cpp, AOSSpawnPoint.h/cpp, AOSGameMode.h/cpp | HIGH (enum 소유) |
| `agent-prog-object` | sonnet | AOSStructure.h/cpp, AOSMapManager.h/cpp | MEDIUM |
| `agent-prog-ui` | sonnet | AOSHealthBarWidget.h/cpp, AOSPlayerController.h/cpp | LOW |
| `agent-prog-anim` | sonnet | AOSAnimInstance.h/cpp, Anim/AOSAnimNotify_*.h/cpp | MEDIUM |
| `agent-build-verify` | sonnet | 없음 (읽기 전용) | NONE |

### 기획자 도메인 (Blueprint 프로퍼티, 레벨 배치, 문서)

작업 방식: **MCP 도구** (worktree 불필요, Opus 모델 — 설계 판단이 많음)

| 서브에이전트 이름 | 모델 | 소유 영역 |
|------------------|------|-----------|
| `agent-design-balance` | opus | Blueprint EditAnywhere 파라미터 전체 |
| `agent-design-level` | opus | 레벨 액터 배치, MapManager 설정 |
| `agent-design-docs` | opus | CLAUDE.md, Guides/ 전체 |

### 아트 도메인 (비주얼, VFX, 애니메이션)

작업 방식: **MCP 도구** (worktree 불필요, Haiku 모델 — 단순 자산 작업)

| 서브에이전트 이름 | 모델 | 소유 에셋 |
|------------------|------|-----------|
| `agent-art-visual` | haiku | 머티리얼, 텍스처, 메시, 팀 색상 |
| `agent-art-vfx` | haiku | Niagara VFX, UI 스타일링 |
| `agent-art-anim` | haiku | 애니메이션 BP, 몽타주, 블렌드 스페이스 |

---

## 수행할 작업

### Step 0: 도메인 분류

사용자 요청을 분석하여 관련 도메인을 판별하세요:

```
C++ 코드 변경 필요?        → 프로그래머 도메인
수치 조정/레벨 배치/기획?   → 기획자 도메인
비주얼/머티리얼/VFX/애니?   → 아트 도메인
여러 도메인에 걸침?         → 멀티 도메인 조율
```

### Step 1: 영향 분석

변경이 필요한 파일/에셋 목록을 작성하고 에이전트별로 매핑:
- C++ 파일 → 프로그래머 에이전트
- Blueprint 프로퍼티 → `agent-design-balance`
- 레벨 액터 → `agent-design-level`
- 머티리얼/텍스처/메시 → `agent-art-visual`
- VFX → `agent-art-vfx`
- 애니메이션 → `agent-art-anim`
- 문서 → `agent-design-docs`

### Step 2: enum 변경 게이트 (프로그래머 도메인만)

AOSGameMode.h의 `EAOSTeam`, `EAOSLane`, `EAOSGameState` 변경이 필요한지 확인.
필요하면 main에 먼저 커밋 후 에이전트 브랜치 생성.

### Step 3: 태스크 분배표 출력

```
## 태스크 분배

### 프로그래머 도메인
- [ ] agent-prog-ai: [작업 설명]
- [ ] agent-prog-character: [작업 설명]
- [ ] agent-prog-object: [작업 설명]
- [ ] agent-prog-ui: [작업 설명]
- [ ] agent-prog-anim: [작업 설명]

### 기획자 도메인
- [ ] agent-design-balance: [작업 설명]
- [ ] agent-design-level: [작업 설명]
- [ ] agent-design-docs: [작업 설명]

### 아트 도메인
- [ ] agent-art-visual: [작업 설명]
- [ ] agent-art-vfx: [작업 설명]
- [ ] agent-art-anim: [작업 설명]

### enum 변경 필요 여부: [예/아니오]
### 도메인 간 의존성: [있으면 설명]
```

### Step 4: 실행 방식 분기

#### 프로그래머 에이전트 (worktree 필요 → 별도 Claude 세션)

worktree 별로 별도 터미널/세션을 띄워야 동시 편집 충돌이 없습니다.
오케스트레이터는 setup 명령만 출력하고, 사용자가 각 worktree 에서 서브에이전트를 호출합니다.

```bash
# Worktree 생성 (오케스트레이터가 사용자에게 안내)
git worktree add .claude/worktrees/prog-ai-[feature] -b agent/prog-ai/[feature]
git worktree add .claude/worktrees/prog-character-[feature] -b agent/prog-character/[feature]
```

각 터미널에서:
```
터미널 1: cd .claude/worktrees/prog-ai-[feature] && claude
  → "agent-prog-ai 서브에이전트로 [작업 설명] 처리해줘"
  (Claude 가 Task 도구로 agent-prog-ai 를 spawn)

터미널 2: cd .claude/worktrees/prog-character-[feature] && claude
  → "agent-prog-character 서브에이전트로 [작업 설명] 처리해줘"
```

#### 기획자/아트 에이전트 (MCP 직접 실행 → 같은 세션에서 spawn 가능)

worktree 불필요. 오케스트레이터(현재 세션)가 `Task` 도구로 직접 spawn:

```
Task(
  subagent_type: "agent-design-balance",
  description: "타워 공격 사거리 조정",
  prompt: "BP_Team1Tower / BP_Team2Tower 의 AttackRange 를 600 → 800 으로 변경.
           DetectionRange 도 800 → 1000 으로 늘려서 마진 유지."
)
```

여러 MCP 에이전트가 서로 다른 에셋을 만지면 한 메시지에서 **병렬로** Task 호출 가능
(같은 Blueprint 동시 접근만 금지).

**전제조건**: `mcp__unreal-engine__system_control` 로 에디터 연결 확인

### Step 5: 도메인 간 의존성 순서

멀티 도메인 태스크의 실행 순서:

```
1. 프로그래머 먼저 (C++ 클래스/API 가 존재해야 Blueprint/에셋 작업 가능)
2. 기획자 + 아트 병렬 (프로그래머 완료 후)
3. 통합 검증
```

---

## 머지/검증 순서

### Phase 1: 프로그래머 머지 (순차)
1. `agent-design-docs` (코드 충돌 없음)
2. `agent-prog-ui` (최소 외부 의존성)
3. `agent-prog-anim` (AnimInstance, Character 의존)
4. `agent-prog-object` (중간 결합도)
5. `agent-prog-character` (enum 소유)
6. `agent-prog-ai` (최고 결합도)
→ 각 단계 후 `agent-build-verify`

### Phase 2: 기획자 검증 (머지 불필요)
- `agent-design-balance`: `get_property` 로 값 확인
- `agent-design-level`: `get_level_actors` 로 배치 확인

### Phase 3: 아트 검증 (머지 불필요)
- `agent-art-visual`: 머티리얼 적용 확인
- `agent-art-vfx`: VFX 확인
- `agent-art-anim`: 애니메이션 확인
→ `capture_viewport` 로 시각 검증

---

## 주의사항

- `.claude/coordination/INTERFACE_CONTRACTS.md` — 에이전트 간 안정 인터페이스 확인
- `.claude/coordination/CROSS_DOMAIN_REQUESTS.md` — 도메인 간 요청 확인
- `.claude/coordination/ASSET_OWNERSHIP.md` — 에셋 소유권 충돌 방지
- `.claude/coordination/AGENT_STATUS.md` — 활성 세션 추적
- 한 에이전트만 필요한 소규모 작업은 worktree 없이 직접 spawn 가능
- MCP 에이전트는 동일 Blueprint 에 동시 접근 금지 — 같은 메시지에서 병렬 spawn 시 자산 겹침 확인
