// AI 하네스 (b): 첫 C++ Automation Test — "에이전트가 스스로 돌려 통과/실패를 읽는" 피드백 루프의 시작점.
//
// 대상: AAOSGameState::GetDraftSequence() — 벤픽 드래프트의 정적 14스텝 시퀀스(밴4 + 픽10, 팀당 밴2·픽5).
//   UWorld/에디터 인스턴스 불필요(순수 정적 데이터)라 헤드리스로 가볍게 돈다 → 첫 테스트로 이상적.
//
// ── 헤드리스 실행 (에디터 GUI 없이, 에이전트/CI 가 통과 여부를 자동 판독) ───────────────
//   & "$env:UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
//       "E:\Unreal Project\TDProject\TDProject.uproject" `
//       -ExecCmds="Automation RunTests TDProject.AOS; Quit" `
//       -unattended -nullrhi -nosplash -nopause -log
//   → 로그 마지막의 "... Test Completed. Result={Success|Fail}" / "Automation Test Succeeded" 로 판정.
//
// ── 에디터에서 실행 ──────────────────────────────────────────────────────────────────
//   Tools → Session Frontend → Automation 탭 → "TDProject.AOS.DraftSequence" 체크 → Start Tests.
//
// 신규 .cpp 추가 → 풀 리빌드(또는 프로젝트 파일 재생성 후 빌드) 필요. WITH_DEV_AUTOMATION_TESTS
// 가드라 Shipping 빌드에는 포함되지 않음.

#include "Misc/AutomationTest.h"
#include "AOSGameState.h"   // FAOSDraftStep, AAOSGameState::GetDraftSequence()
#include "AOSGameMode.h"    // EAOSTeam

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAOSDraftSequenceTest,
	"TDProject.AOS.DraftSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::ServerContext | EAutomationTestFlags::ProductFilter)

bool FAOSDraftSequenceTest::RunTest(const FString& /*Parameters*/)
{
	const TArray<FAOSDraftStep>& Seq = AAOSGameState::GetDraftSequence();

	// 1) 총 14스텝 (밴4 + 픽10). 실패 시 이후 인덱스 접근을 막기 위해 조기 반환.
	TestEqual(TEXT("드래프트 시퀀스 총 스텝 수"), Seq.Num(), 14);
	if (Seq.Num() != 14)
	{
		return false;
	}

	// 2) 앞 4스텝 = 밴, 교대 순서 T1, T2, T1, T2.
	const EAOSTeam ExpectedBanTeams[4] = {
		EAOSTeam::Team1, EAOSTeam::Team2, EAOSTeam::Team1, EAOSTeam::Team2
	};
	for (int32 i = 0; i < 4; ++i)
	{
		TestTrue(FString::Printf(TEXT("스텝 %d 은 밴이어야 함"), i), Seq[i].bBan);
		TestEqual(FString::Printf(TEXT("밴 스텝 %d 의 팀"), i),
			static_cast<int32>(Seq[i].Team), static_cast<int32>(ExpectedBanTeams[i]));
	}

	// 3) 뒤 10스텝 = 픽, 스네이크 순서 T1,T2,T2,T1,T1,T2,T2,T1,T1,T2.
	const EAOSTeam ExpectedPickTeams[10] = {
		EAOSTeam::Team1, EAOSTeam::Team2, EAOSTeam::Team2, EAOSTeam::Team1, EAOSTeam::Team1,
		EAOSTeam::Team2, EAOSTeam::Team2, EAOSTeam::Team1, EAOSTeam::Team1, EAOSTeam::Team2
	};
	for (int32 i = 0; i < 10; ++i)
	{
		const int32 Idx = 4 + i;
		TestFalse(FString::Printf(TEXT("스텝 %d 은 픽이어야 함"), Idx), Seq[Idx].bBan);
		TestEqual(FString::Printf(TEXT("픽 스텝 %d 의 팀"), Idx),
			static_cast<int32>(Seq[Idx].Team), static_cast<int32>(ExpectedPickTeams[i]));
	}

	// 4) 집계 불변식: 팀당 밴2·픽5 (전체 고유 드래프트의 핵심 균형 조건).
	int32 T1Ban = 0, T2Ban = 0, T1Pick = 0, T2Pick = 0;
	for (const FAOSDraftStep& Step : Seq)
	{
		if (Step.bBan)
		{
			(Step.Team == EAOSTeam::Team1 ? T1Ban : T2Ban)++;
		}
		else
		{
			(Step.Team == EAOSTeam::Team1 ? T1Pick : T2Pick)++;
		}
	}
	TestEqual(TEXT("Team1 밴 수"), T1Ban, 2);
	TestEqual(TEXT("Team2 밴 수"), T2Ban, 2);
	TestEqual(TEXT("Team1 픽 수"), T1Pick, 5);
	TestEqual(TEXT("Team2 픽 수"), T2Pick, 5);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
