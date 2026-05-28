import os
import sys
import json
import shutil
import threading
from mcp.server.fastmcp import FastMCP
from langchain_community.document_loaders import DirectoryLoader, TextLoader
from langchain_text_splitters import RecursiveCharacterTextSplitter
from langchain_huggingface import HuggingFaceEmbeddings
from langchain_community.vectorstores import Chroma

# ============================================================
# 경로 설정 — 우선순위:
#   1) 환경변수 TDPROJECT_SOURCE / TDPROJECT_RAG_DB
#   2) 스크립트 위치 기준 자동 추론 (Mcp_Tools/ue_rag_mcp.py → ../Source, ./chroma_db)
#   3) 위 두 경로가 모두 실패하면 아래 PROJECT_ROOT_FALLBACK 사용
# 다른 컴퓨터로 이전 시: 위치만 맞춰두면 자동 추론됨. 이전 머신과 동일하게
# 사용하려면 환경변수만 지정하면 됨.
# ============================================================
PROJECT_ROOT_FALLBACK = r"E:\Unreal Project\TDProject"

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_AUTO_PROJECT_ROOT = os.path.dirname(_THIS_DIR)  # Mcp_Tools/ → 프로젝트 루트

UNREAL_PROJECT_SOURCE_PATH = (
    os.environ.get("TDPROJECT_SOURCE")
    or os.path.join(_AUTO_PROJECT_ROOT, "Source")
)
if not os.path.isdir(UNREAL_PROJECT_SOURCE_PATH):
    UNREAL_PROJECT_SOURCE_PATH = os.path.join(PROJECT_ROOT_FALLBACK, "Source")

DB_DIR = (
    os.environ.get("TDPROJECT_RAG_DB")
    or os.path.join(_THIS_DIR, "chroma_db")
)

print(f"[ue_rag_mcp] Source: {UNREAL_PROJECT_SOURCE_PATH}", file=sys.stderr)
print(f"[ue_rag_mcp] DB    : {DB_DIR}", file=sys.stderr)
# ============================================================

mcp = FastMCP("Unreal_RAG_Server")
vector_store = None
_init_lock = threading.Lock()
_init_done = False

# 인덱싱 시점의 "소스 최신 mtime" 을 기록하는 메타 파일.
# 시작 시 현재 소스의 최신 mtime 과 비교 → 더 최신이면 자동 전체 재인덱싱(stale 감지).
_META_PATH = os.path.join(DB_DIR, "index_meta.json")
_SOURCE_EXTS = (".h", ".hpp", ".hh", ".hxx", ".c", ".cpp", ".cc", ".cxx")


def _latest_source_mtime():
    """Source 트리에서 가장 최근에 수정된 헤더/구현 파일의 mtime (없으면 0.0)."""
    latest = 0.0
    for root, _dirs, files in os.walk(UNREAL_PROJECT_SOURCE_PATH):
        for name in files:
            if name.lower().endswith(_SOURCE_EXTS):
                try:
                    m = os.path.getmtime(os.path.join(root, name))
                except OSError:
                    continue
                if m > latest:
                    latest = m
    return latest


def _read_indexed_mtime():
    """이전 인덱싱이 기록한 소스 mtime (메타 없거나 손상 시 0.0 → 재인덱싱 유도)."""
    try:
        with open(_META_PATH, "r", encoding="utf-8") as fp:
            return float(json.load(fp).get("source_mtime", 0.0))
    except (OSError, ValueError, TypeError):
        return 0.0


def _write_indexed_mtime(mtime):
    try:
        with open(_META_PATH, "w", encoding="utf-8") as fp:
            json.dump({"source_mtime": mtime}, fp)
    except OSError as e:
        print(f"[ue_rag_mcp] index_meta 기록 실패: {e}", file=sys.stderr)


def _initialize_vector_db():
    """벡터 DB 초기화 (백그라운드 스레드 또는 첫 호출 시 실행).

    소스가 인덱스보다 최신이면 자동으로 전체 재인덱싱한다(stale 감지).
    """
    global vector_store, _init_done

    with _init_lock:
        if _init_done:
            return

        embeddings = HuggingFaceEmbeddings(model_name="intfloat/multilingual-e5-small")

        latest_src = _latest_source_mtime()
        db_exists = os.path.exists(DB_DIR) and os.listdir(DB_DIR)
        indexed_mtime = _read_indexed_mtime() if db_exists else 0.0

        # DB 존재 + 메타 있음 + 소스가 더 최신이 아님 → 로드만 (재임베딩 생략)
        if db_exists and indexed_mtime > 0.0 and indexed_mtime >= latest_src:
            vector_store = Chroma(persist_directory=DB_DIR, embedding_function=embeddings)
            print("기존 벡터 DB를 로드했습니다. (소스 변경 없음)", file=sys.stderr)
            _init_done = True
            return

        # 재인덱싱 필요 — 구버전/오래된 인덱스가 있으면 비우고 새로 만든다.
        if db_exists:
            print("소스가 인덱스보다 최신입니다 — 전체 재인덱싱합니다.", file=sys.stderr)
            try:
                shutil.rmtree(DB_DIR)
            except OSError as e:
                print(f"[ue_rag_mcp] 기존 DB 삭제 실패(계속 진행): {e}", file=sys.stderr)
        else:
            print("언리얼 C++ 코드 인덱싱을 시작합니다 (최초 실행 시 시간 소요)...", file=sys.stderr)

        # .h / .cpp 파일 로드
        loader = DirectoryLoader(
            UNREAL_PROJECT_SOURCE_PATH,
            glob="**/*.[hc]*",
            loader_cls=TextLoader,
            loader_kwargs={"encoding": "utf-8", "autodetect_encoding": False},
            silent_errors=True,  # 인코딩 오류 파일은 건너뜀
        )
        docs = loader.load()

        text_splitter = RecursiveCharacterTextSplitter(
            chunk_size=1000,
            chunk_overlap=200,
            separators=["\n\n", "\n", "}", " ", ""],
        )
        chunks = text_splitter.split_documents(docs)

        vector_store = Chroma.from_documents(
            documents=chunks,
            embedding=embeddings,
            persist_directory=DB_DIR,
        )
        _write_indexed_mtime(latest_src)
        print(f"인덱싱 완료! 총 {len(chunks)}개의 코드 청크가 저장되었습니다.", file=sys.stderr)
        _init_done = True


@mcp.tool()
def search_unreal_code(query: str) -> str:
    """
    언리얼 프로젝트의 C++ 코드베이스(클래스, 함수, 구조체 등)를 검색합니다.
    로직 분석이나 버그 추적을 위해 코드를 찾을 때 이 도구를 사용하세요.
    """
    # 아직 초기화 안 됐으면 여기서 수행 (첫 호출 시 약간 느릴 수 있음)
    if not _init_done:
        _initialize_vector_db()

    if not vector_store:
        return "오류: 벡터 DB 초기화에 실패했습니다."

    results = vector_store.similarity_search(query, k=4)

    formatted_results = []
    for doc in results:
        source = doc.metadata.get("source", "Unknown")
        formatted_results.append(f"--- [파일: {source}] ---\n{doc.page_content}\n")

    return "\n".join(formatted_results)


# 서버 시작 즉시 응답 가능하도록 DB 초기화는 백그라운드에서 수행
threading.Thread(target=_initialize_vector_db, daemon=True).start()

if __name__ == "__main__":
    mcp.run()
