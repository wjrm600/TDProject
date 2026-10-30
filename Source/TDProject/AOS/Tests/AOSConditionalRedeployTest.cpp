// 조건부 재배치 판정 테스트 — 직전 라운드에 RedeployLaneLossThreshold(기본 2) 이상 라인을 진 팀만
// 다음 준비 단계에서 배치를 바꿀 수 있다 (GAME_VISION "조건부 재배치").
//
// 판정 = static 순수 함수 2개 (월드 불필요):
//   · AAOSGameMode::CountLanesLost(LaneWinners, Team) — 라인별 승자(0=무승부, 1=Team1, 2=Team2) 중
//     상대 팀이 이긴 라인 수. 무승부는 패배가 아니다.
//   · AAOSGameMode::ShouldAllowRedeploy(LanesLost, Threshold, bHadDeployment) — 진 라인 >= 기준이면 자유.
//     직전 라운드에 아무도 배치하지 않았으면 예외로 자유(빈 배치로 잠기면 영영 못 풀리는 교착 방지).
// 기준값·규칙 스위치는 protected 라 GameMode CDO 에서 리플렉션으로 읽어 C++ 기본값을 고정한다.
//
// 실행: UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests TDProject.AOS; Quit" -unattended -nullrhi -nosplash -log

#include "Misc/AutomationTest.h"
#include "AOSGameMode.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAOSConditionalRedeployTest,
	"TDProject.AOS.ConditionalRedeploy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::ServerContext | EAutomationTestFlags::ProductFilter)

bool FAOSConditionalRedeployTest::RunTest(const FString& /*Parameters*/)
{
	const EAOSTeam T1 = EAOSTeam::Team1;
	const EAOSTeam T2 = EAOSTeam::Team2;

	// 1) 패배 라인 수 (Top/Mid/Bottom 승자)
	TestEqual(TEXT("T2 가 3라인 승 → T1 패배 3"), AAOSGameMode::CountLanesLost({ 2, 2, 2 }, T1), 3);
	TestEqual(TEXT("T2 가 3라인 승 → T2 패배 0"), AAOSGameMode::CountLanesLost({ 2, 2, 2 }, T2), 0);
	TestEqual(TEXT("1승 2패 → T1 패배 2"), AAOSGameMode::CountLanesLost({ 1, 2, 2 }, T1), 2);
	TestEqual(TEXT("1승 2패 → T2 패배 1"), AAOSGameMode::CountLanesLost({ 1, 2, 2 }, T2), 1);
	TestEqual(TEXT("무승부는 패배 아님 (2무 1패)"), AAOSGameMode::CountLanesLost({ 0, 0, 2 }, T1), 1);
	TestEqual(TEXT("전부 무승부 → 양 팀 패배 0"), AAOSGameMode::CountLanesLost({ 0, 0, 0 }, T1)
		+ AAOSGameMode::CountLanesLost({ 0, 0, 0 }, T2), 0);
	TestEqual(TEXT("결과 없음(빈 배열) → 패배 0"), AAOSGameMode::CountLanesLost({}, T1), 0);

	// 2) 재배치 허용 판정 (기준 2)
	TestTrue(TEXT("2라인 패배 → 재배치 가능"), AAOSGameMode::ShouldAllowRedeploy(2, 2, true));
	TestTrue(TEXT("3라인 패배 → 재배치 가능"), AAOSGameMode::ShouldAllowRedeploy(3, 2, true));
	TestFalse(TEXT("1라인 패배 → 배치 고정"), AAOSGameMode::ShouldAllowRedeploy(1, 2, true));
	TestFalse(TEXT("0라인 패배 → 배치 고정"), AAOSGameMode::ShouldAllowRedeploy(0, 2, true));
	TestTrue(TEXT("직전 배치 없음 → 패배 수와 무관하게 재배치 가능 (교착 방지)"),
		AAOSGameMode::ShouldAllowRedeploy(0, 2, false));

	// 3) 합성: 한 라운드 결과에서 두 팀 권한 (1승 2패 = 진 팀만 재배치)
	const TArray<int32> OneWinTwoLoss = { 1, 2, 2 };
	TestTrue(TEXT("1승 2패 — T1 재배치 가능"), AAOSGameMode::ShouldAllowRedeploy(
		AAOSGameMode::CountLanesLost(OneWinTwoLoss, T1), 2, true));
	TestFalse(TEXT("1승 2패 — T2 배치 고정"), AAOSGameMode::ShouldAllowRedeploy(
		AAOSGameMode::CountLanesLost(OneWinTwoLoss, T2), 2, true));

	// 1승 1패 1무 → 양 팀 모두 패배 1 → 둘 다 고정
	const TArray<int32> Mixed = { 1, 2, 0 };
	TestFalse(TEXT("1승 1패 1무 — T1 고정"), AAOSGameMode::ShouldAllowRedeploy(
		AAOSGameMode::CountLanesLost(Mixed, T1), 2, true));
	TestFalse(TEXT("1승 1패 1무 — T2 고정"), AAOSGameMode::ShouldAllowRedeploy(
		AAOSGameMode::CountLanesLost(Mixed, T2), 2, true));

	// 4) C++ 기본값 (리플렉션). 프로퍼티 rename 시 찾지 못해 실패로 검출.
	const FIntProperty* ThresholdProp = FindFProperty<FIntProperty>(AAOSGameMode::StaticClass(), TEXT("RedeployLaneLossThreshold"));
	const FBoolProperty* EnableProp = FindFProperty<FBoolProperty>(AAOSGameMode::StaticClass(), TEXT("bEnableConditionalRedeploy"));
	if (TestNotNull(TEXT("RedeployLaneLossThreshold 프로퍼티 존재"), ThresholdProp))
	{
		TestEqual(TEXT("재배치 기준 기본값 = 2라인"),
			ThresholdProp->GetPropertyValue_InContainer(GetDefault<AAOSGameMode>()), 2);
	}
	if (TestNotNull(TEXT("bEnableConditionalRedeploy 프로퍼티 존재"), EnableProp))
	{
		TestTrue(TEXT("조건부 재배치 기본 켜짐"),
			EnableProp->GetPropertyValue_InContainer(GetDefault<AAOSGameMode>()));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
