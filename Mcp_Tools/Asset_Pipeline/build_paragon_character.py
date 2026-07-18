"""
build_paragon_character.py — Paragon 히어로를 고유 캐릭터로 편입하는 골든 경로 자동화.

[배경] 2026-07-18 확정: 고유 캐릭터 = Epic Paragon 히어로 애셋(메시/애니)을 리타깃 없이
직접 사용. Kwang(0)/Greystone(1) 2회 수동 검증으로 확립된 K/G 4단계를 이 스크립트로 굳혔다.
캐릭터당 반복 노동(BS/ABP/BP복제/몽타주7/GA·GE8/로스터/배선)을 config 하나로 실행한다.

[실행] 이 스크립트는 UE 에디터 내부 Python 전용(`unreal` 모듈 필요). MCP 로 호출:
    import sys; sys.path.insert(0, r'E:/Unreal Project/TDProject/Mcp_Tools/Asset_Pipeline')
    import importlib, build_paragon_character as bpc; importlib.reload(bpc)
    log = bpc.run(bpc.CONFIGS['Greystone'])          # 또는 아래 CONFIG 스키마로 새 dict
    open(r'E:/.../Saved/Temp/bpc.log','w',encoding='utf-8').write("\n".join(log))
  (execute_python 은 예외 시 stdout 을 버리므로 로그는 파일로 남겨 Read 로 확인.)

[BS 리빌드 = 유일한 MCP 후처리] BlendSpace 그리드 삼각분할은 Python 네이티브 API 가
없다 → run() 직후 MCP 로 1줄:
    animation_physics force_rebuild_blend_space  /Game/AOS/Anim/BS_<Name>_Locomotion

[캐릭터당 남는 수동(에디터)] ① ABP AnimGraph 배선(BS→DefaultSlot→Output, ABP_Kwang 복사)
② AM_<Name>_Attack 에 B·C 클립 + AttackA/AttackB/Crit 3섹션(크리 시스템 활성).

[CONFIG 스키마]
    {
      'name': 'Greystone',                         # 에셋 접미사 (BP_Char_Greystone 등)
      'display_name': 'Greystone',                 # 로스터 표시명
      'pack_anim': '/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations',
      'skeleton':  '.../Meshes/Greystone_Skeleton',
      'mesh':      '.../Meshes/Greystone',
      'roster_index': 1,                           # TDProj_GM.CharacterRoster 인덱스(고유 0~4)
      'z_offset': -88,                             # 발 원점 전제(대부분 -88). PIE 로 미세조정.
      'loco': {'idle':'Idle','fwd':'Jog_Fwd','bwd':'Jog_Bwd',
               'left':'Jog_Left','right':'Jog_Right'},   # strafe 미사용 통일
      'montages': {'Attack':'Attack_A_Slow','Q':'Ability_Q','W':'Cast','E':'Ability_E',
                   'R':'Ability_Ultimate','Death':'Death','HitReact':'HitReact_Front'},
    }
"""
import unreal

eal = unreal.EditorAssetLibrary
atools = unreal.AssetToolsHelpers.get_asset_tools()
BEL = unreal.BlueprintEditorLibrary

AOS_ANIM = "/Game/AOS/Anim"
AOS_MONT = "/Game/AOS/Anim/Montages"
CHARS    = "/Game/Characters"
GA_DIR   = "/Game/AOS/GAS/Abilities"
GE_DIR   = "/Game/AOS/GAS/Effects/Cooldowns"
GM       = "/Game/TDProj_GM"
# 템플릿 = 검증된 Kwang 세트 (복제 원본)
TPL_BP   = "/Game/Characters/BP_Char_Kwang"
TPL_GA   = "/Game/AOS/GAS/Abilities/Kwang"
TPL_GE   = "/Game/AOS/GAS/Effects/Cooldowns/Kwang"


def _bp_cdo(path):
    bp = unreal.load_asset(path)
    return bp, unreal.get_default_object(bp.generated_class())


# ── ① BlendSpace (8방향, strafe 미사용) ─────────────────────────────
def build_blendspace(cfg):
    log = []
    name = cfg['name']
    bs_path = f"{AOS_ANIM}/BS_{name}_Locomotion"
    skel = unreal.load_asset(cfg['skeleton'])
    if eal.does_asset_exist(bs_path):
        eal.delete_asset(bs_path)
    f = unreal.BlendSpaceFactoryNew()
    f.set_editor_property('target_skeleton', skel)
    bs = atools.create_asset(f"BS_{name}_Locomotion", AOS_ANIM, unreal.BlendSpace, f)

    # 축: X=Direction[-180,180] grid4, Y=Speed[0,600] grid1 (blend_parameters 직접 mutate)
    bp = bs.get_editor_property('blend_parameters')
    x, y, z = bp[0], bp[1], bp[2]
    x.set_editor_property('display_name', 'Direction'); x.set_editor_property('min', -180.0)
    x.set_editor_property('max', 180.0); x.set_editor_property('grid_num', 4)
    y.set_editor_property('display_name', 'Speed'); y.set_editor_property('min', 0.0)
    y.set_editor_property('max', 600.0); y.set_editor_property('grid_num', 1)
    bs.set_editor_property('blend_parameters', [x, y, z])

    # 샘플 10개: Idle 5방향(Speed0) + Jog 5(Speed600). Direction Fwd0/Right+90/Left-90/Bwd±180
    L = cfg['loco']
    A = lambda n: unreal.load_asset(f"{cfg['pack_anim']}/{n}")
    plan = [(-180, 0, L['idle']), (-90, 0, L['idle']), (0, 0, L['idle']),
            (90, 0, L['idle']), (180, 0, L['idle']),
            (0, 600, L['fwd']), (90, 600, L['right']), (-90, 600, L['left']),
            (180, 600, L['bwd']), (-180, 600, L['bwd'])]
    samples = []
    for sx, sy, anim in plan:
        s = unreal.BlendSample()
        s.set_editor_property('animation', A(anim))
        s.set_editor_property('sample_value', unreal.Vector(float(sx), float(sy), 0.0))
        samples.append(s)
    bs.set_editor_property('sample_data', samples)
    eal.save_asset(bs_path, False)
    log.append(f"[BS] {bs_path}  샘플 {len(samples)}  ⚠️ MCP force_rebuild_blend_space 필요")
    return log


# ── ② ABP (AnimBlueprintFactory + parent=AOSAnimInstance) ───────────
def build_abp(cfg):
    log = []
    name = cfg['name']
    abp_path = f"{AOS_ANIM}/ABP_{name}"
    skel = unreal.load_asset(cfg['skeleton'])
    if eal.does_asset_exist(abp_path):
        eal.delete_asset(abp_path)
    f = unreal.AnimBlueprintFactory()
    f.set_editor_property('target_skeleton', skel)
    try:
        f.set_editor_property('parent_class', unreal.AOSAnimInstance)
    except Exception as e:
        log.append(f"[ABP] parent_class 팩토리 지정 실패({str(e)[:40]}) → reparent 폴백")
    abp = atools.create_asset(f"ABP_{name}", AOS_ANIM, unreal.AnimBlueprint, f)
    cdo = unreal.get_default_object(abp.generated_class())
    if not isinstance(cdo, unreal.AOSAnimInstance):
        BEL.reparent_blueprint(abp, unreal.AOSAnimInstance)
    eal.save_asset(abp_path, False)
    ok = isinstance(unreal.get_default_object(unreal.load_asset(abp_path).generated_class()), unreal.AOSAnimInstance)
    log.append(f"[ABP] {abp_path}  AOSAnimInstance={ok}  ⚠️ AnimGraph 배선 수동")
    return log


# ── ③ BP_Char (Kwang 복제 → 메시/ABP/Z + 로스터 교체) ───────────────
def build_character_bp(cfg):
    log = []
    name = cfg['name']
    dst = f"{CHARS}/BP_Char_{name}"
    if eal.does_asset_exist(dst):
        eal.delete_asset(dst)
    eal.duplicate_asset(TPL_BP, dst)
    bp, cdo = _bp_cdo(dst)
    mc = cdo.get_editor_property('mesh')
    mc.set_editor_property('skeletal_mesh_asset', unreal.load_asset(cfg['mesh']))
    mc.set_editor_property('anim_class', unreal.load_asset(f"{AOS_ANIM}/ABP_{name}").generated_class())
    mc.set_editor_property('relative_location', unreal.Vector(0, 0, float(cfg['z_offset'])))
    BEL.compile_blueprint(bp)
    eal.save_asset(dst, False)
    log.append(f"[BP] {dst}  mesh={mc.get_editor_property('skeletal_mesh_asset').get_name()} z={cfg['z_offset']}")

    # 로스터 교체 (EditDefaultsOnly struct → import_text)
    gmbp, gcdo = _bp_cdo(GM)
    roster = gcdo.get_editor_property('CharacterRoster')
    idx = cfg['roster_index']
    e = roster[idx]
    e.import_text(f"(CharacterClass=BlueprintGeneratedClass'{dst}.BP_Char_{name}_C',"
                  f"DisplayName=INVTEXT(\"{cfg.get('display_name', name)}\"),Portrait=None)")
    roster[idx] = e
    gcdo.set_editor_property('CharacterRoster', roster)
    BEL.compile_blueprint(gmbp)
    eal.save_asset(GM, False)
    log.append(f"[Roster] [{idx}] -> BP_Char_{name}_C")
    return log


# ── ④ 몽타주 7종 (팩토리 자동생성) + BP 배선 ─────────────────────────
def build_montages(cfg):
    log = []
    name = cfg['name']
    made = {}
    for suffix, clip in cfg['montages'].items():
        mont_name = f"AM_{name}_{suffix}"
        path = f"{AOS_MONT}/{mont_name}"
        src = unreal.load_asset(f"{cfg['pack_anim']}/{clip}")
        if not src:
            log.append(f"[Montage] {mont_name}: 소스없음({clip})"); continue
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        f = unreal.AnimMontageFactory()
        f.set_editor_property('source_animation', src)
        f.set_editor_property('target_skeleton', src.get_skeleton())
        m = atools.create_asset(mont_name, AOS_MONT, unreal.AnimMontage, f)
        eal.save_asset(path, False)
        made[suffix] = m
        log.append(f"[Montage] {mont_name} <- {clip} ({m.get_editor_property('sequence_length'):.2f}s)")

    # BP_Char 배선: SkillMontages(TMap 값 교체) + Attack/Death/HitReact
    bp, cdo = _bp_cdo(f"{CHARS}/BP_Char_{name}")
    MA = lambda s: made.get(s)
    sm = cdo.get_editor_property('SkillMontages')
    for k in list(sm.keys()):
        v = sm[k]; nm = v.get_name() if v else ""
        for suf in ('Q', 'W', 'E', 'R'):
            if nm.endswith(f"_{suf}") and MA(suf):   # 템플릿 AM_Kwang_Q 등
                sm[k] = MA(suf)
    cdo.set_editor_property('SkillMontages', sm)
    for prop, suf in [('AttackMontage', 'Attack'), ('DeathMontage', 'Death'), ('HitReactMontage', 'HitReact')]:
        if MA(suf):
            cdo.set_editor_property(prop, MA(suf))
    BEL.compile_blueprint(bp)
    eal.save_asset(f"{CHARS}/BP_Char_{name}", False)
    log.append(f"[Montage] BP 배선: SkillMontages/Attack/Death/HitReact -> {name}")
    return log


# ── ⑤ GA/GE (Kwang 세트 복제 → 참조 교체 + StartupAbilities) ─────────
def build_abilities(cfg):
    log = []
    name = cfg['name']
    ga_dir = f"{GA_DIR}/{name}"
    ge_dir = f"{GE_DIR}/{name}"
    eal.make_directory(ga_dir); eal.make_directory(ge_dir)
    for slot in ('Q', 'W', 'E', 'R'):
        for src, dp in [(f"{TPL_GE}/BP_GE_Cooldown_Kwang_{slot}", f"{ge_dir}/BP_GE_Cooldown_{name}_{slot}"),
                        (f"{TPL_GA}/BP_GA_Kwang_{slot}",          f"{ga_dir}/BP_GA_{name}_{slot}")]:
            if eal.does_asset_exist(dp):
                eal.delete_asset(dp)
            eal.duplicate_asset(src, dp)
    # 쿨다운 참조 교체
    for slot in ('Q', 'W', 'E', 'R'):
        ga = unreal.load_asset(f"{ga_dir}/BP_GA_{name}_{slot}")
        gc = unreal.get_default_object(ga.generated_class())
        ge = unreal.load_asset(f"{ge_dir}/BP_GE_Cooldown_{name}_{slot}")
        try:
            gc.set_editor_property('CooldownGameplayEffectClass', ge.generated_class())
        except Exception:
            gc.import_text(f"(CooldownGameplayEffectClass=BlueprintGeneratedClass"
                           f"'{ge_dir}/BP_GE_Cooldown_{name}_{slot}.BP_GE_Cooldown_{name}_{slot}_C')")
        BEL.compile_blueprint(ga)
        eal.save_asset(f"{ga_dir}/BP_GA_{name}_{slot}", False)
        eal.save_asset(f"{ge_dir}/BP_GE_Cooldown_{name}_{slot}", False)
    # StartupAbilities 배선 (GA_Attack 유지, Kwang GA → 신규)
    bp, cdo = _bp_cdo(f"{CHARS}/BP_Char_{name}")
    new = []
    for cls in cdo.get_editor_property('StartupAbilities'):
        nm = cls.get_name() if cls else ""
        repl = None
        for slot in ('Q', 'W', 'E', 'R'):
            if nm.endswith(f"Kwang_{slot}_C"):
                repl = unreal.load_asset(f"{ga_dir}/BP_GA_{name}_{slot}").generated_class()
        new.append(repl if repl else cls)
    cdo.set_editor_property('StartupAbilities', new)
    BEL.compile_blueprint(bp)
    eal.save_asset(f"{CHARS}/BP_Char_{name}", False)
    log.append(f"[GA/GE] {name} 8종 복제 + 배선: {[c.get_name() if c else None for c in new]}")
    return log


def run(cfg):
    """골든 경로 5단계 실행. 반환 = 로그 리스트. 실행 후 BS 리빌드(MCP) 잊지 말 것."""
    log = [f"=== build_paragon_character: {cfg['name']} (roster {cfg['roster_index']}) ==="]
    log += build_blendspace(cfg)     # ①
    log += build_abp(cfg)            # ②
    log += build_character_bp(cfg)   # ③ (ABP 참조 → ② 뒤)
    log += build_montages(cfg)       # ④ (BP 배선 → ③ 뒤)
    log += build_abilities(cfg)      # ⑤ (BP 배선 → ③ 뒤)
    log.append(f"=== 완료. 다음: (1) MCP force_rebuild_blend_space {AOS_ANIM}/BS_{cfg['name']}_Locomotion "
               f"(2) 에디터: ABP AnimGraph 배선 + AM_{cfg['name']}_Attack 3섹션 ===")
    return log


# 검증된 캐릭터 config (신규 = pack/skeleton/mesh/roster_index/montages 만 바꿔 복제)
CONFIGS = {
    'Kwang': {
        'name': 'Kwang', 'display_name': 'Kwang',
        'pack_anim': '/Game/ParagonKwang/Characters/Heroes/Kwang/Animations',
        'skeleton': '/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_Skeleton',
        'mesh': '/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/KwangRosewood',
        'roster_index': 0, 'z_offset': -88,
        'loco': {'idle': 'Idle', 'fwd': 'Jog_Fwd', 'bwd': 'Jog_Bwd', 'left': 'Jog_Left', 'right': 'Jog_Right'},
        'montages': {'Attack': 'PrimaryAttack_A_Slow', 'Q': 'PrimaryAttack_Air', 'W': 'Cast',
                     'E': 'Ability_R', 'R': 'PrimaryAttack_C_Slow', 'Death': 'Death_Bwd', 'HitReact': 'Hitreact_Fwd'},
    },
    'Greystone': {
        'name': 'Greystone', 'display_name': 'Greystone',
        'pack_anim': '/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations',
        'skeleton': '/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone_Skeleton',
        'mesh': '/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone',
        'roster_index': 1, 'z_offset': -88,
        'loco': {'idle': 'Idle', 'fwd': 'Jog_Fwd', 'bwd': 'Jog_Bwd', 'left': 'Jog_Left', 'right': 'Jog_Right'},
        'montages': {'Attack': 'Attack_A_Slow', 'Q': 'Ability_Q', 'W': 'Cast', 'E': 'Ability_E',
                     'R': 'Ability_Ultimate', 'Death': 'Death', 'HitReact': 'HitReact_Front'},
    },
    'Grux': {  # 야수형 양손 무기(weapon_l/r 내장), 로코모션 Jog_Lft/Rgt(이름 주의)
        'name': 'Grux', 'display_name': 'Grux',
        'pack_anim': '/Game/ParagonGrux/Characters/Heroes/Grux/Animations',
        'skeleton': '/Game/ParagonGrux/Characters/Heroes/Grux/Meshes/Grux_Skeleton',
        'mesh': '/Game/ParagonGrux/Characters/Heroes/Grux/Meshes/Grux',
        'roster_index': 2, 'z_offset': -88,
        'loco': {'idle': 'Idle', 'fwd': 'Jog_Fwd', 'bwd': 'Jog_Bwd', 'left': 'Jog_Lft', 'right': 'Jog_Rgt'},
        'montages': {'Attack': 'PrimaryAttack_LA', 'Q': 'DoublePain', 'W': 'Cast', 'E': 'Stampede',
                     'R': 'Ultimate_Roar', 'Death': 'Death_A', 'HitReact': 'HitReact_Front'},
    },
    'Crunch': {  # 맨손 격투 로봇(무기본 없음 — 부착 이슈 자체 無). idle=Idle_Combat(브롤러 스탠스)
        'name': 'Crunch', 'display_name': 'Crunch',
        'pack_anim': '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations',
        'skeleton': '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch_Skeleton',
        'mesh': '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch',
        'roster_index': 3, 'z_offset': -88,
        'loco': {'idle': 'Idle_Combat', 'fwd': 'Jog_Fwd', 'bwd': 'Jog_Bwd', 'left': 'Jog_Left', 'right': 'Jog_Right'},
        'montages': {'Attack': 'Ability_Combo_01', 'Q': 'Ability_Uppercut', 'W': 'Cast', 'E': 'Ability_GutPunch',
                     'R': 'Ability_Hook_Empowered', 'Death': 'Death_A', 'HitReact': 'HitReact_Front'},
    },
}
