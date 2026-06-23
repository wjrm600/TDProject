# AI 기반 3D 모델 생성 · 교체 파이프라인

**대상 독자**: 이 프로젝트의 에이전트/개발자. 현재 UE5 Mannequin 플레이스홀더 메시를
AI 생성 3D 모델로 교체하는 전체 절차를 단계별로 기술합니다.

---

## 0. 전략 요약

| 대상 에셋 | 방법 | 도구 | 비용 |
|----------|------|------|------|
| **캐릭터** (20종, 애니메이션 필요) | 클라우드 — 자동 리깅 + T포즈 | Meshy 또는 Tripo | 무료 티어 / 저가 |
| **구조물 · 프롭** (타워, 커맨드센터) | 로컬 — 리깅 불필요 | ComfyUI-3D-Pack + Hunyuan3D 2.x / TRELLIS | $0 |

### 핵심 제약 (반드시 지킬 것)

- 캐릭터는 **UE5 Mannequin 스켈레톤** (`SK_Mannequin` 계열) + **ABP_AOSCharacter** 파이프라인에 묶여 있음.
  새 메시는 이 스켈레톤으로 IK Retarget 해야 기존 몽타주(AM_Attack / AM_HitReact / AM_Death /
  AM_Alex_* 스킬 4종)와 ABP가 **코드 수정 없이** 그대로 재사용됨.
- 코드 기준 (확인된 실제 경로, `AOSCharacter.cpp` `SetupCharacterDefaults`):
  - 기본 메시: `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`
  - 기본 ABP: `/Game/AOS/Anim/ABP_AOSCharacter` (폴백: ABP_Unarmed)
  - **BP_Char_*의 Mesh 슬롯이 설정돼 있으면 코드 폴백은 건너뜀** → BP에서만 교체하면 됨.

---

## 1. 로컬 브랜치 — 구조물 · 프롭 (ComfyUI-3D-Pack + Hunyuan3D 2.x)

### 1-1. VRAM 요구사항

| 모델 | 최소 VRAM | 권장 |
|------|-----------|------|
| Hunyuan3D-2.0 (image→3D) | 12 GB | 16 GB |
| TRELLIS (image→3D) | 12 GB | 16 GB |
| ComfyUI-3D-Pack (Stable Zero123 등) | 8 GB | 12 GB |

현재 개발 머신: RTX 3080 (10 GB VRAM). Hunyuan3D-2.0 표준 모델은 **12 GB 이상 요구**로
VRAM 부족 가능성. 아래 대안 중 하나 선택:

- **Hunyuan3D-2.0-mini** (Tencent 공식 경량 버전, ~8 GB): 품질 소폭 하락이나 RTX 3080 대응.
- **TRELLIS** (Microsoft, BSD-3, ~10 GB): 로컬 실행 대응 확인 후 사용.
- **ComfyUI-3D-Pack + Zero123++**: VRAM 8 GB대도 동작, 단순 구조물에 충분.

### 1-2. 설치 절차 (Windows, RTX 3080)

#### A. ComfyUI-3D-Pack (ComfyUI 커스텀 노드)

```powershell
# 1. ComfyUI 이미 설치돼 있다면 custom_nodes 폴더로 이동
cd <ComfyUI 설치 경로>\ComfyUI\custom_nodes

# 2. 클론
git clone https://github.com/MrForExample/ComfyUI-3D-Pack.git
cd ComfyUI-3D-Pack

# 3. 의존성 (Python 3.11 / CUDA 11.8+ 전제)
pip install -r requirements.txt

# 4. ComfyUI 재시작 → 3D 노드 등록 확인
```

#### B. Hunyuan3D-2.0-mini (로컬, VRAM ~8GB)

```powershell
# 공식 HuggingFace 모델 다운로드
pip install huggingface_hub
python -c "from huggingface_hub import snapshot_download; snapshot_download('tencent/Hunyuan3D-2', local_dir='./hunyuan3d2')"

# 또는 Hunyuan3D-2-mini 로 VRAM 절약
# https://huggingface.co/tencent/Hunyuan3D-2mini
```

> 사용자 직접 작업: HuggingFace 계정 필요 (무료). 대형 모델(~10 GB) 다운로드이므로
> 충분한 저장 공간(SSD 30 GB 여유) 확보 후 진행.

#### C. TRELLIS (대안)

```powershell
pip install git+https://github.com/microsoft/TRELLIS.git
# 상세: https://github.com/microsoft/TRELLIS — Windows 설치 문서 별도 확인
```

### 1-3. image → 3D 워크플로

1. **참조 이미지 준비**: 구조물을 4방향(front/back/left/right) 또는 1장(단면 입력 지원 모델)으로 준비.
   - gen_icon.py 나 ComfyUI 2D 파이프라인으로 아이소메트릭 뷰 생성 후 3D 변환 가능.
     (**검증된 생성 레시피는 §10 참고**)
2. **Hunyuan3D-2 실행** (예시, mini 모델):
   ```python
   # inference.py (공식 예시 기준)
   from hy3dgen.texgen import Hunyuan3DPaintPipeline
   from hy3dgen.shapegen import Hunyuan3DDiTFlowMatchingPipeline

   shape_pipe = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained("tencent/Hunyuan3D-2mini")
   result = shape_pipe(image="tower_front.png")
   result.export("tower.glb")
   ```
3. **출력 포맷**: `.glb` (권장) 또는 `.obj`. UE가 둘 다 임포트 가능.

### 1-4. glTF/FBX export 설정

- **glTF (.glb)**: 텍스처 내장 → 임포트 시 자동 언팩. 권장.
- **FBX**: Blender로 중간 처리 시 사용. `File → Export → FBX` → Mesh only (No Armature for static).

---

## 2. 클라우드 브랜치 — 캐릭터 (Meshy / Tripo)

> **주의**: 계정 가입 및 모델 생성은 **사용자가 직접** 수행. 에이전트는 대신 가입하지 않음.

### 2-1. 서비스 비교

| 항목 | Meshy | Tripo |
|------|-------|-------|
| URL | meshy.ai | tripo3d.ai |
| 무료 크레딧 | 신규 가입 시 제공 | 신규 가입 시 제공 |
| 자동 리깅 | O (Animate 기능) | O (Rigging 기능) |
| T포즈 export | O (FBX) | O (FBX) |
| 텍스처 | O (자동 생성) | O (자동 생성) |
| 권장 사유 | 리깅 품질 안정적 | 메시 디테일 우수 |

### 2-2. 캐릭터 생성 → export 설정

1. **텍스트/이미지 → 3D**: 캐릭터 설명 또는 참조 이미지 입력.
   (참조 이미지는 **§10 레시피**로 생성한 A포즈 정면 컨셉을 업로드하면 art-direction 통제 ↑)
2. **Animate (리깅 단계)**: Meshy 기준 "Animate" 버튼 → 자동 T포즈 스켈레톤 생성.
   - T포즈가 맞지 않으면 수동 bone 편집 가능 (웹 에디터 내).
3. **Export 설정**:
   - Format: **FBX** (UE 임포트 최적)
   - Pose: **T-pose** (필수 — A포즈는 IK Retarget 시 오차 발생)
   - Include: Mesh + Skeleton + Textures
   - Scale: 1.0 (UE에서 스케일 조정 예정)
4. **다운로드**: `<캐릭터명>_T.fbx` 등 명확한 이름으로 저장.

### 2-3. 다운로드 후 파일 배치

```
E:\Unreal Project\TDProject\Mcp_Tools\Asset_Pipeline\RawAssets\3D\
├── Characters\
│   ├── Alex_T.fbx          # Meshy/Tripo에서 받은 T포즈 FBX
│   ├── Alex_Diffuse.png
│   ├── Vega_T.fbx
│   └── ...
└── Structures\
    ├── Tower_Team1.glb     # Hunyuan3D/TRELLIS 출력
    ├── Tower_Team2.glb
    ├── CommandCenter.glb
    └── ...
```

---

## 3. UE 임포트 레시피

### 3-1. 캐릭터 FBX 임포트 (Skeletal Mesh)

에디터 Content Browser에서 `Content/AOS/Meshes/Characters/` 폴더로 이동:

1. **Import** 클릭 → FBX 선택
2. **FBX Import Options**:
   - `Mesh` 탭:
     - Import as: **Skeletal Mesh** (체크)
     - Skeleton: **비워둠** → 새 스켈레톤 자동 생성 (`SK_Alex` 등)
   - `Material` 탭:
     - Import Materials: **On**
     - Import Textures: **On**
   - `Transform` 탭:
     - Import Uniform Scale: `1.0` (실제 크기 확인 후 IK Retarget 단계에서 조정 가능)
3. **Import All** 클릭 → 생성 에셋:
   - `SKM_Alex` (Skeletal Mesh)
   - `SK_Alex` (Skeleton)
   - `MI_Alex_*` (머티리얼 인스턴스)
   - `T_Alex_*` (텍스처)

#### 임포트 주의사항

- **머티리얼 경로**: Import 시 자동 생성된 머티리얼이 잘못된 폴더에 생길 수 있음.
  생성 후 `Content/AOS/Meshes/Characters/Alex/Materials/` 등으로 이동.
- **스케일**: UE 기본 단위는 cm. Meshy/Tripo FBX가 m 단위이면 `100` 배 스케일로 임포트됨 →
  Import Scale을 `1.0`으로 맞추거나, SK 임포트 후 Preview 뷰포트에서 캡슐과 비교해 조정.
- **법선 방향**: 머티리얼이 검게 보이면 FBX Import Options → `Normals` → `Import Normals and Tangents`.

### 3-2. 구조물 glTF/FBX 임포트 (Static Mesh)

`Content/AOS/Meshes/Structures/` 폴더에서:

1. **Import** → `.glb` 또는 `.fbx` 선택
2. **FBX Import Options**:
   - Import as: **Static Mesh** (기본값)
   - Combine Meshes: **On** (부품이 분리된 경우 합치기)
3. 생성 에셋: `SM_Tower_Team1`, `SM_CommandCenter` 등

### 3-3. MCP 기반 일괄 임포트 (스크립트)

에디터가 실행 중이면 `Mcp_Tools/Asset_Pipeline/import_3d_assets.py`를 MCP `execute_script`로 호출:

```
# 캐릭터 FBX 임포트
import_3d_assets.py  skeletal  "E:/..../RawAssets/3D/Characters/Alex_T.fbx"  /Game/AOS/Meshes/Characters/Alex

# 구조물 glTF 임포트
import_3d_assets.py  static  "E:/..../RawAssets/3D/Structures/Tower_Team1.glb"  /Game/AOS/Meshes/Structures
```

---

## 4. IK Retargeter 절차 (캐릭터 전용)

이 단계가 가장 중요합니다. AI 생성 메시를 UE5 Mannequin 스켈레톤으로 맞춰야
**기존 ABP_AOSCharacter + 모든 몽타주가 그대로 동작**합니다.

### 4-1. IK Rig 생성 (소스: 새 캐릭터)

1. Content Browser → `SKM_Alex` 우클릭 → **Create IK Rig**
2. 자동 생성: `IKRig_Alex`
3. IK Rig 에디터에서:
   - Retarget Root: `pelvis` 또는 `Hips` (모델 구조에 따라)
   - Chain 설정: Spine, Left/Right Arm, Left/Right Leg, Head 체인 추가
   - 필수 본 이름 매핑 (Meshy/Tripo FBX 기준 일반 이름):
     ```
     Hips → pelvis
     Spine → spine_01
     LeftArm → upperarm_l
     RightArm → upperarm_r
     LeftLeg → thigh_l
     RightLeg → thigh_r
     ```

### 4-1b. 타깃 IK Rig 생성 (SK_Mannequin) — 최초 1회만

> ⚠️ 이 프로젝트엔 Mannequin IK Rig가 **없습니다**. `Mannequins/Rigs/` 에는
> Control Rig(`CR_Mannequin_Body/FootIK/Procedural`)와 Physics Asset(`PA_Mannequin`)
> 뿐이며, **Control Rig(CR_) ≠ IK Rig** 입니다. 리타깃 타깃용 IK Rig를 직접 만들어야 하고,
> 한 번 만들면 20종 전부 재사용합니다.

1. Content Browser → `Content/Characters/Mannequins/Meshes/SK_Mannequin` 우클릭 → **Create IK Rig**
2. 생성: `IKRig_Mannequin`
3. Retarget Root: `pelvis` / Chain: Spine(spine_01~05), Arm(upperarm·lowerarm·hand _l/_r),
   Leg(thigh·calf·foot _l/_r), Head

### 4-2. IK Retargeter 생성 (소스 → 타깃: SK_Mannequin)

1. Content Browser → 우클릭 → **Animation → IK Retargeter**
2. Source IK Rig: `IKRig_Alex` (4-1에서 만든 것)
3. Target IK Rig: `IKRig_Mannequin` (4-1b에서 직접 생성한 것)
4. Retargeter 에디터에서:
   - **Chain Mapping**: Source 체인 ↔ Target 체인 매핑
   - Preview: T포즈가 자연스럽게 맞으면 OK
   - 팔이 회전이 어긋나면 `Rotation Offset` 조정
5. **메시 리타깃(베이크)**: 우클릭 `SKM_Alex` → **Retarget Skeletal Mesh** → 위 IK Retargeter 선택
   → SK_Mannequin 에 바인딩된 새 메시(`SKM_Alex_Mannequin`) 생성.
   (런타임 리타깃은 ABP에 `Retarget Pose From Mesh` 노드가 필요하고 자동이 아님 →
    단일 ABP 유지를 위해 이 베이크 경로를 권장.)

### 4-3. BP_Char_* 슬롯 교체

에디터에서 `Content/Characters/BP_Char_Alex` 열기:

1. **Components 패널** → `Mesh (CharacterMesh0)` 선택
2. Details 패널:
   - `Skeletal Mesh Asset`: `SKM_Alex` (또는 Retarget된 `SKM_Alex_Mannequin`)
   - `Anim Class`: `ABP_AOSCharacter` **(유지 — 변경 금지)**

> ⚠️ **핵심**: `ABP_AOSCharacter`는 `SK_Mannequin` 스켈레톤에 컴파일돼 있어, **다른 스켈레톤을
> 가진 메시에는 평가되지 않습니다(레퍼런스 포즈로 굳음)**. 따라서 모든 캐릭터 메시가
> `SK_Mannequin` 하나를 공유해야 단일 ABP + 몽타주 8종이 20종 전부에 그대로 적용됩니다.

> **경로 A — 리타깃 불필요 (가능하면 최선)**: Meshy/Tripo가 "UE Mannequin 호환 rig" 프리셋을
> 제공하면, FBX 임포트 시 Skeleton을 **새로 만들지 말고 `SK_Mannequin` 지정**. 본 이름이 맞아
> 4-1·4-1b·4-2 리타깃 단계를 통째로 건너뜀.

> **경로 B — 리타깃 (일반·표준)**: 자체 스켈레톤으로 임포트한 경우 4-1~4-2를 거쳐 `SKM_Alex`를
> SK_Mannequin 으로 리타깃·베이크한 `SKM_Alex_Mannequin` 을 할당. ABP/몽타주 100% 동작.

### 4-4. 검증 체크리스트

PIE 실행 전:

- [ ] `ABP_AOSCharacter`가 Preview에서 Idle 애니메이션 재생 중
- [ ] Mesh가 캡슐 안에 올바르게 위치 (z=-90, yaw=-90 기본값)
- [ ] PIE 후 Montage(AM_Attack) 재생 시 애니메이션 나옴

PIE 실행 후:

- [ ] 캐릭터가 Spawn 후 자연스럽게 서 있음
- [ ] 공격 시 AM_Attack 몽타주 재생됨
- [ ] HitReact/Death 몽타주 정상 재생
- [ ] HP바가 캐릭터 위에 표시됨

---

## 5. 구조물 메시 교체

### 5-1. BP_Team1Tower / BP_Team2Tower

`Content/AOS/Structures/BP_Team1Tower` 열기:

1. **Components** → Static Mesh 컴포넌트 선택
2. `Static Mesh`: `SM_Tower_Team1` 으로 교체
3. Scale/Location 조정 (타워 크기 기준: 캐릭터 2배 높이 목표)
4. 컴파일 + 저장

### 5-2. BP_Team1CommandCenter / BP_Team2CommandCenter

동일 절차. 커맨드센터는 타워보다 크게 (1.5~2배 스케일 권장).

### 5-3. 머티리얼 팀 색상 적용

구조물 머티리얼에 팀 색상 파라미터가 있으면:
- Team1: Red계열 (`FLinearColor(1.0, 0.1, 0.1)`)
- Team2: Blue계열 (`FLinearColor(0.1, 0.1, 1.0)`)

머티리얼 인스턴스 생성: `MI_Tower_Team1`, `MI_Tower_Team2` 각각 팀 색상 파라미터 설정.

---

## 6. 에셋 폴더 구조 및 네이밍 컨벤션

기존 `Content/AOS/` 구조에 맞춰 하위 폴더 추가:

```
Content/AOS/
├── Anim/                           # 기존 — 변경 없음
│   ├── ABP_AOSCharacter.uasset
│   └── Montages/
├── GAS/                            # 기존 — 변경 없음
├── Structures/                     # 기존 — BP만 있음, SM 추가
│   ├── BP_Team1Tower.uasset
│   ├── BP_Team2Tower.uasset
│   ├── BP_Team1CommandCenter.uasset
│   ├── BP_Team2CommandCenter.uasset
│   └── [NEW] Meshes/               # 신규 추가
│       ├── SM_Tower_Team1.uasset
│       ├── SM_Tower_Team2.uasset
│       ├── SM_CommandCenter_Team1.uasset
│       └── SM_CommandCenter_Team2.uasset
├── UI/                             # 기존 — 변경 없음
│   └── Assets/
└── [NEW] Meshes/                   # 신규 추가 (캐릭터 전용)
    └── Characters/
        ├── Alex/
        │   ├── SKM_Alex.uasset
        │   ├── SK_Alex.uasset
        │   ├── IKRig_Alex.uasset
        │   ├── IKRetargeter_Alex.uasset
        │   └── Materials/
        │       ├── MI_Alex_Body.uasset
        │       └── T_Alex_Diffuse.uasset
        ├── Vega/
        ├── Ken/
        ├── Cammy/
        ├── Guile/
        └── Placeholder/            # Unit6~Unit20 공용 메시
```

### 네이밍 접두사 컨벤션

| 접두사 | 타입 | 예시 |
|--------|------|------|
| `SKM_` | Skeletal Mesh | `SKM_Alex`, `SKM_Vega` |
| `SK_` | Skeleton | `SK_Alex` |
| `SM_` | Static Mesh | `SM_Tower_Team1` |
| `IKRig_` | IK Rig | `IKRig_Alex` |
| `IKRetargeter_` | IK Retargeter | `IKRetargeter_Alex` |
| `MI_` | Material Instance | `MI_Alex_Body` |
| `T_` | Texture | `T_Alex_Diffuse` |
| `ABP_` | Animation Blueprint | `ABP_AOSCharacter` (공용, 변경 금지) |

---

## 7. 권장 파일럿 타깃 (첫 생성 + 검증)

전체 20캐릭터 + 구조물을 한 번에 교체하지 말고, 파일럿 2종으로 파이프라인을 먼저 검증합니다.

### 파일럿 A — 구조물 (로컬 Hunyuan3D)

**타깃**: 팀1 타워 (`BP_Team1Tower`)
- 단순 기하 구조 → 3D 생성 품질 확인 쉬움
- Static Mesh → 리깅 불필요
- PIE에서 시각 확인 즉각 가능

**프롬프트 예시** (Hunyuan3D / ComfyUI):
```
fantasy stone tower, medieval, simple geometric shape, game asset, top-down view
```

### 파일럿 B — 캐릭터 (클라우드 Meshy)

**타깃**: `BP_Char_Alex` (가장 많은 스킬 몽타주 보유 → 리타깃 검증에 최적)
- Q/W/E/R 스킬 몽타주 4종 + 기본 공격 몽타주 → IK Retarget 품질을 가장 넓게 검증 가능
- 고유 5종 중 첫 번째 → 성공 시 나머지 4종 동일 패턴 적용

**Meshy 설정**:
```
Text: "muscular male fighter, short hair, sleeveless jacket, fighting stance, T-pose"
Style: Game character
```

---

## 8. MCP 연동 현황

MCP `mcp__unreal-engine__*` 도구가 연결돼 있으면 에디터 Python을 원격 실행 가능.
`Mcp_Tools/Asset_Pipeline/import_3d_assets.py` (→ 아래 섹션)가 FBX/glTF 일괄 임포트 제공.

MCP 미연결 시에는 에디터 **콘텐츠 브라우저 → Import** 수동 진행.

---

## 9. 사용자가 직접 해야 할 단계

에이전트가 대신할 수 없는 작업 목록:

### GPU/로컬 환경

- [ ] GPU VRAM 확인 (RTX 3080 = 10 GB — Hunyuan3D-2.0-mini 또는 TRELLIS 선택)
- [ ] ComfyUI-3D-Pack 설치 (`git clone` + `pip install`)
- [ ] Hunyuan3D-2.0-mini 모델 다운로드 (~6~10 GB, HuggingFace 계정 필요)
- [ ] ComfyUI 실행 확인 (`run_nvidia_gpu.bat`)

### 클라우드 계정

- [ ] Meshy (meshy.ai) 계정 가입 + 무료 크레딧 확인
  또는 Tripo (tripo3d.ai) 계정 가입
- [ ] 캐릭터 텍스트/이미지 입력 → 3D 생성 → Animate(리깅) → FBX(T포즈) export

### UE 에디터 작업 (에이전트가 MCP로 일부 자동화 가능하나 최종 확인은 사용자)

- [ ] FBX Import Options에서 스켈레톤/스케일 설정 확인
- [ ] IK Rig 본 체인 매핑 (모델마다 본 이름 다름 → 수동 확인 필요)
- [ ] ABP_AOSCharacter Preview에서 Idle/Attack 몽타주 재생 확인
- [ ] PIE 실행 후 캐릭터 시각 검증

---

## 10. 컨셉 이미지 생성 레시피 (ComfyUI — 검증된 파일럿 설정)

3D 변환 전, image→3D 입력으로 쓸 **2D 컨셉 이미지**를 로컬 ComfyUI(`:8188`)로 생성합니다.
2026-06 파일럿(타워 + Alex)에서 검증된 설정 — 20종 롤아웃 시 그대로 재현하세요.

### 도구 / 체크포인트
- 스크립트: `Mcp_Tools/Asset_Pipeline/gen_icon.py` (범용 txt2img; `--style`/`--neg`/`--ckpt` 오버라이드)
- 체크포인트: **`dreamshaperXL_lightningDPMSDE.safetensors`** — 반실사 PBR 렌더 톤이라
  애니풍(`animagineXL`, `illustriousXL`)보다 3D 복원에 유리. Lightning이라 8 step 고속.
- ⚠️ 3D 복원용 스타일은 UI 아이콘 기본값과 **반대**: rim light·다크 배경·페인터리 금지 →
  **평탄 조명 + 밝은 단색 배경 + 렌더 톤 + 단일 객체 전체 프레임**.

### 공통 네거티브(NEG)
```
multiple objects, multiple views, collage, two characters, cropped, cut off, out of frame,
dramatic rim light, hard cast shadows, busy cluttered background, environment scenery,
ground plane, text, letters, watermark, signature, logo, blurry, lowres, jpeg artifacts,
motion blur, dynamic action pose, weapon swing
```

### 구조물(타워) — STYLE
```
single standalone 3D game asset, full structure centered and complete in frame,
three-quarter elevated view, even soft studio lighting, plain light grey seamless background,
clean stylized PBR render look, no ground shadow, no scenery, photogrammetry reference, high detail
```
```
python gen_icon.py --name Tower_Concept --count 4 \
  --ckpt dreamshaperXL_lightningDPMSDE.safetensors --style "<STYLE_TOWER>" --neg "<NEG>" \
  --prompt "a single fantasy stone defense tower for a MOBA game, fortified medieval turret with battlements, sturdy tapered base, team banner, complete top-to-bottom structure"
```
> 받침(잔디 디오라마)이 같이 잡히면 복원 메시에 바닥이 포함됨. 깔끔한 단일 타워를 원하면
> 프롬프트에 `single slim watchtower, isolated object, no base, no grass` 추가.

### 캐릭터 — STYLE
```
full body character design reference, single character centered and complete in frame,
standing relaxed A-pose, arms slightly away from torso, straight front view,
even soft studio lighting, plain light grey seamless background, clean stylized PBR render look,
no dramatic shadows, high detail
```
> 자동 리깅(Meshy/Tripo) 품질을 위해 **망토·무기 없는 A포즈**가 핵심. NEG의 `weapon swing` /
> `dynamic action pose` 로 액션 포즈를 억제. (망토·검이 나오면 복원/리깅 시 굳은 지오메트리)

### 배경 제거 (선택 — rembg)
- 생성물은 회색 배경. Hunyuan3D/Meshy/Tripo가 입력 시 자체 세그먼트하므로 **필수 아님**.
- 투명 PNG가 필요하면 (rembg 설치 후 — `_cutout.png` 생성):
```
python -c "from rembg import remove; from PIL import Image; remove(Image.open('RawAssets/Tower_Concept_3.png').convert('RGBA')).save('RawAssets/Tower_Concept_3_cutout.png')"
```
- 설치: `pip install "rembg[cpu]" pillow`
  (⚠️ `pip install -r requirements.txt` 는 파일의 한글 주석을 Windows가 cp949로 디코드하다 실패 →
   **패키지를 직접 지정**해 우회. rembg 설치 시 numpy가 상향되어 opencv-python 핀과 경고가 날 수 있음.)

### 출력 / 다음
- `Mcp_Tools/Asset_Pipeline/RawAssets/<name>_0..N.png` (+ `_cutout.png` 투명본).
- 선택본을 §1(타워 → Hunyuan3D image→3D) / §2(캐릭터 → Meshy/Tripo image→3D + Animate) 입력으로 사용.

---

## 11. 캐릭터 애니메이션 통합 — 핵심 발견 + AccuRig 파이프라인 (2026-06-18)

Gemini→Meshy 캐릭터를 **인게임 애니메이션이 동작하도록** 통합하며 확정된 사실들. (Alex/char1 실험 기록)

### 결정적 사실 — 게임 스켈레톤
- 게임 전체(`ABP_AOSCharacter` + 모든 몽타주 + 로코모션 `Boss_Idle`/`Boss_Run_F_InP`)가 쓰는 스켈레톤은
  **`/Game/BossyEnemy/SkeletalMesh/SK_Mannequin_UE4_WithWeapon_Skeleton`** (UE4 마네킹, BossyEnemy 팩).
  **UE5 마네킹(`SK_Mannequin`/`SKM_Manny_Simple`)이 아님!** (코드 폴백은 UE5지만 BP_Char_*는 UE4 마네킹 메시 사용)
- 69본: `root → pelvis → spine_01~03 → neck_01 → head`, `clavicle/upperarm/lowerarm/hand _l/r`(+손가락·트위스트),
  `thigh/calf/foot/ball _l/r`, `ik_*`, `weapon`.

### Meshy = Mixamo 본 네이밍 (매핑표)
| Mixamo (Meshy) | UE4 마네킹 |
|---|---|
| Hips | pelvis |
| **Spine02→Spine01→Spine** (이름 역순! Spine02가 골반쪽) | spine_01→02→03 |
| neck / Head | neck_01 / head |
| Left/RightShoulder | clavicle_l/r |
| Left/RightArm→ForeArm→Hand | upperarm→lowerarm→hand |
| Left/RightUpLeg→Leg→Foot→ToeBase | thigh→calf→foot→ball |

### UE5.7 한계 (전부 확인됨 — 시간 낭비 방지)
- ❌ 우클릭 **메시 리타깃 메뉴 없음** (Skeleton / Create / Asset Actions 서브메뉴 다 확인).
- ❌ IK Retargeter는 **애니만** export (메시 리스킨 기능 없음).
- ❌ Python `IKRetargetBatchOperation.duplicate_and_retarget` = **AnimSequence/Montage만** 처리, **메시·AnimBlueprint는 스킵**(빈 결과).
- ❌ AnimBlueprint 자동 리타깃 불가 ("리타깃 애니메이션"은 애니 전송 창일 뿐).

### ⚠️ Blender 왕복의 함정 (= 뒤틀림의 원인)
- Blender FBX 임포트가 **UE 본 방향(orientation)을 자체 규칙으로 변환** → 재export 시 원래 UE 방향과 어긋남(upperarm 등 ~90° 차이).
- 증상: **정지(ref)포즈는 멀쩡, 애니 적용 시 메시가 뒤틀림.** (병합은 되는데 PIE에서 꼬임)
- 즉 **Mixamo/오토릭/Blender 경유 리스킨은 UE 마네킹 본 방향을 안 지켜서** 인게임 애니가 깨짐. `send2ue`는 방향 보존하지만 **Blender 5.1과 비호환**(send2ue 2.4.3은 4.x용).

### ✅ 검증된 정답 = AccuRig + 호환 스켈레톤 + 트랜슬레이션 리타겟팅 (2026-06-18, Alex/char1 PIE 확인)

인게임 **idle/run/공격까지 정상** 확인. 전체 흐름:

**1. Gemini 이미지 → Meshy 메시(텍스처 포함)**

**2. AccuRig(무료) 자동 리깅 → FBX export (Unreal 프로파일 + T-pose)**
- 산출물 = **UE 표준 본 네이밍**(root/pelvis/spine_01…/clavicle_l/upperarm_l/…/thigh_l/foot_l + ik_* 포함) + **UE 좌표계 본 방향 보존** → Blender 90° 뒤틀림 함정 원천 회피.
- 단 AccuRig는 **118본**(spine_01~05 + cc_base_* 표정/트위스트/가슴/발가락 + metacarpal) → 게임 마네킹 **69본(spine 3개)과 정확히 일치하진 않음**.

**3. UE 임포트 — 새 스켈레톤 생성** (`skeleton=기존마네킹` ❌)
- 118 vs 69 본 트리 mismatch라 기존 마네킹 지정 시 **병합 실패** → `opts.skeleton=None`(새 `char1_accurig_Skeleton` 생성).
- char1이 마네킹 **핵심 본을 전부 보유**(없는 건 트위스트 8 + `weapon`뿐) → 호환 가능.

**4. 호환 스켈레톤 등록 (리타깃·전용 ABP·몽타주 복제 전부 불필요!)**
```python
char_skel.add_compatible_skeleton(mann_skel)
unreal.EditorAssetLibrary.save_asset(char_skel_path, False)
```
→ 기존 `ABP_AOSCharacter` + 모든 몽타주를 char1 메시에 **그대로** 사용(본 이름 매칭 포즈 복사).

**5. ⚠️ 본 트랜슬레이션 리타겟팅 수정 (안 하면 메시가 늘어남!)**
- 증상: FBX 임포트 기본값 = **모든 본 `Animation` 모드** → 마네킹 애니(키~180)의 본 **위치값**까지 char1(키~170)에 적용 → **길쭉하게 늘어남**. (ref포즈/정지 프리뷰는 멀쩡, **애니 재생 시에만** 드러남)
- 수정: `root=Animation`, `pelvis=AnimationScaled`, **나머지 전부 `Skeleton`**.
- Python 직접 setter 없음 → **`bone_tree` item-assignment write-through** 트릭:
```python
sk = unreal.load_object(None, char_skel_path)
comp = unreal.SkeletalMeshComponent(); comp.set_skeletal_mesh_asset(mesh)
names = [str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())]  # 인덱스 = bone_tree 인덱스
bt = sk.get_editor_property('bone_tree')  # BoneNode 배열. 인덱싱=복사본 반환→개별 mutate 무효
A,AS,S = (unreal.BoneTranslationRetargetingMode.ANIMATION,
          unreal.BoneTranslationRetargetingMode.ANIMATION_SCALED,
          unreal.BoneTranslationRetargetingMode.SKELETON)
for i in range(len(bt)):
    m = A if names[i]=='root' else (AS if names[i]=='pelvis' else S)
    node = bt[i]; node.set_editor_property('translation_retargeting_mode', m); bt[i] = node  # ← item 할당이 write-through
unreal.EditorAssetLibrary.save_asset(char_skel_path, False)
```
- ⚠️ 함정: `sk.set_editor_property('bone_tree', …)` 는 **read-only**(배열 통째 set 불가). `bt[i]=node` **item 할당만** 실제로 써짐(새로 get 해 검증). BoneNode `name`은 protected → 메시 `get_bone_name(i)`로 인덱스 매핑.

**6. BP_Char_* 배선**: 메시 컴포넌트 `skeletal_mesh_asset`=char1, **AnimClass는 `ABP_AOSCharacter` 그대로**. `save_asset(only_if_is_dirty=False)`.
- 키 차이(170 vs 180)로 발이 뜨거나 묻히면 메시 컴포넌트 Z 오프셋(기본 -80) 조정.

> 폐기된 시도: ① 외부 Blender 리스킨(`char1_ue4_rig.fbx`) 본 방향 90° 어긋남 ② Blender 기본 export 방향/병합 깨짐 ③ send2ue=Blender 5.1 비호환 ④ `skeleton=기존마네킹` 직접 지정=118 vs 69 본 트리 mismatch 병합 실패. **→ AccuRig + 호환 스켈레톤 + 리타겟팅 수정이 검증된 유일 경로.**

### 벤픽 3D 프리뷰
- `AAOSCharacterPreviewStage`도 ABP로 포즈를 적용하므로, 위 5단계까지 끝낸 char1은 프리뷰에서도 정상(과거 Blender 리스킨은 프리뷰에서도 뭉개졌으나 AccuRig 경로는 해소).

### 임포트 헬퍼 (검증됨)
- FBX 스켈레탈 임포트(새 스켈레톤) + PBR 머티리얼 + 호환 스켈레톤 등록 + 리타겟팅 수정 + `BP_Char_*` 메시 배선 = 전부 `system_control execute_python`(MCP)로 자동화.
- 시각 검증: `SkeletalMeshActor` 스폰 → `comp.play_animation(Boss_Run_F_InP, True)` 로 **애니 재생 상태** 캡처(ref포즈론 늘어남 안 보임). ⚠️ UE 파이썬 `Rotator(roll, pitch, yaw)` 순서 주의(카메라 셋업).

---

## 11. 커스텀 애니메이션 워크플로 (Blender 왕복) — A4 검증완료 2026-06-21

기존 마켓/Mixamo 애니를 Blender로 수정·과장해 캐릭터 시그니처 동작을 만드는 **하이브리드 워크플로**. 검증: `AM_Alex_R`(= `Boss_Attack_Uppercut_RM` 래핑) → Blender 척추 lean 수정 → **무왜곡 왕복** 확인.

**전제**: Alex 스킬 애니는 **마네킹 스켈레톤(`SK_Mannequin_UE4_WithWeapon_Skeleton`)** 위에 있고 char1_accurig엔 호환 스켈레톤으로 재생됨 → 애니 수정/임포트는 **마네킹 스켈레톤 기준**. 몽타주(`AM_Alex_*`)는 Anim Sequence를 래핑(소스는 `get_dependencies`로 확인).

**1) UE 애니 → FBX export** (Python/MCP)
```python
t=unreal.AssetExportTask(); t.set_editor_property('object', anim_seq)
t.set_editor_property('filename', out_fbx); t.set_editor_property('automated', True)
t.set_editor_property('exporter', unreal.AnimSequenceExporterFBX())
unreal.Exporter.run_asset_export_task(t)
```

**2) Blender 임포트 — ⭐핵심⭐**
```python
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)   # ← False 필수
```
⚠️ `automatic_bone_orientation=True`(기본)면 Blender가 본을 재정렬 → 재export 시 UE 본 축과 어긋나 **메시 심각 왜곡**(동작은 맞는데 몸이 꼬임 — A4 1차 실패 원인). `False`로 UE 본 축 보존(Blender에서 본이 못생겨 보여도 포즈 편집은 정상).

**3) Blender 편집** — 포즈모드/data API로 키프레임 수정(예: 마무리 척추 lean). 본은 `rotation_mode='QUATERNION'`(FBX 기본) 유지하며 `pb.rotation_quaternion @ offset` 후 `keyframe_insert`. ops는 컨텍스트 부족으로 실패 → `temp_override(window=...)` + data API.

**4) Blender export** — ⚠️ **씬 프레임 범위 = 액션 범위로 먼저 맞출 것**(`scene.frame_end=int(action.frame_range[1])`) — 안 하면 기본 250프레임까지 정지 프레임이 붙어 길이가 늘어남(6.4s→8.3s 버그).
```python
bpy.ops.export_scene.fbx(filepath=out, object_types={'ARMATURE'}, add_leaf_bones=False,
  bake_anim=True, axis_forward='-Z', axis_up='Y', primary_bone_axis='Y', secondary_bone_axis='X')
```

**5) UE 재임포트** (`FbxImportUI`, anim)
```python
ui=unreal.FbxImportUI(); ui.set_editor_property('import_mesh', False)
ui.set_editor_property('import_animations', True); ui.set_editor_property('skeleton', mannequin_skel)
ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)
```
- 검증: `sequence_length`가 원본과 일치하는지(예 6.43s). **시각 확인 필수** — 본 왜곡은 데이터로 안 잡히고 재생 메시로만 보임.

**6) 몽타주 배선** — ⚠️ 몽타주 슬롯 애니 교체는 **Python 비노출**(`slot_animation_tracks` 접근 불가) → **몽타주 에디터에서 수동**으로 새 Anim Sequence를 슬롯에 드래그/Replace. 또는 `SkillMontages` TMap(GameplayTag 키)에 새 몽타주 할당. `AOSAnimNotify_AttackHit`를 타격 프레임에 배치(데미지 타이밍).

> **함정 3종**: ① import `automatic_bone_orientation=False`(왜곡 방지 핵심) ② export 전 씬 프레임 범위=액션 범위 ③ 몽타주 슬롯 편집은 에디터 수동. 본 이름은 마네킹=AccuRig 공통(pelvis/spine/upperarm/hand…)이라 리타겟 불필요.

---

## 12. 참고 링크

- ComfyUI-3D-Pack: https://github.com/MrForExample/ComfyUI-3D-Pack
- Hunyuan3D-2: https://huggingface.co/tencent/Hunyuan3D-2
- Hunyuan3D-2-mini: https://huggingface.co/tencent/Hunyuan3D-2mini
- TRELLIS: https://github.com/microsoft/TRELLIS
- Meshy: https://www.meshy.ai
- Tripo: https://www.tripo3d.ai
- UE5 IK Retargeter 공식 문서: https://docs.unrealengine.com/5.7/en-US/ik-rig-animation-retargeting-in-unreal-engine/
- 기존 UI 아이콘 파이프라인: `Mcp_Tools/Asset_Pipeline/README.md`
- 스킬 몽타주 슬롯 정보: `Guides/03_Implementation/UPPER_LOWER_BODY_SPLIT.md`
- 캐릭터 로스터 상세: `CLAUDE.md` "캐릭터 로스터" 섹션
