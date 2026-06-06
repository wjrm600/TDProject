"""
update_roster_portraits.py — 에디터 내부 Python (unreal 모듈 사용).
CharacterRoster Portrait 필드를 업데이트한다.
  - 인덱스 0~4: 신규 임포트된 T_Portrait_* 텍스처 할당
  - 인덱스 5~19: Portrait = None (플레이스홀더 UE 로고 제거)

실행: UE 에디터 메뉴 → Edit → Execute Python Script → 이 파일 선택
또는: unreal-engine MCP execute_script (if supported)
"""
import unreal

# GameMode 클래스 로드 → CDO 획득
#  load_object 는 BlueprintGeneratedClass(클래스)를 반환한다. 클래스에 직접
#  get_editor_property 하면 "Failed to find property 'character_roster'" 에러.
#  → get_default_object(class) 로 CDO 를 얻어야 인스턴스 프로퍼티 접근 가능.
gm_class_path = "/Game/TDProj_GM.TDProj_GM_C"
gm_class = unreal.load_object(None, gm_class_path)
if gm_class is None:
    unreal.log_error("[update_roster] Failed to load class " + gm_class_path)
    raise RuntimeError("class load failed")
gm_cdo = unreal.get_default_object(gm_class)
if gm_cdo is None:
    unreal.log_error("[update_roster] Failed to get CDO from class")
    raise RuntimeError("CDO get failed")

# 현재 로스터 읽기
roster = gm_cdo.get_editor_property("character_roster")
unreal.log(f"[update_roster] Loaded roster with {len(roster)} entries")

# 초상화 매핑 (인덱스 → 텍스처 경로)
PORTRAIT_MAP = {
    0: "/Game/AOS/UI/Assets/T_Portrait_Alex.T_Portrait_Alex",
    1: "/Game/AOS/UI/Assets/T_Portrait_Vega.T_Portrait_Vega",
    2: "/Game/AOS/UI/Assets/T_Portrait_Ken.T_Portrait_Ken",
    3: "/Game/AOS/UI/Assets/T_Portrait_Cammy.T_Portrait_Cammy",
    4: "/Game/AOS/UI/Assets/T_Portrait_Guile.T_Portrait_Guile",
}

def set_portrait(entry, idx):
    """Portrait 갱신: set_editor_property 우선, EditDefaultsOnly 로 막히면
    import_text(부분 — Portrait 필드만) 폴백. import_text 는 명시한 필드만 갱신하므로
    CharacterClass/DisplayName 은 보존된다."""
    if idx in PORTRAIT_MAP:
        tex = unreal.load_object(None, PORTRAIT_MAP[idx])
        if tex is None:
            unreal.log_error("[update_roster] idx %d: texture not found %s" % (idx, PORTRAIT_MAP[idx]))
            return
        token = "(Portrait=\"/Script/Engine.Texture2D'%s'\")" % PORTRAIT_MAP[idx]
        desc = tex.get_name()
    else:
        tex = None
        token = "(Portrait=None)"
        desc = "None"
    # 1차: set_editor_property
    try:
        entry.set_editor_property("portrait", tex)
        unreal.log("[update_roster] idx %d: Portrait -> %s (set_editor_property)" % (idx, desc))
        return
    except Exception as e:
        unreal.log_warning("[update_roster] idx %d: set_editor_property 실패 (%s) -> import_text 폴백" % (idx, e))
    # 2차: import_text 부분 갱신
    try:
        entry.import_text(token)
        unreal.log("[update_roster] idx %d: Portrait -> %s (import_text)" % (idx, desc))
    except Exception as e2:
        unreal.log_error("[update_roster] idx %d: import_text 실패: %s" % (idx, e2))

updated_roster = []
for i, entry in enumerate(roster):
    set_portrait(entry, i)
    updated_roster.append(entry)

gm_cdo.set_editor_property("character_roster", updated_roster)

# 강제 저장
unreal.EditorAssetLibrary.save_asset("/Game/TDProj_GM", only_if_is_dirty=False)
unreal.log("[update_roster] TDProj_GM saved successfully.")
