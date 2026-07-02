# -*- coding: utf-8 -*-
"""make_contact_sheet.py — 렌더 프레임들을 라벨 달린 그리드 1장으로 합성 (PIL).

Read 판독 규격: 시트 폭 <=1600px. 기본 셀 320px(5열) / keyposes 는 512px(3열).

단독 실행:
  python make_contact_sheet.py --dir Previews/Alex_Q --view front --fps 30
"""
import argparse
import glob
import os
import re

from PIL import Image, ImageDraw, ImageFont

MAX_SHEET_W = 1600
LABEL_H = 26


def _font(size=16):
    try:
        return ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", size)
    except Exception:
        return ImageFont.load_default()


def compose(files, out_path, fps=30, cell_px=320, cols=None, labels=None,
            frame_start=1):
    """files: (프레임번호, 경로) 리스트 or 경로 리스트. 라벨 = F### · #.##s"""
    if not files:
        raise ValueError("합성할 프레임 없음")
    norm = []
    for item in files:
        if isinstance(item, tuple):
            norm.append(item)
        else:
            m = re.search(r"f(\d+)_", os.path.basename(item))
            norm.append((int(m.group(1)) if m else 0, item))
    if cols is None:
        cols = max(1, MAX_SHEET_W // cell_px)
    cols = min(cols, len(norm))
    rows = (len(norm) + cols - 1) // cols

    cell_h = cell_px + LABEL_H
    sheet = Image.new("RGB", (cols * cell_px, rows * cell_h), (24, 24, 28))
    draw = ImageDraw.Draw(sheet)
    font = _font()

    for i, (frame, path) in enumerate(norm):
        img = Image.open(path).convert("RGB")
        img.thumbnail((cell_px, cell_px), Image.LANCZOS)
        x = (i % cols) * cell_px
        y = (i // cols) * cell_h
        sheet.paste(img, (x + (cell_px - img.width) // 2,
                          y + (cell_px - img.height) // 2))
        if labels and i < len(labels):
            text = labels[i]
        else:
            t = (frame - frame_start) / float(fps)
            text = "F%d - %.2fs" % (frame, t)
        draw.rectangle([x, y + cell_px, x + cell_px, y + cell_h], fill=(24, 24, 28))
        draw.text((x + 6, y + cell_px + 4), text, fill=(255, 220, 120), font=font)
        draw.rectangle([x, y, x + cell_px - 1, y + cell_h - 1],
                       outline=(60, 60, 70))

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    sheet.save(out_path)
    return out_path


def compose_view(frames_dir, view, out_path=None, fps=30, cell_px=320,
                 frames=None, frame_start=1):
    """frames_dir 의 f###_<view>.png 를 모아 시트 합성. frames 지정 시 그 프레임만."""
    files = sorted(glob.glob(os.path.join(frames_dir, "f*_%s.png" % view)))
    pairs = []
    for p in files:
        m = re.search(r"f(\d+)_", os.path.basename(p))
        if not m:
            continue
        f = int(m.group(1))
        if frames and f not in frames:
            continue
        pairs.append((f, p))
    pairs.sort()
    out_path = out_path or os.path.join(frames_dir, "sheet_%s.png" % view)
    return compose(pairs, out_path, fps=fps, cell_px=cell_px,
                   frame_start=frame_start)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    ap.add_argument("--view", required=True)
    ap.add_argument("--fps", type=int, default=30)
    ap.add_argument("--cell", type=int, default=320)
    ap.add_argument("--out")
    ap.add_argument("--frame-start", type=int, default=1)
    a = ap.parse_args()
    print(compose_view(a.dir, a.view, a.out, a.fps, a.cell,
                       frame_start=a.frame_start))
