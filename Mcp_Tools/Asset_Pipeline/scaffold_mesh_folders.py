"""
scaffold_mesh_folders.py — Content/AOS/Meshes 하위 폴더 구조를 에디터에서 생성하는 스크립트.

[실행 방법] PIE 종료 후, MCP execute_python 으로:
  # 방법 A: 인라인 code 파라미터로
  code = open("E:/Unreal Project/TDProject/Mcp_Tools/Asset_Pipeline/scaffold_mesh_folders.py").read()

  # 방법 B: file 파라미터로
  file = "E:/Unreal Project/TDProject/Mcp_Tools/Asset_Pipeline/scaffold_mesh_folders.py"

[생성 폴더]
  /Game/AOS/Meshes/Characters/Alex/
  /Game/AOS/Meshes/Characters/Alex/Materials/
  /Game/AOS/Meshes/Characters/Vega/
  /Game/AOS/Meshes/Characters/Vega/Materials/
  /Game/AOS/Meshes/Characters/Ken/
  /Game/AOS/Meshes/Characters/Ken/Materials/
  /Game/AOS/Meshes/Characters/Cammy/
  /Game/AOS/Meshes/Characters/Cammy/Materials/
  /Game/AOS/Meshes/Characters/Guile/
  /Game/AOS/Meshes/Characters/Guile/Materials/
  /Game/AOS/Meshes/Characters/Placeholder/
  /Game/AOS/Meshes/Structures/
"""

import unreal

FOLDERS = [
    "/Game/AOS/Meshes/Characters/Alex",
    "/Game/AOS/Meshes/Characters/Alex/Materials",
    "/Game/AOS/Meshes/Characters/Vega",
    "/Game/AOS/Meshes/Characters/Vega/Materials",
    "/Game/AOS/Meshes/Characters/Ken",
    "/Game/AOS/Meshes/Characters/Ken/Materials",
    "/Game/AOS/Meshes/Characters/Cammy",
    "/Game/AOS/Meshes/Characters/Cammy/Materials",
    "/Game/AOS/Meshes/Characters/Guile",
    "/Game/AOS/Meshes/Characters/Guile/Materials",
    "/Game/AOS/Meshes/Characters/Placeholder",
    "/Game/AOS/Meshes/Structures",
]

created = []
already_existed = []

for folder in FOLDERS:
    if unreal.EditorAssetLibrary.does_directory_exist(folder):
        already_existed.append(folder)
    else:
        ok = unreal.EditorAssetLibrary.make_directory(folder)
        if ok:
            created.append(folder)
            unreal.log(f"[scaffold] 생성: {folder}")
        else:
            unreal.log_error(f"[scaffold] 실패: {folder}")

unreal.log(f"[scaffold] 완료 — 신규 {len(created)}개, 기존 {len(already_existed)}개")
if already_existed:
    unreal.log(f"[scaffold] 기존 폴더: {already_existed}")
