# 새 컴퓨터에서 프로젝트 + MCP 환경 구성 가이드

클론만으로는 바로 MCP를 쓸 수 없습니다. 아래 단계를 순서대로 진행하세요.

---

## 사전 확인

MCP가 동작하려면 다음 3가지가 모두 갖춰져야 합니다.

| 구성 요소 | 역할 | 포함 위치 |
|-----------|------|-----------|
| `Plugins/McpAutomationBridge` | UE 에디터 내 MCP HTTP 서버 (포트 3000) | **이미 저장소에 포함** |
| `.mcp.json` | Claude Code가 MCP 서버 주소를 찾는 설정 | **이미 저장소에 포함** |
| Claude Code CLI | MCP 클라이언트 | 새 컴퓨터에 직접 설치 필요 |

---

## 1단계 — 필수 소프트웨어 설치

### 1-1. Unreal Engine 5.7

1. [Epic Games Launcher](https://www.epicgames.com/store/ko/download) 설치
2. Library → Engine Versions → `5.7` 설치
3. 설치 옵션에서 **"Editor symbols for debugging"** 체크 (선택)

### 1-2. Visual Studio 2022

[Visual Studio 2022](https://visualstudio.microsoft.com/) Community 이상 설치.
설치 시 워크로드 선택:

- ✅ **C++를 사용한 게임 개발** (Game development with C++)
- ✅ **C++를 사용한 데스크톱 개발** (Desktop development with C++)

> UE 5.7은 VS 2022 필수입니다. VS 2019는 지원하지 않습니다.

### 1-3. Node.js 18 이상

[nodejs.org](https://nodejs.org/) 에서 LTS 버전 설치.
Claude Code CLI가 Node.js 기반입니다.

```bash
node --version   # v18.x 이상 확인
```

### 1-4. Claude Code CLI

```bash
npm install -g @anthropic/claude-code
claude --version  # 설치 확인
```

설치 후 Anthropic 계정 로그인:

```bash
claude
# 최초 실행 시 브라우저로 OAuth 인증 진행
```

---

## 2단계 — 저장소 클론

```bash
git clone https://github.com/wjrm600/TDProject.git
cd TDProject
```

---

## 3단계 — UE 에디터에서 프로젝트 열기

1. `TDProject.uproject` 파일 더블클릭
2. **"Would you like to rebuild now?"** 팝업 → **Yes** 클릭
   - C++ 소스와 `McpAutomationBridge` 플러그인을 자동으로 컴파일합니다
   - 첫 컴파일은 5~15분 소요

3. 에디터가 열리면 플러그인 활성화 확인:
   - 메뉴 → **Edit → Plugins** → 검색창에 `MCP` 입력
   - `MCP Automation Bridge` → **Enabled** 체크 확인
   - 비활성화 상태라면 체크 후 에디터 재시작

> 플러그인이 활성화되면 에디터 실행 중 `http://localhost:3000/mcp` 에 MCP 서버가 자동으로 기동됩니다.

---

## 4단계 — Claude Code에서 MCP 연결 확인

**에디터를 열어둔 상태에서** 터미널을 프로젝트 루트에서 실행하세요.

```bash
cd TDProject
claude
```

Claude Code 세션 진입 후 MCP 연결 확인:

```
/mcp
```

`unreal-engine` 서버가 **connected** 상태로 표시되면 정상입니다.

---

## 주의사항

### MCP는 에디터가 실행 중일 때만 작동합니다

Claude Code에서 `mcp__unreal-engine__*` 툴을 호출하려면 Unreal Editor가 열려 있어야 합니다. 에디터를 닫으면 포트 3000 서버도 종료됩니다.

### `.claude/settings.local.json` 경로 확인

이 파일에는 현재 컴퓨터의 절대 경로가 일부 포함되어 있습니다.
다른 컴퓨터에서 경로가 다를 경우 해당 파일의 경로를 직접 수정하세요.

```json
// .claude/settings.local.json 예시 — 경로를 실제 환경에 맞게 수정
{
  "hooks": {
    "Stop": [...]  // git push 자동화 훅 (경로 확인 필요)
  }
}
```

### 빌드 실패 시

플러그인 컴파일 오류가 발생하면:

```bash
# 캐시 삭제 후 재시도
rm -rf Binaries Intermediate Plugins/McpAutomationBridge/Binaries Plugins/McpAutomationBridge/Intermediate
# TDProject.uproject 다시 더블클릭
```

---

## 체크리스트 요약

```
[ ] UE 5.7 설치
[ ] Visual Studio 2022 (C++ 게임 개발 워크로드 포함)
[ ] Node.js 18+ 설치
[ ] Claude Code CLI 설치 + 로그인
[ ] git clone
[ ] TDProject.uproject 열기 → 플러그인 컴파일
[ ] McpAutomationBridge 플러그인 활성화 확인
[ ] 에디터 실행 상태에서 claude 실행 → /mcp 연결 확인
```
