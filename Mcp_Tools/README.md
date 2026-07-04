# MCP Tools — 새 컴퓨터에서의 셋업 가이드

이 폴더는 TDProject에서 사용하는 **두 개의 MCP(Model Context Protocol) 서버**를
다른 컴퓨터에서도 똑같이 재현할 수 있도록 모아둔 곳입니다.

| MCP 서버 | 역할 | 실행 방식 |
|---------|------|----------|
| `unreal-engine` | UE 에디터 자동화(액터/에셋/레벨/머티리얼/AI 제어 등) | **`McpAutomationBridge`(0.5.30) 플러그인**이 에디터 안에서 WebSocket 서버(`:8090`/`:8091`)를 띄우고, Node 패키지 **`unreal-engine-mcp-server`**(`npx`)가 거기에 붙어 Claude에 MCP를 중계. |
| `unreal-rag` | 프로젝트 C++ 코드베이스 의미 검색(RAG) | **`ue_rag_mcp.py`** 가 ChromaDB + HuggingFace 임베딩으로 `Source/` 인덱싱 |

---

## 1. 사전 요구사항

- **OS**: Windows 10/11 (macOS도 가능, 경로만 조정)
- **Python**: 3.10+ (권장: 시스템 PATH에서 `python` 호출 가능해야 함)
- **Node.js**: 18+ (`npx` 사용)
- **Unreal Engine**: 5.7 (Epic 공식 또는 GitHub 소스 빌드)
- **Claude Desktop** 또는 **Claude Code**

---

## 1-1. Unreal Engine 엔진 경로 등록

엔진 설치 위치는 컴퓨터마다 다릅니다(설치본 vs 소스 빌드, 드라이브 다름).
빌드 명령·툴 호출이 일관되도록 **환경변수 `UE_ROOT`** 에 엔진 루트를
등록하는 것을 권장합니다.

### 등록 방법 (Windows, PowerShell — 관리자 권한 불필요)

```powershell
# 자기 머신의 엔진 루트로 치환
setx UE_ROOT "G:\UnrealEngine_Release\UE_5.7"
```

`setx` 는 **새 터미널부터** 적용됩니다. 등록 후 PowerShell/cmd 를 새로 열고
`echo $env:UE_ROOT` 또는 `echo %UE_ROOT%` 로 확인하세요.

### 흔한 설치 위치

| 설치 형태 | 일반적 경로 예시 |
|----------|----------------|
| Epic Games Launcher (설치본) | `C:\Program Files\Epic Games\UE_5.7` |
| GitHub 소스 빌드 | 사용자가 클론한 임의 폴더 (예: `G:\UnrealEngine_Release\UE_5.7`) |

`UE_ROOT\Engine\Build\BatchFiles\Build.bat` 가 존재하면 올바른 경로입니다.

### 빌드 명령 (UE_ROOT 사용)

```powershell
& "$env:UE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    TDProject Win64 Development `
    -Project="<PROJECT_ROOT>\TDProject.uproject"
```

### Claude Code 사용자에게

- 머신별 정보이므로 `.claude/settings.local.json` 의 permission 에 본인의
  실제 Build.bat 경로를 명시하거나, 사용자 auto-memory(`MEMORY.md`)에
  `UE_ROOT` 값을 메모해 두면 다음 세션에서 Claude 가 빌드 명령을
  올바르게 만들 수 있습니다. `settings.local.json` 은 git 추적이
  되지 않으므로(`.gitignore`) 안전하게 머신별 경로를 박아도 됩니다.

#### 자동 셋업 프롬프트 (복붙용)

새 컴퓨터에서 프로젝트를 클론한 직후, **그 컴퓨터의 Claude Code 세션**에
아래 프롬프트를 그대로 붙여넣으면 경로 검증 → 환경변수 등록 명령 출력 →
auto-memory 갱신 → `.claude/settings.local.json` 패치까지 한 번에
처리합니다.

````
이 프로젝트는 머신별로 Unreal Engine 5.7 엔진 루트가 다릅니다.
이 컴퓨터의 엔진 경로를 등록하고 기억해 줘.

1. 내 컴퓨터의 엔진 루트는 다음과 같아:
   <여기에 본인 경로 적기, 예: D:\Epic\UE_5.7  또는  C:\Program Files\Epic Games\UE_5.7>

2. 해줄 일:
   (a) 위 경로 안에 `Engine\Build\BatchFiles\Build.bat` 가 실제로 있는지 확인.
       없으면 잘못된 경로라고 알려주고 멈출 것.
   (b) 환경변수 UE_ROOT 등록 명령(`setx UE_ROOT "<경로>"`)을 출력해서
       내가 직접 새 PowerShell 창에서 실행하도록 안내해 줘.
       (setx 는 새 터미널부터 적용되므로 직접 실행하지는 말 것.)
   (c) auto-memory 의 `reference_ue_engine_path.md` 를 이 컴퓨터의 경로로
       갱신하고 (없으면 새로 만들고), `MEMORY.md` 인덱스 한 줄도 맞춰 줘.
   (d) `.claude/settings.local.json` 의 permissions.allow 에 들어 있는
       Build.bat 항목이 옛 경로면, 새 경로로 교체해 줘.
       (이 파일은 git 추적 안 됨 → 머신별로 안전하게 갱신 가능)

3. 빌드 실행은 절대 직접 하지 마. 명령만 보여주고 내가 직접 실행함.

자세한 절차는 `Mcp_Tools/README.md` §1-1 참고.
````

> **메모**: 위 프롬프트의 `<여기에 본인 경로 적기>` 자리만 자기 머신
> 경로로 바꿔서 붙여넣으면 됩니다. Claude 가 잘못된 경로를 알아서
> 거르고, 환경변수 등록 명령은 사용자가 직접 실행하도록만 안내합니다.

---

## 2. unreal-engine MCP — 셋업

`Plugins/McpAutomationBridge/`(0.5.30) 플러그인이 에디터 안에서 **WebSocket 서버**(기본
`:8090`, `:8091`)를 띄우고, Node 패키지 **`unreal-engine-mcp-server`** 가 거기에 붙어
Claude에 MCP(stdio)를 중계합니다. Node 서버는 `npx` 로 자동 실행되어 사전 설치는 불필요합니다
(Node.js 18+ 필요). 설정은 §4-2 참고.

확인 방법:
1. 프로젝트를 빌드하고 에디터 실행 (플러그인 첫 로드 시 리빌드를 요구할 수 있음 → 빌드)
2. `Edit → Plugins`에서 **MCP Automation Bridge** 가 Enabled 인지 확인
3. 출력 로그에서 WebSocket 서버가 `8091`(또는 `8090`) 포트에서 listening 중이라는 메시지 확인
   (포트/TLS 는 `Project Settings → Plugins → MCP Automation Bridge` 에서 조정)

---

## 3. unreal-rag MCP — 셋업

### 3-1. Python 의존성 설치

```bash
cd "<PROJECT_ROOT>\Mcp_Tools"
python -m pip install -r requirements.txt
```

### 3-2. 경로 자동 추론

`ue_rag_mcp.py`는 다음 순서로 경로를 결정합니다.

1. 환경변수 `TDPROJECT_SOURCE` / `TDPROJECT_RAG_DB` 가 있으면 사용
2. 없으면 스크립트 위치 기준 `../Source`와 `./chroma_db`로 자동 추론
3. 둘 다 실패하면 스크립트 상단 `PROJECT_ROOT_FALLBACK` 사용 (기본: `E:\Unreal Project\TDProject`)

따라서 이 폴더 위치만 맞춰두면(`<PROJECT_ROOT>/Mcp_Tools/ue_rag_mcp.py`) **별도 수정 없이 동작**합니다.

다른 위치에서 실행하려면:

```powershell
$env:TDPROJECT_SOURCE = "D:\path\to\TDProject\Source"
$env:TDPROJECT_RAG_DB = "D:\path\to\TDProject\Mcp_Tools\chroma_db"
```

### 3-3. 인덱싱 & 자동 갱신 (stale 감지)

서버를 처음 띄우면 `Source/`의 `.h`/`.cpp` 파일을 모두 임베딩하여 `chroma_db/`에 저장합니다.
시간이 다소 걸립니다(수십 초~수분).

**자동 stale 감지**: 인덱싱 시점의 "소스 최신 수정시각(mtime)"을 `chroma_db/index_meta.json`에
기록해 두고, 서버 시작 시 현재 `Source/`의 최신 mtime과 비교합니다.

- 소스 변경 없음 → 기존 인덱스를 **로드만** (빠름)
- 소스가 더 최신(코드 추가/수정) → **자동으로 전체 재인덱싱** (기존 DB를 비우고 재생성)

즉 코드를 바꾼 뒤 서버(또는 MCP 클라이언트)를 재시작하면 인덱스가 알아서 최신으로 갱신됩니다.
별도의 수동 삭제는 보통 불필요합니다. (강제로 전체 재빌드하려면 서버 종료 후
`chroma_db/` 폴더를 지우고 재시작하면 됩니다.)

> `index_meta.json`이 없는 구버전 인덱스는 stale로 간주되어, 패치 후 첫 실행 시 1회 자동 재인덱싱됩니다.

`chroma_db/` 디렉토리는 `.gitignore`에 등록되어 있으므로 커밋되지 않습니다.
새 컴퓨터에서는 자동으로 재생성됩니다.

---

## 4. Claude Desktop / Claude Code 설정 등록

### 4-1. 설정 파일 위치

| 클라이언트 | Windows 경로 |
|-----------|-------------|
| Claude Desktop | `%APPDATA%\Claude\claude_desktop_config.json` |
| Claude Desktop (Store) | `%LOCALAPPDATA%\Packages\Claude_pzs8sxrjxfjjc\LocalCache\Roaming\Claude\claude_desktop_config.json` |
| Claude Code | `%USERPROFILE%\.claude\config.json` (또는 프로젝트별 `.claude/settings.json`) |
| 안티그라비티(Antigravity) | `%USERPROFILE%\.gemini\config\mcp_config.json` (IDE·CLI 공용) |

### 4-2. 템플릿 적용

`Mcp_Tools/claude_desktop_config.example.json`을 위 위치에 복사한 뒤
`${PROJECT_ROOT}` 부분을 실제 경로로 치환합니다.

예시 (Windows):

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "npx",
      "args": ["-y", "unreal-engine-mcp-server"],
      "env": { "UE_PROJECT_PATH": "E:\\Unreal Project\\TDProject\\TDProject.uproject" }
    },
    "unreal-rag": {
      "command": "python",
      "args": ["E:\\Unreal Project\\TDProject\\Mcp_Tools\\ue_rag_mcp.py"]
    }
  }
}
```

> 백슬래시는 JSON에서 `\\` 로 두 번 써야 합니다.

### 4-3. 환경변수 사용 (선택)

경로를 직접 박지 않고 환경변수로 빼고 싶다면:

```json
{
  "mcpServers": {
    "unreal-rag": {
      "command": "python",
      "args": ["${TDPROJECT_ROOT}\\Mcp_Tools\\ue_rag_mcp.py"],
      "env": {
        "TDPROJECT_SOURCE": "${TDPROJECT_ROOT}\\Source",
        "TDPROJECT_RAG_DB": "${TDPROJECT_ROOT}\\Mcp_Tools\\chroma_db"
      }
    }
  }
}
```

### 4-4. 안티그라비티(Antigravity) IDE

안티그라비티는 프로젝트 `.mcp.json` 이 아니라 **`~/.gemini/config/mcp_config.json`**
(IDE·CLI 공용)을 읽습니다. ⚠️ 이 파일이 비어 있으면 IDE 의 MCP 목록에 "No MCP Servers"
로 뜨므로, 아래처럼 표준 `mcpServers` 형식으로 직접 채웁니다. IDE 안에서는
`Settings → Customizations → Add MCP +` 또는 agent 패널 `...` →
`Manage MCP Servers → View raw config` 로 같은 파일을 편집할 수 있습니다.

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "C:\\Program Files\\nodejs\\npx.cmd",
      "args": ["-y", "unreal-engine-mcp-server"],
      "env": {
        "UE_PROJECT_PATH": "E:\\Unreal Project\\TDProject\\TDProject.uproject",
        "MCP_AUTOMATION_PORT": "8091",
        "PATH": "C:\\Program Files\\nodejs;C:\\Windows\\System32;C:\\Windows;C:\\Windows\\System32\\Wbem"
      }
    },
    "unreal-rag": {
      "command": "python",
      "args": ["E:\\Unreal Project\\TDProject\\Mcp_Tools\\ue_rag_mcp.py"]
    }
  }
}
```

> - **원격(HTTP) 서버는 `url` 이 아니라 `serverUrl` 키**를 씁니다(안티그라비티 규격 — Cursor/VS Code 와 다름).
> - Windows 에서 `npx` 가 PATH 에서 안 잡히면 위처럼 `npx.cmd` 절대경로 + `env.PATH` 를 명시하세요.
> - 작성 후 IDE 의 `Refresh ↻` 로 인식시키고, 에디터를 켜두면 `unreal-engine` 이 connected 됩니다.
> - `mcp_config.json` 은 홈 디렉토리(머신별 절대경로)라 **git 추적 대상이 아닙니다** — 새 머신에서는 위 형식으로 다시 작성.

### 4-5. blender MCP (선택 — 라이브 Blender 제어 / 애니·메시 작업)

Claude 가 Blender 를 직접 제어(씬 조회·파이썬 실행·Hyper3D/Sketchfab 에셋 생성)하려면
**`blender-mcp`** 서버를 추가합니다. `uvx`(uv 툴러너)로 실행하며, **Blender 쪽에 BlenderMCP 애드온**이
켜져 있어야 연결됩니다.

```json
{
  "mcpServers": {
    "blender": {
      "type": "stdio",
      "command": "uvx",
      "args": ["blender-mcp"]
    }
  }
}
```

> - `uvx` 가 PATH 에 없으면 절대경로로: `C:\\Users\\<user>\\AppData\\Local\\Programs\\Python\\Python311\\Scripts\\uvx.exe`.
> - **uv 설치**: `pip install uv` (또는 https://docs.astral.sh/uv/). 첫 실행 시 `uvx` 가 `blender-mcp` 를 자동 다운로드.
> - Blender 에서 **BlenderMCP 애드온**을 설치·활성화 → 사이드바(N) → **BlenderMCP → Start MCP Server** 로 서버를 켜야 `blender` 가 connected.
> - ⚠️ **애니 자가검증 렌더(`render_anim_preview.py`/`render_skin_preview.py`/`bl_render_uefbx.py`)는 이 MCP 서버가 필요 없습니다** — 헤드리스 Blender 를 subprocess 로 직접 띄우는 방식(창 없는 렌더). blender MCP 는 **라이브 Blender 조작(뷰포트·생성)** 용.

> **⚠️ 프로젝트 `.mcp.json` vs 로컬 절대경로**: 저장소에 커밋된 `.mcp.json` 은 **제네릭 템플릿**(`npx`/`python`/`uvx` 상대 커맨드)입니다.
> 머신마다 `npx`/`uvx`/`python` 이 PATH 에서 안 잡히면 위처럼 **절대경로 + `env.PATH`** 로 로컬 오버라이드하되, **그 머신별 `.mcp.json` 변경은 커밋하지 마세요**(로컬 유지). 새 머신 셋업은 이 문서 형식대로 다시 작성.

---

## 5. 동작 확인

Claude를 재시작한 뒤 다음을 시도해 보세요.

- **unreal-engine**: 에디터를 켠 상태에서 "현재 레벨에 있는 액터 목록 보여줘" → 액터 리스트 응답
- **unreal-rag**: "AOSCharacter::SetTeam이 어디서 호출되는지 검색해줘" → 코드 청크 응답

응답이 없거나 오류가 나면:
- Claude 로그(`%APPDATA%\Claude\logs\`)에서 MCP 서버 stderr 확인
- `python "<PROJECT_ROOT>\Mcp_Tools\ue_rag_mcp.py"` 직접 실행하여 경로/의존성 출력 확인

---

## 6. 파일 목록

| 파일 | 설명 | Git 추적 |
|------|------|---------|
| `ue_rag_mcp.py` | RAG MCP 서버 본체 | O |
| `requirements.txt` | Python 의존성 | O |
| `claude_desktop_config.example.json` | Claude Desktop/Code MCP 설정 템플릿 | O |
| `README.md` | 이 파일 | O |
| `chroma_db/` | 벡터 DB 캐시 (자동 생성) | X (.gitignore) |
