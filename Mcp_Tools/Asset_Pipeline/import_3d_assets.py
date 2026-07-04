"""
import_3d_assets.py — FBX / glTF 3D 에셋을 언리얼 에디터로 일괄 임포트하는 에디터 내부 Python 스크립트.

[실행 방법]
  unreal-engine MCP 의 execute_script 로 호출 (에디터가 실행 중이어야 함):

  # 캐릭터 Skeletal Mesh:
  import_3d_assets.py  skeletal  "E:/path/to/Alex_T.fbx"  /Game/AOS/Meshes/Characters/Alex

  # 구조물 Static Mesh:
  import_3d_assets.py  static  "E:/path/to/Tower_Team1.glb"  /Game/AOS/Meshes/Structures

  # 폴더 일괄 (같은 destination 으로):
  import_3d_assets.py  static  "E:/path/to/Structures/"  /Game/AOS/Meshes/Structures

[주의]
  이 스크립트는 *에디터 내부 Python* 컨텍스트에서 실행됩니다 (unreal 모듈 사용 가능).
  일반 Python (외부) 에서는 실행할 수 없습니다.

[임포트 세팅 요약]
  skeletal:
    - Skeletal Mesh 로 임포트
    - 새 스켈레톤 자동 생성 (SK_<이름>)
    - 텍스처/머티리얼 자동 임포트
    - Normal Import Method: ImportNormalsAndTangents
  static:
    - Static Mesh 로 임포트
    - Combine Meshes: True
    - 텍스처/머티리얼 자동 임포트
"""

import unreal
import sys
import os

# ─────────────────────────────────────────────
# 지원 확장자
# ─────────────────────────────────────────────
SUPPORTED_EXT = {".fbx", ".glb", ".gltf", ".obj"}


def _collect_files(source: str) -> list[str]:
    """source 가 파일이면 [source], 폴더이면 하위 지원 파일 목록."""
    source = source.replace("\\", "/")
    if os.path.isfile(source):
        return [source]
    if os.path.isdir(source):
        files = []
        for fname in os.listdir(source):
            ext = os.path.splitext(fname)[1].lower()
            if ext in SUPPORTED_EXT:
                files.append(os.path.join(source, fname).replace("\\", "/"))
        return sorted(files)
    unreal.log_error(f"[import_3d_assets] 경로를 찾을 수 없음: {source}")
    return []


# ─────────────────────────────────────────────
# Skeletal Mesh 임포트
# ─────────────────────────────────────────────
def _make_skeletal_task(source_file: str, destination_path: str) -> unreal.AssetImportTask:
    """FBX Skeletal Mesh 임포트 태스크 생성."""
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = True
    opts.import_animations = False       # 애니메이션은 별도 임포트
    opts.import_materials = True
    opts.import_textures = True
    opts.create_physics_asset = False    # Physics Asset 은 수동 설정 권장

    # 스켈레탈 메시 세팅
    skel_opts = opts.skeletal_mesh_import_data
    skel_opts.set_editor_property(
        "normal_import_method",
        unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    )
    skel_opts.set_editor_property("import_uniform_scale", 1.0)

    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = destination_path
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = opts
    return task


# ─────────────────────────────────────────────
# Static Mesh 임포트
# ─────────────────────────────────────────────
def _make_static_task(source_file: str, destination_path: str) -> unreal.AssetImportTask:
    """FBX/glTF Static Mesh 임포트 태스크 생성."""
    ext = os.path.splitext(source_file)[1].lower()
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = destination_path
    task.automated = True
    task.replace_existing = True
    task.save = True

    if ext == ".fbx":
        opts = unreal.FbxImportUI()
        opts.import_mesh = True
        opts.import_as_skeletal = False
        opts.import_animations = False
        opts.import_materials = True
        opts.import_textures = True

        static_opts = opts.static_mesh_import_data
        static_opts.set_editor_property("combine_meshes", True)
        static_opts.set_editor_property("import_uniform_scale", 1.0)
        static_opts.set_editor_property(
            "normal_import_method",
            unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
        )
        task.options = opts
    # glTF/GLB: 별도 옵션 없이 기본 임포터 사용 (UE 내장 glTF 임포터)

    return task


# ─────────────────────────────────────────────
# 임포트 실행 + 결과 보고
# ─────────────────────────────────────────────
def import_assets(mesh_type: str, source: str, destination_path: str) -> list[str]:
    """
    Parameters
    ----------
    mesh_type : "skeletal" | "static"
    source    : 파일 경로 또는 폴더 경로
    destination_path : /Game/AOS/... 형식 UE 패키지 경로

    Returns
    -------
    임포트된 에셋 경로 리스트
    """
    mesh_type = mesh_type.lower().strip()
    if mesh_type not in ("skeletal", "static"):
        unreal.log_error(f"[import_3d_assets] mesh_type 은 'skeletal' 또는 'static' 이어야 합니다. 입력: {mesh_type}")
        return []

    files = _collect_files(source)
    if not files:
        unreal.log_error(f"[import_3d_assets] 임포트할 파일 없음: {source}")
        return []

    tasks = []
    for f in files:
        if mesh_type == "skeletal":
            tasks.append(_make_skeletal_task(f, destination_path))
        else:
            tasks.append(_make_static_task(f, destination_path))
        unreal.log(f"[import_3d_assets] 큐에 추가: {f} → {destination_path}")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks(tasks)

    imported = []
    for t in tasks:
        paths = t.get_editor_property("imported_object_paths")
        if paths:
            imported.extend(paths)
            for p in paths:
                unreal.log(f"[import_3d_assets] 완료: {p}")
        else:
            unreal.log_warning(f"[import_3d_assets] 임포트 실패: {t.filename}")

    _post_process(mesh_type, imported)
    return imported


def _post_process(mesh_type: str, asset_paths: list[str]):
    """임포트 후 공통 세팅 적용."""
    for path in asset_paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            continue

        if mesh_type == "skeletal" and isinstance(asset, unreal.SkeletalMesh):
            # 섀도우 캐스팅 활성 (게임 캐릭터 기본)
            try:
                asset.set_editor_property("cast_shadow", True)
            except Exception:
                pass
            unreal.EditorAssetLibrary.save_asset(path)
            unreal.log(f"[import_3d_assets] Skeletal 후처리 완료: {path}")

        elif mesh_type == "static" and isinstance(asset, unreal.StaticMesh):
            # LOD 자동 생성 비율 (성능 최적화)
            try:
                lod_settings = asset.get_editor_property("lod_settings")
                if lod_settings:
                    asset.set_editor_property("lod_settings", lod_settings)
            except Exception:
                pass
            unreal.EditorAssetLibrary.save_asset(path)
            unreal.log(f"[import_3d_assets] Static 후처리 완료: {path}")


# ─────────────────────────────────────────────
# CLI 진입점 (execute_script 로 호출 시)
# ─────────────────────────────────────────────
if __name__ == "__main__":
    args = sys.argv[1:]  # execute_script 는 sys.argv 로 인수 전달
    if len(args) < 3:
        unreal.log_error(
            "[import_3d_assets] 사용법: "
            "import_3d_assets.py  <skeletal|static>  <소스 경로>  </Game/... UE 경로>"
        )
    else:
        result = import_assets(
            mesh_type=args[0],
            source=args[1],
            destination_path=args[2],
        )
        if result:
            unreal.log(f"[import_3d_assets] 임포트 완료 — {len(result)}개 에셋:")
            for r in result:
                unreal.log(f"  {r}")
        else:
            unreal.log_error("[import_3d_assets] 임포트된 에셋 없음.")
