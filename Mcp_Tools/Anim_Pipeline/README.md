# Anim_Pipeline — 애니메이션 시각 피드백 루프

Blender 애니메이션을 **Claude 가 스스로 보고 판정**할 수 있게 하는 렌더 파이프라인.
뷰포트 스크린샷(화면 그랩 — 창이 가려지면 검은 화면, 사용자 병행 작업 차단)을 폐기하고,
**창 없는 별도 헤드리스 프로세스의 실제 렌더**로 대체한다. 렌더 중 사용자는 아무
작업이나 병행 가능하다.

## 핵심 루프 (애니 1회 수정 사이클)

```
Blender 편집 (라이브 MCP 세션이면 반드시 bpy.ops.wm.save_mainfile() 먼저!)
  → python render_anim_preview.py --blend <blend> [--action <액션>]
  → Claude 가 sheet_*.png / keyposes.png 를 Read 로 판독
  → 체크리스트(키포즈 일치 · 타이밍 · 실루엣 · 접지) 판정 → 재수정
```

전체 렌더(3뷰 × ~15프레임)에 **10초 미만** (Workbench 엔진, 프레임당 <0.1s).

## 파일

| 파일 | 실행 위치 | 역할 |
|------|----------|------|
| `render_anim_preview.py` | 시스템 Python | 오케스트레이터 CLI — 헤드리스 Blender 서브프로세스 → 시트 합성 |
| `bl_render_preview.py` | Blender 내부 | 렌더 본체 (프록시 본 메시·카메라·지면·MP4) |
| `make_contact_sheet.py` | 시스템 Python | PIL 그리드 합성 (단독 CLI 겸용) |
| `extract_ref_poses.py` | 시스템 Python | 레퍼런스 영상/GIF → 키포즈 스틸 시트 |
| `Previews/` | 산출물 | git 미추적 (.gitignore) |

## 사용 예

```powershell
# 기본: 3뷰 콘택트 시트 + 키포즈(균등 5장)
python render_anim_preview.py --blend "..\..\BlenderAssets\Alex_Q.blend" --action Alex_Q_RW

# 키포즈 명시 + 사용자 확인용 MP4
python render_anim_preview.py --blend "..\..\BlenderAssets\Alex_Death.blend" `
    --keyposes 1,10,16,22,31,41 --mp4

# 레퍼런스 영상에서 목표 포즈 8장 추출
python extract_ref_poses.py --src ref_death.gif --count 8
```

## 시트 판독 가이드 (Claude 용)

- **본 색**: 파랑 = 왼쪽(`_l`) / 빨강 = 오른쪽(`_r`) / 주황 = 중앙(척추·머리) /
  **노랑 = 무기 본** (짧은 부착 소켓이어도 캐릭터 키 75% 블레이드로 연장 표시 —
  검 궤적 판독용)
- **머리**: head 본에 구체 부착 (인체 방향 판독)
- **지면**: 어두운 슬래브, 윗면 = 아마추어 원점 z (UE 관례). 발이 이 라인에
  닿는지로 접지/부유 판정
- **front 뷰** = 캐릭터가 카메라를 봄 (빨강=화면 왼쪽이 정상). `--front-axis` 로 교정
- **sheet_*.png** = 타이밍/궤적 판독 (셀 320px) / **keyposes.png** = 포즈 품질
  판독 (셀 512px)

## 함정 (실측으로 확정된 것)

1. **아마추어(본)는 F12 렌더에 안 나옴** → 스킨 메시 없는 blend 는 자동으로 본
   프록시 메시 생성 (`ensure_renderable`). 삭제/저장 안 함 — 헤드리스 메모리에서만.
2. **프록시 지오메트리는 본 로컬 단위** — UE FBX 아마추어는 오브젝트 스케일
   (보통 0.01)이 있어 월드 단위와 혼용하면 100배 어긋남 (굵기 캡은 로컬 환산).
3. **Blender 5.1 렌더 엔진 id**: `BLENDER_EEVEE_NEXT` 없음 → `BLENDER_EEVEE`.
   auto 순서 = workbench → eevee → cycles(CPU).
4. **Blender 5.x 동영상 출력**: `image_settings.file_format='FFMPEG'` 는 제거됨 →
   `image_settings.media_type='VIDEO'` 먼저 설정.
5. **씬 프레임 범위는 액션 범위로 강제** (`resolve_action`) — 과거 export 길이
   6.4→8.3s 버그의 상시 방어. blend 에 UE 원본 테이크(`root|Unreal Take|...`)가
   공존하므로 `--action` 명시가 안전.
6. **라이브 Blender MCP 세션에서 편집 중이면 디스크가 구버전** — 렌더 전
   `bpy.ops.wm.save_mainfile()`. (폴백: `bl_render_preview.py` 를 라이브 세션에서
   `exec(...); main(cfg)` 로 직접 호출 가능, `cfg["cleanup"]=True` 권장)

## 의존

- Blender 5.1 (`BLENDER_EXE` 환경변수로 오버라이드, 기본
  `C:\Program Files\Blender Foundation\Blender 5.1\blender.exe`)
- 시스템 Python: `pip install -r requirements.txt` (Pillow, imageio[-ffmpeg])
