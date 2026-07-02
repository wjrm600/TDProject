# -*- coding: utf-8 -*-
"""bl_anim_qa.py — 애니메이션 수치 QA (Blender 내부, 헤드리스).

과거 실패 사례를 자동 검출한다:
  - 발 슬라이딩 (접지 구간 발 XY 표류)          ← 접지 검증 실측 이력
  - 무릎 과신전 (>177°)                        ← R 제작 중 "무릎 과신전" 기형
  - 팝핑 (프레임 간 로컬 회전 스파이크)          ← 절차 회전 실수
  - 길이 정합 (blend 저장된 씬 범위 ≠ 액션 범위) ← export 6.4→8.3s 버그
  - 루프 정합 (첫↔끝 포즈 델타, --loop 시)      ← Idle/Move 루프 이음새
  - 골반 궤적 (원점 XY 드리프트, 높이 범위)      ← Q "재생 위치 원점 이탈" 버그

사용 (단독):
  blender.exe -b <blend> -P bl_anim_qa.py -- <cfg.json>
cfg 키: out_dir(필수), action, loop(bool), thresholds(dict, 선택)

출력: <out_dir>/qa_report.txt (사람용) + qa_report.json (기계용)
"""
import json
import math
import os
import sys

import bpy

DEFAULT_THRESHOLDS = {
    "contact_height_cm": 6.0,   # 이 높이(cm) 미만이면 접지로 간주 (ball 기준)
    "slide_cm": 3.0,            # 크립 런 누적 이동 허용치
    "creep_min_cm_f": 0.5,      # 이 미만 = 완전 고정(플랜트) — 슬라이드 아님
    "creep_max_cm_f": 6.0,      # 이 초과 = 의도적 스텝(빠른 재배치) — 슬라이드 아님
    "knee_deg": 177.0,          # 무릎 각도 상한 (180=완전 신전)
    "pop_deg_per_frame": 40.0,  # 프레임 간 로컬 회전 상한
    "loop_deg": 5.0,            # 루프 첫↔끝 포즈 회전 델타 허용치
    "drift_cm": 10.0,           # 골반 시작↔끝 XY 드리프트 허용치 (루프/제자리)
}

FOOT_BONES = ["ball_l", "ball_r", "foot_l", "foot_r"]
LEG_CHAINS = [("thigh_l", "calf_l", "foot_l"), ("thigh_r", "calf_r", "foot_r")]


def find_armature():
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if not arms:
        raise RuntimeError("아마추어 없음")
    return arms[0]


def resolve_action(arm, action_name=None):
    ad = arm.animation_data
    if action_name:
        act = bpy.data.actions.get(action_name)
        if act is None:
            raise RuntimeError("액션 '%s' 없음. 존재: %s"
                               % (action_name, [a.name for a in bpy.data.actions]))
        ad.action = act
        try:
            if act.slots and ad.action_slot is None:
                ad.action_slot = act.slots[0]
        except AttributeError:
            pass
    act = ad.action
    if act is None:
        raise RuntimeError("바인딩된 액션 없음")
    fs, fe = act.frame_range
    return act, int(math.floor(fs)), int(math.ceil(fe))


def sample(arm, start, end):
    """전 프레임 본 데이터 샘플링 → {frame: {bone: (world_head, local_quat)}}"""
    scene = bpy.context.scene
    data = {}
    for f in range(start, end + 1):
        scene.frame_set(f)
        mw = arm.matrix_world
        frame = {}
        for pb in arm.pose.bones:
            frame[pb.name] = (
                (mw @ pb.head).copy(),
                pb.matrix_basis.to_quaternion().copy(),
            )
        data[f] = frame
    return data


def detect_unit_scale(data, start, end):
    """월드 단위→cm 환산 계수. 캐릭터 높이 휴리스틱 (3 미만 = 미터 단위)."""
    zs = [p[0].z for frame in data.values() for p in frame.values()]
    height = max(zs) - min(zs)
    return (100.0, "m") if height < 3.0 else (1.0, "cm")


def check_foot_slide(data, start, end, ground_z, to_cm, th):
    """슬라이딩 = 접지 중 '저속 크립' 이동의 누적.

    완전 고정(< creep_min)과 의도적 스텝(> creep_max, 런지 등 빠른 재배치)은
    슬라이드가 아님 — 그 사이 속도의 연속 표류(크립 런)만 잡는다.
    (표준편차 방식은 인플레이스 런지의 정상 스텝을 오탐 — Alex_Q 로 실측)
    """
    issues = []
    worst = 0.0

    def flush(bone, run, out):
        if len(run) < 4:  # 3프레임 미만 크립은 무시
            return 0.0
        p0, p1 = run[0][1], run[-1][1]
        net = math.hypot(p1.x - p0.x, p1.y - p0.y) * to_cm  # 순변위 (진동 제외)
        if net > th["slide_cm"]:
            out.append("%s F%d-%d 크립 슬라이드 순변위 %.1fcm"
                       % (bone, run[0][0], run[-1][0], net))
        return net

    for bone in FOOT_BONES:
        if bone not in data[start]:
            continue
        run = []  # [(frame, pos), ...] 연속 크립 구간
        prev = None
        for f in range(start, end + 1):
            p = data[f][bone][0]
            h = (p.z - ground_z) * to_cm
            in_contact = h < th["contact_height_cm"]
            speed = (math.hypot(p.x - prev.x, p.y - prev.y) * to_cm
                     if prev is not None else 0.0)
            creeping = (in_contact and prev is not None
                        and th["creep_min_cm_f"] <= speed <= th["creep_max_cm_f"])
            if creeping:
                if not run:
                    run.append((f - 1, prev))
                run.append((f, p))
            else:
                worst = max(worst, flush(bone, run, issues))
                run = []
            prev = p
        worst = max(worst, flush(bone, run, issues))
    return issues, worst


def check_knee(data, start, end, th):
    issues = []
    worst = 0.0
    for hip, knee, ankle in LEG_CHAINS:
        if knee not in data[start]:
            continue
        for f in range(start, end + 1):
            try:
                p_h = data[f][hip][0]
                p_k = data[f][knee][0]
                p_a = data[f][ankle][0]
            except KeyError:
                break
            v1, v2 = p_h - p_k, p_a - p_k
            if v1.length < 1e-9 or v2.length < 1e-9:
                continue
            ang = math.degrees(v1.angle(v2))
            worst = max(worst, ang)
            if ang > th["knee_deg"]:
                issues.append("%s F%d 과신전 %.1f°" % (knee, f, ang))
    return issues, worst


def check_popping(data, start, end, th):
    issues = []
    worst = 0.0
    bones = list(data[start].keys())
    for f in range(start + 1, end + 1):
        for b in bones:
            q0 = data[f - 1][b][1]
            q1 = data[f][b][1]
            ang = math.degrees(q0.rotation_difference(q1).angle)
            if ang > 180.0:
                ang = 360.0 - ang
            worst = max(worst, ang)
            if ang > th["pop_deg_per_frame"]:
                issues.append("%s F%d 회전 스파이크 %.0f°/f" % (b, f, ang))
    return issues, worst


def check_loop(data, start, end, th):
    issues = []
    worst = 0.0
    for b in data[start].keys():
        q0, q1 = data[start][b][1], data[end][b][1]
        ang = math.degrees(q0.rotation_difference(q1).angle)
        if ang > 180.0:
            ang = 360.0 - ang
        worst = max(worst, ang)
        if ang > th["loop_deg"]:
            issues.append("%s 첫↔끝 델타 %.1f°" % (b, ang))
    return issues, worst


def check_pelvis(data, start, end, to_cm, th):
    pelvis = "pelvis" if "pelvis" in data[start] else None
    if pelvis is None:
        return ["pelvis 본 없음 — 궤적 검사 생략"], {}
    p0 = data[start][pelvis][0]
    p1 = data[end][pelvis][0]
    drift = math.hypot((p1.x - p0.x), (p1.y - p0.y)) * to_cm
    zs = [data[f][pelvis][0].z * to_cm for f in range(start, end + 1)]
    stats = {"drift_cm": round(drift, 1),
             "height_min_cm": round(min(zs), 1),
             "height_max_cm": round(max(zs), 1)}
    issues = []
    if drift > th["drift_cm"]:
        issues.append("골반 시작↔끝 XY 드리프트 %.1fcm" % drift)
    return issues, stats


def main(cfg):
    out_dir = cfg["out_dir"]
    os.makedirs(out_dir, exist_ok=True)
    th = dict(DEFAULT_THRESHOLDS, **cfg.get("thresholds", {}))

    arm = find_armature()
    # blend 파일에 저장된 씬 범위를 먼저 기록 (resolve 가 덮어쓰기 전)
    saved_range = (bpy.context.scene.frame_start, bpy.context.scene.frame_end)
    act, start, end = resolve_action(arm, cfg.get("action"))
    range_mismatch = saved_range != (start, end)

    data = sample(arm, start, end)
    to_cm, unit = detect_unit_scale(data, start, end)
    ground_z = arm.matrix_world.translation.z

    results = {}
    slide_issues, slide_worst = check_foot_slide(data, start, end, ground_z, to_cm, th)
    results["foot_slide"] = {"issues": slide_issues, "worst_cm": round(slide_worst, 1)}
    knee_issues, knee_worst = check_knee(data, start, end, th)
    results["knee"] = {"issues": knee_issues, "worst_deg": round(knee_worst, 1)}
    pop_issues, pop_worst = check_popping(data, start, end, th)
    results["popping"] = {"issues": pop_issues, "worst_deg_per_frame": round(pop_worst, 1)}
    results["length"] = {
        "issues": (["blend 저장 씬 범위 %s ≠ 액션 범위 (%d,%d) — export 전 정렬 필요"
                    % (saved_range, start, end)] if range_mismatch else []),
        "saved_scene_range": list(saved_range), "action_range": [start, end]}
    if cfg.get("loop"):
        loop_issues, loop_worst = check_loop(data, start, end, th)
        results["loop"] = {"issues": loop_issues, "worst_deg": round(loop_worst, 1)}
    pelvis_issues, pelvis_stats = check_pelvis(data, start, end, to_cm, th)
    results["pelvis"] = {"issues": pelvis_issues, **pelvis_stats}

    # 심각도: foot_slide 는 기본 WARN — 인플레이스 스킬(런지 등)은 골반 고정
    # 구조상 발이 미끄러지는 게 정상 (승인작 Alex_Q 실측 74cm). 루프 로코모션
    # 검사 시 cfg["strict_slide"]=true 로 FAIL 승격.
    # pelvis 드리프트도 기본 WARN — 사망/이동형 모션은 변위가 의도임.
    # 제자리 모션 검사 시 cfg["in_place"]=true 로 FAIL 승격.
    warn_only = set()
    if not cfg.get("strict_slide"):
        warn_only.add("foot_slide")
    if not cfg.get("in_place"):
        warn_only.add("pelvis")

    lines = ["=== 애니 QA: %s (액션 %s, F%d-%d, 단위 %s) ==="
             % (os.path.basename(bpy.data.filepath), act.name, start, end, unit)]
    n_fail = 0
    for name, r in results.items():
        issues = r.get("issues", [])
        if not issues:
            status = "PASS"
        elif name in warn_only:
            status = "WARN"
        else:
            status = "FAIL"
            n_fail += 1
        extra = {k: v for k, v in r.items() if k != "issues"}
        lines.append("[%s] %s  %s" % (status, name, extra if extra else ""))
        for i in issues[:10]:
            lines.append("       - " + i)
        if len(issues) > 10:
            lines.append("       ... 외 %d건" % (len(issues) - 10))
    lines.append("결과: %s (%d개 항목 FAIL)" % ("PASS" if n_fail == 0 else "FAIL", n_fail))
    report = "\n".join(lines)

    with open(os.path.join(out_dir, "qa_report.txt"), "w", encoding="utf-8") as fp:
        fp.write(report + "\n")
    with open(os.path.join(out_dir, "qa_report.json"), "w", encoding="utf-8") as fp:
        json.dump({"action": act.name, "range": [start, end], "unit": unit,
                   "results": results, "fail_count": n_fail},
                  fp, ensure_ascii=False, indent=2, default=str)
    print(report)
    print("QA_DONE fail=%d" % n_fail)
    return n_fail


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if not argv:
        raise SystemExit("사용법: blender -b <blend> -P bl_anim_qa.py -- <cfg.json>")
    with open(argv[0], encoding="utf-8-sig") as fp:  # PowerShell BOM 허용
        main(json.load(fp))
