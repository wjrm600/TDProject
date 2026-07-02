# -*- coding: utf-8 -*-
"""import_ui_kit.py — [UE 에디터 내부] 매니페스트 기반 UI 키트 배치 임포트.

RawAssets/UI_Kit/<name>_final[.._Hover/_Pressed].png →
/Game/AOS/UI/Assets/<ue_name>[_Hover/_Pressed]

설정 = import_ui_assets.py 와 동일 (TEXTUREGROUP_UI / NO_MIPMAPS / TC_EDITOR_ICON)
+ save_asset. slice_margin 은 위젯 배선 계약으로 로그 출력.

실행: 에디터 MCP execute_script 또는
  UnrealEditor-Cmd.exe <proj> -ExecutePythonScript=<이 파일> (커맨드릿은 Slate 크래시)
"""
import json
import os

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
KIT_DIR = os.path.join(SCRIPT_DIR, "RawAssets", "UI_Kit")
MANIFEST = os.path.join(SCRIPT_DIR, "ui_kit_manifest.json")
DEST = "/Game/AOS/UI/Assets"


def import_png(png_path, dest_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png_path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("replace_existing", True)
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    except RuntimeError as e:
        print("IMPORT_WARN %s: %s" % (dest_name, e))
    tex_path = DEST + "/" + dest_name
    tex = unreal.EditorAssetLibrary.load_asset(tex_path)
    if not tex:
        print("IMPORT_FAIL %s" % dest_name)
        return False
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings",
                            unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("compression_settings",
                            unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    unreal.EditorAssetLibrary.save_asset(tex_path, only_if_is_dirty=False)
    print("IMPORT_OK %s" % tex_path)
    return True


with open(MANIFEST, encoding="utf-8-sig") as fp:
    manifest = json.load(fp)

ok = 0
for comp in manifest["components"]:
    base = os.path.join(KIT_DIR, comp["name"] + "_final")
    suffixes = [""]
    if comp["kind"] == "button3":
        suffixes += ["_Hover", "_Pressed"]
    for sfx in suffixes:
        png = base + sfx + ".png"
        if not os.path.exists(png):
            print("IMPORT_SKIP (파일 없음) %s" % png)
            continue
        if import_png(png, comp["ue_name"] + sfx):
            ok += 1
    print("CONTRACT %s slice_margin=%.2f (DrawAs=Box)" %
          (comp["ue_name"], comp.get("slice_margin", 0.0)))
print("IMPORT_KIT_DONE ok=%d" % ok)
