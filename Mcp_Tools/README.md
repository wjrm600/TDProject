# MCP Tools — 새 컴퓨터에서의 셋업 가이드

이 폴더는 TDProject에서 사용하는 **두 개의 MCP(Model Context Protocol) 서버**를
다른 컴퓨터에서도 똑같이 재현할 수 있도록 모아둔 곳입니다.

| MCP 서버 | 역할 | 실행 방식 |
|---------|------|----------|
| `unreal-engine` | UE 에디터 자동화(액터/에셋/레벨/머티리얼/AI 제어 등) | **`McpAutomationBridge` 플러그인**이 에디터 안에서 HTTP 서버(`localhost:3000`)를 띄움. Claude는 `mcp-remote`로 접속. |
| `unreal-rag` | 프로젝트 C++ 코드베이스 의미 검색(RAG) | **`ue_rag_mcp.py`** 가 ChromaDB + HuggingFace 임베딩으로 `Source/` 인덱싱 |

---

## 1. 사전 요구사항

- **OS**: Windows 10/11 (macOS도 가능, 경로만 조정)
- **Python**: 3.10+ (권장: 시스템 PATH에서 `python` 호출 가능해야 함)
- **Node.js**: 18+ (`npx` 사용)
- **Unreal Engine**: 5.6 또는 5.7 (커스텀 빌드 또는 Epic 공식)
- **Claude Desktop** 또는 **Claude Code**

---

## 2. unreal-engine MCP — 셋업

이 서버는 별도 설치가 필요 없습니다. 프로젝트의 `Plugins/McpAutomationBridge/`
플러그인이 자동으로 활성화되어 에디터를 켜면 `localhost:3000/mcp`에서 응답합니다.

확인 방법:
1. 프로젝트를 빌드하고 에디터 실행
2. `Edit → Plugins`에서 **MCP Automation Bridge** 가 Enabled 인지 확인
3. 출력 로그에서 `MCP Server listening on http://localhost:3000` 류의 메시지 확인

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

### 3-3. 첫 실행 시 인덱싱

서버를 처음 띄우면 `Source/`의 `.h`/`.cpp` 파일을 모두 임베딩하여 `chroma_db/`에 저장합니다.
시간이 다소 걸리며(수십 초~수분), 이후 실행은 캐시를 그대로 사용합니다.

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

### 4-2. 템플릿 적용

`Mcp_Tools/claude_desktop_config.example.json`을 위 위치에 복사한 뒤
`${PROJECT_ROOT}` 부분을 실제 경로로 치환합니다.

예시 (Windows):

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "npx",
      "args": ["-y", "mcp-remote", "http://localhost:3000/mcp"]
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
