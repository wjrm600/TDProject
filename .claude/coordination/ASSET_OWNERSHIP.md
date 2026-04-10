# 에셋 소유권 매핑

Content/ 디렉토리 에셋의 도메인/에이전트별 소유권을 정의합니다.
동일 에셋에 두 에이전트가 동시에 MCP 수정하는 것을 방지합니다.

---

## 아트 도메인

### art-visual 소유

| 에셋 경로 | 설명 |
|-----------|------|
| Content/Characters/Mannequins/Materials/ | 캐릭터 머티리얼 |
| Content/Characters/Mannequins/Textures/ | 캐릭터 텍스처 |
| Content/LevelPrototyping/Materials/ | 레벨 프로토타입 머티리얼 |
| Content/LevelPrototyping/Meshes/ | 레벨 프로토타입 메시 |
| Content/AOS/Materials/ (신규) | AOS 전용 머티리얼 |
| Content/AOS/Meshes/ (신규) | AOS 전용 메시 |

### art-vfx 소유

| 에셋 경로 | 설명 |
|-----------|------|
| Content/AOS/VFX/ (신규) | AOS 전용 VFX |
| Content/*/VFX/ | 기타 VFX 시스템 |

### art-anim 소유

| 에셋 경로 | 설명 |
|-----------|------|
| Content/Characters/Mannequins/Anims/ | 애니메이션 시퀀스, 몽타주, ABP |
| Content/AOS/Animations/ (신규) | AOS 전용 애니메이션 에셋 |

---

## 기획자 도메인

### design-balance 소유

Blueprint 인스턴스의 EditAnywhere 프로퍼티값을 관리합니다.
(Blueprint 자체의 구조는 프로그래머 소유)

| Blueprint | 관리 프로퍼티 |
|-----------|-------------|
| BP_Character | MaxHealth, AttackDamage, AttackRange, AttackCooldown, MovementSpeed |
| BP_Team1Tower, BP_Team2Tower | MaxHealth, AttackDamage, AttackRange, AttackCooldown |
| BP_Team1CommandCenter, BP_Team2CommandCenter | MaxHealth |
| BP_AOSAIController | EnemyDetectionRange, AttackRange |
| BP_AOSPlayerController | CameraHeight, CameraPitch, CameraYaw, CameraMoveSpeed, ZoomSpeed, MinZoomHeight, MaxZoomHeight, MapBoundaryX, MapBoundaryY |
| BP_ThirdPersonGameMode | GameDuration |

### design-level 소유

| 에셋 경로 | 설명 |
|-----------|------|
| Content/AOS/Lvl_ThirdPerson.umap | AOS 메인 레벨 (액터 배치) |
| MapManager의 LanesInfo 설정 | 레인 위치, 타워 위치, 커맨드센터 위치 |
| SpawnPoint 액터 배치 및 설정 | Team, Lane, SpawnIndex, bSpawnEnabled |

### design-docs 소유

| 경로 | 설명 |
|------|------|
| Guides/ 전체 | 기술/기획 문서 |
| CLAUDE.md | 프로젝트 루트 가이드 |

---

## 프로그래머 도메인

프로그래머 에이전트는 C++ 소스 파일을 소유합니다.
소유권은 INTERFACE_CONTRACTS.md에 정의되어 있습니다.

---

## 충돌 방지 규칙

1. **동시 수정 금지**: 같은 Blueprint 에셋을 두 에이전트가 동시에 MCP로 수정하지 않음
2. **AGENT_STATUS.md 확인**: 작업 시작 전 해당 에셋에 다른 에이전트가 활성 상태인지 확인
3. **C++ 구조 변경 시**: 프로그래머가 UPROPERTY를 추가/삭제하면 design-balance에 통보 필요
