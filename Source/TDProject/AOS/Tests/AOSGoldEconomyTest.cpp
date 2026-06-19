// AI 하네스 (A): 골드 경제 산식 테스트 — (b) 드래프트 테스트에 이은 두 번째 Automation Test.
//
// 골드 산식 (AOSGameMode / AOSGameState):
//   · 캐릭터 처치  → 죽은 캐릭터의 "반대 팀" 에 GoldPerCharacterKill   (기본 50)
//   · 구조물 파괴  → 구조물 소유 팀의 "반대 팀" 에 GoldPerStructureKill (기본 150)
//   · 라운드 시작  → 양 팀에 GoldPerRoundIncome                        (기본 100)
//   · 누적(ServerAddGold)은 FMath::Max(0, Gold+Amount) 로 0 미만 클램프(구매 차감 포함)
//
// awarding 자체는 GameMode/GameState 액터 메서드(월드/권한 필요)라 순수 단위테스트가 곤란 →
// 산식의 핵심인 "반대 팀" 매핑을 static AAOSGameMode::GetOpposingTeam() 으로 추출해 검증한다
// ("테스트되게 설계" — 동시에 코드 전반의 중복 ternary 통합).
// 보상 수치는 protected 라 GameMode CDO 에서 리플렉션으로 읽어 C++ 기본값을 고정한다
// (실제 런타임 값은 BP TDProj_GM override 가능 — 그건 데이터라 MCP get_property 로 별도 확인 영역).
//
// 실행: UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests TDProject.AOS; Quit" -unattended -nullrhi -nosplash -log

#include "Misc/AutomationTest.h"
#include "AOSGameMode.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAOSGoldEconomyTest,
	"TDProject.AOS.GoldEconomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::ServerContext | EAutomationTestFlags::ProductFilter)

// GameMode CDO 의 int32 UPROPERTY 를 리플렉션으로 읽는다(protected 우회). 못 찾으면 -1.
static int32 ReadGoldCDO(const TCHAR* PropName)
{
	const FIntProperty* Prop = FindFProperty<FIntProperty>(AAOSGameMode::StaticClass(), PropName);
	return Prop ? Prop->GetPropertyValue_InContainer(GetDefault<AAOSGameMode>()) : -1;
}

bool FAOSGoldEconomyTest::RunTest(const FString& /*Parameters*/)
{
	// 1) 반대-팀 매핑 (산식 핵심: 보상 대상 = 죽은/파괴된 쪽의 반대 팀)
	TestEqual(TEXT("GetOpposingTeam(Team1) == Team2"),
		(int32)AAOSGameMode::GetOpposingTeam(EAOSTeam::Team1), (int32)EAOSTeam::Team2);
	TestEqual(TEXT("GetOpposingTeam(Team2) == Team1"),
		(int32)AAOSGameMode::GetOpposingTeam(EAOSTeam::Team2), (int32)EAOSTeam::Team1);
	// 대칭성: 반대의 반대는 자기 자신
	TestEqual(TEXT("반대의 반대 == 자기자신 (Team1)"),
		(int32)AAOSGameMode::GetOpposingTeam(AAOSGameMode::GetOpposingTeam(EAOSTeam::Team1)),
		(int32)EAOSTeam::Team1);
	TestEqual(TEXT("반대의 반대 == 자기자신 (Team2)"),
		(int32)AAOSGameMode::GetOpposingTeam(AAOSGameMode::GetOpposingTeam(EAOSTeam::Team2)),
		(int32)EAOSTeam::Team2);

	// 2) 보상 수치 (C++ CDO 기본값, 리플렉션). 프로퍼티 rename 시 -1 → 아래 TestEqual 이 실패로 검출.
	const int32 KillGold = ReadGoldCDO(TEXT("GoldPerCharacterKill"));
	const int32 StructGold = ReadGoldCDO(TEXT("GoldPerStructureKill"));
	const int32 RoundGold = ReadGoldCDO(TEXT("GoldPerRoundIncome"));
	AddInfo(FString::Printf(TEXT("골드 산식 (C++ 기본값) — 처치 %d / 구조물 %d / 라운드패시브 %d"),
		KillGold, StructGold, RoundGold));
	TestEqual(TEXT("처치 보상 = 50"), KillGold, 50);
	TestEqual(TEXT("구조물 보상 = 150"), StructGold, 150);
	TestEqual(TEXT("라운드 패시브 = 100"), RoundGold, 100);

	// 3) 경제 설계 불변식 (밸런스가 비상식적으로 바뀌면 검출)
	TestTrue(TEXT("구조물 보상 > 캐릭터 보상"), StructGold > KillGold);
	TestTrue(TEXT("캐릭터 보상 > 0"), KillGold > 0);
	TestTrue(TEXT("라운드 패시브 > 0"), RoundGold > 0);

	// 4) 산식 합성: Team1 캐릭터가 죽으면 → Team2 가 처치 보상을 받는다
	const EAOSTeam Rewarded = AAOSGameMode::GetOpposingTeam(EAOSTeam::Team1);
	TestEqual(TEXT("Team1 처치 시 보상 팀 = Team2"), (int32)Rewarded, (int32)EAOSTeam::Team2);
	AddInfo(FString::Printf(TEXT("예시 — Team1 캐릭터 처치 시 Team2 가 +%d 골드"), KillGold));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
