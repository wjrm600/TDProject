# -*- coding: utf-8 -*-
"""extract_ref_poses.py — 레퍼런스 영상/GIF → 키포즈 스틸 시트.

애니 저작 전에 목표 포즈를 시각 합의하는 용도 (레퍼런스 우선 원칙).

사용 예:
  python extract_ref_poses.py --src ref_death.gif --out Previews/Ref_Death --count 8
  python extract_ref_poses.py --src ref_slam.mp4 --times 0.2,0.55,0.8,1.3

의존: GIF = Pillow 내장 / MP4 = imageio + imageio-ffmpeg (pip, ffmpeg exe 번들이라
PATH 의존 없음).
"""
import argparse
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_contact_sheet import compose  # noqa: E402

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass


def extract_gif(src, times, count):
    img = Image.open(src)
    n_frames = getattr(img, "n_frames", 1)
    durations = []
    total = 0.0
    for i in range(n_frames):
        img.seek(i)
        d = img.info.get("duration", 100) / 1000.0
        durations.append(total)
        total += d
    if times is None:
        idxs = [round(i * (n_frames - 1) / max(count - 1, 1)) for i in range(count)]
    else:
        idxs = [min(range(n_frames), key=lambda i: abs(durations[i] - t))
                for t in times]
    out = []
    for i in idxs:
        img.seek(i)
        out.append((img.convert("RGB").copy(), durations[i]))
    return out


def extract_video(src, times, count):
    import imageio.v2 as imageio
    reader = imageio.get_reader(src)
    meta = reader.get_meta_data()
    fps = meta.get("fps", 30)
    try:
        n_frames = reader.count_frames()
    except Exception:
        n_frames = int(meta.get("duration", 10) * fps)
    if times is None:
        idxs = [round(i * (n_frames - 1) / max(count - 1, 1)) for i in range(count)]
    else:
        idxs = [min(int(t * fps), n_frames - 1) for t in times]
    out = []
    for i in idxs:
        frame = reader.get_data(i)
        out.append((Image.fromarray(frame), i / fps))
    reader.close()
    return out


def main():
    ap = argparse.ArgumentParser(description="레퍼런스 영상/GIF → 키포즈 스틸 시트")
    ap.add_argument("--src", required=True, help="영상/GIF 파일 (로컬 경로)")
    ap.add_argument("--out", help="출력 폴더 (기본: Previews/Ref_<파일명>)")
    ap.add_argument("--times", help="추출 시점 초 단위 (예: 0.2,0.5,1.1)")
    ap.add_argument("--count", type=int, default=8, help="균등 추출 장수 (기본 8)")
    args = ap.parse_args()

    src = os.path.abspath(args.src)
    if not os.path.exists(src):
        raise SystemExit("[오류] 소스 없음: %s" % src)
    base = os.path.splitext(os.path.basename(src))[0]
    out_dir = os.path.abspath(args.out or os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "Previews", "Ref_" + base))
    os.makedirs(out_dir, exist_ok=True)

    times = [float(t) for t in args.times.split(",")] if args.times else None
    ext = os.path.splitext(src)[1].lower()
    if ext == ".gif":
        stills = extract_gif(src, times, args.count)
    else:
        stills = extract_video(src, times, args.count)

    files, labels = [], []
    for i, (img, t) in enumerate(stills):
        p = os.path.join(out_dir, "ref_%02d.png" % i)
        img.save(p)
        files.append((i, p))
        labels.append("#%d - %.2fs" % (i, t))

    sheet = compose(files, os.path.join(out_dir, "ref_sheet.png"),
                    cell_px=512, labels=labels)
    print("키포즈 %d장 추출 완료" % len(files))
    print("  시트: %s" % sheet)


if __name__ == "__main__":
    main()
