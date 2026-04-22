import os
import sys
import threading
from mcp.server.fastmcp import FastMCP
from langchain_community.document_loaders import DirectoryLoader, TextLoader
from langchain_text_splitters import RecursiveCharacterTextSplitter
from langchain_huggingface import HuggingFaceEmbeddings
from langchain_community.vectorstores import Chroma

# ============================================================
# 설정 — 새 컴퓨터에서 클론 후 이 두 경로를 본인 환경에 맞게 수정하세요
# ============================================================
UNREAL_PROJECT_SOURCE_PATH = r"E:\Unreal Project\TDProject\Source"
DB_DIR = r"E:\Unreal Project\TDProject\Mcp_Tools\chroma_db"
# ============================================================

mcp = FastMCP("Unreal_RAG_Server")
vector_store = None
_init_lock = threading.Lock()
_init_done = False


def _initialize_vector_db():
    """벡터 DB 초기화 (백그라운드 스레드 또는 첫 호출 시 실행)"""
    global vector_store, _init_done

    with _init_lock:
        if _init_done:
            return

        print("언리얼 C++ 코드 인덱싱을 시작합니다 (최초 실행 시 시간 소요)...", file=sys.stderr)
        embeddings = HuggingFaceEmbeddings(model_name="intfloat/multilingual-e5-small")

        # DB가 이미 존재하면 로드만 수행
        if os.path.exists(DB_DIR) and os.listdir(DB_DIR):
            vector_store = Chroma(persist_directory=DB_DIR, embedding_function=embeddings)
            print("기존 벡터 DB를 로드했습니다.", file=sys.stderr)
            _init_done = True
            return

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
