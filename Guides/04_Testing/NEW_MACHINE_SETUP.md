# 새 컴퓨터 셋업 가이드

클론만으로는 바로 동작하지 않습니다. 아래 단계를 **순서대로** 진행하세요.

---

## 전체 흐름 한눈에 보기

```
1. 필수 소프트웨어 설치
   ├── Unreal Engine 5.7
   ├── Visual Studio 2026 (or 2022)
   ├── Python 3.11+
   ├── Node.js 18+
   ├── Claude Desktop
   └── Claude Code CLI

2. 저장소 클론

3. RAG MCP 설치
   ├── ue_rag_mcp.py 복사 (C:\Mcp_Tools\)
   └── pip install -r requirements.txt

4. claude_desktop_config.json 설정
   └── Claude Desktop 재시작

5. Unreal Editor 실행 → 플러그인 컴파일

6. 동작 확인
```

---

## 1단계 — 필수 소프트웨어 설치

### Unreal Engine 5.7

1. [Epic Games Launcher](https://www.epicgames.com/store/ko/download) 설치
2. Library → Engine Versions → `5.7` 설치
3. (선택) 설치 옵션에서 **"Editor symbols for debugging"** 체크

### Visual Studio 2026 또는 2022

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

### Claude Desktop

[claude.ai/download](https://claude.ai/download) 에서 설치 후 로그인.

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
git clone https://github.com/wjrm600/TDProject.git C:\UnrealProject\TDProject
cd C:\UnrealProject\TDProject
```

> **경로를 다르게 클론한 경우** 이후 설정 파일들의 경로도 그에 맞게 수정해야 합니다.

---

## 3단계 — RAG MCP 설치

RAG MCP는 C++ 코드를 벡터 DB로 인덱싱해서 Claude가 코드를 검색할 수 있게 해줍니다.

### 3-1. 스크립트 폴더 생성 및 복사

```powershell
mkdir C:\Mcp_Tools
copy C:\UnrealProject\TDProject\Mcp_Tools\ue_rag_mcp.py C:\Mcp_Tools\
```

### 3-2. Python 의존성 설치

```bash
pip install -r C:\UnrealProject\TDProject\Mcp_Tools\requirements.txt
```

설치 패키지: `mcp`, `langchain-community`, `langchain-text-splitters`, `chromadb`, `sentence-transformers`

> 처음 설치는 모델 다운로드 포함 5~10분 소요될 수 있습니다.

### 3-3. 경로 확인 (클론 경로가 다른 경우)

`C:\Mcp_Tools\ue_rag_mcp.py` 상단의 두 경로를 실제 환경에 맞게 수정:

```python
UNREAL_PROJECT_SOURCE_PATH = r"C:\UnrealProject\TDProject\Source"  # 클론한 경로로 수정
DB_DIR = r"C:\Mcp_Tools\chroma_db"                                  # 그대로 사용 권장
```

> **주의**: Python 경로는 반드시 `r"..."` raw string으로 작성하세요.  
> `"C:\UnrealProject\..."` 처럼 쓰면 `\U`가 유니코드 이스케이프로 해석되어 SyntaxError 발생합니다.

---

## 4단계 — claude_desktop_config.json 설정

Claude Desktop이 두 MCP 서버를 인식하도록 설정 파일을 작성합니다.

### 4-1. 설정 파일 위치

```
C:\Users\{사용자명}\AppData\Local\Packages\Claude_pzs8sxrjxfjjc\LocalCache\Roaming\Claude\claude_desktop_config.json
```

> 파일 탐색기 주소창에 `%LOCALAPPDATA%\Packages\Claude_pzs8sxrjxfjjc\LocalCache\Roaming\Claude\` 를 붙여넣으면 바로 이동합니다.

### 4-2. 설정 내용

파일이 없으면 새로 만들고, 있으면 `mcpServers` 항목을 아래와 같이 작성합니다:

```json
{
  "preferences": {
    "coworkWebSearchEnabled": true,
    "sidebarMode": "epitaxy",
    "coworkScheduledTasksEnabled": true,
    "ccdScheduledTasksEnabled": true
  },
  "mcpServers": {
    "unreal-engine": {
      "type": "url",
      "url": "http://localhost:3000/mcp"
    },
    "unreal_rag": {
      "command": "python",
      "args": [
        "C:\\Mcp_Tools\\ue_rag_mcp.py"
      ]
    }
  }
}
```

> **JSON 작성 주의사항**
> - `preferences`와 `mcpServers` 사이에 `,` 쉼표 필수
> - 경로는 `\\` 이중 백슬래시 사용 (JSON 문자열 규칙)
> - `"` 따옴표는 반드시 영문 straight quote 사용 (곱슬 따옴표 `"` 금지)

### 4-3. Claude Desktop 재시작

설정 파일 저장 후 Claude Desktop을 완전히 종료했다가 다시 실행합니다.

---

## 5단계 — Unreal Editor 실행 및 플러그인 컴파일

1. `C:\UnrealProject\TDProject\TDProject.uproject` 더블클릭
2. **"Would you like to rebuild now?"** 팝업 → **Yes**
   - `McpAutomationBridge` 플러그인 포함 전체 C++ 컴파일 (5~15분)
3. 에디터 열린 후 플러그인 활성화 확인:
   - 메뉴 → **Edit → Plugins** → 검색창에 `MCP` 입력
   - `MCP Automation Bridge` → **Enabled** 체크 확인
   - 비활성화 상태면 체크 후 에디터 재시작

> 플러그인이 활성화되면 에디터 실행 중 `http://localhost:3000/mcp` 에 MCP 서버가 자동 기동됩니다.

---

## 6단계 — 동작 확인

### unreal-engine MCP 확인

**Unreal Editor가 열린 상태에서** 터미널에서 포트 확인:

```powershell
netstat -ano | findstr ":3000"
# TCP    127.0.0.1:3000    0.0.0.0:0    LISTENING  <PID>  ← 이렇게 나오면 정상
```

### unreal_rag MCP 확인

Python 문법 및 의존성 확인:

```bash
python -c "import ast; ast.parse(open('C:/Mcp_Tools/ue_rag_mcp.py').read()); print('OK')"
```

### Claude Code에서 MCP 연결 확인

프로젝트 루트에서 Claude Code 실행:

```bash
cd C:\UnrealProject\TDProject
claude
```

Claude Code 세션에서:

```
/mcp
```

`unreal-engine` 서버가 **connected** 상태로 표시되면 정상입니다.

### RAG 첫 실행 (코드 인덱싱)

Claude Desktop에서 `search_unreal_code` 도구를 처음 호출하면 자동으로 인덱싱을 시작합니다.

- 임베딩 모델 다운로드: `intfloat/multilingual-e5-small` (~500MB, 최초 1회)
- C++ 코드 청크 생성 및 ChromaDB 저장: 수분 소요
- 이후 재실행 시에는 기존 DB를 바로 로드하여 빠르게 시작

---

## 트러블슈팅

### unreal-engine MCP가 연결되지 않음

- Unreal Editor가 실행 중인지 확인
- `McpAutomationBridge` 플러그인 활성화 여부 확인
- 방화벽에서 포트 3000 차단 여부 확인

### unreal_rag SyntaxError

경로에 `r"..."` raw string이 누락된 경우입니다:

```python
# 잘못됨
UNREAL_PROJECT_SOURCE_PATH = "C:\UnrealProject\TDProject\Source"

# 올바름
UNREAL_PROJECT_SOURCE_PATH = r"C:\UnrealProject\TDProject\Source"
```

### claude_desktop_config.json 파싱 오류

JSON Validator 사이트(예: [jsonlint.com](https://jsonlint.com))에 내용을 붙여넣어 유효성 확인.
흔한 실수:

- `preferences` 블록 끝 `,` 누락
- 경로에 `\\` 대신 `\` 단일 백슬래시 사용

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
[ ] Visual Studio (C++ 게임 개발 워크로드 포함)
[ ] Python 3.11+ 설치 (PATH 추가 확인)
[ ] Node.js 18+ 설치
[ ] Claude Desktop 설치 + 로그인
[ ] Claude Code CLI 설치 + 로그인
[ ] git clone → C:\UnrealProject\TDProject
[ ] ue_rag_mcp.py → C:\Mcp_Tools\ 복사
[ ] pip install -r requirements.txt
[ ] claude_desktop_config.json 작성
[ ] Claude Desktop 재시작
[ ] TDProject.uproject 열기 → 플러그인 컴파일
[ ] McpAutomationBridge 플러그인 Enabled 확인
[ ] netstat으로 포트 3000 LISTENING 확인
[ ] claude 실행 → /mcp → unreal-engine connected 확인
```
