# 새 컴퓨터 셋업 가이드

클론만으로는 바로 동작하지 않습니다. 아래 단계를 **순서대로** 진행하세요.

---

## 전체 흐름 한눈에 보기

```
1. 필수 소프트웨어 설치
   ├── Unreal Engine 5.7
   ├── Visual Studio 2022 이상
   ├── Python 3.11+
   ├── Node.js 18+
   └── Claude Code CLI

2. 저장소 클론

3. RAG MCP 준비
   └── pip install -r requirements.txt   (경로는 자동 추론)

4. Unreal Editor 실행 → 플러그인 컴파일

5. 동작 확인
```

> **MCP 서버 설정 파일(`.mcp.json`)은 저장소에 이미 포함**되어 있어 별도 JSON 작성 불필요.
> 두 MCP 서버(`unreal-engine`, `unreal-rag`) 모두 클론 후 자동 인식됩니다.

---

## 1단계 — 필수 소프트웨어 설치

### Unreal Engine 5.7

1. [Epic Games Launcher](https://www.epicgames.com/store/ko/download) 설치
2. Library → Engine Versions → `5.7` 설치
3. (선택) 설치 옵션에서 **"Editor symbols for debugging"** 체크

#### UE_ROOT 환경변수 등록 (권장)

엔진 설치 위치는 머신마다 다르므로(`C:\Program Files\Epic Games\UE_5.7`,
`G:\UnrealEngine_Release\UE_5.7` 등), 빌드 명령을 일관되게 쓰려면
환경변수 `UE_ROOT` 에 엔진 루트를 등록하세요.

```powershell
# 본인 머신의 엔진 루트로 치환 (관리자 권한 불필요)
setx UE_ROOT "C:\Program Files\Epic Games\UE_5.7"
```

`setx` 는 새 터미널부터 적용됩니다. 등록 후 새 PowerShell 에서
`echo $env:UE_ROOT` 로 확인하세요. `UE_ROOT\Engine\Build\BatchFiles\Build.bat`
가 존재하면 정상입니다.

자세한 내용 및 Claude Code 자동 셋업 프롬프트는
[`Mcp_Tools/README.md` §1-1](../../Mcp_Tools/README.md) 참고.

### Visual Studio 2022 이상

[Visual Studio](https://visualstudio.microsoft.com/) 설치 시 워크로드:

- ✅ **C++를 사용한 게임 개발** (Game development with C++)
- ✅ **C++를 사용한 데스크톱 개발** (Desktop development with C++)

### Python 3.11 이상

[python.org](https://www.python.org/downloads/) 에서 설치.
설치 시 **"Add Python to PATH"** 반드시 체크.

```bash
python --version   # 3.11.x 이상 확인
```

### Node.js 18 이상

[nodejs.org](https://nodejs.org/) 에서 LTS 버전 설치.

```bash
node --version   # v18.x 이상 확인
```

### Claude Code CLI

```bash
npm install -g @anthropic/claude-code
claude --version   # 설치 확인
```

최초 실행 시 브라우저로 OAuth 인증:

```bash
claude
```

---

## 2단계 — 저장소 클론

```bash
git clone https://github.com/wjrm600/TDProject.git
cd TDProject
```

---

## 3단계 — RAG MCP 준비

RAG MCP는 C++ 코드를 벡터 DB로 인덱싱해서 Claude가 코드를 의미론적으로 검색할 수 있게 해줍니다.
스크립트는 이미 저장소 안에 있고, **경로는 자동 추론**되므로 패키지 설치만 하면 됩니다.

### 3-1. 경로 자동 추론 (수정 불필요)

`Mcp_Tools/ue_rag_mcp.py` 는 다음 우선순위로 경로를 결정합니다.

1. 환경변수 `TDPROJECT_SOURCE` / `TDPROJECT_RAG_DB` 가 있으면 사용
2. 없으면 스크립트 위치 기준 `../Source` 와 `./chroma_db` 로 자동 추론
3. 둘 다 실패하면 `PROJECT_ROOT_FALLBACK` 사용

따라서 `Mcp_Tools/` 폴더 위치만 그대로면 **별도 수정 없이 동작**합니다.
다른 위치에서 실행하고 싶다면 환경변수만 지정하세요:

```powershell
$env:TDPROJECT_SOURCE = "D:\path\to\TDProject\Source"
$env:TDPROJECT_RAG_DB = "D:\path\to\TDProject\Mcp_Tools\chroma_db"
```

> **chroma_db 폴더**는 최초 실행 시 자동 생성되며 `.gitignore`에 등록되어 있습니다.

### 3-2. Python 의존성 설치

```bash
pip install -r Mcp_Tools/requirements.txt
```

설치 패키지: `mcp`, `langchain-community`, `langchain-huggingface`, `langchain-text-splitters`, `chromadb`, `sentence-transformers`, `chardet`

> 처음 설치는 모델 다운로드 포함 5~10분 소요될 수 있습니다.

---

## 4단계 — Unreal Editor 실행 및 플러그인 컴파일

1. `TDProject.uproject` 더블클릭
2. **"Would you like to rebuild now?"** 팝업 → **Yes**
   - `McpAutomationBridge` 플러그인 포함 전체 C++ 컴파일 (5~15분)
3. 에디터 열린 후 플러그인 활성화 확인:
   - 메뉴 → **Edit → Plugins** → 검색창에 `MCP` 입력
   - `MCP Automation Bridge` → **Enabled** 체크 확인
   - 비활성화 상태면 체크 후 에디터 재시작

> 플러그인이 활성화되면 에디터 실행 중 `http://localhost:3000/mcp` 에 MCP 서버가 자동 기동됩니다.

---

## 5단계 — 동작 확인

### unreal-engine MCP 확인 (포트 확인)

**Unreal Editor가 열린 상태에서** 터미널에서 포트 확인:

```powershell
netstat -ano | findstr ":3000"
# TCP    127.0.0.1:3000    0.0.0.0:0    LISTENING  <PID>  ← 이렇게 나오면 정상
```

### Claude Code에서 MCP 연결 확인

**Unreal Editor를 열어둔 상태로** 프로젝트 루트에서 Claude Code 실행:

```bash
cd TDProject
claude
```

Claude Code 세션 안에서:

```
/mcp
```

아래 두 서버가 모두 **connected** 상태여야 합니다:

```
unreal-engine   connected   (Unreal Editor 실행 중일 때만 active)
unreal-rag      connected
```

### RAG 첫 실행 (코드 인덱싱)

`unreal-rag` 서버는 Claude Code 시작 시 `Source/` 전체를 임베딩합니다.

- 임베딩 모델 다운로드: `intfloat/multilingual-e5-small` (~500MB, **최초 1회만**)
- C++ 코드 청크 생성 및 ChromaDB 저장: 수분 소요
- 이후 실행부터는 기존 DB를 그대로 로드하여 빠르게 시작

---

## 트러블슈팅

### unreal-engine MCP가 연결되지 않음

- Unreal Editor가 실행 중인지 확인 (`unreal-engine` 서버는 에디터 의존)
- `McpAutomationBridge` 플러그인 활성화 여부 확인
- 방화벽에서 포트 3000 차단 여부 확인

### unreal-rag 가 잘못된 폴더를 인덱싱

스크립트는 자동 추론으로 `Mcp_Tools/../Source` 를 사용합니다. 폴더 구조가
표준과 다르거나, fallback 경로(`PROJECT_ROOT_FALLBACK`)가 잘못 잡힌 경우
환경변수로 명시적으로 지정하세요:

```powershell
$env:TDPROJECT_SOURCE = "C:\UnrealProject\TDProject\Source"
$env:TDPROJECT_RAG_DB = "C:\UnrealProject\TDProject\Mcp_Tools\chroma_db"
```

서버 시작 시 stderr 에 `[ue_rag_mcp] Source: <경로>` / `[ue_rag_mcp] DB: <경로>`
가 출력되므로 실제 사용 중인 경로를 확인할 수 있습니다.

### .claude/settings.local.json 경로 오류

이 파일에는 현재 컴퓨터의 절대 경로가 포함될 수 있습니다.
경로 관련 오류가 발생하면 해당 파일을 직접 열어 경로를 수정하세요.

### 빌드 실패 시

플러그인 컴파일 오류가 발생하면 캐시 삭제 후 재시도:

```powershell
Remove-Item -Recurse -Force Binaries, Intermediate, Plugins\McpAutomationBridge\Binaries, Plugins\McpAutomationBridge\Intermediate
# TDProject.uproject 다시 더블클릭
```

---

## 체크리스트

```
[ ] Unreal Engine 5.7 설치
[ ] UE_ROOT 환경변수 등록 (setx UE_ROOT "<엔진 루트>")
[ ] Visual Studio 2022+ (C++ 게임 개발 워크로드 포함)
[ ] Python 3.11+ 설치 (PATH 추가 확인)
[ ] Node.js 18+ 설치
[ ] Claude Code CLI 설치 + 로그인
[ ] git clone
[ ] pip install -r Mcp_Tools/requirements.txt
[ ] TDProject.uproject 열기 → Yes (플러그인 컴파일)
[ ] McpAutomationBridge 플러그인 Enabled 확인
[ ] netstat으로 포트 3000 LISTENING 확인
[ ] claude 실행 → /mcp → unreal-engine + unreal-rag connected 확인
```
